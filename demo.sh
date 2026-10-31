#!/usr/bin/env bash
# demo.sh -- guided live demonstration following the Review 3 "Suggested Final
# Demonstration Sequence" (Section 8 of the guidelines).
#   ./demo.sh            pause after every step (press Enter to continue)
#   ./demo.sh --no-pause run straight through
cd "$(dirname "$0")" || exit 1
PAUSE=1; [ "$1" = "--no-pause" ] && PAUSE=0
J=./build/jitvm
B='\033[1;34m'; G='\033[1;32m'; Y='\033[1;33m'; N='\033[0m'

stage() { echo; echo -e "${B}==================================================================${N}"; echo -e "${B} $1${N}"; echo -e "${B}==================================================================${N}"; }
run()   { echo; echo -e "${G}\$ $*${N}"; "$@"; echo -e "${Y}(exit code $?)${N}"; [ $PAUSE = 1 ] && read -r -p "  [Enter] " _; }

stage "1. ENVIRONMENT -- version, dependencies, build"
run uname -sm
run c++ --version
run make clean
run make
run $J --backend-info

stage "2. NORMAL USE CASE -- Review 2 loop, now compiled to native code"
run $J tests/programs/01_valid_stable_loop.jbc
run $J --compare bench/programs/b1_sum_loop.jbc

stage "3. BOUNDARY / LESS-COMMON VALID CASES"
run $J -q --stats tests/programs/07_trace_exact_limit.jbc    # exactly MAX_TRACE_LENGTH = 50 ops
run $J tests/programs/02_below_threshold.jbc                 # never becomes hot
run $J tests/programs/09_nested_loops.jbc                    # inner traced, outer blacklisted
run $J -vv tests/programs/22_side_exit_restores_state.jbc    # mid-body side exit + snapshot restore

stage "4. INVALID / FAILURE INPUTS"
run $J tests/errors/e01_syntax_errors.jbc                    # all front-end errors with line numbers
run $J tests/errors/e02_bad_jump_target.jbc                  # verifier error
run $J tests/programs/03_invalid_unknown_op.jbc              # recorder abort + blacklist
run $J tests/programs/15_div_not_jitable.jbc                 # legal op the JIT refuses
run $J -q tests/errors/e03_division_by_zero.jbc              # runtime error, exit 3
run $J -q --max-steps=2000000 tests/errors/e04_infinite_loop.jbc  # caught inside native code, exit 4

stage "5. INTERNAL EVIDENCE -- TraceIR, SSA, LIR and machine code"
run $J -q -O1 --dump-ir tests/programs/12_loop_invariant.jbc  # without loop peeling
run $J -q -O2 --dump-ir tests/programs/12_loop_invariant.jbc  # with loop peeling (paper novelty)
run $J -q -O2 --dump-asm bench/programs/b5_polynomial.jbc

stage "6. EVALUATION RESULTS -- test suite and benchmarks"
run make test
run ./build/jit_bench --reps 3 --out "${TMPDIR:-/tmp}"

stage "7. EVALUATOR-SELECTED INPUT"
echo "Write any program into demo_input.jbc (see README for the ISA), then run:"
echo "    $J --compare demo_input.jbc        # interpreter vs JIT, identical-result check + speed-up"
echo "    $J -vv --stats demo_input.jbc      # full JIT event log"
if [ -f demo_input.jbc ]; then run $J --compare demo_input.jbc; fi
echo; echo -e "${G}Demo complete.${N}"
