#!/usr/bin/env bash
# run_cli_tests.sh -- end-to-end tests of the jitvm command-line tool:
# exit codes, diagnostics and the --compare differential mode.
# usage: tests/run_cli_tests.sh [path/to/jitvm]
JITVM="${1:-./build/jitvm}"
DIR="$(cd "$(dirname "$0")" && pwd)"
pass=0; fail=0

expect() {  # expect <name> <expected-exit> <grep-pattern-or-empty> -- args...
    local name="$1" code="$2" pattern="$3"; shift 4
    local out; out="$("$JITVM" "$@" 2>&1)"; local rc=$?
    if [ "$rc" -ne "$code" ]; then
        echo "  FAIL: $name (exit $rc, expected $code)"; fail=$((fail+1)); return
    fi
    if [ -n "$pattern" ] && ! grep -qE -- "$pattern" <<<"$out"; then
        echo "  FAIL: $name (output does not match /$pattern/)"; fail=$((fail+1)); return
    fi
    echo "  ok   $name"; pass=$((pass+1))
}

echo "=== CLI tests ($JITVM) ==="
P="$DIR/programs"; E="$DIR/errors"
expect "valid program runs"                    0 "Register R0 = 10"            -- -q "$P/01_valid_stable_loop.jbc"
expect "JIT events logged"                     0 "Hot loop detected at pc 3 \\(line 6\\)" -- "$P/01_valid_stable_loop.jbc"
expect "--stats reports compiled trace"        0 "traces compiled +: 1"        -- -q --stats "$P/01_valid_stable_loop.jbc"
expect "--dump-ir shows SSA + Phi"             0 "Phi R0"                      -- -q --dump-ir "$P/01_valid_stable_loop.jbc"
expect "--dump-asm shows machine code"         0 "(LOOP:|no machine code)"     -- -q --dump-asm "$P/01_valid_stable_loop.jbc"
expect "-vv shows deopt with restored regs"    0 "restored R0=10"              -- -vv "$P/01_valid_stable_loop.jbc"
expect "--mode=interp runs program"            0 "^Register R0 = 10$"          -- -q --mode=interp "$P/01_valid_stable_loop.jbc"
expect "--backend=ir works"                    0 "IR executor"                 -- --backend=ir "$P/01_valid_stable_loop.jbc"
expect "--compare differential PASSED"         0 "differential check PASSED"   -- --compare "$P/12_loop_invariant.jbc"
expect "--compare on side-exit program"        0 "differential check PASSED"   -- --compare "$P/22_side_exit_restores_state.jbc"
expect "--check prints disassembly"            0 "OK \(14 instructions"        -- --check "$P/09_nested_loops.jbc"
expect "--backend-info"                        0 "host architecture"           -- --backend-info
expect "syntax errors -> exit 2"               2 "unknown mnemonic 'LOAD'"     -- "$E/e01_syntax_errors.jbc"
expect "syntax errors carry line numbers"      2 "e01_syntax_errors.jbc:5: error: invalid register 'R16'" -- "$E/e01_syntax_errors.jbc"
expect "undefined label reported"              2 "undefined label 'nowhere'"   -- "$E/e01_syntax_errors.jbc"
expect "bad jump target -> exit 2"             2 "outside the program"         -- "$E/e02_bad_jump_target.jbc"
expect "division by zero -> exit 3"            3 "division by zero at pc 4"    -- -q "$E/e03_division_by_zero.jbc"
expect "infinite loop (JIT) -> exit 4"         4 "step limit"                  -- -q --max-steps=200000 "$E/e04_infinite_loop.jbc"
expect "infinite loop (interp) -> exit 4"      4 "step limit"                  -- -q --mode=interp --max-steps=200000 "$E/e04_infinite_loop.jbc"
expect "Review 2 case 8 caught -> exit 4"      4 "step limit"                  -- -q --max-steps=200000 "$E/r2_case8_original.jbc"
expect "empty program -> exit 2"               2 "program is empty"            -- "$E/e05_empty.jbc"
expect "warnings still run -> exit 0"          0 "may be read before"          -- -q "$E/w01_warnings.jbc"
expect "--strict rejects warnings -> exit 2"   2 "verification failed"         -- -q --strict "$E/w01_warnings.jbc"
expect "missing file -> exit 2"                2 "cannot open file"            -- "$E/nope.jbc"
expect "unknown option -> exit 1"              1 "unknown option"              -- --frobnicate "$P/01_valid_stable_loop.jbc"
expect "bad numeric option -> exit 1"          1 "invalid numeric value"       -- --hot=abc "$P/01_valid_stable_loop.jbc"
JITVM_NO_NATIVE=1 "$JITVM" "$P/01_valid_stable_loop.jbc" 2>&1 | grep -q "IR executor" \
    && { echo "  ok   JITVM_NO_NATIVE=1 forces the IR executor"; pass=$((pass+1)); } \
    || { echo "  FAIL: JITVM_NO_NATIVE=1 forces the IR executor"; fail=$((fail+1)); }

echo "CLI tests: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
