# SDL3 GPU fork: desktop builds use CMake on every platform.
CMAKE ?= cmake
BUILD_DIR ?= ../build
BUILD_TYPE ?= Release
CMAKE_ARGS ?=
.PHONY: all configure release debug clean install
all: configure
	$(CMAKE) --build $(BUILD_DIR) --target fteqw --config $(BUILD_TYPE) --parallel
configure:
	$(CMAKE) -S .. -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(CMAKE_ARGS)
release:
	$(MAKE) all BUILD_TYPE=Release
debug:
	$(MAKE) all BUILD_TYPE=Debug
clean:
	$(CMAKE) --build $(BUILD_DIR) --target clean
install:
	$(CMAKE) --install $(BUILD_DIR) --config $(BUILD_TYPE)
