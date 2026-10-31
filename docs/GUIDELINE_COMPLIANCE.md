# Review 3 Guideline Compliance Checklist

Every requirement of *BCSE307 Project Review 3 Guidelines* (copy in `docs/previous_reviews/`)
mapped to where it is met. ✅ = done in this package · ⬜ = the team must do it before the review.

## Section 1 — Expected outcome

| Requirement | Where it is met | |
|---|---|---|
| Closure of all Review 2 corrections | Report §3.4 (Tables 3–4); slide 3; regression tests `tests/test_main.cpp` §3 | ✅ |
| Stable, integrated, reproducible implementation with build/run instructions | `make`, `make test`; README; Report App. A | ✅ |
| Live demo with valid, invalid, boundary, failure inputs | `demo.sh`; `docs/DEMO_SCRIPT.md`; slide 10 | ✅ |
| Evidence of correctness, comparison, benchmarking | Report §9–10; `results/`; slides 11–12 | ✅ |
| Algorithms, design decisions, compiler concepts, limitations, trade-offs | Report §6–7, §11; slides 5, 8, 13 | ✅ |
| Repository, README, test suite, final report, presentation | this folder | ✅ |
| Verifiable individual contribution; viva readiness | Report §12; `docs/CONTRIBUTION_LOG.md`; `docs/LEARNING_GUIDE.md`; `docs/VIVA_PREP.md` | ✅ (⬜ confirm split) |
| Limitations, lessons, extensions, publication potential | Report §11, §13; slide 16 | ✅ |

## Section 2 — Evaluation components (10 marks)

| Component | Evidence prepared |
|---|---|
| 1. Completeness & correctness (2.5) | Objective matrix (Report Table 2): all 6 objectives + 2 extensions; 210/210 checks on 2 CPUs |
| 2. Live demonstration (1.5) | 7-stage demo (`demo.sh`), normal/boundary/invalid/failure inputs, `--compare` for evaluator input |
| 3. Testing & evaluation (1.5) | 10-section suite, golden + differential + fuzz tests, 8 benchmarks, charts, interpretation |
| 4. Technical depth & innovation (1.0) | SSA optimiser, loop peeling from DLS 2012, linear-scan allocation, two native back ends |
| 5. Report, repository, reproducibility (1.0) | Report in required structure; README; logs for both CPUs; `results/plot_results.py` |
| 6. Individual contribution (1.5) | Contribution matrix + per-member files and tests; ⬜ commits from each member's account |
| 7. Presentation & viva (1.0) | 18-slide deck with speaker notes; viva Q&A; learning guide; acknowledgement slide |

## Section 3 — Member-wise presentation (18–22 min + 8–12 min viva, demo ≥ 8 min)

| Presenter | Time | Slides | Demo commands |
|---|---|---|---|
| M1 Atul Vipin | 4 min | 1–5, 16 | `--backend-info`, `01_valid_stable_loop` |
| M2 Emmanuel Joshy | 4–5 min | 6–7 | `e01`, `--check`, `w01`, `03`, `11` |
| M3 Yadesh Waran M | 4–5 min | 8–9 | `-O1/-O2 --dump-ir`, `--dump-asm`, `--compare b2` |
| M4 Tanvi Gupta | 4–5 min | 11–14, 18 | `-vv 22`, `e04`, `make test`, `jit_bench` |
| All | 8–12 min | 10, 15, 17 | evaluator-selected input with `--compare` |

## Section 4 — Presentation structure (14–18 content slides)

Deck `Review3_Presentation_Team6.pptx`: **18 content slides in exactly the guideline order**
(1 title … 18 references) **+ 3 backup slides** (deoptimisation trace, full benchmark table,
bytecode language) for the viva. ✅

## Section 5 — Final report (15–25 pages excluding appendices)

`Review3_Report_Team6.docx` / `.pdf`: cover + declaration + contents (pp. 1–4), **main body
pp. 5–24 = 20 pages**, appendices A–G (pp. 25–35). Abstract = 246 words (required 200–300). ✅

| Required part | Report |
|---|---|
| Cover page and declaration | pp. 1–2 (⬜ fill review date, sign) |
| 1 Abstract … 14 References | §1–§14, same titles and order as the guideline |
| Appendices: user manual, installation, sample runs, test table, grammar, contribution log, diagrams | App. A (manual + install), B (grammar/ISA), C (sample runs), D (test table), E (code listings), F (demo plan, presentation allocation, evaluator record), G (x86-64 results); diagrams in §6; contribution log in `docs/` |

## Section 6 — Mandatory contribution matrix

Report §12, Table "Mandatory final contribution matrix" — all six columns filled. ⬜ Confirm it
matches what each member really did.

## Section 7 — Final submission checklist

| Item | Status |
|---|---|
| Final presentation following the prescribed structure | ✅ |
| Complete final report in the required format | ✅ |
| Stable integrated source code, buildable package | ✅ |
| Repository link with complete history, README, licence/acknowledgements, dependencies | README ✅, LICENSE ✅ · ⬜ push to `github.com/Yadesh-005/JIT` with commits from each member |
| Build/run instructions verified on the demonstration system | ⬜ run `make && make test` on the Mac once before the review |
| Test suite: valid, invalid, boundary, failure, regression | ✅ `tests/programs`, `tests/errors`, `tests/test_main.cpp` |
| Result tables, logs, charts, benchmarks | ✅ `results/`, `docs/figures/` |
| Sample inputs needed for reproduction | ✅ `tests/`, `bench/programs/` |
| Architecture, module, interface, data-flow diagrams matching the implementation | ✅ Report Figs 1–2, Tables 7–8 |
| Contribution matrix and log with evidence | ✅ (⬜ confirm) |
| Known limitations, unsupported cases, future work | ✅ Report §11, §13; README |
| Backup copy of code, report, presentation, demo recording | ⬜ copy this folder to a USB drive / cloud; optionally screen-record `./demo.sh` |
| External code, libraries, ideas acknowledged | ✅ Report §14, declaration, slide 18 (no third-party code) |
| No credentials, personal data or unapproved copyrighted material | ✅ |

## Section 8 — Demonstration sequence

All seven stages are scripted in `demo.sh` and `docs/DEMO_SCRIPT.md`; the blank evaluator
record table is in Report Appendix F. ✅
