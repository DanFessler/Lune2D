.PHONY: build run dev dev-web build-native clean configure web web-install

# Vite dev server (override if port is taken).
WEB_DEV_PORT ?= 5173
WEB_DEV_URL  ?= http://127.0.0.1:$(WEB_DEV_PORT)/

configure:
	cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64

web-install:
	cd web && npm install

web: web-install
	cd web && npm run build

build-native: configure
	cmake --build build

build: web
	cmake --build build

run: build
	./build/asteroids

# Native app + Vite HMR (no `npm run build`). Ctrl+C stops both.
dev: web-install build-native
	@trap 'kill 0' EXIT; \
	cd web && npm run dev -- --host 127.0.0.1 --port $(WEB_DEV_PORT) --strictPort & \
	ready=; i=0; \
	while [ $$i -lt 40 ]; do \
	  if curl -sf -o /dev/null "$(WEB_DEV_URL)"; then ready=1; break; fi; \
	  i=$$((i+1)); sleep 0.25; \
	done; \
	if [ -z "$$ready" ]; then echo "error: Vite did not respond at $(WEB_DEV_URL)"; exit 1; fi; \
	WEBVIEW_DEV_URL="$(WEB_DEV_URL)" WEBVIEW_INSPECTABLE=1 ./build/asteroids

# Vite only (use a second terminal for ./build/asteroids with WEBVIEW_DEV_URL set).
dev-web: web-install
	cd web && npm run dev -- --host 127.0.0.1 --port $(WEB_DEV_PORT) --strictPort

clean:
	rm -rf build
