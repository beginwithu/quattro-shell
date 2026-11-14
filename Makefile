.PHONY: all build test clean

all: build

build:
	mkdir -p build
	cd build && qmake6 ../quattro-cpp.pro && $(MAKE) -j$$(nproc)

test:
	mkdir -p build/tests
	cd build/tests && qmake6 ../../tests/tests.pro && $(MAKE) -j$$(nproc)
	QT_QPA_PLATFORM=offscreen ./build/bin/test_core

clean:
	rm -rf build
