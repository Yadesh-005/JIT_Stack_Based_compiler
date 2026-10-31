# Trace-Based JIT Compiler for a Custom Bytecode VM -- BCSE307 A2, Team 6
# Works with g++ (Linux) and Apple clang (macOS, Intel or Apple Silicon).
CXX      ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
BUILD    := build

CORE_SRCS := src/isa.cpp src/assembler.cpp src/verifier.cpp src/trace_ir.cpp \
             src/recorder.cpp src/optimizer.cpp src/backend_ir.cpp \
             src/backend_native.cpp src/vm.cpp
CORE_OBJS := $(CORE_SRCS:src/%.cpp=$(BUILD)/%.o)

.PHONY: all test bench clean demo legacy

all: $(BUILD)/jitvm $(BUILD)/jit_tests $(BUILD)/jit_bench

$(BUILD):
	@mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.cpp src/*.h | $(BUILD)
	$(CXX) $(CXXFLAGS) -Isrc -c $< -o $@

$(BUILD)/jitvm: $(CORE_OBJS) $(BUILD)/jitvm.o
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/jit_tests: $(CORE_OBJS) tests/test_main.cpp src/*.h
	$(CXX) $(CXXFLAGS) -Isrc tests/test_main.cpp $(CORE_OBJS) -o $@

$(BUILD)/jit_bench: $(CORE_OBJS) bench/bench_main.cpp src/*.h
	$(CXX) $(CXXFLAGS) -Isrc bench/bench_main.cpp $(CORE_OBJS) -o $@

# Unit + golden + differential + fuzz + Review 2 regression tests.
test: $(BUILD)/jitvm $(BUILD)/jit_tests
	./$(BUILD)/jit_tests
	./tests/run_cli_tests.sh ./$(BUILD)/jitvm

bench: $(BUILD)/jit_bench
	./$(BUILD)/jit_bench --out results

# Original Review 2 prototype, kept unchanged for comparison.
legacy: | $(BUILD)
	$(CXX) -std=c++17 -O2 -w legacy/review2/vm.cpp legacy/review2/main.cpp -o $(BUILD)/review2_vm

demo: $(BUILD)/jitvm
	./demo.sh

clean:
	rm -rf $(BUILD)
