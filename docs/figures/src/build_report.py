import csv
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(__file__))
from docxlib import Report, DARK  # noqa: E402

PROJ = "/home/claude/proj"
TEMPLATE = "/home/claude/r2/review2.docx"
OUT = sys.argv[1] if len(sys.argv) > 1 else "/home/claude/report/Review3_Report_Team6.docx"
PAGES = {}
if len(sys.argv) > 2:
    import json
    PAGES = json.load(open(sys.argv[2]))
FIG = PROJ + "/docs/figures/"
ARM = "/tmp/arm/rep/"


def rd(p):
    return open(p).read().rstrip("\n")


def bench(arch):
    rows = {}
    for r in csv.DictReader(open(f"{PROJ}/results/bench_{arch}.csv")):
        rows.setdefault(r["benchmark"], {})[r["configuration"]] = (float(r["median_seconds"]), float(r["speedup_vs_interpreter"]))
    return rows


R = Report(TEMPLATE, "Review 3")
P, B, H1, H2, H3, T, C = R.para, R.bullet, R.h1, R.h2, R.h3, R.table, R.code

# =========================================================== cover
P("VELLORE INSTITUTE OF TECHNOLOGY", align="center", before=600, after=120, size=16, color=DARK, bold=True)
P("School of Computer Science and Engineering", align="center", after=480, size=11, italic=True)
P("BCSE307 — COMPILER DESIGN PROJECT", align="center", after=120, size=13, bold=True)
P("PROJECT REVIEW 3", align="center", after=480, size=20, bold=True, color="C00000")
P("Final Demonstration, Evaluation and Project Completion Review", align="center", after=200, size=11, italic=True)
P("A2: Trace-Based JIT Compiler for a Custom Bytecode Virtual Machine", align="center", after=480, size=13, bold=True)
T(["Field", "Details"], [
    ["Course Code / Title", "BCSE307 - Compiler Design"],
    ["Project ID / Title", "A2: Trace-Based JIT Compiler for a Custom Bytecode Virtual Machine"],
    ["Team Number", "Team 6"],
    ["Team Members & Reg. No.", "YADESH WARAN M [24BCE2402]\nEMMANUEL JOSHY [24BCE0916]\nTANVI GUPTA [24BAI0254]\nATUL VIPIN [24BCE2465]"],
    ["Review Date", "____ / ____ / 2026"],
    ["Faculty / Evaluator", "Dr. Shalini L"],
    ["GitHub Repository Link", "https://github.com/Yadesh-005/JIT/tree/main"],
], [2.1, 4.4], size=10)

# =========================================================== declaration
R.page_break()
P("DECLARATION OF ORIGINALITY", align="center", before=200, after=300, size=14, bold=True, color=DARK)
P("We, the undersigned members of Team 6, declare that the project titled **\"Trace-Based JIT Compiler for a Custom "
  "Bytecode Virtual Machine\"** (Project ID A2), submitted for Review 3 of BCSE307 – Compiler Design, is the result of "
  "our own work carried out under the guidance of Dr. Shalini L, School of Computer Science and Engineering, Vellore "
  "Institute of Technology.")
P("The source code, test suite, benchmarks and this report were developed for this project. No third-party source "
  "code or library other than the C++ standard library and the POSIX operating-system interface is used. Published "
  "research that influenced the design — in particular the loop-peeling technique of Ardö, Bolz and Fijałkowski "
  "[1] — is cited where it is used, and all books, papers, manuals and tools are acknowledged in Section 14. "
  "An AI assistant was used for drafting and review during development; every part of the submitted work has been "
  "read, tested and is understood and owned by the team member named in Section 12.")
P("The results reported in Sections 9 and 10 were produced by the code in the repository and can be reproduced with "
  "`make test` and `make bench`.", after=480)
T(["Name", "Register No.", "Signature", "Date"], [
    ["Yadesh Waran M", "24BCE2402", "", ""], ["Emmanuel Joshy", "24BCE0916", "", ""],
    ["Tanvi Gupta", "24BAI0254", "", ""], ["Atul Vipin", "24BCE2465", "", ""]],
    [2.0, 1.4, 1.9, 1.2], size=10)

# =========================================================== TOC
R.page_break()
P("Table of Contents", align="left", after=200, size=15, bold=True, color=DARK)
R.toc_placeholder()

# =========================================================== 1 Abstract
H1("1. Abstract", page_break=True)
P("This project delivers a mixed-mode virtual machine for a custom register bytecode with a trace-based "
  "just-in-time (JIT) compiler that generates native AArch64 (Apple Silicon) and x86-64 machine code. A front end "
  "assembles the textual bytecode and verifies it with control-flow and data-flow analysis. The interpreter counts "
  "taken backward branches; when a loop becomes hot (HOT_THRESHOLD = 3) the recorder captures the executed path as "
  "TraceIR, turning every branch into a guard. The optimiser converts the trace to static single assignment (SSA) "
  "form and applies constant folding, common-subexpression elimination, redundant-guard elimination and dead-code "
  "elimination.")
P("The research novelty for Review 3 implements the central idea of Ardö, Bolz and Fijałkowski, *Loop-Aware "
  "Optimizations in PyPy's Tracing JIT* (DLS 2012): the trace is emitted as a peeled first iteration followed by the "
  "loop body, so the same forward passes also perform loop-invariant code motion. Optimised traces are register-"
  "allocated with linear scan and encoded as machine code; a portable IR executor is the fallback. A failing guard "
  "deoptimises through a snapshot that restores the VM registers and resumes interpretation at the exact bytecode "
  "position.")
P("All Review 2 defects were corrected. The system passes 183 automated checks and 27 command-line tests on both "
  "Apple Silicon and x86-64, including 3,520 differential runs per machine in which every JIT configuration "
  "reproduced the interpreter's output exactly. On loop benchmarks the native JIT is 10–62× faster than the "
  "interpreter on Apple Silicon (14–75× on x86-64), and loop peeling alone adds 1.9–3.0× on loop-invariant code.")

# =========================================================== 2 Intro
H1("2. Introduction and Motivation")
H2("2.1 Context")
P("A bytecode interpreter is portable and simple, but every executed instruction pays for fetching, decoding and "
  "dispatching it, and every intermediate value travels through memory. Production language runtimes such as the "
  "HotSpot JVM, V8, LuaJIT and PyPy therefore combine an interpreter with a JIT compiler that translates frequently "
  "executed code into machine code while the program runs. *Trace-based* JIT compilers [3]–[6] go one step further: "
  "instead of compiling whole functions they record the single path a hot loop actually takes, compile that straight-"
  "line path aggressively, and protect every assumption with a guard that falls back to the interpreter when the "
  "assumption no longer holds.")
H2("2.2 Need and beneficiaries")
P("Real JITs are hundreds of thousands of lines long, which makes their core ideas difficult to study. The aim of "
  "this project is a complete, small and fully tested trace-based JIT in which every stage — profiling, recording, "
  "intermediate representation, optimisation, register allocation, instruction encoding and deoptimisation — is "
  "visible and can be demonstrated in isolation. The beneficiaries are students and teachers of compiler design "
  "(each module maps to a course topic), developers who need a small embeddable scripting VM for resource-constrained "
  "settings (the motivation behind HotpathVM [4]), and anyone prototyping trace optimisations, who can add a pass "
  "and validate it immediately with the differential test harness.")
H2("2.3 Relevance to compiler design")
T(["Compiler-design topic", "Where it appears in the project"], [
    ["Lexical and syntax analysis", "Two-pass assembler for the .jbc language with full error recovery (Section 8.2)"],
    ["Semantic analysis / static checks", "Verifier: operand and target checks, CFG reachability, definitely-assigned data-flow analysis"],
    ["Intermediate representations", "Three levels: recorded TraceIR, SSA TraceIR with Phi nodes and snapshots, slot-based LIR"],
    ["Data-flow analysis and SSA", "Forward must-analysis in the verifier; SSA construction; liveness for DCE and register allocation"],
    ["Machine-independent optimisation", "Constant folding, algebraic simplification, CSE/value numbering, guard elimination, DCE, loop peeling (LICM)"],
    ["Code generation", "Instruction selection by templates, linear-scan register allocation, x86-64 and AArch64 encoding, peephole optimisation"],
    ["Run-time environment", "Interpreter, profiler, code cache, calling convention between VM and generated code, deoptimisation"],
], [2.2, 4.3], caption="Mapping of course concepts to project modules")
H2("2.4 What Review 3 adds")
P("At Review 2 the system could detect hot loops and record TraceIR, but nothing consumed the recorded trace. For "
  "Review 3 the team completed the back half of the pipeline (SSA optimiser, LIR, two native code generators, code "
  "cache, deoptimisation), corrected every Review 2 defect, implemented the research-paper novelty, and built an "
  "automated test and benchmark infrastructure that runs on the demonstration Mac.")

# =========================================================== 3 Problem
H1("3. Problem Statement, Objectives, Scope and Limitations")
H2("3.1 Problem statement")
P("Design and implement a virtual machine for a custom register bytecode that starts executing programs in an "
  "interpreter, identifies hot loops at run time, records and optimises their execution traces, compiles them to native "
  "machine code, and transparently returns to the interpreter whenever a speculative assumption fails — while "
  "guaranteeing that every program produces exactly the same output and final state as pure interpretation.")
H2("3.2 Objective-compliance matrix")
T(["Objective", "Measurable criterion", "Evidence", "Status"], [
    ["O1 Interpreter for the custom bytecode", "All 13 opcodes executed with defined 32-bit semantics; runtime errors reported", "Golden tests 24/24; safety tests 11/11", "Achieved"],
    ["O2 Hot-loop detection", "Loop becomes hot on the 3rd taken back-edge; threshold configurable", "Test §4: thresholds 1, 2, 3, 4, 5, 9", "Achieved"],
    ["O3 Trace recording with limits", "Guard for every branch direction; 5 abort conditions; MAX_TRACE_LENGTH = 50", "Recorder tests 24/24; Review 2 cases 27/27", "Achieved"],
    ["O4 TraceIR optimisation", "SSA + folding, CSE, guard elimination, DCE; fewer ops per iteration", "12_loop_invariant: 6 → 3 ops; optimiser tests 16/16", "Achieved"],
    ["O5 Native code generation + code cache", "Real machine code for x86-64 and AArch64; ≥ 10× on loop benchmarks", "B1–B6: 10–62× (Apple Silicon), 14–75× (x86-64)", "Achieved"],
    ["O6 Snapshots, deoptimisation, mixed-mode", "Identical output/registers to the interpreter in every configuration", "3,520 differential runs, 0 mismatches", "Achieved"],
    ["O7 Research novelty (extension)", "Loop peeling [1] implemented and evaluated", "B2: 1.9× (AArch64) / 3.0× (x86-64) over -O1", "Achieved"],
    ["O8 Robustness and reproducibility (extension)", "No crash on invalid input; one-command build and test on the demo Mac", "27 CLI tests; ASan/UBSan clean; logs for 2 architectures", "Achieved"],
], [1.55, 2.05, 2.05, 0.85], size=9, caption="Final objectives, measurable criteria and evidence")
H2("3.3 Final scope")
B("**In scope:** text assembler and verifier; interpreter for 13 opcodes on 16 × 32-bit registers; hot-loop profiler "
  "with back-off and blacklisting; trace recorder; three IR levels; SSA optimiser with loop peeling; linear-scan register "
  "allocation; native back ends for x86-64 (System V) and AArch64 (AAPCS64, including Apple's MAP_JIT rules); portable "
  "IR executor; snapshot deoptimisation; fuel-based step limit inside compiled code; command-line driver, test suite, "
  "benchmark harness and documentation.")
B("**Out of scope (documented as future work):** side traces and trace trees, calls and memory/heap instructions, "
  "floating point, garbage collection, multithreading.")
H2("3.4 Closure of Review 2 action items")
T(["Review 2 item (Section 11–12 of Review 2)", "Action taken for Review 3", "Owner", "Status"], [
    ["Missing snapshot state: side exits could not reconstruct registers (High)", "Every guard owns a Snapshot (register → SSA value, resume pc); exit stubs write it back; fuel exits snapshot the loop header", "Tanvi G.", "Closed"],
    ["Backend complexity: emitting raw x86-64 is error-prone (Medium)", "Narrow LIR (14 ops) + template encoders for x86-64 and AArch64; forked self-test; IR executor fallback", "Atul V. / Yadesh M.", "Closed"],
    ["TraceIR optimiser pass (DCE, constant folding) – Week 7", "SSA optimiser: folding, simplification, CSE, guard elimination, DCE, loop peeling", "Yadesh M.", "Closed"],
    ["Interpreter-to-native transition (OSR)", "Loop header patched with internal TRACE_ENTER opcode; trace entered at the header", "Emmanuel J.", "Closed"],
    ["Coupling of profiler/recorder inside one class flagged for refactoring", "Split into recorder, optimiser, lowering and back-end modules with explicit interfaces", "Atul V.", "Closed"],
    ["Coverage gaps: multi-loop and nested-loop programs", "Tests 06, 09, 11, 24 + random nested-loop fuzzing", "Tanvi G.", "Closed"],
    ["Weeks 8–12: deopt, native code cache, mixed-mode regression, benchmarking", "All delivered; results in Sections 9–10", "All", "Closed"],
], [2.0, 2.75, 1.0, 0.75], size=9, caption="Review 2 risks and pending work, and their closure")
P("While completing the system the team re-examined the Review 2 code and found the defects in Table 4. Each was "
  "reproduced against the unchanged Review 2 sources (kept in `legacy/review2/`), fixed, and is now covered by a "
  "regression test.", keep_next=True)
T(["#", "Review 2 defect (reproduced)", "Correction", "Regression test"], [
    ["D1", "When a loop exited during recording, recording continued into the following code and no later loop could ever be traced", "Recorder aborts with \"loop exited while recording\"", "11_loop_exit_during_recording"],
    ["D2", "LOAD_CONST, PRINT_REG and forward branches inside the loop were not recorded, so the trace did not represent the loop", "Every supported instruction recorded; each branch direction becomes a guard with an exit pc", "10, 16, 18, 20, fuzzing"],
    ["D3", "MAX_TRACE_LENGTH was checked only for ADD_INT", "Length checked for every recorded op", "04, 07 (boundary 50)"],
    ["D4", "Test case 8 never terminated (R0 never incremented)", "Corrected test; VM step limit and fuel check catch non-termination", "08; r2_case8_original"],
    ["D5", "Register indices were not checked (out-of-bounds write, found with ASan)", "Verifier rejects R16+ and bad jump targets before execution", "Verifier tests, e01, e02"],
    ["D6", "ADD_INT used signed overflow (undefined behaviour, found with UBSan)", "Defined 32-bit wrap-around shared by interpreter, folder and back ends", "14_wraparound"],
    ["D7", "After an abort the counter passed HOT_THRESHOLD and the loop was never retried (accidental blacklist)", "Explicit back-off: counter reset, retry once, then blacklist", "03, 09"],
    ["D8", "The recorded trace was never used; two divergent copies of the VM existed", "Single source of truth; trace optimised, compiled and executed", "Whole suite"],
], [0.35, 2.55, 2.25, 1.35], size=8.5, caption="Defects found in the Review 2 code and their correction")
H2("3.5 Changes from the approved plan")
B("**Register VM.** The Review 2 detail table called the VM \"stack-based\"; the implementation has always been a "
  "16-register machine. The Review 3 title and documents use the register-based description consistently.")
B("**In-house code generators instead of LLVM ORC.** The Review 2 plan considered LLVM ORC. We instead wrote "
  "encoders for both target ISAs: no external dependency for the evaluator to install, compilation takes microseconds, "
  "and the instruction encoding itself becomes a demonstrable part of the project.")
B("**ISA extended** with SUB_INT, MUL_INT, AND_INT, MOV, JUMP, JUMP_IF_EQ and DIV_INT so that realistic loops, "
  "branches and a deliberately non-JIT-able instruction can be demonstrated; the original six opcodes keep their "
  "numbering, so all Review 2 programs still run unchanged.")
B("**Textual assembler** added so that the evaluator can type new programs during the viva.")
H2("3.6 Limitations (summary)")
P("One linear trace per loop (no side traces or trace trees); integer-only ISA without memory operations or calls; "
  "template-based instruction selection. Section 11 discusses limitations and their measured impact in detail.")

# =========================================================== 4 Related work
H1("4. Background and Related Work")
H2("4.1 Concepts")
B("**Mixed-mode execution.** Code starts in the interpreter; only code proven hot by profiling is compiled, so cold "
  "code never pays compilation cost.")
B("**Trace.** The sequence of instructions executed during one iteration of a hot loop, starting at the loop header "
  "and ending when control returns to it. Branches become **guards**; a failing guard causes a **side exit**.")
B("**SSA.** Every value is assigned exactly once; values that differ between loop iterations meet in **Phi** nodes at "
  "the loop header. SSA makes value numbering, folding and liveness simple.")
B("**Deoptimisation.** Returning from optimised code to the interpreter requires reconstructing the interpreter's "
  "state from the optimised state — the idea introduced by Hölzle et al. for SELF [9]. We store it as snapshots.")
H2("4.2 Related systems")
T(["System", "Venue", "Approach", "Relation to this project"], [
    ["Dynamo [3]", "PLDI 2000", "Trace-based optimisation of native binaries; NET trace selection", "Origin of hot-trace selection at backward branches"],
    ["HotpathVM [4]", "VEE 2006", "Trace JIT for a JVM on small devices; SSA traces", "Cited in Review 2; same resource-constrained motivation"],
    ["TraceMonkey [5]", "PLDI 2009", "Trace JIT for JavaScript; type specialisation; trace trees", "Guards/side exits; trace trees are our future work"],
    ["PyPy [6], [7]", "ICOOOLPS 2009, PEPM 2011", "Meta-tracing JIT; allocation removal by partial evaluation", "Snapshot/forward-optimisation design"],
    ["LuaJIT 2 [13]", "open source", "SSA trace IR; LOOP pass by copy-substitution", "Source of the loop-peeling idea adopted in [1]"],
    ["Ardö et al. [1]", "DLS 2012", "Loop peeling + existing forward passes = LICM", "**Novelty implemented in Review 3**"],
    ["This project", "—", "Register bytecode; SSA trace IR; peeling; linear-scan RA; x86-64 + AArch64", "Complete pipeline in ≈ 3,100 lines of C++"],
], [1.15, 1.05, 2.2, 2.1], size=9, caption="Comparison with related trace-based systems")
H2("4.3 Selected paper and its novelty")
P("**H. Ardö, C. F. Bolz and M. Fijałkowski, \"Loop-Aware Optimizations in PyPy's Tracing JIT,\" Proc. 8th "
  "Symposium on Dynamic Languages (DLS '12), ACM, pp. 63–72, 2012** [1].")
P("A tracing JIT records one iteration of a loop, so a naïve compiler optimises that iteration in isolation and "
  "re-executes loop-invariant work on every iteration. Writing a separate loop-invariant code motion (LICM) pass for a "
  "trace IR is awkward, because guards and side effects make it unclear what may be moved. The paper's novelty is to "
  "*avoid writing LICM altogether*: the trace is duplicated into a **preamble** (the first iteration, peeled off) and a "
  "**peeled loop**; the JIT's existing forward optimisations then run over the concatenation. When the optimiser "
  "reaches an operation in the loop body whose inputs are unchanged since the preamble, its value-numbering table "
  "already contains the preamble's result, so the operation is replaced by that result — it has been hoisted. "
  "Redundant guards disappear the same way. Values the loop needs from the preamble are passed in as extra loop "
  "arguments. The authors report substantial speed-ups on numerical benchmarks for PyPy and observe that LuaJIT, "
  "which inspired the technique, benefits similarly.")
H2("4.4 Identified gap and our contribution")
P("The Review 2 prototype — like many teaching VMs — stopped at an IR that nothing consumed. Our contribution is a "
  "self-contained, verifiable implementation of the full path from bytecode to deoptimisable native code, with the "
  "loop-peeling optimisation of [1] adapted to a register-based SSA trace IR. Our adaptation differs from PyPy in "
  "three ways: (i) loop-carried values are explicit Phi nodes rather than jump arguments; (ii) invariant preamble "
  "values are visible in the loop body because they live in allocated registers or slots, so no extra arguments are "
  "needed; and (iii) a Phi fix-point removes Phi nodes whose back-edge value equals their entry value, which allows "
  "constants re-materialised inside the loop to fold.")

# =========================================================== 5 Requirements
H1("5. Requirements and Technology Stack")
T(["Category", "Requirement / choice"], [
    ["Language", "C++17, no compiler extensions"],
    ["Compilers verified", "Apple clang (Xcode CLT) on macOS; g++ 11.4 (AArch64) and g++ 13.3 / clang 18 (x86-64) on Linux"],
    ["Build tools", "GNU make (`make`), or `./build.sh` without make; bash for test/demo scripts"],
    ["Operating systems", "macOS 11+ (Apple Silicon or Intel); Linux x86-64 and arm64"],
    ["Hardware used for results", "Apple Silicon Mac (demo machine, measurements in its arm64 Linux environment); Intel Xeon 2.1 GHz (x86-64)"],
    ["Libraries", "C++ standard library and POSIX only (mmap, mprotect, fork, waitpid); Apple: pthread_jit_write_protect_np, sys_icache_invalidate"],
    ["Optional", "Python 3 + matplotlib to redraw charts; AddressSanitizer/UBSan for sanitizer builds"],
    ["Test programs", "24 functional programs, 7 error programs, 8 benchmarks (`tests/`, `bench/`), random program generator"],
], [1.8, 4.7], caption="Technology stack and environment")
P("**Functional requirements:** assemble and verify programs with line-numbered diagnostics; interpret all "
  "opcodes; detect, record, optimise and compile hot loops; leave compiled code at any failing guard with the exact "
  "interpreter state; expose internals (event log, IR and machine-code dumps, statistics); and offer a differential "
  "comparison mode for evaluator-selected inputs. ")
P("**Non-functional requirements:** bit-identical results in every mode; never execute writable memory (W^X); no "
  "crash on any input (all errors become diagnostics and exit codes); bounded execution (step limit, also inside "
  "native code); single-command build and test on the demonstration Mac.")

# =========================================================== 6 Design
H1("6. System Design")
H2("6.1 Final architecture")
R.figure(FIG + "architecture.png", 6.5, "Final architecture and data flow (module groups show primary owners)")
P("A program flows through four groups of modules. The **front end** turns text into a verified `Program`. The "
  "**runtime** interprets it, counts taken backward branches and, for a hot loop, drives the **trace recorder**. A "
  "closed trace is handed to the **JIT compiler**: SSA construction with loop peeling, forward optimisation, lowering "
  "to LIR, then native code generation (or the IR executor). The compiled trace is installed in the **code cache** by "
  "patching the loop-header instruction; when the interpreter next reaches the header it enters native code, which "
  "runs until a guard fails, and the exit stub restores the interpreter state.")
H2("6.2 Modules and interfaces")
T(["Module", "Files", "Key data structures", "Interface"], [
    ["Assembler", "assembler.h/.cpp", "Program {code, srcLine, labels}, Diagnostic", "assemble(text) → AssembleResult"],
    ["Verifier", "verifier.h/.cpp", "CFG successors, IN/OUT bitsets", "verifyProgram(Program) → diagnostics"],
    ["VM / interpreter / profiler", "vm.h/.cpp", "registers[16], code_ (patched copy), loopCounters_, traceAt_", "loadProgram(), run() → RunResult"],
    ["Trace recorder", "recorder.h/.cpp", "RecordedTrace {headerPC, IROp[]}", "start(), record(instr, pc, taken)"],
    ["Optimiser", "optimizer.h/.cpp", "SSATrace {SInst[], Snapshot[], loopStart}", "buildOptimizedTrace(raw, OptOptions)"],
    ["Lowering", "trace_ir.h/.cpp", "LIRTrace {LInst[], LExit[], numSlots}", "lowerToLIR(SSATrace)"],
    ["Back ends", "backend*.h/.cpp", "CompiledTrace, ExecMemory, emitter buffers", "compileNativeBackend(), compileIRBackend(), run(ctx)"],
    ["CLI / tools", "jitvm.cpp, tests/, bench/", "VMConfig, VMStats", "command-line options, exit codes"],
], [1.35, 1.25, 2.1, 1.8], size=8.5, caption="Modules, data structures and interfaces")
H2("6.3 Execution state machine")
R.figure(FIG + "state_machine.png", 5.6, "VM state machine (extends the three-state machine of Review 2 Section 3.2)")
P("Review 2 described three states (Interpreting, Recording, and a future Executing state). The final machine adds "
  "Compiling and the transitions out of Executing: a failing guard or exhausted fuel returns to Interpreting through a "
  "snapshot and resumes interpretation at the exact bytecode position.")
H2("6.4 Intermediate representations")
T(["Level", "Produced by", "Form", "Purpose"], [
    ["1 Recorded TraceIR", "Recorder", "Register-based ops: IR_ADD_INT R0,R0,R1; IR_GUARD_LT R0,R2 (exit → pc 6)", "Faithful record of the executed path (Review 2 IR, completed)"],
    ["2 SSA TraceIR", "Optimiser", "Values v0…vn, Phi nodes at the loop header, guards with snapshots", "All optimisations; deoptimisation metadata"],
    ["3 LIR", "Lowering", "Operations on numbered slots / immediates; LOOP label; exits with write-back lists", "Backend-neutral input to register allocation and code generation"],
], [1.25, 0.95, 2.6, 1.7], size=9, caption="The three IR levels")
H2("6.5 Key design decisions")
T(["Decision", "Alternative considered", "Rationale and trade-off"], [
    ["Trace-based JIT", "Method-based JIT", "Recorder needs no CFG analysis; optimiser sees straight-line code. Cost: divergent paths exit (B7)."],
    ["Three IR levels", "Optimise the recorded IR directly", "SSA makes CSE/DCE trivial; LIR decouples allocation and encoding from optimisation."],
    ["Lazy register write-back via snapshots", "Write VM registers on every iteration", "Removes all memory traffic from loops; exits pay a few stores instead."],
    ["Own x86-64/AArch64 encoders", "LLVM ORC / libgccjit", "No dependency, µs compile time, encoding visible in the demo. Cost: no instruction scheduling."],
    ["Patch loop header with TRACE_ENTER", "Look up the code cache before every instruction", "Zero lookup cost for ordinary instructions (LuaJIT's JLOOP technique)."],
    ["Fork-isolated native self-test", "Trust the encoder", "A wrong encoding on an untested OS can never crash the demo; VM falls back to the IR executor."],
    ["Fuel counter in compiled loops", "No limit in native code", "Infinite loops in JIT code are still reported (exit code 4) at the cost of one decrement per iteration."],
    ["Abort + back-off + blacklist", "Retry forever / never retry", "Bounded recording cost for loops that cannot be traced."],
], [1.75, 1.6, 3.15], size=8.5, caption="Design decisions and trade-offs")
H2("6.6 Run-time interface between VM and generated code")
P("Compiled traces are C-callable functions `int trace(TraceContext*)`. The context holds the VM register file, a "
  "slot array (slot 0 = fuel) and a VM pointer used by the PRINT helper. The return value is the index of the exit "
  "taken; the exit has already written its snapshot into the register file, so the VM only sets `pc = resumePC`.")
C("struct TraceContext {            // layout fixed: native code reads offsets 0 and 8\n"
  "    int32_t* regs;               // VM register file R0..R15\n"
  "    int32_t* slots;              // slot 0 = fuel, others = spilled SSA values\n"
  "    void*    vm;                 // handed back to jitPrintHelper()\n"
  "};")

# =========================================================== 7 Algorithms
H1("7. Algorithms and Methodology")
H2("7.1 Profiling and hot-loop detection")
P("For every taken jump whose target is at or before the jump's own address (`currentPC`, captured before `pc++` — "
  "the Review 2 fix) the profiler increments a per-target counter, skipping targets that already own a trace or are "
  "blacklisted. When the counter reaches HOT_THRESHOLD recording starts at the target. Cost: O(1) per backward branch "
  "(array lookup).")
H2("7.2 Trace recording")
T(["Instruction executed while recording", "Level-1 TraceIR emitted"], [
    ["LOAD_CONST, ADD/SUB/MUL/AND_INT, MOV, PRINT_REG", "Corresponding IR op"],
    ["JUMP_IF_x taken to the header", "Guard(cond) with exit = fall-through pc, then IR_JUMP_LOOP → **completed**"],
    ["JUMP_IF_x taken forward", "Guard(cond) with exit = fall-through pc"],
    ["JUMP_IF_x not taken (target ≠ header)", "Guard(¬cond) with exit = branch target"],
    ["JUMP_IF_x not taken, target = header", "**Abort**: loop exited while recording"],
    ["Backward branch to another target", "**Abort**: inner loop (traced separately)"],
    ["JUMP to header / forward JUMP", "IR_JUMP_LOOP (completed) / nothing (path straightened)"],
    ["DIV_INT, UNKNOWN_OP, HALT; length > 50", "**Abort** (not supported / end of program / trace too long)"],
], [3.0, 3.5], size=9, caption="Recording rules")
H2("7.3 SSA construction and forward optimisation")
P("The builder keeps an environment `env[r]` mapping each VM register to its current SSA value. A register read "
  "before it is written becomes a `LoadReg`; MOV just copies the environment entry (implicit copy propagation). Each "
  "arithmetic operation passes through the following steps in a single forward pass — O(n log n) for a trace of n "
  "operations because tables are balanced trees:")
R.numbered([
    "**Constant folding** when both operands are constants, using the same wrap-around helpers as the interpreter.",
    "**Algebraic simplification:** x+0, 0+x, x−0, x·1 → x; x·0, x&0, x−x → 0; x&x, x&−1 → x.",
    "**Value numbering / CSE:** commutative operands are ordered, then (op, a, b) is looked up; a hit reuses the earlier value.",
    "**Guard elimination:** a guard with constant operands that holds, or an identical (kind, a, b) guard seen earlier, is removed.",
    "**Snapshot creation:** each surviving guard records every register whose current value differs from its LoadReg, plus the resume pc.",
])
H2("7.4 Loop peeling (research novelty)")
P("`buildOnce()` in `src/optimizer.cpp` implements the adaptation of [1]:", keep_next=True)
R.numbered([
    "Create LoadReg values for all registers the trace touches (entry block).",
    "**Preamble:** replay the recorded trace once through the forward optimiser (iteration 1).",
    "Emit the loop header. For every register written in the trace, create Phi(entry = preamble value). Registers "
    "proven invariant by the fix-point (step 6) get no Phi; their environment entry stays the preamble value.",
    "Emit a FuelCheck whose snapshot is the loop-header state.",
    "**Loop body:** replay the same trace again, *with the CSE and guard tables still filled from the preamble*. Any "
    "operation whose operands are not Phi values hashes to a preamble value and vanishes — loop-invariant code motion.",
    "**Phi fix-point:** a Phi whose back-edge value equals its entry value (or itself) is redundant; mark its register "
    "invariant and rebuild. Repeat until stable (at most 17 rounds; typically 2).",
    "Run DCE (backward liveness from guards, prints, Phis and snapshot entries).",
])
P("Correctness: the preamble dominates the loop, and a CSE hit requires *identical SSA operands*, which are "
  "loop-invariant by construction because loop-varying values are always fresh Phi values. A removed body guard is "
  "identical to a preamble guard on invariant values, so it cannot fail in later iterations. Example "
  "(`tests/programs/12_loop_invariant.jbc`) — SSA loop body without and with peeling:")
C("-O1 (no peeling): loop body                    -O2 (peeling): preamble computes v9,v10,v12 once\n"
  "  v16 = Mul v3, v4      ; a*b (invariant)        v9  = Mul v3, v4\n"
  "  v17 = Add v1, v16                              v10 = Add v1, v9\n"
  "  v19 = Mul v17, #3                              v12 = Mul v10, #3\n"
  "  v20 = Add v11, v19    ; acc += k               ---- loop header ----\n"
  "  v21 = Add v1, v10     ; i += 1                 v20 = Add v12, v18   ; acc += k\n"
  "  GuardLT v21, v2                                v21 = Add v1, v17    ; i += 1\n"
  "  (5 Phi nodes)                                  GuardLT v21, v2      (2 Phi nodes)")
P("The loop body shrinks from 6 to 3 operations and three Phi nodes disappear (R6, R7, R8 are proven invariant).")
H2("7.5 Lowering, Phi coalescing and parallel copies")
P("Each live non-constant SSA value receives a slot; constants become immediates. Phi nodes are implemented by copies "
  "before the loop (entry values) and at the back-edge. **Phi coalescing** lets a back-edge value share its Phi's slot "
  "when the Phi is not read after that value is defined (checked against later operands, snapshot uses and other "
  "Phis' back-edges), removing the copy. When a back-edge source is itself a Phi (e.g. swapping two registers), the "
  "copies are made through temporaries — the classic parallel-copy problem (`19_swap_parallel_copy`).")
H2("7.6 Linear-scan register allocation")
P("Following Poletto and Sarkar [8], each slot gets a live interval over the linear LIR; snapshot write-backs count "
  "as uses at their guard. A slot live on entry to the loop (Phi slots, invariants) is extended to the back-edge. "
  "Intervals are processed by start point; expired intervals free their register; if none is free, the interval "
  "ending furthest away is spilled to memory. Complexity O(n·K) for n intervals and K registers (K = 10 on x86-64, "
  "11 on AArch64). The fuel counter has a dedicated register; caller-saved pool registers are saved only around the "
  "PRINT helper call.")
H2("7.7 Native code generation")
P("Each LIR operation is translated by a fixed template: one small function emits the bit pattern of each "
  "instruction kind, with register numbers and offsets OR-ed into its fields. Two simple peephole rules improve the "
  "result: results are computed directly into the allocated destination register, and the last guard is fused with "
  "the back-edge (*loop inversion*: `b.lt LOOP; b exit` instead of `b.ge exit; b LOOP`). Together with Phi "
  "coalescing, the AArch64 loop for benchmark B1 (`sum += i; i += 1; while i < n`) is seven instructions:")
def loop_lines(path):
    ls = rd(path).split("\n")
    a = next(i for i, l in enumerate(ls) if l.rstrip().endswith("LOOP:"))
    b = next(i for i, l in enumerate(ls) if "exit0:" in l)
    return "\n".join(l.rstrip() for l in ls[a:b])
C(loop_lines(ARM + "asm_b1_full.txt"))
H2("7.8 Deoptimisation and the fuel check")
P("When a guard fails, control jumps to an out-of-line exit stub that stores each (register, value) pair of the "
  "guard's snapshot into the VM register file, saves the fuel counter and returns the exit index. The VM sets "
  "`pc = resumePC` and continues interpreting. Because VM registers are written only at exits, the loop itself performs "
  "no memory traffic. The fuel check at the loop header exits through a snapshot of the header state when the step "
  "budget is exhausted, so an infinite loop inside compiled code is still reported.")
H2("7.9 Complexity summary")
T(["Phase", "Time complexity", "Notes"], [
    ["Assembly", "O(L) for L source lines", "two passes; label table is a balanced tree"],
    ["Verification", "O(N·I) bit-vector iterations", "N instructions, I ≤ depth of loop nesting + 2 iterations in practice"],
    ["Recording", "O(1) per executed instruction", "bounded by MAX_TRACE_LENGTH = 50"],
    ["SSA + optimisation", "O(F · n log n)", "F fix-point rounds (≤ 17, typically 2), n ≤ 2 × 52 ops"],
    ["Register allocation", "O(n·K)", "K = 10 / 11"],
    ["Code generation", "O(n)", "one template per LIR op, linear fix-ups"],
    ["Trace execution", "O(1) dispatch overhead per iteration", "a single fused compare-and-branch closes the loop"],
], [1.6, 1.8, 3.1], size=9, caption="Algorithmic complexity of each phase")

# =========================================================== 8 Implementation
H1("8. Implementation Details")
H2("8.1 Code organisation")
loc = {}
for f in os.listdir(PROJ + "/src"):
    loc[f] = sum(1 for _ in open(PROJ + "/src/" + f))
def L(*fs):
    return str(sum(loc[f] for f in fs))
T(["Files", "Lines", "Responsibility", "Primary owner"], [
    ["isa.h, isa.cpp", L("isa.h", "isa.cpp"), "Opcode table, wrap-around arithmetic", "Emmanuel J."],
    ["assembler.h/.cpp", L("assembler.h", "assembler.cpp"), "Two-pass assembler, disassembler, diagnostics", "Emmanuel J."],
    ["verifier.h/.cpp", L("verifier.h", "verifier.cpp"), "Structural checks, reachability, definite assignment", "Emmanuel J."],
    ["recorder.h/.cpp", L("recorder.h", "recorder.cpp"), "Trace recording rules and aborts", "Emmanuel J."],
    ["trace_ir.h/.cpp", L("trace_ir.h", "trace_ir.cpp"), "IR definitions, printers, lowering, Phi coalescing", "Yadesh M."],
    ["optimizer.h/.cpp", L("optimizer.h", "optimizer.cpp"), "SSA construction, forward opts, loop peeling, DCE", "Yadesh M."],
    ["backend_native.cpp", L("backend_native.cpp"), "Linear scan, x86-64 + AArch64 encoders, W^X memory, self-test", "Yadesh M. / Atul V."],
    ["backend_ir.cpp, backend.h", L("backend_ir.cpp", "backend.h"), "Portable IR executor (reference / fallback), backend interface", "Yadesh M."],
    ["vm.h/.cpp", L("vm.h", "vm.cpp"), "Interpreter, profiler, code cache, deopt handling, statistics", "Atul V. / Tanvi G."],
    ["jitvm.cpp", L("jitvm.cpp"), "Command-line driver, --compare mode", "Atul V."],
    ["tests/, bench/", str(sum(1 for _ in open(PROJ + "/tests/test_main.cpp")) + sum(1 for _ in open(PROJ + "/tests/run_cli_tests.sh")) + sum(1 for _ in open(PROJ + "/bench/bench_main.cpp"))), "Test suite, CLI tests, benchmark harness", "Tanvi G."],
], [1.6, 0.55, 3.0, 1.35], size=9, caption="Source organisation (lines of code)")
H2("8.2 Module-wise explanation")
H3("Front end (assembler and verifier)")
P("Pass 1 of the assembler strips comments, collects any number of `label:` prefixes per line and counts "
  "instructions; pass 2 parses mnemonics and operands, checking operand counts, register names (R0–R15), int32 "
  "ranges (hex constants up to 0xFFFFFFFF are accepted as two's-complement) and labels. Errors are collected rather "
  "than thrown, so one run reports all of them. The verifier then runs a work-list reachability search over the CFG "
  "and a forward *must* analysis (IN = ∩ OUT of predecessors, OUT = IN ∪ DEF) iterated to a fix-point.")
H3("Interpreter, profiler and code cache")
P("`VirtualMachine::run()` is a fetch-decode-execute loop over a working copy of the bytecode. When a trace is "
  "compiled, the header instruction is overwritten with an internal TRACE_ENTER opcode, so the normal dispatch path "
  "has no code-cache check at all:")
C("Instruction instr = code_[pc_];\n"
  "if (instr.op == OP_TRACE_ENTER) {                 // patched loop header\n"
  "    if (!recorder_.active()) {\n"
  "        if (!enterTrace(*traceAt_[pc_])) { res.status = RunStatus::StepLimit; break; }\n"
  "        continue;                                 // resumed at the exit's pc\n"
  "    }\n"
  "    instr = program_.code[pc_];                   // recording another loop: interpret\n"
  "}")
H3("Optimiser and back ends")
P("The optimiser is a `Builder` class whose `arith()` and `guard()` methods implement Section 7.3; `buildOnce()` "
  "implements peeling and `buildOptimizedTrace()` the Phi fix-point. The native emitters (`struct X64`, `struct A64`) "
  "share a byte buffer and keep a parallel list of mnemonics, so `--dump-asm` prints an annotated listing of exactly "
  "the bytes that execute. Executable memory is written while RW and flipped to RX (Linux, Intel macOS), or allocated "
  "with `MAP_JIT` and toggled with `pthread_jit_write_protect_np` on Apple Silicon, followed by an instruction-cache "
  "flush.")
H2("8.3 Build process and integration")
P("`make` builds the VM, test suite and benchmark harness; `make test`, `make bench`, `./demo.sh` and "
  "`make legacy` (the unchanged Review 2 prototype) are described in Appendix A. "
  "Integration followed the interfaces in Table 7: the recorder delivers `RecordedTrace`, the optimiser delivers "
  "`SSATrace`, lowering delivers `LIRTrace`, and both back ends implement `CompiledTrace::run()`. Because every "
  "interface is a plain data structure, each module is unit-tested on its own and the whole chain is checked by "
  "differential tests after every change.")

# =========================================================== 9 Testing
H1("9. Testing Strategy and Test Cases")
H2("9.1 Strategy")
P("The interpreter defines the reference semantics; the JIT must be observationally identical. Testing is layered:")
B("**Unit tests** for assembler, verifier, recorder, optimiser and deoptimisation (sections 1–6 of `jit_tests`).")
B("**Golden tests:** interpreter output and final state of 24 programs compared with checked-in expected files.")
B("**Differential matrix:** each program × {native, IR} × {-O0, -O1, -O2} × HOT_THRESHOLD {1, 2, 3, 5, 8} must "
  "match the interpreter exactly (720 runs). Different thresholds start recording at different iterations, "
  "exercising different trace entry states.")
B("**Random-program fuzzing:** 400 generated terminating programs (nested counted loops, if/else, prints, DIV and "
  "UNKNOWN_OP) × 7 JIT configurations, including MAX_TRACE_LENGTH = 8 (2,800 runs).")
B("**CLI tests:** 27 end-to-end checks of exit codes and diagnostics (`tests/run_cli_tests.sh`).")
B("**Robustness:** the whole suite under AddressSanitizer + UBSan; strict-warning builds with GCC and Clang; both "
  "CPU architectures.")
H2("9.2 Test-suite summary")
T(["Test section", "Checks", "AArch64", "x86-64"], [
    ["1. Assembler (front end)", "25", "25 / 25", "25 / 25"],
    ["2. Verifier (static analysis)", "14", "14 / 14", "14 / 14"],
    ["3. Review 2 regression (Instruction-vector API)", "27", "27 / 27", "27 / 27"],
    ["4. Recorder / profiler behaviour", "24", "24 / 24", "24 / 24"],
    ["5. Optimiser", "16", "16 / 16", "16 / 16"],
    ["6. Deoptimisation / snapshot restore", "16", "16 / 16", "16 / 16"],
    ["7. Golden output", "24", "24 / 24", "24 / 24"],
    ["8. Differential matrix (720 runs)", "24", "24 / 24", "24 / 24"],
    ["9. Random-program fuzzing (2,800 runs)", "2", "2 / 2", "2 / 2"],
    ["10. Safety: step limit, runtime errors, rejection", "11", "11 / 11", "11 / 11"],
    ["CLI tests (tests/run_cli_tests.sh)", "27", "27 / 27", "27 / 27"],
    ["**Total**", "**210**", "**210 / 210**", "**210 / 210**"],
], [3.4, 0.8, 1.15, 1.15], size=9.5, center_cols=(1, 2, 3), caption="Test results on both architectures (logs in results/)")
H2("9.3 Representative test cases: expected versus actual")
T(["Type", "Input", "Expected result", "Actual result"], [
    ["Positive", "01_valid_stable_loop", "Hot at pc 3 on 3rd back-edge; R0 = 1..10; trace compiled", "Pass — 6 of 9 iterations in native code"],
    ["Positive", "09_nested_loops", "Total 14400; inner loop compiled, outer blacklisted", "Pass — 14400"],
    ["Positive", "12_loop_invariant (-O2)", "acc = 129000; loop body 3 ops", "Pass — 3 ops, 3 invariant regs"],
    ["Boundary", "07_trace_exact_limit", "Exactly 50 ops accepted", "Pass — compiled"],
    ["Boundary", "04_trace_limit", "53 ops → abort, blacklist", "Pass — \"Max trace length reached\""],
    ["Boundary", "14_wraparound", "INT32_MAX + 1 = −2147483648 in every mode", "Pass"],
    ["Negative", "03_invalid_unknown_op", "Abort ×2, blacklist; R0 = 20", "Pass"],
    ["Negative", "e01_syntax_errors", "6 errors with line numbers; exit 2", "Pass"],
    ["Failure", "e03_division_by_zero", "Runtime error at pc 4; exit 3", "Pass"],
    ["Failure", "e04_infinite_loop (JIT and interpreter)", "Step limit; exit 4; no hang", "Pass — fuel exit from native loop"],
    ["Stress", "Random programs (400 × 7)", "Identical to interpreter", "Pass — 2,800 / 2,800"],
    ["Regression", "11_loop_exit_during_recording (D1)", "Following loop at pc 7 traced", "Pass"],
    ["Deopt", "22_side_exit_restores_state", "R0=77 R3=149226 R4=−2926 R5=5929", "Pass — all levels and backends"],
], [0.8, 1.9, 2.15, 1.65], size=8.5, caption="Representative test cases (all programs: Appendix D)")
H2("9.4 Defect and debugging log (Review 3 development)")
T(["Defect found", "Detected by", "Correction", "Status"], [
    ["`--compare` reported a mismatch for programs stopped by the step limit", "Differential sweep", "Stop points differ by design (instructions vs. iterations); compare status only", "Fixed"],
    ["Disassembler output could not be re-assembled", "Round-trip unit test", "pc numbers moved into comments", "Fixed"],
    ["AArch64 only 2.6× faster: every value went through memory", "Benchmark on the Mac", "Register allocation of hot slots and fuel", "Fixed"],
    ["IR executor slower than the interpreter (0.4×)", "Benchmark", "Constants placed in slots so every op reads two array cells; Phi coalescing", "Fixed"],
    ["Loop body 2× slower without Phi coalescing", "Ablation on the Mac (B1 18× → 7.7×)", "Coalescing kept; back-edge copies removed", "Fixed"],
    ["Short-lived temporary spilled to memory", "Machine-code listing (B5)", "Linear-scan allocation with interval reuse", "Fixed"],
    ["~14 % overhead on non-JIT-able loops", "Benchmark B8", "Bytecode patching instead of per-instruction cache lookup", "Reduced to ~6 %"],
], [2.0, 1.45, 2.3, 0.75], size=8.5, caption="Defects found and fixed during Review 3 development")

# =========================================================== 10 Results
H1("10. Results and Evaluation")
H2("10.1 Benchmark setup")
P("Eight benchmark programs (`bench/programs/`) cover tight loops (B1), loop-invariant computation (B2), nested "
  "loops (B3), loop-carried copies (B4), arithmetic-heavy bodies (B5), redundant bounds checks (B6), an alternating "
  "branch (B7) and a loop containing the non-JIT-able DIV_INT (B8). `jit_bench` times only `VirtualMachine::run()`, "
  "reports the median of 7 runs, and checks that every configuration produced exactly the interpreter's output "
  "(it did, for all 80 measurements). Machines: the Apple Silicon demonstration Mac (measured inside its arm64 Linux "
  "environment, g++ 11.4) and an Intel Xeon 2.1 GHz cloud machine (g++ 13.3). Timings on the shared cloud machine "
  "vary by roughly ±10 %.")
H2("10.2 Correctness")
P("Across both architectures, 3,520 differential runs per machine (720 matrix + 2,800 fuzz) produced 0 mismatches; "
  "about 2,850 traces were compiled during fuzzing alone, so the comparison exercised compiled code rather than just the "
  "interpreter. All Review 2 regression cases pass through the original `loadProgram(std::vector<Instruction>)` API.")
H2("10.3 Performance")
R.figure(FIG + "speedup.png", 6.6, "Speed-up over the interpreter for the IR executor and the native back end at -O1 and -O2")
NAMES = {"b1_sum_loop": "B1 sum loop", "b2_loop_invariant": "B2 loop-invariant", "b3_nested_loops": "B3 nested loops",
         "b4_fibonacci_mod": "B4 Fibonacci", "b5_polynomial": "B5 polynomial", "b6_bounds_checked": "B6 bounds checks",
         "b7_branchy": "B7 branchy", "b8_div_not_jitable": "B8 DIV (not JIT-able)"}
def bench_table(arch, title):
    d = bench(arch)
    rows = []
    for n, lab in NAMES.items():
        r = d[n]
        f = lambda c: f"{r[c][0]*1000:.1f} ms ({r[c][1]:.1f}×)"
        rows.append([lab, f"{r['Interpreter'][0]*1000:.0f} ms", f("JIT IR-executor -O2"), f("JIT native -O0"),
                     f("JIT native -O1"), f("JIT native -O2")])
    T(["Benchmark", "Interpreter", "IR executor -O2", "Native -O0", "Native -O1", "Native -O2"], rows,
      [1.45, 0.85, 1.05, 1.05, 1.05, 1.05], size=8.5, caption=f"Median run time and speed-up, {title}")
bench_table("AArch64", "Apple Silicon (AArch64) — x86-64 results in Appendix G")
H2("10.4 Effect of the optimisations")
R.figure(FIG + "loop_body_ops.png", 6.0, "Operations executed per loop iteration at each optimisation level")
P("Loop peeling is the only pass that changes B2 and 12_loop_invariant (8 → 3 and 6 → 3 operations), and it "
  "accounts for the jump from 33× to 62× on Apple Silicon and from 25× to 75× on x86-64 in B2. Constant folding "
  "halves the body of 21_constant_folding at -O1, and guard elimination removes the duplicated bounds checks of "
  "20_redundant_guards and B6. B5 has no redundancy to remove, so all three levels execute the same nine operations; "
  "its gain comes entirely from native code and register allocation.")
H2("10.5 Interpretation")
B("**Where the speed-up comes from.** The interpreter spends ≈ 3 ns per bytecode instruction on dispatch and memory "
  "traffic. The compiled B1 loop is 7 machine instructions per iteration with every value in a register; VM registers "
  "are written only once, at the exit.")
B("**B7 (branchy)** gains only about 1.8×: the branch alternates, so every second iteration leaves the trace and is "
  "interpreted. Side traces would fix this (Section 13).")
B("**B8 (DIV)** runs at about 0.94× of the interpreter: the loop is blacklisted after two aborted recordings and the "
  "remaining cost is profiling of backward branches.")
B("**IR executor** runs at 0.6–2× the interpreter's speed: it benefits from the optimiser but still dispatches "
  "every operation, including the Phi copies. Its purpose is not speed but correctness: it executes the optimised IR "
  "without any machine code, so differential tests can tell optimiser bugs from code-generator bugs, and it is the "
  "fallback on CPUs without a native back end.")
B("**Comparison with [1].** The paper reports that peeling substantially reduces PyPy's run time on numerical "
  "kernels; we observe the same qualitative effect — large gains exactly where loop-invariant work exists (B2), no "
  "effect where it does not (B1, B5).")
P("Representative console output (normal run and deoptimisation) is reproduced in Appendix C.")

# =========================================================== 11 Errors / limitations
H1("11. Error Handling, Limitations and Known Issues")
H2("11.1 Error handling")
T(["Condition", "Detected in", "Behaviour", "Exit code"], [
    ["Unknown mnemonic, bad operand count/register/number, undefined or duplicate label", "Assembler", "All errors listed with file:line; nothing executed", "2"],
    ["Register R16+, jump target outside program, invalid opcode, empty program", "Verifier", "Program rejected (also via the old vector API)", "2"],
    ["Unreachable code, read-before-write, fall-off-end, missing HALT, self-jump", "Verifier", "Warnings; program runs (`--strict` rejects)", "0 / 2"],
    ["UNKNOWN_OP", "Interpreter / recorder", "Skipped with a warning (suppressed after 3); recording aborts", "0"],
    ["Division by zero", "Interpreter", "Runtime error with pc and source line", "3"],
    ["Non-termination", "Interpreter / fuel check in native code", "Step limit reached", "4"],
    ["Native backend unavailable or self-test fails", "Start-up (forked child)", "IR executor used automatically", "0"],
    ["Native compilation fails for a trace (e.g. > 4000 slots)", "Back end", "That trace uses the IR executor", "0"],
    ["`--compare` finds different results", "CLI", "Both register files printed", "5"],
], [2.35, 1.35, 2.1, 0.7], size=8.5, caption="Error conditions and behaviour")
H2("11.2 Limitations")
B("**Single linear trace per loop.** No side traces or trace trees: divergent control flow costs side exits (B7); an "
  "outer loop containing an inner loop is not compiled (its inner loop is).")
B("**Instruction set.** 16 integer registers; no memory, calls, floating point or I/O other than PRINT_REG; DIV_INT "
  "is interpreter-only.")
B("**Code generation.** Template instruction selection without scheduling; register pools of 10/11 registers; "
  "immediate operands always materialised in a register on AArch64.")
B("**Trace length.** MAX_TRACE_LENGTH = 50 ops (configurable with `--max-trace`).")
H2("11.3 Known issues")
B("The automated logs were produced on Linux (x86-64 and the Mac's arm64 environment). The macOS-specific path "
  "(`MAP_JIT`, `pthread_jit_write_protect_np`) is compiled only on macOS; run `make test` once on the demo Mac before "
  "the review. If its self-test ever failed, the VM would automatically use the IR executor and report it in "
  "`--backend-info`.")
B("Benchmark timings on shared cloud hardware fluctuate by about ±10 %; the reported values are medians of 7 runs.")

# =========================================================== 12 Contribution
H1("12. Member-Wise Contribution")
P("Each member owns a measurable technical output, demonstrates it during the review, and is prepared for viva "
  "questions on it (see `docs/VIVA_PREP.md`). The detailed log with file-level evidence is `docs/CONTRIBUTION_LOG.md`.")
T(["Member", "Primary Technical Ownership", "Final Deliverable Produced", "Evidence / Commit / Test", "Integration / Support Contribution", "Viva Module"], [
    ["Atul Vipin (Member 1 – Project Lead / Integrator)", "Architecture, VM state machine, code cache, executable memory and self-test, CLI, build",
     "Integrated mixed-mode VM; jitvm tool; Makefile, build.sh, demo.sh", "vm.cpp, jitvm.cpp, backend.h, ExecMemory; CLI tests (27)", "Interfaces between modules; demo machine setup", "System integration"],
    ["Emmanuel Joshy (Member 2 – Front End / Analysis)", "ISA, assembler, verifier, interpreter core, profiler, recorder",
     "Front end with line diagnostics and data-flow analysis; completed recorder", "isa.*, assembler.*, verifier.*, recorder.*; tests §1, §2, §4", "RecordedTrace interface to optimiser", "Parsing, verification, recording"],
    ["Yadesh Waran M (Member 3 – Core / Back End)", "SSA optimiser incl. loop peeling, lowering, linear-scan allocation, x86-64 and AArch64 code generation",
     "Optimiser and two native back ends; IR executor", "optimizer.*, trace_ir.*, backend_native.cpp, backend_ir.cpp; tests §5", "LIR interface to back ends; performance tuning", "Optimisation and code generation"],
    ["Tanvi Gupta (Member 4 – Testing / Release)", "Snapshots and deoptimisation, test suite, benchmarks, documentation",
     "Deopt design; 183-check suite + fuzzer; benchmark harness; README; report", "Snapshot/LExit, enterTrace exits; test_main.cpp, bench/, results/", "Runs regression + benchmarks for every merge", "Deopt, testing, results"],
], [1.15, 1.2, 1.15, 1.15, 1.0, 0.85], size=8, caption="Mandatory final contribution matrix")


# =========================================================== 13 Conclusion
H1("13. Conclusion and Future Work")
H2("13.1 Objectives achieved")
P("All six approved objectives and both extensions were achieved (Table 2). The Review 2 prototype — a profiler and "
  "recorder whose output nothing used — has become a complete trace-based JIT: verified input, recorded traces, an "
  "SSA optimiser with the loop-peeling technique of Ardö et al., register-allocated native code for two instruction "
  "sets, and deoptimisation that has never produced a result different from the interpreter in 7,040 differential "
  "runs on two machines.")
H2("13.2 Key learning")
B("Correctness infrastructure comes first: the differential harness found or confirmed every bug in this report "
  "within seconds, which made aggressive optimisation safe to attempt.")
B("Measure before believing: register allocation and Phi coalescing each doubled native speed, while peeling was "
  "the only optimisation that removed work from every iteration. Simplifying the code generator was also measured — "
  "removing Phi coalescing halved the speed on Apple Silicon, so it was kept.")
B("A research idea can be adapted rather than copied: peeling transferred directly, while PyPy's jump arguments became "
  "Phi nodes and a fix-point in our SSA design.")
H2("13.3 Future work and publication potential")
B("**Side traces and trace trees** [5] for hot side exits (B7) and nested loops.")
B("**Allocation removal / escape analysis** [7] once heap objects are added to the ISA.")
B("**Better instruction selection:** immediate operand forms on AArch64, instruction scheduling, more registers.")
B("**Typed values and calls** to bring the VM closer to a dynamic-language runtime.")
P("A short workshop or student-research paper is feasible: *\"Loop peeling in a minimal tracing JIT: a two-ISA "
  "evaluation\"*, contributing a small, fully reproducible artefact that isolates the effect of peeling, register "
  "allocation and coalescing on x86-64 and Apple Silicon.")

# =========================================================== 14 References
H1("14. References")
refs = [
    "H. Ardö, C. F. Bolz and M. Fijałkowski, \"Loop-aware optimizations in PyPy's tracing JIT,\" in Proc. 8th Symposium on Dynamic Languages (DLS '12), ACM, 2012, pp. 63–72, doi: 10.1145/2384577.2384586.",
    "BCSE307 Compiler Design Project, \"Project Review 3 Guidelines – Final Demonstration, Evaluation and Project Completion Review,\" 2026; and Team 6, \"Project Review 2 Report,\" 16-09-2026.",
    "V. Bala, E. Duesterwald and S. Banerjia, \"Dynamo: A transparent dynamic optimization system,\" in Proc. ACM PLDI, 2000, pp. 1–12.",
    "A. Gal, C. W. Probst and M. Franz, \"HotpathVM: An effective JIT compiler for resource-constrained devices,\" in Proc. ACM/USENIX VEE, 2006, pp. 144–153.",
    "A. Gal et al., \"Trace-based just-in-time type specialization for dynamic languages,\" in Proc. ACM PLDI, 2009, pp. 465–478.",
    "C. F. Bolz, A. Cuni, M. Fijałkowski and A. Rigo, \"Tracing the meta-level: PyPy's tracing JIT compiler,\" in Proc. ICOOOLPS, 2009, pp. 18–25.",
    "C. F. Bolz, A. Cuni, M. Fijałkowski, M. Leuschel, S. Pedroni and A. Rigo, \"Allocation removal by partial evaluation in a tracing JIT,\" in Proc. ACM PEPM, 2011, pp. 43–52.",
    "M. Poletto and V. Sarkar, \"Linear scan register allocation,\" ACM Trans. Program. Lang. Syst., vol. 21, no. 5, pp. 895–913, 1999.",
    "U. Hölzle, C. Chambers and D. Ungar, \"Debugging optimized code with dynamic deoptimization,\" in Proc. ACM PLDI, 1992, pp. 32–43.",
    "R. Cytron, J. Ferrante, B. K. Rosen, M. N. Wegman and F. K. Zadeck, \"Efficiently computing static single assignment form and the control dependence graph,\" ACM TOPLAS, vol. 13, no. 4, pp. 451–490, 1991.",
    "A. V. Aho, M. S. Lam, R. Sethi and J. D. Ullman, Compilers: Principles, Techniques, and Tools, 2nd ed. Pearson, 2006.",
    "B. Stroustrup, The C++ Programming Language, 4th ed. Addison-Wesley, 2013.",
    "M. Pall, \"LuaJIT 2.0 – SSA IR and optimisations,\" luajit.org, accessed Oct. 2026.",
    "Intel Corporation, Intel® 64 and IA-32 Architectures Software Developer's Manual, Vol. 2 (instruction set reference).",
    "Arm Ltd., Arm® Architecture Reference Manual for A-profile Architecture; and Procedure Call Standard for the Arm 64-bit Architecture (AAPCS64).",
    "Apple Inc., \"Porting just-in-time compilers to Apple silicon,\" Apple Developer Documentation (MAP_JIT, pthread_jit_write_protect_np).",
    "LLVM Project, \"AddressSanitizer\" and \"UndefinedBehaviorSanitizer\" documentation, clang.llvm.org.",
]
for i, r in enumerate(refs, 1):
    p = P(f"[{i}] {r}", align="left", after=80)
    p.paragraph_format.left_indent = __import__("docx").shared.Inches(0.35)
    p.paragraph_format.first_line_indent = __import__("docx").shared.Inches(-0.35)

# =========================================================== Appendices
H1("Appendix A: User Manual and Installation Guide", page_break=True)
R.heading(toc=False, level=2, text="A.1 Installation on macOS (demonstration machine)")
R.numbered([
    "Install the compiler once: `xcode-select --install` (provides Apple clang and make).",
    "Open Terminal: `cd ~/Desktop/\"compiler JIT project\"`.",
    "Build and test: `make && make test` — must end with `TOTAL: 183 checks passed, 0 failed` and `CLI tests: 27 passed`.",
    "Confirm the native backend: `./build/jitvm --backend-info` → `native AArch64 backend (self-test passed)`.",
])
R.heading(toc=False, level=2, text="A.2 Installation on Linux")
P("`sudo apt install g++ make` (or any C++17 compiler), then `make && make test`. Without make: `./build.sh`.")
R.heading(toc=False, level=2, text="A.3 Command-line options")
T(["Option", "Meaning"], [
    ["--mode=jit | --mode=interp", "JIT enabled (default) or pure interpreter"],
    ["--backend=native | --backend=ir", "Native machine code (default, automatic fallback) or portable IR executor"],
    ["-O0 | -O1 | -O2", "No optimisation / forward optimisations / + loop peeling (default)"],
    ["--hot=N, --max-trace=N, --max-steps=N", "HOT_THRESHOLD (3), MAX_TRACE_LENGTH (50), step limit (2·10⁹)"],
    ["-q, -v, -vv", "Program output only / JIT events (default) / plus trace entries, exits, restored registers"],
    ["--dump-ir, --dump-asm", "Print Level-1, SSA and LIR for each trace / annotated machine code"],
    ["--stats, --regs, --time", "Statistics, final registers, execution time"],
    ["--check, --strict", "Assemble + verify + disassemble only / treat warnings as errors"],
    ["--compare", "Run interpreter and JIT, check identical results, print speed-up"],
    ["--backend-info", "Host architecture and native backend status"],
    ["JITVM_NO_NATIVE=1 (environment)", "Force the IR executor"],
], [2.3, 4.2], size=9)
R.heading(toc=False, level=2, text="A.4 Writing a program")
P("Create a text file ending in `.jbc` with one instruction per line (grammar in Appendix B), then run "
  "`./build/jitvm --compare myprog.jbc`.")

H1("Appendix B: Bytecode Grammar and ISA Specification", page_break=True)
C("line        := { label ':' } [ instruction ] [ comment ]\n"
  "instruction := MNEMONIC [ operand { ',' operand } ]      (mnemonics are case-insensitive)\n"
  "operand     := register | immediate | target\n"
  "register    := 'R' ( 0 | 1 | ... | 15 )\n"
  "immediate   := [ '-' ] decimal | '0x' hex                (must fit in 32 bits)\n"
  "target      := label | '@' index | index\n"
  "comment     := ';' text | '#' text | '//' text")
T(["#", "Opcode", "Operands", "Semantics (32-bit two's complement)", "JIT"], [
    ["0", "LOAD_CONST", "rd, imm", "rd ← imm", "yes"], ["1", "ADD_INT", "rd, ra, rb", "rd ← ra + rb (wraps)", "yes"],
    ["2", "JUMP_IF_LT", "ra, rb, t", "if ra < rb (signed): pc ← t", "guard"], ["3", "PRINT_REG", "ra", "print \"Register Ra = value\"", "helper call"],
    ["4", "HALT", "—", "stop", "aborts recording"], ["5", "UNKNOWN_OP", "—", "skipped with a warning", "no"],
    ["6", "SUB_INT", "rd, ra, rb", "rd ← ra − rb (wraps)", "yes"], ["7", "MUL_INT", "rd, ra, rb", "rd ← ra × rb (low 32 bits)", "yes"],
    ["8", "AND_INT", "rd, ra, rb", "rd ← ra & rb", "yes"], ["9", "MOV", "rd, ra", "rd ← ra", "copy-propagated"],
    ["10", "JUMP", "t", "pc ← t", "straightened"], ["11", "JUMP_IF_EQ", "ra, rb, t", "if ra = rb: pc ← t", "guard"],
    ["12", "DIV_INT", "rd, ra, rb", "rd ← ra / rb (truncating); error if rb = 0; INT_MIN/−1 = INT_MIN", "no"],
], [0.35, 1.1, 0.95, 3.0, 1.1], size=8.5, caption="Instruction set (opcodes 0–5 unchanged from Review 2)")

H1("Appendix C: Sample Runs", page_break=True)
R.heading(toc=False, level=2, text="C.1 Normal run with JIT events (Apple Silicon)")
C(rd(ARM + "run01.txt"))
R.heading(toc=False, level=2, text="C.2 Mid-body side exit and snapshot restore")
C(rd(ARM + "deopt22.txt"))
R.heading(toc=False, level=2, text="C.3 Front-end errors (exit code 2)")
C(rd("/tmp/rep/e01.txt"))
R.heading(toc=False, level=2, text="C.4 Verifier warnings")
C(rd("/tmp/rep/w01.txt"))
R.heading(toc=False, level=2, text="C.5 Unsupported opcode: abort, retry, blacklist")
C(rd("/tmp/rep/run03.txt"))
R.heading(toc=False, level=2, text="C.6 Nested loops")
C(rd(ARM + "run09.txt"))
R.heading(toc=False, level=2, text="C.7 Review 2 defect D1 fixed: loop exits while recording")
C(rd(ARM + "run11.txt"))
R.heading(toc=False, level=2, text="C.8 Runtime errors")
C(rd("/tmp/rep/e03.txt") + "\n" + rd("/tmp/rep/e04.txt"))
R.heading(toc=False, level=2, text="C.9 Differential comparison and statistics (Apple Silicon)")
C(rd(ARM + "cmp_b2.txt"))
C(rd(ARM + "stats23.txt"))

H1("Appendix D: Complete Test-Program Table", page_break=True)
progs = sorted(os.listdir(PROJ + "/tests/programs"))
rows = []
for f in progs:
    first = [l for l in open(PROJ + "/tests/programs/" + f) if l.startswith(";")]
    desc = first[0][1:].strip() if first else ""
    desc = re.sub(r"^Review 2 case \d \(([^)]*)\):\s*", r"Review 2 case — \1: ", desc)
    exp = open(PROJ + "/tests/expected/" + f.replace(".jbc", ".out")).read().strip().split("\n")
    outs = [l.replace("Register ", "") for l in exp if l.startswith("Register")]
    shown = ", ".join(outs[-3:]) if len(outs) <= 3 else f"{len(outs)} lines … " + ", ".join(outs[-2:])
    rows.append([f.replace(".jbc", ""), desc, shown, "Pass"])
T(["Program", "Purpose", "Expected output (golden)", "Result"], rows, [1.55, 2.75, 1.65, 0.55], size=7.5,
  caption="All functional test programs (each also passes 30 JIT configurations)")
erows = [["e01_syntax_errors", "Six front-end errors with line numbers", "exit 2", "Pass"],
         ["e02_bad_jump_target", "Jump target outside the program", "exit 2", "Pass"],
         ["e03_division_by_zero", "Division by zero inside a loop", "exit 3, pc 4", "Pass"],
         ["e04_infinite_loop", "Non-terminating loop (interpreter and native)", "exit 4", "Pass"],
         ["e05_empty", "Only comments", "exit 2", "Pass"],
         ["w01_warnings", "Unreachable code, read-before-write, no HALT", "warnings, exit 0; --strict exit 2", "Pass"],
         ["r2_case8_original", "Review 2 case 8 as submitted (never terminates)", "exit 4", "Pass"]]
T(["Error program", "Purpose", "Expected", "Result"], erows, [1.55, 2.75, 1.65, 0.55], size=8)

H1("Appendix E: Generated Code Listings", page_break=True)
R.heading(toc=False, level=2, text="E.1 AArch64 machine code for 01_valid_stable_loop (prologue and preamble)")
C(rd(ARM + "asm01_full.txt").split("---- Trace #0: AArch64 machine code ----\n")[1].split("LOOP:")[0].rstrip()[:3500])
R.heading(toc=False, level=2, text="E.2 x86-64 loop for B1")
C(rd("/tmp/rep/asm_x86_b1.txt"))
R.heading(toc=False, level=2, text="E.3 Full IR dump for 12_loop_invariant at -O2")
C(rd("/tmp/rep/ir12_o2.txt"))

H1("Appendix F: Demonstration Plan and Evaluator Record", page_break=True)
T(["Presenter", "Time", "Slides", "Live demonstration"], [
    ["Atul Vipin", "4 min", "1–5, 16", "Build, --backend-info, end-to-end run of 01_valid_stable_loop"],
    ["Emmanuel Joshy", "4–5 min", "6–7", "Syntax errors, verifier warnings, recorder aborts, Review 2 bug fix"],
    ["Yadesh Waran M", "4–5 min", "8–9", "-O1 vs -O2 IR dumps, machine-code listing, --compare on B2"],
    ["Tanvi Gupta", "4–5 min", "11–14, 18", "-vv deopt, fuel exit, make test, benchmarks, limitations"],
    ["All", "8–12 min", "10, 15, 17", "Evaluator-selected input with --compare; viva"],
], [1.3, 0.8, 0.9, 3.5], size=9, caption="Presentation and demonstration allocation")

T(["Stage", "Command(s)", "Expected evidence"], [
    ["1. Environment", "make clean && make; ./build/jitvm --backend-info", "Successful build; native AArch64 backend"],
    ["2. Normal use case", "./build/jitvm tests/programs/01_valid_stable_loop.jbc", "Profiler → recorder → optimiser → backend events; correct output"],
    ["3. Boundary case", "07_trace_exact_limit, 09_nested_loops, -vv 22_side_exit…", "Limit handling, blacklisting, register restoration"],
    ["4. Invalid / failure", "e01, e02, 03, 15, e03, e04 (--max-steps=2000000)", "Diagnostics, safe aborts, exit codes 2/3/4"],
    ["5. Internal evidence", "-O1/-O2 --dump-ir 12_loop_invariant; --dump-asm b5", "TraceIR, SSA, LIR, machine code"],
    ["6. Evaluation result", "make test; ./build/jit_bench --reps 3", "210/210 checks; benchmark table"],
    ["7. Evaluator-selected input", "./build/jitvm --compare demo_input.jbc", "Identical results + speed-up"],
], [1.4, 2.8, 2.3], size=8.5, caption="Live demonstration sequence (also automated in demo.sh)")
T(["Test / Scenario", "Expected Behaviour", "Observed Output / Evidence", "Result / Evaluator Remarks"], [
    ["1. Evaluator-selected input", "", "", ""], ["2. Boundary or failure condition", "", "", ""],
    ["3. Rebuild / reproducibility check", "make clean && make && make test passes", "", ""]],
    [1.6, 1.7, 1.6, 1.6], size=9, caption="Evaluator-selected test record (to be filled during the viva)")

H1("Appendix G: x86-64 Benchmark Results", page_break=True)
bench_table("x86_64", "Intel Xeon 2.1 GHz (x86-64)")
P("Timings on this shared cloud machine vary by about ±10 % between runs; values are medians of 7 runs.")

R.build_toc(PAGES)
R.save(OUT)
print("saved", OUT, "headings", len(R.headings))
import json
json.dump([[l, t, b] for l, t, b in R.headings], open("/home/claude/report/headings.json", "w"))
