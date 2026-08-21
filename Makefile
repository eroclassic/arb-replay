.PHONY: configure build test sanitize clean

configure:
	cmake --preset debug

build: configure
	cmake --build --preset debug

test: build
	ctest --preset debug

sanitize:
	cmake --preset sanitizers
	cmake --build --preset sanitizers
	ctest --preset sanitizers

clean:
	cmake -E remove_directory build

