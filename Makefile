.PHONY: configure build test sanitize clean

CMAKE := $(shell command -v cmake 2>/dev/null)
ifeq ($(CMAKE),)
CMAKE := /Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake
endif
CTEST := $(dir $(CMAKE))ctest

configure:
	$(CMAKE) --preset debug

build: configure
	$(CMAKE) --build --preset debug

test: build
	$(CTEST) --preset debug

sanitize:
	$(CMAKE) --preset sanitizers
	$(CMAKE) --build --preset sanitizers
	$(CTEST) --preset sanitizers

clean:
	$(CMAKE) -E remove_directory build
