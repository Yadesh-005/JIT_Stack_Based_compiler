// test_main.cpp -- automated test suite for the trace-based JIT.
//
// Sections
//   1. Assembler (front end) unit tests
//   2. Verifier (static analysis) unit tests
//   3. Review 2 regression: the eight Instruction-vector programs from the
//      Review 2 report, run through the original loadProgram(vector) API
//   4. Recorder / profiler behaviour (abort, blacklist, multi-loop, nested)
//   5. Optimizer unit tests (folding, CSE, guard elimination, DCE, peeling)
//   6. Deoptimisation / side-exit state restoration
//   7. Golden-output tests (tests/programs/*.jbc vs tests/expected/*.out)
//   8. Differential matrix: every program x backend x -O level x threshold
//   9. Random-program differential fuzzing
//  10. Safety: step limit, runtime errors, rejected programs
//
// usage: jit_tests [--tests DIR] [--fuzz N] [--seed S] [--update-golden] [--report FILE]
#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "assembler.h"
#include "backend.h"
#include "optimizer.h"
#include "verifier.h"
#include "vm.h"

namespace {

struct Section {
    std::string name;
    int pass = 0, fail = 0;
};
std::vector<Section> g_sections;
std::vector<std::string> g_failures;

void section(const std::string& name) {
    g_sections.push_back({name});
    std::cout << "\n=== " << name << " ===\n";
}
void check(bool ok, const std::string& what) {
    Section& s = g_sections.back();
    if (ok) s.pass++;
    else {
        s.fail++;
        g_failures.push_back("[" + s.name + "] " + what);
        std::cout << "  FAIL: " << what << "\n";
    }
}

struct Run {
    RunResult result;
    std::string out, log;
    int32_t regs[NUM_REGS];
    VMStats stats;
    std::vector<int> headers;    // compiled trace headers
    std::vector<OptStats> opt;   // per trace
};

Run runProgram(const Program& p, VMConfig cfg) {
    Run r;
    VirtualMachine vm(cfg);
    std::ostringstream out, log;
    vm.setOutput(&out);
    vm.setLog(&log);
    vm.loadProgram(p);
    r.result = vm.run();
    r.out = out.str();
    r.log = log.str();
    for (int i = 0; i < NUM_REGS; i++) r.regs[i] = vm.reg(i);
    r.stats = vm.stats();
    for (const auto& t : vm.traces()) {
        r.headers.push_back(t->headerPC);
        r.opt.push_back(t->ssa.stats);
    }
    return r;
}

VMConfig interpCfg() {
    VMConfig c;
    c.jit = false;
    c.verbosity = 1;
    return c;
}
VMConfig jitCfg(VMConfig::Backend b = VMConfig::Backend::Native, int opt = 2, int hot = 3) {
    VMConfig c;
    c.backend = b;
    c.optLevel = opt;
    c.hotThreshold = hot;
    c.verbosity = 1;
    return c;
}

bool sameResult(const Run& a, const Run& b) {
    if (a.result.status == RunStatus::StepLimit && b.result.status == RunStatus::StepLimit) return true;
    return a.result.status == b.result.status && a.out == b.out && std::memcmp(a.regs, b.regs, sizeof a.regs) == 0;
}

std::string statusLine(const Run& r) {
    std::ostringstream os;
    os << "status: " << runStatusName(r.result.status);
    if (!r.result.error.empty()) os << " (" << r.result.error << " at pc " << r.result.errorPC << ")";
    os << "\nregs:";
    for (int i = 0; i < NUM_REGS; i++) os << " " << r.regs[i];
    os << "\n";
    return os.str();
}

bool contains(const std::string& hay, const std::string& needle) { return hay.find(needle) != std::string::npos; }

Program asmOrDie(const std::string& src, const std::string& name = "<test>") {
    AssembleResult ar = assemble(src, name);
    if (!ar.ok()) {
        for (auto& d : ar.diagnostics) std::cout << formatDiagnostic(ar.program, d) << "\n";
        check(false, "test program failed to assemble: " + name);
    }
    return ar.program;
}

std::vector<std::string> listJbc(const std::string& dir) {
    std::vector<std::string> v;
    if (DIR* d = opendir(dir.c_str())) {
        while (dirent* e = readdir(d)) {
            std::string n = e->d_name;
            if (n.size() > 4 && n.substr(n.size() - 4) == ".jbc") v.push_back(n);
        }
        closedir(d);
    }
    std::sort(v.begin(), v.end());
    return v;
}

std::string readFile(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// ------------------------------------------------------------------ 1
void testAssembler() {
    section("1. Assembler (front end)");
    {
        AssembleResult r = assemble(
            "; comment\n"
            "start:  load_const r0, 0x10   # hex\n"
            "        LOAD_CONST R1, -5     // negative\n"
            "loop:   add_int R0, R0, R1\n"
            "        jump_if_lt R1, R0, loop\n"
            "        jump @0\n"
            "end:    HALT\n");
        check(r.ok(), "valid program assembles");
        check(r.program.code.size() == 6, "six instructions");
        check(r.program.code[0].op == LOAD_CONST && r.program.code[0].arg2 == 16, "hex immediate 0x10 = 16");
        check(r.program.code[1].arg2 == -5, "negative immediate");
        check(r.program.code[3].op == JUMP_IF_LT && r.program.code[3].arg3 == 2, "label resolves to pc 2");
        check(r.program.code[4].op == JUMP && r.program.code[4].arg1 == 0, "@0 numeric target");
        check(r.program.labels.count("END") && r.program.labels.at("END") == 5, "label table");
        check(r.program.srcLine[0] == 2, "source line tracking");
    }
    {
        AssembleResult r = assemble("JUMP fwd\nLOAD_CONST R0, 1\nfwd: HALT\n");
        check(r.ok() && r.program.code[0].arg1 == 2, "forward label reference");
    }
    {
        AssembleResult r = assemble("a: b: HALT\n");
        check(r.ok() && r.program.labels.at("A") == 0 && r.program.labels.at("B") == 0, "two labels on one line");
    }
    {
        AssembleResult r = assemble("LOAD_CONST R0, 0xFFFFFFFF\nLOAD_CONST R1, 2147483647\nLOAD_CONST R2, -2147483648\nHALT\n");
        check(r.ok() && r.program.code[0].arg2 == -1 && r.program.code[1].arg2 == 2147483647 &&
                  r.program.code[2].arg2 == (int)-2147483648LL,
              "int32 boundary immediates");
    }
    struct Bad { const char* src; const char* expect; };
    const Bad bad[] = {
        {"LOAD R0, 1\n", "unknown mnemonic"},
        {"ADD_INT R0, R1\n", "expects 3 operands"},
        {"ADD_INT R0, R16, R1\n", "invalid register"},
        {"ADD_INT R0, X1, R1\n", "invalid register"},
        {"LOAD_CONST R0, 4294967297\n", "does not fit"},
        {"LOAD_CONST R0, 2147483648\n", "does not fit in int32"},
        {"LOAD_CONST R0, 12abc\n", "malformed number"},
        {"JUMP nowhere\n", "undefined label"},
        {"x: HALT\nx: HALT\n", "duplicate label"},
        {"r3: HALT\n", "looks like a register"},
        {"HALT R0\n", "expects 0 operands"},
    };
    for (const Bad& b : bad) {
        AssembleResult r = assemble(b.src);
        bool found = false;
        for (auto& d : r.diagnostics) found |= contains(d.message, b.expect);
        check(!r.ok() && found, std::string("rejects: ") + b.expect);
    }
    {
        AssembleResult r = assemble("LOAD R0, 1\nADD_INT R0\nHALT\nJUMP nope\n");
        int errs = 0;
        for (auto& d : r.diagnostics) errs += d.level == Diagnostic::Error;
        check(errs == 3, "reports all errors, not just the first (3 errors)");
        check(r.diagnostics.size() >= 1 && r.diagnostics[0].line == 1, "error carries line number");
    }
    {
        AssembleResult r = assemble("LOAD_CONST R0, 5\nloop: ADD_INT R0, R0, R0\nJUMP_IF_LT R0, R1, loop\nHALT\n");
        AssembleResult r2 = assemble(disassemble(r.program));
        bool same = r2.ok() && r2.program.code.size() == r.program.code.size();
        for (size_t i = 0; same && i < r.program.code.size(); i++)
            same = std::memcmp(&r.program.code[i], &r2.program.code[i], sizeof(Instruction)) == 0;
        check(same, "disassemble -> assemble round trip");
    }
}

// ------------------------------------------------------------------ 2
void testVerifier() {
    section("2. Verifier (static analysis)");
    auto has = [](const std::vector<Diagnostic>& ds, Diagnostic::Level l, const std::string& s) {
        for (auto& d : ds) if (d.level == l && contains(d.message, s)) return true;
        return false;
    };
    {
        Program p = programFromInstructions({{ADD_INT, 0, 20, 1}, {HALT, 0, 0, 0}}, "t");
        check(has(verifyProgram(p), Diagnostic::Error, "register operand"), "register R20 rejected");
    }
    {
        Program p = programFromInstructions({{JUMP_IF_LT, 0, 1, 7}, {HALT, 0, 0, 0}}, "t");
        check(has(verifyProgram(p), Diagnostic::Error, "outside the program"), "jump target 7 rejected");
    }
    {
        Program p = programFromInstructions({{JUMP, -1, 0, 0}}, "t");
        check(has(verifyProgram(p), Diagnostic::Error, "outside the program"), "negative jump target rejected");
    }
    {
        Program p = programFromInstructions({{(Opcode)99, 0, 0, 0}}, "t");
        check(has(verifyProgram(p), Diagnostic::Error, "invalid opcode"), "invalid opcode value rejected");
    }
    {
        Program p = programFromInstructions({}, "t");
        check(has(verifyProgram(p), Diagnostic::Error, "empty"), "empty program rejected");
    }
    {
        Program p = asmOrDie("ADD_INT R0, R5, R1\nJUMP skip\nLOAD_CONST R3, 1\nskip: PRINT_REG R0\n");
        auto ds = verifyProgram(p);
        check(has(ds, Diagnostic::Warning, "R5 may be read before"), "read-before-write warning (R5)");
        check(has(ds, Diagnostic::Warning, "R1 may be read before"), "read-before-write warning (R1)");
        check(!has(ds, Diagnostic::Warning, "R0 may be read"), "no warning for R0 (written first)");
        check(has(ds, Diagnostic::Warning, "unreachable"), "unreachable instruction warning");
        check(has(ds, Diagnostic::Warning, "fall off the end"), "fall-off-end warning");
        check(has(ds, Diagnostic::Warning, "no HALT"), "missing HALT warning");
    }
    {
        // R2 is assigned on only one path -> may be undefined at the join.
        Program p = asmOrDie(
            "LOAD_CONST R0, 1\nLOAD_CONST R1, 2\nJUMP_IF_LT R0, R1, skip\nLOAD_CONST R2, 5\n"
            "skip: PRINT_REG R2\nHALT\n");
        check(has(verifyProgram(p), Diagnostic::Warning, "R2 may be read"), "data-flow join: R2 defined on one path only");
    }
    {
        Program p = asmOrDie("LOAD_CONST R0, 0\nLOAD_CONST R1, 1\nloop: ADD_INT R0, R0, R1\nJUMP_IF_LT R0, R1, loop\nHALT\n");
        auto ds = verifyProgram(p);
        check(ds.empty(), "clean loop program has no diagnostics");
    }
    {
        Program p = asmOrDie("self: JUMP self\n");
        check(has(verifyProgram(p), Diagnostic::Warning, "infinite loop"), "self-jump warning");
    }
}

// ------------------------------------------------------------------ 3
void testReview2Regression() {
    section("3. Review 2 regression (Instruction-vector API)");
    std::vector<Instruction> validStableLoop = {
        {LOAD_CONST, 0, 0, 0}, {LOAD_CONST, 1, 1, 0}, {LOAD_CONST, 2, 10, 0},
        {ADD_INT, 0, 0, 1}, {PRINT_REG, 0, 0, 0}, {JUMP_IF_LT, 0, 2, 3}, {HALT, 0, 0, 0}};
    std::vector<Instruction> belowThresholdLoop = {
        {LOAD_CONST, 0, 0, 0}, {LOAD_CONST, 1, 1, 0}, {LOAD_CONST, 2, 3, 0},
        {ADD_INT, 0, 0, 1}, {PRINT_REG, 0, 0, 0}, {JUMP_IF_LT, 0, 2, 3}, {HALT, 0, 0, 0}};
    std::vector<Instruction> invalidInstructionLoop = {
        {LOAD_CONST, 0, 0, 0}, {LOAD_CONST, 1, 1, 0}, {LOAD_CONST, 2, 20, 0}, {ADD_INT, 0, 0, 1},
        {PRINT_REG, 0, 0, 0}, {UNKNOWN_OP, 0, 0, 0}, {JUMP_IF_LT, 0, 2, 3}, {HALT, 0, 0, 0}};
    std::vector<Instruction> traceLimitLoop = {{LOAD_CONST, 0, 0, 0}, {LOAD_CONST, 1, 1, 0}, {LOAD_CONST, 2, 50, 0}};
    traceLimitLoop.push_back({ADD_INT, 0, 0, 1});
    for (int i = 0; i < 51; i++) traceLimitLoop.push_back({ADD_INT, 3, 3, 3});
    traceLimitLoop.push_back({PRINT_REG, 0, 0, 0});
    traceLimitLoop.push_back({JUMP_IF_LT, 0, 2, 3});
    traceLimitLoop.push_back({HALT, 0, 0, 0});
    std::vector<Instruction> zeroIterationLoop = {
        {LOAD_CONST, 0, 10, 0}, {LOAD_CONST, 1, 5, 0}, {JUMP_IF_LT, 0, 1, 1}, {PRINT_REG, 0, 0, 0}, {HALT, 0, 0, 0}};
    std::vector<Instruction> multipleHotLoops = {
        {LOAD_CONST, 0, 0, 0}, {LOAD_CONST, 1, 1, 0}, {LOAD_CONST, 2, 10, 0}, {ADD_INT, 0, 0, 1},
        {PRINT_REG, 0, 0, 0}, {JUMP_IF_LT, 0, 2, 3}, {LOAD_CONST, 4, 0, 0}, {LOAD_CONST, 5, 1, 0},
        {LOAD_CONST, 6, 10, 0}, {ADD_INT, 4, 4, 5}, {PRINT_REG, 4, 0, 0}, {JUMP_IF_LT, 4, 6, 9}, {HALT, 0, 0, 0}};
    std::vector<Instruction> traceExactLimitLoop = {{LOAD_CONST, 0, 0, 0}, {LOAD_CONST, 1, 1, 0}, {LOAD_CONST, 2, 55, 0}};
    traceExactLimitLoop.push_back({ADD_INT, 0, 0, 1});
    for (int i = 0; i < 48; i++) traceExactLimitLoop.push_back({ADD_INT, 3, 3, 3});
    traceExactLimitLoop.push_back({PRINT_REG, 0, 0, 0});
    traceExactLimitLoop.push_back({JUMP_IF_LT, 0, 2, 3});
    traceExactLimitLoop.push_back({HALT, 0, 0, 0});
    // Case 8 as submitted in Review 2 never terminates (R0 never changes).
    std::vector<Instruction> unknownOpColdPath = {
        {LOAD_CONST, 0, 0, 0}, {LOAD_CONST, 1, 1, 0}, {UNKNOWN_OP, 0, 0, 0}, {LOAD_CONST, 2, 2, 0},
        {JUMP_IF_LT, 0, 2, 3}, {PRINT_REG, 0, 0, 0}, {HALT, 0, 0, 0}};

    auto run = [](const std::vector<Instruction>& prog, VMConfig cfg) {
        Run r;
        VirtualMachine vm(cfg);
        std::ostringstream out, log;
        vm.setOutput(&out);
        vm.setLog(&log);
        bool ok = vm.loadProgram(prog);  // Review 2 API
        (void)ok;
        r.result = vm.run();
        r.out = out.str();
        r.log = log.str();
        for (int i = 0; i < NUM_REGS; i++) r.regs[i] = vm.reg(i);
        r.stats = vm.stats();
        for (const auto& t : vm.traces()) r.headers.push_back(t->headerPC);
        return r;
    };
    std::string seq;
    for (int i = 1; i <= 10; i++) seq += "Register R0 = " + std::to_string(i) + "\n";

    Run a = run(validStableLoop, jitCfg());
    check(a.out == seq, "case 1: prints R0 = 1..10");
    check(contains(a.log, "Hot loop detected at pc 3"), "case 1: hot loop detected at pc 3");
    check(a.stats.tracesCompiled == 1 && a.headers[0] == 3, "case 1: one trace at header 3");
    check(a.stats.traceIterations > 0, "case 1: loop iterations executed in compiled code");

    Run b = run(belowThresholdLoop, jitCfg());
    check(b.out == "Register R0 = 1\nRegister R0 = 2\nRegister R0 = 3\n", "case 2: prints 1..3");
    check(!contains(b.log, "PROFILER") && b.stats.tracesCompiled == 0, "case 2: profiler never fires");

    Run c = run(invalidInstructionLoop, jitCfg());
    check(c.regs[0] == 20 && c.result.status == RunStatus::Halted, "case 3: completes with R0 = 20");
    check(contains(c.log, "Unsupported opcode detected"), "case 3: recorder aborts on UNKNOWN_OP");
    check(contains(c.log, "Ignored unknown opcode"), "case 3: interpreter skips UNKNOWN_OP");
    check(c.stats.blacklisted == 1 && c.stats.tracesCompiled == 0, "case 3: loop blacklisted, no trace");

    Run d = run(traceLimitLoop, jitCfg());
    check(contains(d.log, "Max trace length reached"), "case 4: MAX_TRACE_LENGTH abort");
    check(d.regs[0] == 50 && d.stats.tracesCompiled == 0, "case 4: completes, no trace compiled");

    Run e = run(zeroIterationLoop, jitCfg());
    check(e.out == "Register R0 = 10\n" && e.stats.recordAborts == 0, "case 5: zero-iteration loop");

    Run f = run(multipleHotLoops, jitCfg());
    check(f.stats.tracesCompiled == 2 && f.headers[0] == 3 && f.headers[1] == 9, "case 6: two traces (headers 3 and 9)");
    check(f.regs[0] == 10 && f.regs[4] == 10, "case 6: both loops complete");

    Run g = run(traceExactLimitLoop, jitCfg());
    check(g.stats.tracesCompiled == 1 && !contains(g.log, "Max trace length"), "case 7: exactly 50 ops accepted");
    check(g.regs[0] == 55, "case 7: completes with R0 = 55");

    VMConfig lim = jitCfg();
    lim.maxSteps = 100000;
    Run h = run(unknownOpColdPath, lim);
    check(h.result.status == RunStatus::StepLimit, "case 8 (as submitted): non-termination caught by step limit (JIT)");
    VMConfig limI = interpCfg();
    limI.maxSteps = 100000;
    Run h2 = run(unknownOpColdPath, limI);
    check(h2.result.status == RunStatus::StepLimit, "case 8 (as submitted): caught by step limit (interpreter)");

    // Every case: JIT result identical to the pure interpreter.
    const std::vector<Instruction>* all[] = {&validStableLoop, &belowThresholdLoop, &invalidInstructionLoop,
                                             &traceLimitLoop, &zeroIterationLoop, &multipleHotLoops,
                                             &traceExactLimitLoop};
    int k = 1;
    for (auto* p : all) {
        Run i = run(*p, interpCfg());
        Run j = run(*p, jitCfg());
        check(sameResult(i, j), "case " + std::to_string(k++) + ": JIT == interpreter");
    }
    // Invalid program via the old API is rejected instead of executing garbage.
    Run bad = run({{ADD_INT, 0, 99, 0}, {HALT, 0, 0, 0}}, jitCfg());
    check(bad.result.status == RunStatus::RuntimeError && contains(bad.result.error, "verification"),
          "invalid Instruction vector rejected by verifier");
}

// ------------------------------------------------------------------ 4
void testRecorder(const std::string& dir) {
    section("4. Recorder / profiler behaviour");
    auto load = [&](const std::string& n) { return assembleFile(dir + "/programs/" + n).program; };
    {
        Run r = runProgram(load("11_loop_exit_during_recording.jbc"), jitCfg());
        check(contains(r.log, "loop exited while recording"), "loop exit during recording aborts cleanly");
        check(r.stats.tracesCompiled == 1 && r.headers[0] == 7, "the following loop (line 7) is still traced");
    }
    {
        Run r = runProgram(load("09_nested_loops.jbc"), jitCfg());
        check(r.regs[5] == 14400, "nested loops: total = 14400");
        check(contains(r.log, "inner loop"), "outer recording aborts at inner back-edge");
        check(r.stats.tracesCompiled == 1 && r.headers[0] == 6, "inner loop (line 6) compiled");
        check(r.stats.blacklisted == 1, "outer loop blacklisted after 2 attempts");
        check(r.stats.traceEntries >= 15, "inner trace re-entered on every outer iteration");
    }
    {
        Run r = runProgram(load("15_div_not_jitable.jbc"), jitCfg());
        check(contains(r.log, "DIV_INT is not supported"), "DIV_INT aborts recording");
        check(r.regs[3] == 392, "DIV loop result = 392 in the interpreter");
    }
    {
        Run r = runProgram(load("16_countdown_jump_if_eq.jbc"), jitCfg());
        check(r.stats.tracesCompiled == 1 && r.regs[3] == 325, "loop closed by unconditional JUMP is traced");
    }
    for (int hot : {1, 2, 3, 4, 5, 9}) {
        VMConfig c = jitCfg(VMConfig::Backend::Native, 2, hot);
        Run r = runProgram(load("01_valid_stable_loop.jbc"), c);
        bool expectTrace = hot <= 8;  // 9 taken back-edges in total
        check((r.stats.tracesCompiled == 1) == expectTrace && r.regs[0] == 10,
              "HOT_THRESHOLD=" + std::to_string(hot) + " behaves correctly");
    }
    {
        VMConfig c = jitCfg();
        c.maxTraceLength = 2;
        Run r = runProgram(load("01_valid_stable_loop.jbc"), c);
        check(r.stats.tracesCompiled == 1, "MAX_TRACE_LENGTH=2 accepts a 2-op body");
        c.maxTraceLength = 1;
        Run r2 = runProgram(load("01_valid_stable_loop.jbc"), c);
        check(r2.stats.tracesCompiled == 0 && contains(r2.log, "Max trace length"), "MAX_TRACE_LENGTH=1 aborts");
    }
    {
        Run r = runProgram(load("10_if_else_in_loop.jbc"), jitCfg());
        check(r.regs[3] == 2450 && r.regs[4] == 50, "if/else loop result");
        check(r.stats.exits >= 10, "alternating branch causes repeated side exits");
    }
    {
        // Alternating branch: the trace exits on every other iteration and is
        // re-entered; results must still match the interpreter.
        Program p = asmOrDie(
            "LOAD_CONST R0, 0\nLOAD_CONST R1, 1\nLOAD_CONST R2, 2000\nLOAD_CONST R6, 0\n"
            "loop: AND_INT R5, R0, R1\nJUMP_IF_EQ R5, R6, even\nADD_INT R4, R4, R1\nJUMP next\n"
            "even: ADD_INT R3, R3, R0\nnext: ADD_INT R0, R0, R1\nJUMP_IF_LT R0, R2, loop\nHALT\n");
        Run r = runProgram(p, jitCfg());
        Run i = runProgram(p, interpCfg());
        check(r.stats.traceEntries > 500 && r.stats.exits == r.stats.traceEntries, "alternating branch: repeated side exits and re-entries");
        check(sameResult(i, r), "alternating-branch program correct");
    }
    {
        // Phase change: path A for i < 500, path B afterwards. The phase-A trace
        // side-exits on every entry in phase B; results must still be exact.
        Program p = load("24_phase_change.jbc");
        Run r = runProgram(p, jitCfg());
        Run i = runProgram(p, interpCfg());
        check(r.stats.tracesCompiled == 1 && r.stats.exits > 2000, "phase change: stale trace keeps side-exiting (known limitation)");
        check(sameResult(i, r), "phase-change program correct");
    }
}

// ------------------------------------------------------------------ 5
SSATrace traceOf(const Program& p, int opt, int index = 0) {
    VMConfig c = jitCfg(VMConfig::Backend::IR, opt, 3);
    VirtualMachine vm(c);
    std::ostringstream o;
    vm.setOutput(&o);
    vm.setLog(nullptr);
    vm.loadProgram(p);
    vm.run();
    if ((int)vm.traces().size() <= index) return SSATrace();
    return vm.traces()[index]->ssa;
}

void testOptimizer(const std::string& dir) {
    section("5. Optimizer (SSA, folding, CSE, guard elimination, DCE, loop peeling)");
    auto load = [&](const std::string& n) { return assembleFile(dir + "/programs/" + n).program; };
    {
        Program p = load("12_loop_invariant.jbc");
        SSATrace o1 = traceOf(p, 1), o2 = traceOf(p, 2);
        check(o1.stats.loopOps == 6, "-O1 loop body keeps 6 ops (no peeling)");
        check(o2.stats.loopOps == 3, "-O2 loop body has 3 ops (invariant MULs hoisted)");
        check(o2.stats.hoisted == 3, "-O2 hoisted 3 invariant ops into the preamble");
        check(o2.stats.phisRemoved == 3, "-O2 proved R6, R7, R8 loop-invariant (Phi removed)");
        bool mulInLoop = false;
        for (auto& s : o2.insts) if (!s.dead && s.inLoop && s.op == SOp::Mul) mulInLoop = true;
        check(!mulInLoop, "no Mul left inside the -O2 loop");
    }
    {
        Program p = load("21_constant_folding.jbc");
        SSATrace o = traceOf(p, 2);
        check(o.stats.folded >= 4, "constant folding + algebraic simplification (>= 4)");
        bool onlyAdds = true;
        for (auto& s : o.insts) if (!s.dead && s.inLoop && (s.op == SOp::Sub)) onlyAdds = false;
        check(onlyAdds, "x - x folded to 0 (no Sub in loop)");
        check(o.stats.loopOps == 3, "constant-folded loop body: acc+=42, i+=1, guard");
        SSATrace o0 = traceOf(p, 0);
        check(o0.stats.folded == 0 && o0.stats.loopOps > o.stats.loopOps, "-O0 performs no folding");
    }
    {
        Program p = load("20_redundant_guards.jbc");
        SSATrace o = traceOf(p, 2);
        check(o.stats.guardsRemoved >= 1, "duplicate bounds check removed");
        check(o.stats.loopGuards == 2, "loop keeps 2 guards (bounds check + loop condition)");
        SSATrace o0 = traceOf(p, 0);
        check(o0.stats.loopGuards == 3, "-O0 keeps all 3 guards");
    }
    {
        // Dead value: R7 overwritten before use and never observed between.
        Program p = asmOrDie(
            "LOAD_CONST R0, 0\nLOAD_CONST R1, 1\nLOAD_CONST R2, 100\n"
            "loop: MUL_INT R7, R0, R0\nLOAD_CONST R7, 5\nADD_INT R0, R0, R1\nJUMP_IF_LT R0, R2, loop\nHALT\n");
        SSATrace o = traceOf(p, 2);
        bool mul = false;
        for (auto& s : o.insts) if (!s.dead && s.op == SOp::Mul) mul = true;
        check(!mul && o.stats.dceRemoved >= 1, "dead MUL eliminated by DCE");
    }
    {
        Program p = load("19_swap_parallel_copy.jbc");
        VMConfig c = jitCfg(VMConfig::Backend::IR, 2, 3);
        VirtualMachine vm(c);
        std::ostringstream o;
        vm.setOutput(&o);
        vm.setLog(nullptr);
        vm.loadProgram(p);
        vm.run();
        bool temps = !vm.traces().empty() && vm.traces()[0]->lir.numSlots > 0;
        int phiSwap = 0;
        for (auto& s : vm.traces()[0]->ssa.insts)
            if (!s.dead && s.op == SOp::Phi && vm.traces()[0]->ssa.insts[s.b].op == SOp::Phi) phiSwap++;
        check(temps && phiSwap >= 2, "swap creates Phi->Phi back-edges (parallel copy)");
        check(vm.reg(0) == 5 && vm.reg(1) == 3, "parallel copy correct (R0=5, R1=3)");
    }
    {
        // SSA well-formedness on every test program: operands defined before use.
        bool ok = true;
        for (const std::string& n : listJbc(dir + "/programs")) {
            Program p = load(n);
            for (int lvl = 0; lvl <= 2; lvl++) {
                SSATrace t = traceOf(p, lvl);
                for (size_t i = 0; i < t.insts.size(); i++) {
                    const SInst& s = t.insts[i];
                    if (s.dead) continue;
                    if (s.op == SOp::Phi) { ok &= s.a >= 0 && s.a < (int)i && s.b >= 0; continue; }
                    if (s.a >= 0) ok &= s.a < (int)i && !t.insts[s.a].dead;
                    if (s.b >= 0) ok &= s.b < (int)i && !t.insts[s.b].dead;
                }
            }
        }
        check(ok, "SSA well-formed (defs dominate uses, no dead operands) for all programs/levels");
    }
}

// ------------------------------------------------------------------ 6
void testDeopt(const std::string& dir) {
    section("6. Deoptimisation / snapshot restore");
    auto load = [&](const std::string& n) { return assembleFile(dir + "/programs/" + n).program; };
    Program p = load("22_side_exit_restores_state.jbc");
    Run i = runProgram(p, interpCfg());
    for (auto b : {VMConfig::Backend::Native, VMConfig::Backend::IR}) {
        for (int lvl = 0; lvl <= 2; lvl++) {
            Run j = runProgram(p, jitCfg(b, lvl, 3));
            std::string tag = std::string(b == VMConfig::Backend::Native ? "native" : "ir") + " -O" + std::to_string(lvl);
            check(sameResult(i, j), "mid-body side exit restores R0,R3,R4,R5 exactly (" + tag + ")");
            check(j.stats.traceIterations > 50, "most iterations ran in compiled code (" + tag + ")");
        }
    }
    check(i.regs[0] == 77 && i.regs[3] == 149226 && i.regs[4] == -2926 && i.regs[5] == 5929,
          "reference values R0=77 R3=149226 R4=-2926 R5=5929");
    {
        // Side exit followed by re-entry: gcd alternates directions.
        Program g = load("18_gcd_subtraction.jbc");
        Run a = runProgram(g, interpCfg());
        Run b = runProgram(g, jitCfg(VMConfig::Backend::Native, 2, 1));
        check(sameResult(a, b) && b.regs[0] == 21, "gcd with repeated side exits and re-entries = 21");
    }
    {
        // Fuel exit: an infinite loop inside compiled code returns to the VM.
        Program inf = asmOrDie("LOAD_CONST R1, 1\nloop: ADD_INT R0, R0, R1\nJUMP loop\n");
        VMConfig c = jitCfg();
        c.maxSteps = 5000000;
        Run r = runProgram(inf, c);
        check(r.result.status == RunStatus::StepLimit && r.stats.traceIterations > 1000000,
              "fuel check stops an infinite loop inside native code");
        check(r.regs[0] > 1000000, "registers restored from the fuel-exit snapshot");
    }
}

// ------------------------------------------------------------------ 7
void testGolden(const std::string& dir, bool update) {
    section("7. Golden output (interpreter reference)");
    for (const std::string& n : listJbc(dir + "/programs")) {
        Program p = assembleFile(dir + "/programs/" + n).program;
        Run r = runProgram(p, interpCfg());
        std::string actual = r.out + statusLine(r);
        std::string path = dir + "/expected/" + n.substr(0, n.size() - 4) + ".out";
        if (update) {
            std::ofstream(path) << actual;
            check(true, n);
            continue;
        }
        std::string expected = readFile(path);
        check(!expected.empty() && expected == actual, n + " matches " + path);
    }
}

// ------------------------------------------------------------------ 8
void testDifferential(const std::string& dir, int& runs) {
    section("8. Differential matrix (JIT vs interpreter)");
    for (const std::string& n : listJbc(dir + "/programs")) {
        Program p = assembleFile(dir + "/programs/" + n).program;
        Run ref = runProgram(p, interpCfg());
        int bad = 0, total = 0;
        for (auto b : {VMConfig::Backend::Native, VMConfig::Backend::IR})
            for (int lvl = 0; lvl <= 2; lvl++)
                for (int hot : {1, 2, 3, 5, 8}) {
                    Run j = runProgram(p, jitCfg(b, lvl, hot));
                    total++;
                    if (!sameResult(ref, j)) {
                        bad++;
                        std::cout << "    mismatch: " << n << " backend=" << (b == VMConfig::Backend::Native ? "native" : "ir")
                                  << " -O" << lvl << " hot=" << hot << "\n";
                    }
                }
        runs += total;
        check(bad == 0, n + ": " + std::to_string(total) + " configurations identical");
    }
}

// ------------------------------------------------------------------ 9
struct Gen {
    std::mt19937 rng;
    std::vector<std::string> lines;
    int label = 0;
    explicit Gen(unsigned seed) : rng(seed) {}
    int rnd(int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); }
    std::string data() { static const int regs[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 13}; return "R" + std::to_string(regs[rnd(0, 10)]); }
    std::string newLabel() { return "L" + std::to_string(label++); }
    void stmt(int depth) {
        int k = rnd(0, 99);
        if (k < 55) {
            static const char* ops[] = {"ADD_INT", "SUB_INT", "MUL_INT", "AND_INT", "ADD_INT", "SUB_INT"};
            int o = rnd(0, 7);
            if (o < 6) lines.push_back(std::string(ops[o]) + " " + data() + ", " + data() + ", " + data());
            else if (o == 6) lines.push_back("MOV " + data() + ", " + data());
            else lines.push_back("LOAD_CONST " + data() + ", " + std::to_string(rnd(-1000, 1000)));
        } else if (k < 62) {
            lines.push_back("PRINT_REG " + data());
        } else if (k < 64) {
            lines.push_back("DIV_INT " + data() + ", " + data() + ", R15");  // divisor 1: never traps, never JIT-able
        } else if (k < 65) {
            lines.push_back("UNKNOWN_OP");
        } else if (k < 80) {
            // if / if-else
            std::string a = data(), b = data(), skip = newLabel();
            const char* br = rnd(0, 1) ? "JUMP_IF_LT" : "JUMP_IF_EQ";
            lines.push_back(std::string(br) + " " + a + ", " + b + ", " + skip);
            block(depth, 1, 3);
            if (rnd(0, 1)) {
                std::string end = newLabel();
                lines.push_back("JUMP " + end);
                lines.push_back(skip + ":");
                block(depth, 1, 3);
                lines.push_back(end + ":");
            } else {
                lines.push_back(skip + ":");
            }
        } else if (depth < 3) {
            std::string c = "R" + std::to_string(10 + depth);
            std::string top = newLabel();
            int iters = rnd(0, 14);
            if (rnd(0, 1)) {
                // while (c != 0) { body; c-- }   -- closed by JUMP, exit guard NE
                std::string done = newLabel();
                lines.push_back("LOAD_CONST " + c + ", " + std::to_string(iters));
                lines.push_back(top + ": JUMP_IF_EQ " + c + ", R14, " + done);
                block(depth + 1, 1, 4);
                lines.push_back("SUB_INT " + c + ", " + c + ", R15");
                lines.push_back("JUMP " + top);
                lines.push_back(done + ":");
            } else {
                // do { body; c-- } while (0 < c)
                lines.push_back("LOAD_CONST " + c + ", " + std::to_string(iters + 1));
                lines.push_back(top + ":");
                block(depth + 1, 1, 4);
                lines.push_back("SUB_INT " + c + ", " + c + ", R15");
                lines.push_back("JUMP_IF_LT R14, " + c + ", " + top);
            }
        } else {
            lines.push_back("ADD_INT " + data() + ", " + data() + ", R15");
        }
    }
    void block(int depth, int lo, int hi) {
        int n = rnd(lo, hi);
        for (int i = 0; i < n; i++) stmt(depth);
    }
    std::string program() {
        lines.clear();
        label = 0;
        lines.push_back("LOAD_CONST R14, 0");
        lines.push_back("LOAD_CONST R15, 1");
        for (int r : {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 13})
            lines.push_back("LOAD_CONST R" + std::to_string(r) + ", " + std::to_string(rnd(-50, 50)));
        block(0, 2, 6);
        for (int r : {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 13}) lines.push_back("PRINT_REG R" + std::to_string(r));
        lines.push_back("HALT");
        std::string s;
        for (auto& l : lines) s += l + "\n";
        return s;
    }
};

void testFuzz(int count, unsigned seed, int& runs, int& tracesSeen) {
    section("9. Random-program differential fuzzing");
    Gen g(seed);
    int bad = 0;
    struct Cfg { VMConfig::Backend b; int opt, hot, maxLen; };
    const Cfg cfgs[] = {
        {VMConfig::Backend::Native, 2, 1, 50}, {VMConfig::Backend::Native, 2, 3, 50},
        {VMConfig::Backend::Native, 1, 2, 50}, {VMConfig::Backend::Native, 0, 3, 50},
        {VMConfig::Backend::Native, 2, 2, 8},  {VMConfig::Backend::IR, 2, 1, 50},
        {VMConfig::Backend::IR, 2, 3, 50},
    };
    for (int i = 0; i < count; i++) {
        std::string src = g.program();
        AssembleResult ar = assemble(src, "fuzz");
        if (!ar.ok()) { bad++; std::cout << "    generator produced invalid program\n" << src; continue; }
        VMConfig ic = interpCfg();
        ic.verbosity = 0;
        Run ref = runProgram(ar.program, ic);
        for (const Cfg& c : cfgs) {
            VMConfig jc = jitCfg(c.b, c.opt, c.hot);
            jc.maxTraceLength = c.maxLen;
            jc.verbosity = 0;
            Run j = runProgram(ar.program, jc);
            runs++;
            tracesSeen += j.stats.tracesCompiled;
            if (!sameResult(ref, j)) {
                bad++;
                if (bad <= 3) {
                    std::cout << "    MISMATCH (program " << i << ", -O" << c.opt << " hot=" << c.hot << ")\n";
                    std::ofstream("fuzz_failure_" + std::to_string(bad) + ".jbc") << src;
                }
            }
        }
    }
    check(bad == 0, std::to_string(count) + " random programs x 7 JIT configurations agree with the interpreter");
    check(tracesSeen > count, "fuzzing actually exercised compiled traces (" + std::to_string(tracesSeen) + " traces)");
}

// ------------------------------------------------------------------ 10
void testSafety(const std::string& dir) {
    section("10. Safety: step limit, runtime errors, rejected programs");
    auto load = [&](const std::string& n) { return assembleFile(dir + "/errors/" + n).program; };
    for (bool jit : {false, true}) {
        std::string m = jit ? " (JIT)" : " (interpreter)";
        VMConfig c = jit ? jitCfg() : interpCfg();
        c.maxSteps = 200000;
        Run r = runProgram(load("e04_infinite_loop.jbc"), c);
        check(r.result.status == RunStatus::StepLimit, "infinite loop stopped by step limit" + m);
        Run d = runProgram(load("e03_division_by_zero.jbc"), jit ? jitCfg() : interpCfg());
        check(d.result.status == RunStatus::RuntimeError && contains(d.result.error, "division by zero") &&
                  d.result.errorPC == 4,
              "division by zero reported at pc 4" + m);
        Run e = runProgram(load("r2_case8_original.jbc"), c);
        check(e.result.status == RunStatus::StepLimit, "Review 2 case 8 (non-terminating) caught" + m);
    }
    {
        AssembleResult ar = assembleFile(dir + "/errors/e01_syntax_errors.jbc");
        int errs = 0;
        for (auto& d : ar.diagnostics) errs += d.level == Diagnostic::Error;
        check(!ar.ok() && errs == 6, "syntax error file: 6 errors reported");
    }
    {
        AssembleResult ar = assembleFile(dir + "/errors/e02_bad_jump_target.jbc");
        bool err = false;
        for (auto& d : verifyProgram(ar.program)) err |= d.level == Diagnostic::Error;
        check(ar.ok() && err, "bad jump target caught by verifier");
        VirtualMachine vm(jitCfg());
        check(!vm.loadProgram(ar.program) && vm.run().status == RunStatus::RuntimeError, "VM refuses unverified program");
    }
    {
        AssembleResult ar = assembleFile(dir + "/errors/e05_empty.jbc");
        check(ar.ok() && ar.program.code.empty() && !verifyProgram(ar.program).empty(), "empty program rejected");
    }
    {
        AssembleResult ar = assembleFile(dir + "/errors/does_not_exist.jbc");
        check(!ar.ok(), "missing file reported");
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::string dir = "tests";
    int fuzz = 400;
    unsigned seed = 20261021;
    bool update = false;
    std::string report;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--tests" && i + 1 < argc) dir = argv[++i];
        else if (a == "--fuzz" && i + 1 < argc) fuzz = std::stoi(argv[++i]);
        else if (a == "--seed" && i + 1 < argc) seed = (unsigned)std::stoul(argv[++i]);
        else if (a == "--update-golden") update = true;
        else if (a == "--report" && i + 1 < argc) report = argv[++i];
    }
    std::cout << "Trace-based JIT test suite\n";
    std::cout << "native backend: " << nativeBackendStatus() << "\n";
    int diffRuns = 0, fuzzRuns = 0, fuzzTraces = 0;
    testAssembler();
    testVerifier();
    testReview2Regression();
    testRecorder(dir);
    testOptimizer(dir);
    testDeopt(dir);
    testGolden(dir, update);
    testDifferential(dir, diffRuns);
    testFuzz(fuzz, seed, fuzzRuns, fuzzTraces);
    testSafety(dir);

    int pass = 0, fail = 0;
    std::ostringstream table;
    table << "| Section | Passed | Failed |\n|---|---|---|\n";
    std::cout << "\n==================== SUMMARY ====================\n";
    for (auto& s : g_sections) {
        std::cout << "  " << (s.fail ? "FAIL " : "ok   ") << s.name << ": " << s.pass << " passed, " << s.fail << " failed\n";
        table << "| " << s.name << " | " << s.pass << " | " << s.fail << " |\n";
        pass += s.pass;
        fail += s.fail;
    }
    std::cout << "  differential runs: " << diffRuns << " (matrix) + " << fuzzRuns << " (fuzz, "
              << fuzzTraces << " traces compiled)\n";
    std::cout << "  TOTAL: " << pass << " checks passed, " << fail << " failed\n";
    for (auto& f : g_failures) std::cout << "  " << f << "\n";
    std::cout << "=================================================\n";
    if (!report.empty()) {
        std::ofstream r(report);
        r << "native backend: " << nativeBackendStatus() << "\n\n" << table.str()
          << "\nTOTAL: " << pass << " passed, " << fail << " failed; differential runs: " << diffRuns
          << " matrix + " << fuzzRuns << " fuzz (" << fuzzTraces << " traces compiled)\n";
    }
    return fail == 0 ? 0 : 1;
}
