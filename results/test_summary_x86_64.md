native backend: native x86-64 backend (self-test passed)

| Section | Passed | Failed |
|---|---|---|
| 1. Assembler (front end) | 25 | 0 |
| 2. Verifier (static analysis) | 14 | 0 |
| 3. Review 2 regression (Instruction-vector API) | 27 | 0 |
| 4. Recorder / profiler behaviour | 24 | 0 |
| 5. Optimizer (SSA, folding, CSE, guard elimination, DCE, loop peeling) | 16 | 0 |
| 6. Deoptimisation / snapshot restore | 16 | 0 |
| 7. Golden output (interpreter reference) | 24 | 0 |
| 8. Differential matrix (JIT vs interpreter) | 24 | 0 |
| 9. Random-program differential fuzzing | 2 | 0 |
| 10. Safety: step limit, runtime errors, rejected programs | 11 | 0 |

TOTAL: 183 passed, 0 failed; differential runs: 720 matrix + 2800 fuzz (2834 traces compiled)
