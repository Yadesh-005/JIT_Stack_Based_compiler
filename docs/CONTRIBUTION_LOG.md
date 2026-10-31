# Member-Wise Contribution Log (Review 3)

> **Proposed allocation — confirm before submission.** This split follows the
> Review 2 contribution record and its "next responsibility" column. Edit it so that it
> matches what each member actually did; the evaluator may check commits and files.

| Member | Role (Review 3 guideline) | Primary technical ownership | Files / evidence | Tests owned | Viva module |
|---|---|---|---|---|---|
| Atul Vipin (24BCE2465) | Member 1 – Project Lead / System Integrator | Architecture, VM state machine and code cache (header patching), executable memory (W^X, MAP_JIT), native self-test, CLI, build | `src/vm.cpp` (run, compileTrace, enterTrace), `src/vm.h`, `src/jitvm.cpp`, `src/backend.h`, `ExecMemory` + self-test in `src/backend_native.cpp`, `Makefile`, `build.sh`, `demo.sh` | CLI tests (`tests/run_cli_tests.sh`), Review 2 regression section | Integration, state machine, objective compliance |
| Emmanuel Joshy (24BCE0916) | Member 2 – Front End / Analysis | ISA, assembler, verifier (CFG + data-flow), interpreter core, profiler, trace recorder completion | `src/isa.*`, `src/assembler.*`, `src/verifier.*`, `src/recorder.*`, interpreter switch in `src/vm.cpp` | Sections 1, 2, 4 of `tests/test_main.cpp`; `tests/errors/*` | Parsing, verification, recording rules, error handling |
| Yadesh Waran M (24BCE2402) | Member 3 – Core Processing / Back End | SSA TraceIR, optimiser incl. loop peeling (paper novelty), LIR lowering + phi coalescing, linear-scan register allocation, x86-64/AArch64 code generation, IR executor | `src/trace_ir.*`, `src/optimizer.*`, `src/backend_native.cpp` (allocateRegisters, X64, A64), `src/backend_ir.cpp` | Section 5 (optimiser) | Optimisation, code generation, performance |
| Tanvi Gupta (24BAI0254) | Member 4 – Testing / Evaluation / Release | Snapshots and deoptimisation (exit stubs, fuel exits), test suite (golden, differential, fuzz), benchmarks, results, documentation, report | `Snapshot`/`LExit` design, `enterTrace` exit handling, `tests/test_main.cpp`, `tests/programs/*`, `bench/*`, `results/*`, `README.md`, report | Sections 3, 6–10; benchmark harness | Deopt, testing strategy, results, limitations |

## Integration responsibilities

* Atul – merges modules, owns the build and the demonstration machine setup.
* Emmanuel ↔ Yadesh – interface: `RecordedTrace` (Level-1 TraceIR, `src/trace_ir.h`).
* Yadesh ↔ Tanvi – interface: `Snapshot` / `LExit` (what an exit must restore).
* Tanvi – runs `make test` + `make bench` on every merge and records results in `results/`.

## Suggested commit plan for the shared repository

Commit each module from its owner's account, e.g.
`feat(frontend): two-pass assembler with line diagnostics`,
`feat(opt): loop peeling (Ardo et al. DLS'12)`, `feat(backend): AArch64 emitter`,
`test: differential fuzzing`, `docs: Review 3 report` — so that the history matches this matrix.
