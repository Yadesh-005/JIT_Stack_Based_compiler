# Learning Guide — understand and explain every part of the project

This guide follows **one small program** through the whole system, stage by stage. Each
section says *what* the stage does, *why* it exists, *where* the code is, and gives a
**30-second explanation** you can say in the viva. Run the commands yourself as you read —
seeing the output is the fastest way to understand it.

> Build once: `make`. Every command below is run from the project folder.

---

## 0. The whole project in 60 seconds

A normal **interpreter** reads one bytecode instruction at a time, works out what it is, and
does it. That "work out what it is" step happens again on every instruction, millions of times
— it is slow.

A **JIT (just-in-time) compiler** watches the program while it runs. When a loop has run a few
times (it is "hot"), the JIT **records** exactly which instructions one iteration executed,
**optimises** that list, turns it into **real machine code** for the CPU, and from then on runs
the machine code instead of interpreting. If something happens that the recorded path did not
expect (a branch goes the other way), the machine code **bails out** and the interpreter takes
over again with exactly the right register values (**deoptimisation**).

Our project does all of that for a small custom bytecode, and adds one research idea —
**loop peeling** (Ardö, Bolz & Fijałkowski, DLS 2012) — that removes repeated work from loops.

```
.jbc text ─► Assembler ─► Verifier ─► Interpreter ──hot loop──► Recorder
                                         ▲                         │ TraceIR
                                         │                         ▼
                           Exit stub ◄── Native code ◄── Codegen ◄── Optimiser (SSA + peeling)
                       (restore regs)    (runs loop)   (registers)
```

---

## 1. The bytecode and the assembler — `src/isa.h`, `src/assembler.cpp`

The VM has 16 registers `R0`–`R15`, each a 32-bit integer. A program is a list of
instructions. Our running example (`tests/programs/12_loop_invariant.jbc`):

```
      LOAD_CONST R0, 0        ; i = 0
      LOAD_CONST R1, 1        ; step = 1
      LOAD_CONST R2, 1000     ; n = 1000
      LOAD_CONST R3, 7        ; a = 7
      LOAD_CONST R4, 6        ; b = 6
      LOAD_CONST R5, 0        ; acc = 0
loop: MUL_INT    R6, R3, R4   ; R6 = a*b          <- same value every iteration!
      ADD_INT    R6, R6, R1   ; R6 = a*b + 1      <- same every iteration
      LOAD_CONST R7, 3        ;                   <- same every iteration
      MUL_INT    R8, R6, R7   ; R8 = (a*b+1)*3    <- same every iteration
      ADD_INT    R5, R5, R8   ; acc += R8
      ADD_INT    R0, R0, R1   ; i += 1
      JUMP_IF_LT R0, R2, loop ; while (i < n)
      PRINT_REG  R5           ; prints 129000
      HALT
```

The **assembler** turns this text into `Instruction` structs (`{op, arg1, arg2, arg3}` — the
same format as Review 2). It works in **two passes**:
1. Pass 1 finds every `label:` and remembers which instruction number it points to.
2. Pass 2 reads each instruction; when it sees `loop` as a jump target it looks the number up.

Two passes are needed because a jump can point *forward* to a label not yet seen. Errors are
**collected, not thrown**, so one run reports all of them with line numbers:

```bash
./build/jitvm tests/errors/e01_syntax_errors.jbc
```

**30-second explanation:** "The assembler is the lexer and parser of our language. Pass one builds
a symbol table of labels; pass two parses operands and resolves labels. It checks register names,
number ranges and operand counts and reports every error with its line, like a real compiler."

---

## 2. The verifier — `src/verifier.cpp`

Before running, we check the program is safe and warn about likely mistakes.
* **Errors** (program rejected): register outside R0–R15, jump target outside the program,
  empty program. (In Review 2, `R20` silently wrote outside the register array.)
* **Warnings:** unreachable instructions, falling off the end, no HALT, and *registers that may
  be read before they are written*.

That last one is a classic **data-flow analysis** ("definitely assigned"). For each instruction
we compute the set of registers that are *certainly* written on **every** path reaching it:
`IN = intersection of OUT of all predecessors`, `OUT = IN ∪ {register this instruction writes}`.
We repeat until nothing changes (a fix-point). If an instruction reads a register not in `IN`,
we warn.

```bash
./build/jitvm -q tests/errors/w01_warnings.jbc
```

**30-second explanation:** "The verifier is our semantic analysis. Structural errors stop the
program; then a reachability search over the control-flow graph and a forward must-analysis —
intersection at join points — find unreachable code and possible reads of uninitialised registers."

---

## 3. The interpreter — `src/vm.cpp`, `VirtualMachine::run()`

A loop: fetch `code_[pc]`, `pc++`, then a `switch` on the opcode. Arithmetic wraps around at
32 bits on purpose (`wrapAdd` in `isa.h`) so the interpreter, the optimiser and the machine
code always agree (Review 2 used plain `int +`, which is undefined behaviour on overflow).

`currentPC` is saved **before** `pc++` — the Review 2 fix — so we can tell whether a jump goes
backwards from *where the jump itself is*.

---

## 4. The profiler — finding hot loops

A jump to a target **at or before** the jump itself is a **back-edge**: it closes a loop. Every
time a back-edge is taken we add 1 to a counter for its target. When the counter reaches
`HOT_THRESHOLD = 3`, we start recording.

```bash
./build/jitvm tests/programs/01_valid_stable_loop.jbc     # "[PROFILER] Hot loop detected at pc 3"
./build/jitvm --hot=1 tests/programs/01_valid_stable_loop.jbc
```

**Why wait?** Compiling costs time. Code that runs only once or twice is cheaper to interpret.

---

## 5. The recorder — `src/recorder.cpp`

While recording, every instruction the interpreter executes is also written into a list —
the **trace** (Level-1 TraceIR). One iteration of our example records:

```
IR_MUL_INT R6, R3, R4
IR_ADD_INT R6, R6, R1
IR_LOAD_CONST R7, 3
IR_MUL_INT R8, R6, R7
IR_ADD_INT R5, R5, R8
IR_ADD_INT R0, R0, R1
IR_GUARD_LT R0, R2  (exit -> pc 13)     <- the branch
IR_JUMP_LOOP -> pc 6
```

The key idea: a trace is **straight-line code**. The branch `JUMP_IF_LT` was *taken* while
recording, so we write a **guard**: "check that R0 < R2 is still true; if not, leave the trace
and continue interpreting at pc 13". Every branch becomes a guard on the direction we saw.

Recording **aborts** (safely, nothing breaks) when the trace gets longer than 50 ops, on
`UNKNOWN_OP`/`DIV_INT`/`HALT`, on a back-edge to a *different* loop (an inner loop), or if the
loop *exits* while we are recording (Review 2 bug D1). After 2 failed attempts the loop is
**blacklisted** and just interpreted.

```bash
./build/jitvm -q --dump-ir tests/programs/12_loop_invariant.jbc   # first block = recorded TraceIR
./build/jitvm tests/programs/03_invalid_unknown_op.jbc             # abort, retry, blacklist
```

---

## 6. SSA form — the optimiser's view (`src/optimizer.cpp`)

**SSA = Static Single Assignment:** every value is given a new name (`v0, v1, …`) and is
assigned exactly once. "R6" means different things at different times; `v16` always means one
thing. That makes optimisations easy: "are these two computations the same?" becomes "do they
have the same operator and the same operand names?".

* `LoadReg R3` = "the value R3 had when we entered the trace".
* A register that changes inside the loop gets a **Phi** node at the loop header:
  `v10 = Phi R0 [entry v0, back v21]` means "on the first iteration this is v0; on later
  iterations it is v21 (the value computed at the end of the previous iteration)".
* `MOV` creates no instruction at all — the new register simply gets the same SSA name.

The optimised SSA **without** loop peeling (`-O1`) for our example's loop body:

```
v10 = Phi R0 [entry v0, back v21]     ; i
v11 = Phi R5 [entry v5, back v20]     ; acc
...
v16 = Mul v3, v4                       ; a*b        — recomputed EVERY iteration
v17 = Add v1, v16
v19 = Mul v17, #3
v20 = Add v11, v19                     ; acc += ...
v21 = Add v1, v10                      ; i += 1
GuardLT v21, v2
```

---

## 7. The forward optimisations (all in one pass, `Builder::arith` / `Builder::guard`)

| Optimisation | Example | Result |
|---|---|---|
| Constant folding | `10 + 32` (both constants) | `#42` |
| Algebraic simplification | `x + 0`, `x * 1`, `x - x`, `x * 0` | `x`, `x`, `#0`, `#0` |
| Common-subexpression elimination | `Add v1, v2` appears twice | second one reuses the first |
| Guard elimination | the same check `v5 < v9` twice | second check removed |
| Dead-code elimination | value computed but never used or saved | removed |

CSE works with a hash table: key `(operator, operand1, operand2)` → existing value.

```bash
./build/jitvm -q --stats -O0 tests/programs/21_constant_folding.jbc    # loop body 6 ops
./build/jitvm -q --stats -O1 tests/programs/21_constant_folding.jbc    # loop body 3 ops
```

---

## 8. Loop peeling — THE NOVELTY (from the DLS 2012 paper)

**The problem.** In the -O1 code above, `a*b + 1` and `*3` are recomputed every iteration
although they never change. Normal compilers have a special pass for this
("loop-invariant code motion", LICM). Writing a correct LICM pass for traces is tricky.

**The paper's trick.** Don't write LICM at all. Instead:
1. Emit the recorded iteration **twice**: first a **preamble** (iteration 1), then the loop body.
2. Run the *same* forward optimiser over both, **keeping its CSE table**.
3. When the optimiser reaches `Mul v3, v4` in the loop body, the table already contains
   `Mul v3, v4` from the preamble → it reuses the preamble's result. The work has moved out of
   the loop!

Why does it only remove the *invariant* work? Because anything that changes from iteration to
iteration flows through a **Phi** — and a Phi is a brand-new name, so `Add v1, v17(Phi)` can
never match a preamble entry. Only computations whose inputs are unchanged since the preamble
match.

Result (`-O2`), loop body only:

```
v17 = Phi R0 [entry v14, back v21]
v18 = Phi R5 [entry v13, back v20]
v20 = Add v12, v18        ; acc += k   (k = v12 was computed ONCE in the preamble)
v21 = Add v1, v17         ; i += 1
GuardLT v21, v2
```

6 operations per iteration → 3. On benchmark B2 this alone doubles the speed (33× → 62× on the Mac).

**The Phi fix-point (one extra rule).** `LOAD_CONST R7, 3` writes R7 every iteration, so R7 gets a
Phi — but its entry value and back value are both the constant 3, so the Phi is pointless. We
mark R7 "invariant", rebuild, and repeat until nothing changes (usually 2 rounds).

```bash
./build/jitvm -q -O1 --dump-ir tests/programs/12_loop_invariant.jbc
./build/jitvm -q -O2 --dump-ir tests/programs/12_loop_invariant.jbc
./build/jitvm --compare bench/programs/b2_loop_invariant.jbc
```

**30-second explanation:** "We peel one iteration off the trace and optimise the peeled copy and
the loop together. Because the CSE and guard tables still hold the preamble's results, every
invariant computation in the loop hashes to an existing value and disappears — that is
loop-invariant code motion obtained for free, which is the novelty of Ardö et al. It is safe
because the preamble always runs first and loop-varying values are fresh Phi names that can
never match."

---

## 9. Snapshots and deoptimisation

Each guard carries a **snapshot**: which VM registers must be set to which values, and where
the interpreter must continue, if the guard fails. Example from the dump:

```
GuardLT v21, v2   snap2 -> pc 13 {R0=v21, R5=v20, R6=v10, R7=#3, R8=v12}
```

"If `v21 < v2` is false, write R0, R5, R6, R7, R8 from these values and resume at pc 13."

Inside the loop we **never** write the VM register file — values stay in CPU registers. Only
when leaving do we write them back. That is a big part of the speed-up.

```bash
./build/jitvm -vv tests/programs/22_side_exit_restores_state.jbc
# [DEOPT] ... resume interpreter at pc 12 (restored R0=77 R3=149226 R4=-2926 R5=5929)
```

**Fuel check:** the compiled loop decrements a counter every iteration; at zero it exits.
So even an infinite loop inside machine code is caught by the step limit (exit code 4).

---

## 10. Lowering to LIR — `src/trace_ir.cpp`, `lowerToLIR`

The LIR is a flat list close to machine code: every SSA value gets a numbered **slot**
(`s1, s2, …`), constants become immediates (`#3`). Phi nodes become **copies**: before the loop
(`copy s12 <- s11`) and at the end of each iteration.

**Phi coalescing:** if the old value of a loop variable is not needed after its new value is
computed, the new value is written *into the same slot* — like `i = i + 1` in place — and the
copy disappears. You can see it in the LIR of our example: `add s13 <- s9, s13`.
(We measured removing this: the Mac loop became 2× slower, so we kept it.)

```
LOOP:
  fuel_check  else exit1
  add s13 <- s9, s13         ; acc += k   (in place)
  add s12 <- s2, s12         ; i += step  (in place)
  guard_lt s12, s3  else exit2
  jump LOOP
```

---

## 11. Register allocation — linear scan (`allocateRegisters` in `src/backend_native.cpp`)

CPUs are fast with registers and slower with memory. We have more slots than CPU registers, so
we decide which slot lives in which register. **Linear scan** (Poletto & Sarkar, 1999):

1. For each slot, its **live interval** = from the first instruction that uses it to the last.
2. Values used in the loop that were created before it must stay alive for the *whole* loop.
3. Walk the intervals in order of start. When an interval ends, its register becomes free.
   Give each new interval a free register; if none is free, the value that ends latest goes to
   memory ("spill").

Think of booking meeting rooms: a room is reused as soon as the previous meeting ends.
AArch64 has 11 registers available for this, x86-64 has 10.

---

## 12. Machine code — `src/backend_native.cpp`

Each LIR instruction has a **template**: a function that writes the bytes of the matching CPU
instruction. Every instruction is a fixed bit pattern from the CPU manual with register
numbers OR-ed into fields. Example (AArch64, `ADD w9, w9, w10`):

```
0x0B000000          base pattern of "ADD (32-bit register)"
| (10 << 16)        source register 2 = w10   (bits 16-20)
| (9 << 5)          source register 1 = w9    (bits 5-9)
| 9                 destination       = w9    (bits 0-4)
= 0x0B0A0129
```

The final loop for `sum += i; i += 1; while (i < n)` on your Mac is 7 instructions:

```
LOOP: subs w22, w22, #1     ; fuel
      b.eq exit1
      add  w26, w23, w26    ; sum += i
      add  w23, w24, w23    ; i += step
      cmp  w23, w25
      b.lt LOOP             ; loop inversion: jump back while the condition holds
      b    exit2
```

**Executable memory:** you cannot just write bytes and run them. Memory is either writable or
executable, never both (W^X). On Apple Silicon we use `mmap(MAP_JIT)` and switch writing on
and off with `pthread_jit_write_protect_np`, then flush the instruction cache.

**Safety net:** when the VM starts, it compiles a tiny test trace and runs it in a **child
process** (`fork`). If the child crashes or gets a wrong answer, the VM uses the portable
**IR executor** (`src/backend_ir.cpp`) instead — your demo cannot crash because of codegen.

```bash
./build/jitvm --backend-info
./build/jitvm -q --dump-asm bench/programs/b1_sum_loop.jbc
JITVM_NO_NATIVE=1 ./build/jitvm tests/programs/01_valid_stable_loop.jbc   # forces IR executor
```

**IR executor:** runs the optimised LIR without machine code. It is not about speed (roughly
interpreter speed); it lets the tests compare "optimiser + IR executor" with "optimiser + machine
code", so a bug can be traced to the optimiser or to the code generator.

---

## 13. Entering compiled code — the code cache

When a trace is compiled, the bytecode instruction at the loop header is **patched** to an
internal `TRACE_ENTER` opcode (LuaJIT does the same). When the interpreter reaches it, it calls
the machine code. Ordinary instructions therefore pay nothing for the JIT.

---

## 14. How we know it is correct — testing (`tests/`)

The interpreter is the **reference**. Every test program is run by the interpreter and by 30 JIT
configurations (2 backends × 3 optimisation levels × 5 hot thresholds); output, final registers
and status must be **identical**. A generator also creates 400 random programs (nested loops,
if/else, prints) × 7 configurations. On both CPUs: 3,520 runs each, 0 differences.

```bash
make test
```

---

## 15. Reading the benchmark results

`make bench` → `results/bench_<arch>.csv`. "Speed-up" = interpreter time ÷ JIT time.
* B1–B6 (loops): 10–62× on the Mac.
* B2 is where peeling matters (-O1 33× → -O2 62×).
* B7 (branch alternates): only ~1.8× — the trace exits every other iteration. Limitation.
* B8 (contains DIV, not compiled): ~0.94× — the loop stays interpreted; small profiling cost.

Being honest about B7 and B8 is a strength in the viva: it shows you understand *why*.

---

## 16. Glossary

| Term | Meaning |
|---|---|
| Trace | the list of instructions executed in one loop iteration |
| Guard | a check that the recorded assumption still holds |
| Side exit | leaving compiled code because a guard failed |
| Snapshot | the register values + resume pc needed after a side exit |
| Deoptimisation | going back from optimised code to the interpreter |
| SSA | every value assigned exactly once |
| Phi | value that is the entry value on iteration 1 and the back-edge value afterwards |
| CSE | reuse an identical earlier computation |
| LICM | moving loop-invariant work out of the loop |
| Loop peeling | emitting the first iteration separately (our novelty, gives LICM) |
| Live interval | the range of instructions during which a value is needed |
| W^X | memory is writable or executable, never both |
| Fuel | iteration budget inside compiled code |

## 17. If you remember only ten things

1. Interpret first, compile only hot loops (3 back-edges).
2. A trace is one recorded iteration; branches become guards.
3. Guards carry snapshots; failing a guard restores registers and resumes interpreting.
4. SSA gives every value one name; Phis join iteration values.
5. Forward pass: folding, CSE, guard elimination; then DCE.
6. **Novelty:** peel one iteration; CSE across preamble → invariant work leaves the loop.
7. LIR slots → linear-scan registers → machine-code templates.
8. W^X memory, MAP_JIT on Apple Silicon, forked self-test with IR fallback.
9. Correctness: differential testing against the interpreter, 0 mismatches.
10. Limits: one trace per loop (B7), DIV not compiled (B8), integers only.

## 18. What each member should be able to open and explain

| Member | Open these | Be ready to explain |
|---|---|---|
| Atul (M1) | `src/vm.cpp` (`run`, `compileTrace`, `enterTrace`), `src/jitvm.cpp` | state machine, TRACE_ENTER patching, fuel, self-test, how modules connect |
| Emmanuel (M2) | `src/assembler.cpp`, `src/verifier.cpp`, `src/recorder.cpp` | two passes, definite assignment, recording rules and aborts, Review 2 bugs |
| Yadesh (M3) | `src/optimizer.cpp` (`arith`, `guard`, `buildOnce`), `src/trace_ir.cpp`, `src/backend_native.cpp` | SSA, CSE, **loop peeling**, Phi coalescing, linear scan, one instruction encoding |
| Tanvi (M4) | `tests/test_main.cpp`, `results/`, `Snapshot`/`LExit` in `src/trace_ir.h` | differential testing, fuzzing, snapshots, benchmarks and limitations |
