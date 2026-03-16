# veemu-app

Firmware workspace for the Veemu STM32F401RE digital twin.

---

## Quick Start

```bash
# 1. Start API (Terminal 1)
cd ../veemu-api && VEEMU_BOARD_DIR=../veemu-engine/boards ./veemu-apiserver

# 2. Start frontend (Terminal 2)
cd ../veemu-frontend && python3 -m http.server 3000

# 3. Build and flash (Terminal 3)
cd ../veemu-app && make flash

# 4. Open browser → http://localhost:3000 → click Flash → select build/main.elf
```

---

## Structure

```
veemu-app/
├── main.c                        ← START HERE — call your apps
├── Makefile                      ← build + flash
├── README.md
│
├── platform/                     ← DO NOT EDIT
│   ├── startup.c                 ← Reset_Handler, vector table
│   ├── stm32f401re.h             ← register map (RM0368)
│   └── stm32f401re.ld            ← linker script
│
└── examples/
    ├── blink/
    │   ├── blink.h
    │   └── blink.c               ← toggles PA5 (LD2) 10 times
    └── led_patterns/
        ├── led_patterns.h
        └── led_patterns.c        ← SOS / Heartbeat / Double Flash + B1 button
```

---

## Adding a New Module

**1. Create your files:**
```
examples/my_app/
├── my_app.h    ← void my_app(void);
└── my_app.c    ← implementation
```

**2. Include and call in `main.c`:**
```c
#include "examples/my_app/my_app.h"

int main(void) {
    blink_app();
    ledpatterns_app();
    my_app();        // ← add here
    while (1) {}
}
```

**3. Add to `Makefile` under C_SOURCES:**
```makefile
C_SOURCES += examples/my_app/my_app.c
```

**4. Flash:**
```bash
make flash
```

---

## Hardware — STM32F401RE Nucleo-64

| Pin  | Function  | Notes               |
|------|-----------|---------------------|
| PA5  | LD2 LED   | Active HIGH         |
| PC13 | B1 Button | Active LOW, pull-up |
| PA2  | USART2 TX | 115200 @ 16MHz HSI  |

---

## Flash to Real Hardware

```bash
make
arm-none-eabi-objcopy -O binary build/main.elf build/main.bin
st-flash write build/main.bin 0x08000000
```
