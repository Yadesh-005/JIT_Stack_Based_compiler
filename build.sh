#!/usr/bin/env bash
# build.sh -- build without make (fallback). Produces build/jitvm, build/jit_tests, build/jit_bench.
set -e
cd "$(dirname "$0")"
CXX="${CXX:-c++}"
FLAGS="-std=c++17 -O2 -Wall -Wextra -pedantic -Isrc"
mkdir -p build
CORE="src/isa.cpp src/assembler.cpp src/verifier.cpp src/trace_ir.cpp src/recorder.cpp src/optimizer.cpp src/backend_ir.cpp src/backend_native.cpp src/vm.cpp"
echo "compiler: $($CXX --version | head -1)"
$CXX $FLAGS $CORE src/jitvm.cpp       -o build/jitvm
$CXX $FLAGS $CORE tests/test_main.cpp -o build/jit_tests
$CXX $FLAGS $CORE bench/bench_main.cpp -o build/jit_bench
echo "built: build/jitvm build/jit_tests build/jit_bench"
./build/jitvm --backend-info
