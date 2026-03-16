ENGINE_DIR ?= ../veemu-engine
BINARY      = veemu-apiserver

build:
	CGO_ENABLED=1 \
	CGO_CFLAGS="-I$(ENGINE_DIR)/include -I$(ENGINE_DIR)/unicorn/include" \
	CGO_LDFLAGS="-L$(ENGINE_DIR)/unicorn/build -lunicorn -L$(ENGINE_DIR) -lveemu -lm -lpthread -Wl,-rpath,$(abspath $(ENGINE_DIR)/unicorn/build)" \
	go build -o $(BINARY) .

run: build
	VEEMU_BOARD_DIR=$(ENGINE_DIR)/boards \
	VEEMU_APP_DIR=$(CURDIR)/../veemu-app \
	./$(BINARY)

deps:
	go mod tidy

smoke:
	curl -s http://localhost:8080/health | jq
	curl -s http://localhost:8080/sessions | jq

clean:
	rm -f $(BINARY)

.PHONY: build run deps smoke clean
