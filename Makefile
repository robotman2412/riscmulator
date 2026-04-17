
makeflags: --silent

TESTS ?=

all: test

.PHONY: build
build:
	meson compile -C build

.PHONY: test
test: build
	./build/test/main $(TESTS)

.PHONY: gdb-test
gdb-test: build
	DO_FORK=0 gdb ./build/test/main -ex 'b testcase_error_message' -ex 'r $(TESTS)'

.PHONY: setup
setup:
	rm -rf build
	meson setup build
