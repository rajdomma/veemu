CC      = arm-none-eabi-gcc
CFLAGS  = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
CFLAGS += -O2 -g -Wall -std=c11 -nostdlib
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -I. -Iplatform
LFLAGS  = -Tplatform/stm32f401re.ld -Wl,--gc-sections -nostartfiles -lc -lm -lgcc

# ── Board & platform config ────────────────────────────────────
BOARD      ?= stm32f401re
BOARD_JSON  = ../veemu-engine/boards/$(BOARD).json
PLATFORM_H  = platform/platform_config.h

# ── App selector ──────────────────────────────────────────────
APP    ?= led_patterns
TARGET  = build/$(APP).elf
BIN     = build/$(APP).bin

# ── Sources ───────────────────────────────────────────────────
C_SOURCES  = platform/startup.c
C_SOURCES += platform/systick.c
C_SOURCES += platform/syscalls.c
C_SOURCES += platform/platform_init.c
C_SOURCES += platform/gpio.c
C_SOURCES += platform/uart.c
C_SOURCES += main.c
C_SOURCES += examples/$(APP)/$(APP).c

# ─────────────────────────────────────────────────────────────

# Generate platform_config.h from board JSON
$(PLATFORM_H): $(BOARD_JSON)
	@echo "[GEN ] Generating platform_config.h from $(BOARD_JSON)"
	@python3 tools/gen_platform.py $(BOARD_JSON) $(PLATFORM_H)

all: $(TARGET)
	@arm-none-eabi-size $(TARGET)
	@echo "Output: $(TARGET)"

$(TARGET): $(PLATFORM_H) $(C_SOURCES)
	@mkdir -p build
	$(CC) $(CFLAGS) -DAPP_$(shell echo $(APP) | tr a-z A-Z) \
	    $(C_SOURCES) $(LFLAGS) -o $(TARGET)
	@arm-none-eabi-objcopy -O binary $(TARGET) $(BIN)
	@echo "Built: $(TARGET)  |  $(BIN)"

flash: $(TARGET)
	@echo "[FLASH] App    : $(APP)"
	@echo "[FLASH] Binary : $(TARGET)"
	@curl -s -X POST http://localhost:8080/flash \
	  -F "elf=@$(TARGET)" \
	  -F "board=stm32f401re" | python3 -m json.tool
	@echo "[FLASH] Done — open http://localhost:3000"

flash-hw: $(TARGET)
	st-flash write $(BIN) 0x08000000

clean:
	rm -rf build/ $(PLATFORM_H)

list:
	@echo "Available apps:"
	@ls examples/

.PHONY: all flash flash-hw clean list