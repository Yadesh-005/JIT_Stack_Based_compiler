# Review 3 – Live Demonstration Script (Team 6, A2)

Total: 18–22 min presentation + demo, then 8–12 min viva. The live demo must run
for at least 8 minutes. Every command below is copy-paste ready from the project
root. `./demo.sh` runs the same sequence with a pause after each step.

**Before the review (5 min, on the demo Mac):**

```bash
cd ~/Desktop/"compiler JIT project"
make clean && make && make test        # must end with "TOTAL: ... 0 failed" and "CLI tests: 27 passed"
./build/jitvm --backend-info           # expect: native AArch64 backend (self-test passed)
```

Open two terminal windows (font size ≥ 18) and keep `src/` open in an editor.
If the native backend ever reports "unavailable", everything still works through the IR
executor – say so and continue (`--backend=ir` is the documented fallback).

---

## Member 1 – Atul Vipin (Project Lead / System Integrator) – 4 min

Slides 1–5, then:

```bash
./build/jitvm --backend-info
./build/jitvm tests/programs/01_valid_stable_loop.jbc
```

Talking points: the five-stage pipeline (assembler → verifier → interpreter+profiler →
recorder → optimizer → LIR → native/IR backend → deopt). Point at each log line:
`[PROFILER]` → `[RECORDER]` → `[OPTIMIZER]` → `[BACKEND]` → program output → exit.
Mention the Review 2 corrections table (slide 3) and the code-cache design: loop
headers are patched with an internal TRACE_ENTER opcode (`src/vm.cpp`, `compileTrace`).

## Member 2 – Emmanuel Joshy (Front End / Interpreter / Recorder) – 4–5 min

```bash
./build/jitvm tests/errors/e01_syntax_errors.jbc          # 6 errors, each with a line number
./build/jitvm --check tests/programs/09_nested_loops.jbc  # verifier + disassembly
./build/jitvm -q tests/errors/w01_warnings.jbc            # data-flow warnings (read-before-write, unreachable)
./build/jitvm tests/programs/03_invalid_unknown_op.jbc    # recorder abort, blacklist after 2 attempts
./build/jitvm tests/programs/11_loop_exit_during_recording.jbc   # Review 2 recorder bug, fixed
```

Source to show: `src/assembler.cpp` (two-pass, label table), `src/verifier.cpp`
(definitely-assigned analysis), `src/recorder.cpp` (`record()` – guards for every branch
direction).

## Member 3 – Yadesh Waran M (Optimizer / Code Generation) – 4–5 min

```bash
./build/jitvm -q -O1 --dump-ir tests/programs/12_loop_invariant.jbc   # loop body: 6 ops
./build/jitvm -q -O2 --dump-ir tests/programs/12_loop_invariant.jbc   # loop body: 3 ops (peeling)
./build/jitvm -q --dump-asm bench/programs/b5_polynomial.jbc          # register-allocated loop
./build/jitvm --compare bench/programs/b2_loop_invariant.jbc
```

Explain loop peeling (Ardö, Bolz & Fijałkowski, DLS 2012): the trace is emitted twice,
and CSE / guard elimination tables built while optimising the preamble remove the
invariant work from the loop body – LICM without a LICM pass. Show
`src/optimizer.cpp` (`buildOnce`, phi fix-point) and `src/backend_native.cpp`
(`allocateRegisters`, `A64::compile`).

## Member 4 – Tanvi Gupta (Deoptimisation / Testing / Release) – 4–5 min

```bash
./build/jitvm -vv tests/programs/22_side_exit_restores_state.jbc   # [DEOPT] ... (restored R0=77 R3=... )
./build/jitvm -q --max-steps=2000000 tests/errors/e04_infinite_loop.jbc   # fuel exit, exit code 4
make test
./build/jit_bench --reps 3 --only b2
```

Show `results/` (test logs for x86-64 and AArch64, benchmark CSVs), the README build
instructions, and the limitations slide.

## All members – integrated demo + evaluator input (8–12 min)

Ask the evaluator for a loop; type it into `demo_input.jbc`, e.g.

```
      LOAD_CONST R0, 0
      LOAD_CONST R1, 1
      LOAD_CONST R2, 1000000
      LOAD_CONST R3, 0
loop: MUL_INT    R4, R0, R0
      ADD_INT    R3, R3, R4
      ADD_INT    R0, R0, R1
      JUMP_IF_LT R0, R2, loop
      PRINT_REG  R3
      HALT
```

```bash
./build/jitvm --compare demo_input.jbc      # identical results + speed-up
./build/jitvm -vv --stats demo_input.jbc    # full event log and statistics
./build/jitvm -q --dump-ir demo_input.jbc   # what the optimiser did
```

Rebuild / reproducibility check for the evaluator record: `make clean && make && make test`.
