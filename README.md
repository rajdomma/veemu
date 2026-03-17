# Veemu

Cloud-based STM32 digital twin platform — run, test, and debug ARM Cortex-M4
firmware without physical hardware. The same `.elf` binary runs identically
on both the digital twin and real STM32F401RE silicon.

---

## Quick Start

```bash
# 1. Build Unicorn engine (one-time setup)
cd unicorn && mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j4
cd ../..

# 2. Build everything
make all

# 3. Start servers
make start

# 4. Flash firmware and open browser
make APP=led_patterns flash
open http://localhost:3000
```

---

## Folder Structure

```
veemu/
├── veemu-app/              ← ★ YOUR WORKSPACE — write firmware here
│   ├── main.c              ← entry point (printf + puts work out of the box)
│   ├── Makefile
│   ├── platform/           ← board support layer (do not edit)
│   │   ├── startup.c       ← Reset_Handler → platform_init() → main()
│   │   ├── syscalls.c      ← malloc-free printf, _write → USART2
│   │   ├── platform_init.c ← UART, SysTick, GPIO init
│   │   ├── gpio.h/c        ← LED/button HW API
│   │   ├── uart.h/c        ← print HW API
│   │   ├── systick.h/c     ← g_tick, delay_ms()
│   │   ├── stm32f401re.h   ← register structs + base addresses
│   │   ├── platform_config.h ← auto-generated from board JSON
│   │   └── stm32f401re.ld  ← linker script
│   └── examples/
│       ├── blink/          ← 1Hz LED blink with printf
│       ├── led_patterns/   ← SOS / Heartbeat / Double Flash + B1 button
│       ├── tim_test/       ← TIM2 IRQ 1ms timing test
│       ├── spi_test/       ← SPI1 loopback transfer test
│       └── i2c_test/       ← I2C1 START/ADDR/DATA/STOP test
│
├── veemu-engine/           ← C emulator (Unicorn ARM Cortex-M4)
│   ├── src/
│   │   ├── core/
│   │   │   ├── engine.c    ← Unicorn setup, peripheral hooks, exception injection
│   │   │   └── elf_loader.c
│   │   └── peripherals/
│   │       ├── rcc.c       ← RCC clock enable model
│   │       ├── gpio.c      ← GPIO MODER/BSRR/IDR/ODR model
│   │       ├── uart.c      ← USART SR/DR/BRR/CR1 model + tx_cb
│   │       ├── systick.c   ← SysTick CVR countdown + IRQ injection
│   │       ├── tim.c       ← TIM PSC/ARR/SR + IRQ injection
│   │       ├── spi.c       ← SPI CR1/SR/DR loopback model
│   │       └── i2c.c       ← I2C CR1/SR1/SR2/DR master model
│   ├── include/
│   │   ├── veemu.h         ← public C API
│   │   └── peripheral.h    ← peripheral model interface
│   ├── boards/
│   │   └── stm32f401re.json ← board config (addresses, clocks, pins)
│   └── unicorn/            ← symlink → ../unicorn (Unicorn Engine 2.x)
│
├── veemu-api/              ← Go REST + WebSocket server
│   ├── api/
│   │   ├── flash.go        ← POST /flash — load ELF into engine
│   │   ├── live.go         ← WS /live — stream GPIO/UART events
│   │   ├── uart.go         ← GET /uart — drain UART buffer
│   │   ├── gpio_input.go   ← POST /gpio_input — inject button/pin state
│   │   ├── latest_session.go ← GET /latest-session — frontend auto-sync
│   │   └── sessions.go     ← GET/DELETE /sessions
│   ├── engine/             ← CGo bridge to veemu-engine
│   │   ├── engine.go       ← NewInstance, LoadELF, Run, ReadUART
│   │   └── callbacks.c     ← C callbacks → Go (goUartByte, goGpioChange)
│   └── session/
│       └── manager.go      ← session lifecycle, 1ms run loop
│
├── veemu-frontend/
│   └── index.html          ← browser UI (board view, console, GPIO)
│
└── unicorn/                ← Unicorn Engine (third-party, build separately)
    ├── CMakeLists.txt
    ├── build/              ← libunicorn.dylib/.so (after cmake build)
    └── include/
        └── unicorn/        ← unicorn.h
```

> **Users only need to touch `veemu-app/`.**
> Everything else is infrastructure.

---

## Platform HW API

Every example uses the platform HW API — no direct register access needed:

```c
#include "platform/gpio.h"    // gpio_led_on/off/toggle/get(), gpio_btn_read()
#include "platform/uart.h"    // uart_print(), uart_println(), uart_print_u32()
#include "platform/systick.h" // delay_ms(), g_tick
#include <stdio.h>            // printf() — fully supported, no FILE/malloc needed
```

**Printf is malloc-free** — implemented directly in `syscalls.c` using stack buffers.
Supports: `%s %c %d %i %u %lu %ld %x %X %08x %f %.Nf %%`

---

## Commands

| Command | Description |
|---|---|
| `make all` | Build engine + API |
| `make start` | Start API (:8080) + frontend (:3000) |
| `make stop` | Stop all servers |
| `make APP=led_patterns flash` | Build firmware + flash to digital twin |
| `make APP=blink flash` | Build blink example + flash |
| `make APP=tim_test flash-hw` | Flash to real STM32 via st-flash |
| `make clean` | Clean all build artifacts |
| `make help` | Show all commands |

---

## Adding a New Firmware App

1. Create `veemu-app/examples/my_app/my_app.h` and `my_app.c`
2. Add to `veemu-app/main.c`:
```c
#elif defined(APP_MY_APP)
  #include "examples/my_app/my_app.h"
```
3. Flash to digital twin:
```bash
make APP=my_app flash
```
4. Flash to real HW:
```bash
make APP=my_app flash-hw
```

---

## Hardware — STM32F401RE Nucleo-64

| Pin  | Function       | Notes                        |
|------|----------------|------------------------------|
| PA2  | USART2 TX      | 115200 baud, console output  |
| PA5  | LD2 LED        | Active HIGH                  |
| PC13 | B1 Button      | Active LOW, pull-up          |
| PB8  | I2C1 SCL       | AF4, open-drain, pull-up     |
| PB9  | I2C1 SDA       | AF4, open-drain, pull-up     |
| PA5  | SPI1 SCK       | AF5 (shared with LED)        |
| PA6  | SPI1 MISO      | AF5                          |
| PA7  | SPI1 MOSI      | AF5                          |

> For SPI loopback test on real HW: wire PA6 (MISO) to PA7 (MOSI).

---

## Unicorn Engine Setup

Veemu uses [Unicorn Engine](https://github.com/unicorn-engine/unicorn) 2.x
as the ARM Cortex-M4 emulation backend.

```bash
# Build Unicorn (one-time, from repo root)
cd unicorn
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
cd ../..
```

The built library (`libunicorn.dylib` on macOS, `libunicorn.so` on Linux)
is linked into `veemu-apiserver` via CGo at build time.

**Unicorn version:** 2.1.4+
**Architecture:** `UC_ARCH_ARM`, `UC_MODE_THUMB | UC_MODE_MCLASS`

### Peripheral Emulation

The engine models STM32F4 peripherals as register state machines:

| Peripheral | What's modeled |
|---|---|
| RCC | AHB1ENR, APB1ENR, APB2ENR clock enables |
| GPIO | MODER, BSRR, IDR, ODR — input injection via `/gpio_input` |
| USART2 | SR/DR/BRR/CR1 — TX bytes streamed via WebSocket |
| SysTick | CVR countdown, 1ms IRQ injection |
| TIM2-5 | PSC/ARR/SR, update IRQ injection |
| SPI1-3 | CR1/SR/DR, full-duplex loopback |
| I2C1-3 | CR1/SR1/SR2/DR, master START/ADDR/DATA/STOP |

**Key implementation notes:**
- Peripherals mapped as `UC_PROT_ALL` with page-aligned sizes
- `UC_HOOK_MEM_READ` (before-read) pre-loads model values into Unicorn memory
- `UC_HOOK_MEM_WRITE` captures firmware writes to peripheral registers
- `g_in_read_sync` guard prevents read-sync from triggering write hook

---

## Architecture

```
veemu-app                veemu-api              veemu-engine
(firmware .elf) ──────→ POST /flash ─────→ CGo → libveemu.a
                         WS /live   ←───── GPIO/UART callbacks
                         GET /uart  ←───── UART TX buffer drain
veemu-frontend ←──────── WebSocket stream ──────────────────
B1 button click ──────→ POST /gpio_input → IDR inject → firmware
```

---

## Requirements

| Tool | Version | Purpose |
|------|---------|---------|
| `arm-none-eabi-gcc` | 12+ | compile firmware |
| `go` | 1.21+ | build API server |
| `python3` | any | serve frontend |
| `curl` | any | flash via CLI |
| `cmake` | 3.x+ | build Unicorn |
| `st-flash` | any | flash to real STM32 HW |

---

## Digital Twin vs Real HW

The same `.elf` binary runs on both platforms. Verified on STM32F401RE Nucleo-64:

| Example | Digital Twin | Real HW | Notes |
|---|---|---|---|
| `blink` | ✅ | ✅ | 1Hz LED |
| `led_patterns` | ✅ | ✅ | SOS/Heartbeat/Double Flash |
| `tim_test` | ✅ PASS | ✅ PASS | TIM2 1ms IRQ (NVIC required on HW) |
| `spi_test` | ✅ PASS | ✅ SR flags | Wire PA6→PA7 for loopback |
| `i2c_test` | ✅ PASS | ✅ START/STOP | ADDR/BTF need slave device |