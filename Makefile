
makeflags += --silent

TESTS ?=
RISCV_TEST_PREFIX ?= riscv64-linux-gnu-

all: test

.PHONY: build
build:
	meson compile -C build

.PHONY: test
test: build
		RISCV_TEST_PREFIX=$(RISCV_TEST_PREFIX) \
	./build/test/main $(TESTS)

.PHONY: gdb-test
gdb-test: build
		RISCV_TEST_PREFIX=$(RISCV_TEST_PREFIX) \
		DO_FORK=0 \
	gdb ./build/test/main \
		-ex 'tb main' \
		-ex 'b testcase_error_message' \
		-ex 'r $(TESTS)'

.PHONY: setup
setup:
	rm -rf build
	meson setup build
