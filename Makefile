BUILD_DIR ?= build
BUILD_TYPE ?= RelWithDebInfo
CMAKE_FLAGS ?=

.PHONY: all configure build test bench bazel-test bazel-bench clean format

all: build

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(CMAKE_FLAGS)

build: configure
	cmake --build $(BUILD_DIR) --parallel

test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure --timeout 120

bench: build
	$(BUILD_DIR)/src/benchmarks/sham_benchmarks
	$(BUILD_DIR)/src/benchmarks/sham_queue_compare

bazel-test:
	bazel test //src/tests:sham_tests

bazel-bench:
	bazel run -c opt //src/benchmarks:sham_benchmarks
	bazel run -c opt //src/benchmarks:sham_queue_compare

clean:
	rm -rf $(BUILD_DIR)
	-bazel clean

format:
	clang-format -i \
	  src/sham/include/sham/*.h \
	  src/tests/*.cpp \
	  src/tests/*.h \
	  src/adapters/include/adapters/*.h \
	  src/benchmarks/*.cpp
