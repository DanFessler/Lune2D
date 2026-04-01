.PHONY: build run clean configure

configure:
	cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64

build:
	cmake --build build

run: build
	./build/asteroids

clean:
	rm -rf build
