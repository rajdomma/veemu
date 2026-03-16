# Veemu

Cloud-based STM32 digital twin platform — run, test, and debug ARM Cortex-M4
firmware without physical hardware.

---

## Quick Start

```bash
# 1. Build everything
make all

# 2. Start servers
make start

# 3. Flash firmware and open browser
make APP=led_patterns flash
open http://localhost:3000
```

---

## Folder Structure

```
veemu/
├── veemu-app/          ← ★ YOUR WORKSPACE — write firmware here
│   ├── main.c          ← entry point
│   ├── Makefile
│   ├── platform/       ← do not edit (startup, linker, headers)
│   └── examples/
│       ├── blink/          ← toggles PA5 LED 10 times
│       └── led_patterns/   ← SOS / Heartbeat / Double Flash + B1 button
│
├── veemu-engine/       ← C emulator (Unicorn ARM Cortex-M4)
│   ├── src/            ← engine + peripheral models
│   ├── include/        ← public API (veemu.h)
│   └── boards/         ← board config JSON files
│
├── veemu-api/          ← Go REST + WebSocket server
│   ├── api/            ← HTTP handlers
│   ├── engine/         ← CGo bridge to veemu-engine
│   └── session/        ← session lifecycle manager
│
└── veemu-frontend/
    └── index.html      ← browser UI (board view, console, GPIO)
```

> **Users only need to touch `veemu-app/`.**
> Everything else is infrastructure.

---

## Commands

| Command | Description |
|---|---|
| `make all` | Build engine + API |
| `make start` | Start API (:8080) + frontend (:3000) |
| `make stop` | Stop all servers |
| `make APP=led_patterns flash` | Build firmware + flash to Veemu |
| `make APP=blink flash` | Build blink example + flash |
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
3. Flash:
```bash
make APP=my_app flash
```

---

## Hardware — STM32F401RE Nucleo-64

| Pin  | Function  | Notes               |
|------|-----------|---------------------|
| PA5  | LD2 LED   | Active HIGH         |
| PC13 | B1 Button | Active LOW, pull-up |
| PA2  | USART2 TX | 115200 baud         |

---

## Architecture

```
veemu-app        veemu-api            veemu-engine
(firmware .elf) → POST /flash  →  CGo → libveemu.a (Unicorn)
                  WS  /live    ←  GPIO / UART callbacks
veemu-frontend ← WebSocket stream → board UI
```

---

## Requirements

| Tool | Version | Purpose |
|------|---------|---------|
| `arm-none-eabi-gcc` | any | compile firmware |
| `go` | 1.21+ | build API server |
| `python3` | any | serve frontend |
| `curl` | any | flash via CLI |
