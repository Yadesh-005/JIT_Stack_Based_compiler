// vm.h -- mixed-mode virtual machine: interpreter + profiler + recorder +
// optimiser + code cache + deoptimisation.
//
// State machine:
//   INTERPRETING --(backward branch reaches HOT_THRESHOLD)--> RECORDING
//   RECORDING    --(loop closed)--> compile --> code cache
//   RECORDING    --(abort)--> INTERPRETING  (retry with back-off, then blacklist)
//   INTERPRETING --(pc == header of a compiled trace)--> EXECUTING TRACE
//   EXECUTING    --(guard fails)--> restore snapshot (deopt) --> INTERPRETING
#pragma once

#include <cstdint>
#include <iosfwd>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "assembler.h"
#include "backend.h"
#include "isa.h"
#include "recorder.h"
#include "trace_ir.h"

struct VMConfig {
    enum class Backend { Native, IR };
    bool jit = true;
    Backend backend = Backend::Native;  // falls back to IR if native unavailable
    int optLevel = 2;                   // 0: none, 1: forward opts, 2: + loop peeling
    int hotThreshold = 3;               // HOT_THRESHOLD (Review 2 value)
    int maxTraceLength = 50;            // MAX_TRACE_LENGTH (Review 2 value)
    int maxRecordAttempts = 2;          // aborted recordings before blacklisting
    long long maxSteps = 2000000000LL;  // safety limit (instructions + trace iterations)
    int verbosity = 1;                  // 0 quiet, 1 JIT events, 2 + trace entries/exits
    bool dumpIR = false;
    bool dumpAsm = false;
};

enum class RunStatus { Halted, EndOfProgram, RuntimeError, StepLimit, NotLoaded };
const char* runStatusName(RunStatus s);

struct RunResult {
    RunStatus status = RunStatus::NotLoaded;
    std::string error;
    int errorPC = -1;
    bool ok() const { return status == RunStatus::Halted || status == RunStatus::EndOfProgram; }
};

struct TraceInfo {
    int id = 0;
    int headerPC = -1;
    RecordedTrace raw;
    SSATrace ssa;
    LIRTrace lir;
    std::unique_ptr<CompiledTrace> code;
    std::vector<int32_t> slots;
    std::string backendName;
    uint64_t entries = 0;
    uint64_t iterations = 0;
    std::vector<uint64_t> exitCounts;
};

struct VMStats {
    uint64_t interpreted = 0;
    uint64_t traceEntries = 0;
    uint64_t traceIterations = 0;
    uint64_t exits = 0;
    int tracesCompiled = 0;
    int recordAborts = 0;
    int blacklisted = 0;
};

class VirtualMachine {
public:
    VirtualMachine();
    explicit VirtualMachine(const VMConfig& cfg);
    ~VirtualMachine();

    // Review 2 compatible entry point. Verifies the program; on failure run()
    // reports a RuntimeError instead of executing invalid code.
    bool loadProgram(const std::vector<Instruction>& prog);
    bool loadProgram(const Program& prog);
    const std::vector<Diagnostic>& loadDiagnostics() const { return loadDiags_; }

    RunResult run();

    void setOutput(std::ostream* out) { out_ = out; }
    void setLog(std::ostream* log) { log_ = log; }
    int32_t reg(int i) const { return registers_[i]; }
    const int32_t* registers() const { return registers_; }
    void setReg(int i, int32_t v) { registers_[i] = v; }
    const VMStats& stats() const { return stats_; }
    const std::vector<std::unique_ptr<TraceInfo>>& traces() const { return traces_; }
    const VMConfig& config() const { return cfg_; }
    std::string backendDescription() const;
    void printStats(std::ostream& os) const;

    // Used by the trace backends' PRINT helper.
    void emitPrint(int reg, int32_t value);

private:
    void reset();
    bool logOn(int level) const { return log_ && cfg_.verbosity >= level; }
    std::string where(int pc) const;  // "pc N (line L)"
    void compileTrace(RecordedTrace raw);
    void onRecordAbort(int header, const std::string& why);
    // Executes the compiled trace at pc; returns false if the step limit was hit.
    bool enterTrace(TraceInfo& t);

    VMConfig cfg_;
    Program program_;
    std::vector<Diagnostic> loadDiags_;
    bool loaded_ = false;
    bool useNative_ = false;

    int32_t registers_[NUM_REGS];
    int pc_ = 0;
    long long steps_ = 0;

    // Working copy of the bytecode. When a trace is compiled for a loop
    // header, that instruction is patched to an internal TRACE_ENTER opcode
    // (as LuaJIT patches loops to JLOOP), so ordinary instructions pay no
    // code-cache lookup at all.
    std::vector<Instruction> code_;
    std::vector<int> loopCounters_;     // backward-branch target -> hit count
    std::vector<int> recordAttempts_;   // header -> failed recordings
    std::vector<uint8_t> blacklisted_;
    std::vector<TraceInfo*> traceAt_;   // code cache indexed by header pc
    std::vector<std::unique_ptr<TraceInfo>> traces_;
    TraceRecorder recorder_;
    std::map<int, int> unknownWarnings_;

    std::ostream* out_;
    std::ostream* log_;
    VMStats stats_;
};
