CC      = cc
CFLAGS  = -O2 -Wall -Wextra -Wno-unused-parameter \
          -I./include -I./unicorn/include

LIB     = build/libveemu.a

SRCS    = src/core/engine.c \
          src/core/elf_loader.c \
          src/peripherals/uart.c \
          src/peripherals/rcc.c \
          src/peripherals/gpio.c \
          src/peripherals/systick.c \
          src/peripherals/tim.c \
          src/peripherals/spi.c \
          src/peripherals/i2c.c

OBJS    = $(patsubst src/%.c,build/obj/%.o,$(SRCS))

.PHONY: all clean

all: build/obj/core build/obj/peripherals $(LIB)
	@cp $(LIB) libveemu.a
	@echo "✅  Built: $(LIB)"

build/obj/core build/obj/peripherals:
	mkdir -p $@

build/obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB): $(OBJS)
	ar rcs $@ $(OBJS)

clean:
	rm -rf build/obj build/libveemu.a libveemu.a
