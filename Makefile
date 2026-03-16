# =============================================================================
# Veemu — Root Makefile
# =============================================================================

ENGINE_DIR   = veemu-engine
API_DIR      = veemu-api
APP_DIR      = veemu-app
FRONTEND_DIR = veemu-frontend

FRONTEND_PORT = 3000
APP          ?= led_patterns

.PHONY: all engine api app flash start stop clean help

all: engine api
	@echo ""
	@echo "✅  All services built. Run: make start"

engine:
	@echo "[ENGINE] Building libveemu.a..."
	$(MAKE) -C $(ENGINE_DIR)
	@echo "[ENGINE] Done."

api: engine
	@echo "[API] Building veemu-apiserver..."
	$(MAKE) -C $(API_DIR)
	@echo "[API] Done."

app:
	@echo "[APP] Building $(APP)..."
	$(MAKE) -C $(APP_DIR) APP=$(APP)

flash: app
	@echo "[FLASH] Flashing $(APP) to Veemu..."
	$(MAKE) -C $(APP_DIR) APP=$(APP) flash

start:
	@mkdir -p logs
	@echo "[START] Clearing ports 8080 and $(FRONTEND_PORT)..."
	@lsof -ti:8080 | xargs kill -9 2>/dev/null || true
	@lsof -ti:$(FRONTEND_PORT) | xargs kill -9 2>/dev/null || true
	@sleep 1
	@echo "[START] Starting Veemu API on :8080..."
	@cd $(API_DIR) && \
	    VEEMU_BOARD_DIR=$(CURDIR)/$(ENGINE_DIR)/boards \
	    nohup ./veemu-apiserver > $(CURDIR)/logs/api.log 2>&1 & \
	    echo $$! > $(CURDIR)/logs/api.pid
	@sleep 1
	@echo "[START] Starting frontend on :$(FRONTEND_PORT)..."
	@cd $(FRONTEND_DIR) && \
	    nohup python3 -m http.server $(FRONTEND_PORT) \
	    > $(CURDIR)/logs/frontend.log 2>&1 & \
	    echo $$! > $(CURDIR)/logs/frontend.pid
	@sleep 1
	@echo ""
	@curl -s http://localhost:8080/health || echo "  ⚠️  API not responding — check logs/api.log"
	@echo ""
	@echo "✅  Veemu running:"
	@echo "    API      → http://localhost:8080/health"
	@echo "    Frontend → http://localhost:$(FRONTEND_PORT)"
	@echo "    Logs     → logs/api.log  logs/frontend.log"

stop:
	@echo "[STOP] Stopping servers..."
	@[ -f logs/api.pid ]      && kill $$(cat logs/api.pid)      && rm logs/api.pid      || true
	@[ -f logs/frontend.pid ] && kill $$(cat logs/frontend.pid) && rm logs/frontend.pid || true
	@lsof -ti:8080 | xargs kill -9 2>/dev/null || true
	@lsof -ti:$(FRONTEND_PORT) | xargs kill -9 2>/dev/null || true
	@echo "[STOP] Done."

clean:
	$(MAKE) -C $(ENGINE_DIR) clean
	$(MAKE) -C $(API_DIR)    clean
	$(MAKE) -C $(APP_DIR)    clean
	rm -rf logs/
	@echo "[CLEAN] Done."

help:
	@echo ""
	@echo "Veemu — STM32F401RE Digital Twin Platform"
	@echo ""
	@echo "  make all                    build engine + api"
	@echo "  make start                  start API + frontend"
	@echo "  make stop                   stop all servers"
	@echo "  make APP=led_patterns flash build firmware + flash"
	@echo "  make APP=blink flash        build blink + flash"
	@echo "  make clean                  clean all artifacts"
	@echo "  make help                   show all commands"
	@echo ""
	@echo "  Firmware workspace → veemu-app/"
	@echo "  Available apps:"
	@ls $(APP_DIR)/examples/
	@echo ""
