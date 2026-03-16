package engine

/*
#cgo CFLAGS: -I${SRCDIR}/../../veemu-engine/include -I${SRCDIR}/../../veemu-engine/unicorn/include
#cgo LDFLAGS: -L${SRCDIR}/../../veemu-engine/unicorn/build -lunicorn -L${SRCDIR}/../../veemu-engine -lveemu -lm -lpthread
#include "veemu.h"
#include <stdlib.h>
#include <stdio.h>

void registerCallbacks(veemu_board_t *board);
static veemu_err_t wrap_gpio_set_input(veemu_board_t *b, const char *port, uint8_t pin, int state) {
   return veemu_gpio_set_input(b, port, pin, (bool)state);
}
*/
import "C"
import (
	"errors"
	"sync"
	"unsafe"
)

type GPIOEvent struct {
	Port  string
	Pin   uint8
	State bool
}

type Instance struct {
	board    *C.veemu_board_t
	uartMu   sync.Mutex
	uartBuf  []byte
	gpioMu   sync.Mutex
	gpioEvts []GPIOEvent
}

var (
	instancesMu sync.Mutex
	instances   = map[unsafe.Pointer]*Instance{}
)

func NewInstance(boardCfgPath string) (*Instance, error) {
	cPath := C.CString(boardCfgPath)
	defer C.free(unsafe.Pointer(cPath))

	var board *C.veemu_board_t
	rc := C.veemu_board_create(cPath, &board)
	if rc != C.VEEMU_OK {
		return nil, errors.New("engine: veemu_board_create failed: " + goError(rc))
	}

	inst := &Instance{board: board}
	instancesMu.Lock()
	instances[unsafe.Pointer(board)] = inst
	instancesMu.Unlock()

	// Callbacks registered from callbacks.c via registerCallbacks()
	C.registerCallbacks(board)
	return inst, nil
}

func (i *Instance) SetGPIOInput(port string, pin uint8, state bool) error {
	cPort := C.CString(port)
	defer C.free(unsafe.Pointer(cPort))
	cState := C.int(0)
	if state {
		cState = C.int(1)
	}
	rc := C.wrap_gpio_set_input(i.board, cPort, C.uint8_t(pin), cState)
	if rc != C.VEEMU_OK {
		return errors.New("engine: gpio_set_input failed: " + goError(rc))
	}
	return nil
}

func (i *Instance) LoadELF(b []byte) error {
	cPath := C.CString("/tmp/veemu_flash.elf")
	cMode := C.CString("wb")
	defer C.free(unsafe.Pointer(cPath))
	defer C.free(unsafe.Pointer(cMode))
	f := C.fopen(cPath, cMode)
	if f == nil {
		return errors.New("engine: failed to open temp file")
	}
	buf := C.CBytes(b)
	defer C.free(buf)
	C.fwrite(buf, 1, C.size_t(len(b)), f)
	C.fclose(f)
	rc := C.veemu_board_load_elf(i.board, cPath)
	if rc != C.VEEMU_OK {
		return errors.New("engine: load_elf failed: " + goError(rc))
	}
	return nil
}

func (i *Instance) Reset() error {
	rc := C.veemu_board_reset(i.board)
	if rc != C.VEEMU_OK {
		return errors.New("engine: reset failed: " + goError(rc))
	}
	return nil
}

func (i *Instance) Run(maxInsn uint64) error {
	result := C.veemu_board_run(i.board, C.uint64_t(maxInsn))
	if result == C.VEEMU_RUN_FAULT {
		return errors.New("engine: fault during run")
	}
	return nil
}

func (i *Instance) Stop() {}

func (i *Instance) ReadUART() []byte {
	i.uartMu.Lock()
	defer i.uartMu.Unlock()
	out := make([]byte, len(i.uartBuf))
	copy(out, i.uartBuf)
	i.uartBuf = i.uartBuf[:0]
	return out
}

func (i *Instance) GPIOEvents() []GPIOEvent {
	i.gpioMu.Lock()
	defer i.gpioMu.Unlock()
	out := make([]GPIOEvent, len(i.gpioEvts))
	copy(out, i.gpioEvts)
	i.gpioEvts = i.gpioEvts[:0]
	return out
}

func (i *Instance) Destroy() {
	if i.board == nil {
		return
	}
	instancesMu.Lock()
	delete(instances, unsafe.Pointer(i.board))
	instancesMu.Unlock()
	C.veemu_board_free(i.board)
	i.board = nil
}

// These are called from callbacks.c — do NOT rename.

//export goUartByte
func goUartByte(b C.uint8_t, ctx unsafe.Pointer) {
	instancesMu.Lock()
	inst, ok := instances[ctx]
	instancesMu.Unlock()
	if !ok {
		return
	}
	inst.uartMu.Lock()
	inst.uartBuf = append(inst.uartBuf, byte(b))
	inst.uartMu.Unlock()
}

//export goGpioChange
func goGpioChange(port *C.char, pin C.uint8_t, state C.int, ctx unsafe.Pointer) {
	instancesMu.Lock()
	inst, ok := instances[ctx]
	instancesMu.Unlock()
	if !ok {
		return
	}
	inst.gpioMu.Lock()
	inst.gpioEvts = append(inst.gpioEvts, GPIOEvent{
		Port:  C.GoString(port),
		Pin:   uint8(pin),
		State: state != 0,
	})
	inst.gpioMu.Unlock()
}

func goError(rc C.veemu_err_t) string {
	return C.GoString(C.veemu_strerror(rc))
}
