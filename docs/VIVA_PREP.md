# Viva Preparation – Likely Questions and Model Answers

Each answer points to the source location to open if the evaluator asks to see it.

## Whole system (everyone should be able to answer)

**Q. What does the project do, in one sentence?**
A register-based bytecode VM that interprets programs, detects hot loops, records the
path actually executed as a linear trace, optimises it in SSA form (including loop
peeling for loop-invariant code motion), compiles it to native x86-64 or AArch64 code,
and falls back to the interpreter through snapshot-based deoptimisation when a guard fails.

**Q. Why trace-based rather than method-based JIT?**
A trace only contains the instructions that actually executed, so the recorder needs no
control-flow analysis and the optimiser works on straight-line code where a single forward
pass suffices. The cost: divergent paths cause side exits (see `b7_branchy`), and nested
loops need trace trees, which we list as future work.

**Q. What is a guard?**
A run-time check of an assumption made while recording – e.g. that a branch goes the same
way. Every conditional branch in the trace becomes a guard on the direction observed
(`src/recorder.cpp`, `record()`). If it fails, execution leaves compiled code.

**Q. What happens when a guard fails? (deoptimisation)**
Each guard owns a *snapshot*: the list of VM registers modified so far and the SSA value
each must hold, plus the bytecode pc to resume at. The exit stub writes those values back
into the VM register file and returns the exit index; the interpreter continues at
`resumePC` (`src/trace_ir.h` `Snapshot`, exit stubs in `src/backend_native.cpp`,
`VirtualMachine::enterTrace`). VM registers are never written inside the trace, only at exits.

**Q. How do you know the JIT is correct?**
Differential testing: every program is run by the pure interpreter (the reference
semantics) and by 30 JIT configurations (2 backends × 3 optimisation levels × 5 hot
thresholds); output, final registers and status must be identical. Plus 400 random
programs × 7 configurations (2,800 runs) from a generator of terminating programs with
nested loops, if/else, prints, DIV and UNKNOWN_OP. Run on x86-64 and AArch64, and under
AddressSanitizer + UBSan.

**Q. What were the main Review 2 defects?**
(1) Recording continued past the loop end when the loop exited mid-recording, and then no
later loop could ever be traced; (2) LOAD_CONST, PRINT_REG and forward branches inside the
loop were not recorded, so the trace did not represent the loop; (3) the length limit was
checked only for ADD_INT; (4) test case 8 never terminated; (5) unchecked register indices
(out-of-bounds write) and signed overflow (undefined behaviour); (6) the recorded trace was
never used. All fixed and covered by regression tests (`tests/test_main.cpp` section 3).

## Member 2 – front end, interpreter, profiler, recorder

**Q. How does the assembler resolve forward labels?** Two passes: pass 1 records every
label's instruction index, pass 2 parses operands and looks labels up. All errors are
collected with line numbers instead of stopping at the first (`src/assembler.cpp`).

**Q. What analyses does the verifier perform?** Structural checks (opcodes, register range
R0–R15, jump targets) are errors; CFG reachability (BFS from pc 0) and a forward *must*
data-flow analysis – "definitely assigned", IN = ∩ OUT(preds), OUT = IN ∪ DEF, iterated to a
fix-point – produce warnings for unreachable code and possible read-before-write.

**Q. How is a hot loop detected?** Every taken backward branch increments a counter for its
target; when it reaches HOT_THRESHOLD (3) recording starts. `currentPC` is captured before
`pc++` so "backward" is judged against the jump's own address (Review 2 fix).

**Q. Why abort on an inner-loop back-edge?** A linear trace cannot contain a nested loop.
The inner loop is traced on its own (it becomes hot first); the outer loop's recording
aborts, and after two attempts it is blacklisted. Trace trees (TraceMonkey) are future work.

**Q. What is blacklisting and why have a back-off?** A loop whose recording keeps aborting
would waste time recording forever; after `maxRecordAttempts` (2) it stays interpreted.

## Member 3 – optimiser and code generation

**Q. Why SSA?** Each value is defined once, so value numbering/CSE and constant folding are
simple table lookups and DCE is a liveness walk. MOV produces no instruction (copy
propagation is implicit).

**Q. Explain loop peeling / the paper's novelty.** Ardö, Bolz & Fijałkowski (DLS 2012):
emit the trace twice – a preamble (iteration 1) and the loop body – and run the *same*
forward optimisations over both. While optimising the body, the CSE and guard tables still
contain the preamble's entries, so any body operation whose operands are loop-invariant
hashes to a preamble value and disappears from the loop. That *is* loop-invariant code
motion, obtained without writing a LICM pass. `12_loop_invariant`: 6 → 3 ops per iteration;
`b2_loop_invariant`: ~2.7–3× faster than -O1.

**Q. What is the phi fix-point?** A register assigned in the loop gets a Phi. If its
back-edge value equals its entry value (e.g. `LOAD_CONST R7, 3` every iteration – after
peeling both are the constant 3), the Phi is redundant; the register is marked invariant
and the trace is rebuilt so later operations can fold. Iterated until no change (≤16 rounds).

**Q. Is CSE across the preamble always valid?** Yes: the preamble dominates the loop and
the operands are the *same SSA values*, which are invariant; operations on Phi values never
match preamble entries because Phis are new values.

**Q. How does register allocation work?** Linear scan (Poletto & Sarkar) over the linear LIR:
live intervals include exit-snapshot uses; values live into the loop are extended to the
back-edge; spill the interval ending furthest away. Pools: 10 registers on x86-64, 11 on
AArch64; fuel counter has its own register; caller-saved pool registers are saved around
the PRINT helper call.

**Q. What is Phi coalescing?** If a Phi is not read after its back-edge value is defined,
both share one slot, so the back-edge copy disappears (out-of-SSA copy removal).

**Q. How is executable memory obtained?** W^X: Linux/Intel-mac mmap RW → copy → mprotect
RX → flush I-cache. Apple Silicon: `mmap(MAP_JIT)`, `pthread_jit_write_protect_np(0)` while
writing, `(1)` before executing, `sys_icache_invalidate`.

**Q. What if the native code is wrong on some machine?** At start-up a self-test trace is
compiled and run in a *forked child*; if it crashes or gives a wrong answer the VM silently
uses the portable IR executor. `JITVM_NO_NATIVE=1` forces that path.

**Q. Why keep Phi coalescing — isn't it an extra complication?** We measured it: without it the
B1 loop on Apple Silicon needs two extra register copies per iteration on its critical path and
runs 2× slower (18× → 7.7×). The rule is simple — write the new value of a loop variable into the
same place as the old one when the old one is no longer needed, just like `i = i + 1`.

**Q. What is loop inversion?** The last guard of the loop and the jump back are fused: instead of
"if the condition fails go to the exit; then jump to the top" the code says "if the condition
holds jump to the top; otherwise fall through to the exit" — one branch per iteration.

## Member 4 – deoptimisation, testing, release

**Q. What is the fuel check?** Slot 0 / a dedicated register holds an iteration budget
decremented at the loop header; when it reaches zero the trace exits through a snapshot of
the loop-header state. This keeps the step limit valid inside native code (an infinite loop
is reported, exit code 4, instead of hanging the demo).

**Q. What happens if a loop changes behaviour after it was compiled (`24_phase_change`)?** The
trace's first guard fails on every entry, so the trace side-exits immediately and the interpreter
does the work. Results stay correct; only that phase is slow. Re-recording the loop (or attaching a
side trace) is future work — we deliberately kept the trace manager simple.

**Q. Why does `b7_branchy` only get ~1.8×?** The branch alternates, so the trace exits on
every other iteration; half the work stays in the interpreter. Side traces / trace trees
would fix this (future work).

**Q. Why is `b8_div_not_jitable` slightly slower than the interpreter?** DIV_INT is not
supported by the JIT, so the loop is blacklisted; the remaining cost (~5–8 %) is profiling
on every taken backward branch.

**Q. What do the exit codes mean?** 0 ok, 1 usage, 2 assembly/verification error, 3 runtime
error (division by zero), 4 step limit, 5 `--compare` mismatch.

**Q. How can the evaluator reproduce everything?** `make clean && make && make test`, then
`make bench`; logs for both architectures are in `results/`.

**Q. Why does `--compare` on a tiny program (e.g. `12_loop_invariant`, 1,000 iterations) show a speed-up below 1×?**
The whole run takes microseconds, so the one-off cost of recording, optimising and encoding
the trace (tens of microseconds) dominates. JIT compilation pays off only when the loop runs
long enough — exactly why the profiler waits for HOT_THRESHOLD and why the benchmarks use
millions of iterations. Use `bench/programs/*.jbc` for speed demonstrations.
