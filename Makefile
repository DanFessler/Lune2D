.PHONY: build run clean configure web web-install

configure:
	cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64

web-install:
	cd web && npm install

web: web-install
	cd web && npm run build

build: web
	cmake --build build

run: build
	./build/asteroids

clean:
	rm -rf build
