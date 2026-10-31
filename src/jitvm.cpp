// jitvm.cpp -- command-line driver.
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "assembler.h"
#include "backend.h"
#include "verifier.h"
#include "vm.h"

namespace {

const char* kUsage =
    "Trace-Based JIT Compiler for a Custom Bytecode VM  (BCSE307 A2, Team 6)\n"
    "\n"
    "usage: jitvm [options] program.jbc\n"
    "\n"
    "execution\n"
    "  --mode=jit|interp     JIT enabled (default) or pure interpreter\n"
    "  --backend=native|ir   native machine code (default, falls back to ir) or IR executor\n"
    "  -O0 | -O1 | -O2       optimisation level (default -O2)\n"
    "                          -O0 none, -O1 fold/CSE/guard-elim/DCE, -O2 adds loop peeling\n"
    "  --hot=N               HOT_THRESHOLD: backward branches before recording (default 3)\n"
    "  --max-trace=N         MAX_TRACE_LENGTH (default 50)\n"
    "  --max-steps=N         safety limit on executed steps (default 2000000000)\n"
    "\n"
    "output\n"
    "  -q                    quiet: program output only\n"
    "  -v                    JIT events (default)\n"
    "  -vv                   also trace entries, side exits and restored registers\n"
    "  --dump-ir             print recorded TraceIR, optimised SSA and LIR for each trace\n"
    "  --dump-asm            print the generated machine code listing\n"
    "  --stats               print VM / JIT statistics at the end\n"
    "  --regs                print non-zero registers at the end\n"
    "  --time                print execution time\n"
    "\n"
    "tools\n"
    "  --check               assemble + verify only, print diagnostics and disassembly\n"
    "  --compare             run interpreter and JIT, check identical results, report speed-up\n"
    "  --strict              treat verifier warnings as errors\n"
    "  --backend-info        print native backend status and exit\n"
    "\n"
    "exit codes: 0 ok, 1 usage, 2 assembly/verification error, 3 runtime error,\n"
    "            4 step limit reached, 5 --compare mismatch\n";

bool startsWith(const std::string& s, const char* p) { return s.rfind(p, 0) == 0; }

struct Outcome {
    RunResult result;
    std::string output;
    int32_t regs[NUM_REGS];
    double seconds = 0;
    VMStats stats;
    std::string backend;
};

Outcome runOnce(const Program& prog, VMConfig cfg, std::ostream* out, std::ostream* log) {
    Outcome o;
    VirtualMachine vm(cfg);
    std::ostringstream capture;
    vm.setOutput(out ? out : &capture);
    vm.setLog(log);
    vm.loadProgram(prog);
    auto t0 = std::chrono::steady_clock::now();
    o.result = vm.run();
    auto t1 = std::chrono::steady_clock::now();
    o.seconds = std::chrono::duration<double>(t1 - t0).count();
    o.output = capture.str();
    for (int i = 0; i < NUM_REGS; i++) o.regs[i] = vm.reg(i);
    o.stats = vm.stats();
    o.backend = vm.backendDescription();
    return o;
}

void printRegs(const int32_t* r) {
    std::cout << "Final registers:";
    bool any = false;
    for (int i = 0; i < NUM_REGS; i++)
        if (r[i]) { std::cout << " R" << i << "=" << r[i]; any = true; }
    if (!any) std::cout << " (all zero)";
    std::cout << "\n";
}

int reportResult(const Program& prog, const RunResult& r) {
    if (r.ok()) return 0;
    std::cerr << prog.name << ": ";
    if (r.status == RunStatus::StepLimit) {
        std::cerr << "error: step limit reached (possible infinite loop); raise it with --max-steps\n";
        return 4;
    }
    std::cerr << "runtime error: " << r.error;
    if (r.errorPC >= 0)
        std::cerr << " at pc " << r.errorPC << " (source line " << prog.srcLine[r.errorPC] << ")";
    std::cerr << "\n";
    return 3;
}

}  // namespace

int main(int argc, char** argv) {
    VMConfig cfg;
    std::string file;
    bool stats = false, regs = false, timing = false, check = false, compare = false, strict = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        try {
            if (a == "-h" || a == "--help") { std::cout << kUsage; return 0; }
            else if (a == "--mode=jit") cfg.jit = true;
            else if (a == "--mode=interp") cfg.jit = false;
            else if (a == "--backend=native") cfg.backend = VMConfig::Backend::Native;
            else if (a == "--backend=ir") cfg.backend = VMConfig::Backend::IR;
            else if (a == "-O0" || a == "-O1" || a == "-O2") cfg.optLevel = a[2] - '0';
            else if (startsWith(a, "--hot=")) cfg.hotThreshold = std::stoi(a.substr(6));
            else if (startsWith(a, "--max-trace=")) cfg.maxTraceLength = std::stoi(a.substr(12));
            else if (startsWith(a, "--max-steps=")) cfg.maxSteps = std::stoll(a.substr(12));
            else if (a == "-q") cfg.verbosity = 0;
            else if (a == "-v") cfg.verbosity = 1;
            else if (a == "-vv") cfg.verbosity = 2;
            else if (a == "--dump-ir") cfg.dumpIR = true;
            else if (a == "--dump-asm") cfg.dumpAsm = true;
            else if (a == "--stats") stats = true;
            else if (a == "--regs") regs = true;
            else if (a == "--time") timing = true;
            else if (a == "--check") check = true;
            else if (a == "--compare") compare = true;
            else if (a == "--strict") strict = true;
            else if (a == "--backend-info") {
                std::cout << "host architecture : " << nativeArchName() << "\n";
                std::cout << "native backend    : " << (nativeBackendAvailable() ? "available" : "unavailable")
                          << " -- " << nativeBackendStatus() << "\n";
                return 0;
            } else if (!a.empty() && a[0] == '-') {
                std::cerr << "unknown option '" << a << "'\n\n" << kUsage;
                return 1;
            } else if (file.empty()) file = a;
            else { std::cerr << "only one program file may be given\n"; return 1; }
        } catch (const std::exception&) {
            std::cerr << "invalid numeric value in option '" << a << "'\n";
            return 1;
        }
    }
    if (file.empty()) { std::cerr << kUsage; return 1; }

    // ---- Front end: assemble + verify ----
    AssembleResult ar = assembleFile(file);
    for (const auto& d : ar.diagnostics) std::cerr << formatDiagnostic(ar.program, d) << "\n";
    if (!ar.ok()) {
        std::cerr << file << ": assembly failed with "
                  << ar.diagnostics.size() << " diagnostic(s)\n";
        return 2;
    }
    std::vector<Diagnostic> vd = verifyProgram(ar.program);
    int errors = 0, warnings = 0;
    for (const auto& d : vd) {
        std::cerr << formatDiagnostic(ar.program, d) << "\n";
        (d.level == Diagnostic::Error ? errors : warnings)++;
    }
    if (errors || (strict && warnings)) {
        std::cerr << file << ": verification failed (" << errors << " error(s), " << warnings << " warning(s))\n";
        return 2;
    }
    if (check) {
        std::cout << file << ": OK (" << ar.program.code.size() << " instructions, " << warnings
                  << " warning(s))\n" << disassemble(ar.program);
        return 0;
    }

    // ---- Differential comparison mode ----
    if (compare) {
        VMConfig ic = cfg;
        ic.jit = false;
        ic.verbosity = 0;
        VMConfig jc = cfg;
        jc.jit = true;
        jc.verbosity = 0;
        Outcome a = runOnce(ar.program, ic, nullptr, nullptr);
        Outcome b = runOnce(ar.program, jc, nullptr, nullptr);
        bool bothStepLimit = a.result.status == RunStatus::StepLimit && b.result.status == RunStatus::StepLimit;
        // When both runs are stopped by the safety limit, the stop point differs by
        // design (instructions vs. loop iterations), so only the status is compared.
        bool same = bothStepLimit || (a.output == b.output && a.result.status == b.result.status &&
                                      std::memcmp(a.regs, b.regs, sizeof a.regs) == 0);
        std::cout << b.output;
        std::cout << "----------------------------------------------------------\n";
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "interpreter : " << a.seconds << " s  (" << runStatusName(a.result.status) << ")\n";
        std::cout << "JIT         : " << b.seconds << " s  (" << runStatusName(b.result.status) << ", "
                  << b.backend << ", -O" << cfg.optLevel << ", " << b.stats.tracesCompiled << " trace(s))\n";
        if (b.seconds > 0) std::cout << std::setprecision(2) << "speed-up    : " << a.seconds / b.seconds << "x\n";
        std::cout << "output lines: " << std::count(a.output.begin(), a.output.end(), '\n')
                  << ", final registers and status "
                  << (same ? "IDENTICAL -> differential check PASSED\n" : "DIFFER -> differential check FAILED\n");
        if (bothStepLimit) std::cout << "(both runs stopped by the step limit; registers not compared)\n";
        if (!same) {
            printRegs(a.regs);
            printRegs(b.regs);
        }
        return same ? 0 : 5;
    }

    // ---- Normal execution ----
    VirtualMachine vm(cfg);
    vm.setOutput(&std::cout);
    vm.setLog(&std::cout);
    vm.loadProgram(ar.program);
    if (cfg.verbosity >= 1 && cfg.jit) std::cout << "[JIT] backend: " << vm.backendDescription() << "\n";
    auto t0 = std::chrono::steady_clock::now();
    RunResult r = vm.run();
    auto t1 = std::chrono::steady_clock::now();
    std::cout.flush();
    if (regs) printRegs(vm.registers());
    if (stats) vm.printStats(std::cout);
    if (timing)
        std::cout << "execution time: " << std::fixed << std::setprecision(4)
                  << std::chrono::duration<double>(t1 - t0).count() << " s\n";
    std::cout.flush();
    return reportResult(ar.program, r);
}
