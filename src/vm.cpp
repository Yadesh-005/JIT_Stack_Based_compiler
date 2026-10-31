// vm.cpp -- the mixed-mode VirtualMachine.
#include "vm.h"

#include <climits>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "optimizer.h"
#include "verifier.h"

namespace {
// Internal opcode written over loop headers that own a compiled trace.
constexpr Opcode OP_TRACE_ENTER = (Opcode)100;
}  // namespace

void vmPrintFromTrace(void* vm, int32_t reg, int32_t value) {
    static_cast<VirtualMachine*>(vm)->emitPrint(reg, value);
}

const char* runStatusName(RunStatus s) {
    switch (s) {
        case RunStatus::Halted: return "HALTED";
        case RunStatus::EndOfProgram: return "END_OF_PROGRAM";
        case RunStatus::RuntimeError: return "RUNTIME_ERROR";
        case RunStatus::StepLimit: return "STEP_LIMIT";
        case RunStatus::NotLoaded: return "NOT_LOADED";
    }
    return "?";
}

VirtualMachine::VirtualMachine() : VirtualMachine(VMConfig()) {}

VirtualMachine::VirtualMachine(const VMConfig& cfg) : cfg_(cfg), out_(&std::cout), log_(&std::cout) {
    if (cfg_.hotThreshold < 1) cfg_.hotThreshold = 1;
    if (cfg_.maxTraceLength < 1) cfg_.maxTraceLength = 1;
    useNative_ = cfg_.jit && cfg_.backend == VMConfig::Backend::Native && nativeBackendAvailable();
    reset();
}

VirtualMachine::~VirtualMachine() = default;

std::string VirtualMachine::backendDescription() const {
    if (!cfg_.jit) return "interpreter only (JIT disabled)";
    if (useNative_) return std::string("native ") + nativeArchName();
    if (cfg_.backend == VMConfig::Backend::Native)
        return "IR executor (native unavailable: " + nativeBackendStatus() + ")";
    return "IR executor";
}

void VirtualMachine::reset() {
    for (int i = 0; i < NUM_REGS; i++) registers_[i] = 0;
    pc_ = 0;
    steps_ = 0;
    const size_t n = program_.code.size();
    code_ = program_.code;
    loopCounters_.assign(n, 0);
    recordAttempts_.assign(n, 0);
    blacklisted_.assign(n, 0);
    traceAt_.assign(program_.code.size(), nullptr);
    traces_.clear();
    recorder_ = TraceRecorder();
    unknownWarnings_.clear();
    stats_ = VMStats();
}

bool VirtualMachine::loadProgram(const std::vector<Instruction>& prog) {
    return loadProgram(programFromInstructions(prog, "<program>"));
}

bool VirtualMachine::loadProgram(const Program& prog) {
    program_ = prog;
    loadDiags_ = verifyProgram(program_);
    loaded_ = true;
    for (const auto& d : loadDiags_)
        if (d.level == Diagnostic::Error) loaded_ = false;
    reset();
    return loaded_;
}

void VirtualMachine::emitPrint(int reg, int32_t value) {
    if (out_) *out_ << "Register R" << reg << " = " << value << "\n";
}

std::string VirtualMachine::where(int pc) const {
    std::string s = "pc " + std::to_string(pc);
    if (pc >= 0 && pc < (int)program_.srcLine.size()) s += " (line " + std::to_string(program_.srcLine[pc]) + ")";
    return s;
}

void VirtualMachine::onRecordAbort(int header, const std::string& why) {
    stats_.recordAborts++;
    loopCounters_[header] = 0;
    int attempts = ++recordAttempts_[header];
    if (logOn(1)) {
        if (why.find("max trace length") != std::string::npos)
            *log_ << "[RECORDER] Max trace length reached! Aborting trace safely.\n";
        else if (why.find("unsupported") != std::string::npos || why.find("not supported") != std::string::npos)
            *log_ << "[RECORDER] Unsupported opcode detected! Aborting trace recording. (" << why << ")\n";
        else
            *log_ << "[RECORDER] Aborting trace recording: " << why << ".\n";
    }
    if (attempts >= cfg_.maxRecordAttempts && header < (int)blacklisted_.size()) {
        blacklisted_[header] = 1;
        stats_.blacklisted++;
        if (logOn(1))
            *log_ << "[PROFILER] Loop at " << where(header) << " blacklisted after " << attempts
                  << " failed recordings; it stays in the interpreter.\n";
    }
}

void VirtualMachine::compileTrace(RecordedTrace raw) {
    std::unique_ptr<TraceInfo> t(new TraceInfo());
    t->id = (int)traces_.size();
    t->headerPC = raw.headerPC;
    t->raw = raw;
    t->ssa = buildOptimizedTrace(raw, OptOptions::level(cfg_.optLevel));
    t->lir = lowerToLIR(t->ssa);
    std::string why;
    if (useNative_) t->code = compileNativeBackend(t->lir, why);
    if (!t->code) {
        if (useNative_ && logOn(1))
            *log_ << "[BACKEND] Native compilation failed (" << why << "); using IR executor.\n";
        t->code = compileIRBackend(t->lir);
    }
    t->backendName = t->code->kind();
    t->slots.assign(t->lir.numSlots, 0);
    t->exitCounts.assign(t->lir.exits.size(), 0);
    const OptStats& s = t->ssa.stats;
    if (logOn(1)) {
        *log_ << "[RECORDER] Trace recording complete! Captured " << raw.ops.size() << " IR instructions.\n";
        *log_ << "[OPTIMIZER] Trace #" << t->id << " (-O" << cfg_.optLevel << "): " << s.recordedOps
              << " recorded ops -> " << (cfg_.optLevel >= 2 ? "preamble " + std::to_string(s.preambleOps) + " + " : "")
              << "loop body " << s.loopOps << " ops (" << s.loopGuards << " guards)"
              << "; folded " << s.folded << ", CSE " << s.cseHits << ", hoisted " << s.hoisted
              << ", guards removed " << s.guardsRemoved << ", invariant regs " << s.phisRemoved
              << ", dead " << s.dceRemoved << "\n";
        *log_ << "[BACKEND] Trace #" << t->id << " compiled with " << t->backendName;
        if (t->code->codeSize()) *log_ << " (" << t->code->codeSize() << " bytes of machine code)";
        *log_ << ", " << t->lir.exits.size() << " exits.\n";
    }
    if (cfg_.dumpIR && log_) {
        *log_ << "---- Trace #" << t->id << " (header " << where(t->headerPC) << "): recorded TraceIR (Level 1) ----\n";
        for (const IROp& o : raw.ops) *log_ << "  pc " << std::setw(3) << o.pc << ":  " << formatIROp(o) << "\n";
        *log_ << "---- Trace #" << t->id << ": optimised SSA TraceIR (Level 2) ----\n" << formatSSA(t->ssa);
        *log_ << "---- Trace #" << t->id << ": LIR (Level 3) ----\n" << formatLIR(t->lir);
    }
    if (cfg_.dumpAsm && log_) {
        std::string lst = t->code->listing();
        if (!lst.empty()) *log_ << "---- Trace #" << t->id << ": " << t->backendName << " machine code ----\n" << lst;
        else *log_ << "---- Trace #" << t->id << ": no machine code (IR executor backend) ----\n";
    }
    stats_.tracesCompiled++;
    traceAt_[t->headerPC] = t.get();
    code_[t->headerPC].op = OP_TRACE_ENTER;  // patch the loop header
    traces_.push_back(std::move(t));
}

bool VirtualMachine::enterTrace(TraceInfo& t) {
    long long budget = cfg_.maxSteps - steps_;
    if (budget <= 1) return false;
    int32_t fuel = (int32_t)(budget > INT32_MAX - 1 ? INT32_MAX - 1 : budget);
    t.slots[0] = fuel;
    TraceContext ctx{registers_, t.slots.data(), this};
    if (logOn(2) && t.entries < 5) *log_ << "[EXEC] Entering trace #" << t.id << " at " << where(t.headerPC) << "\n";
    int e = t.code->run(ctx);
    uint64_t iters = (uint64_t)(fuel - t.slots[0]);
    t.entries++;
    t.iterations += iters;
    t.exitCounts[e]++;
    stats_.traceEntries++;
    stats_.traceIterations += iters;
    stats_.exits++;
    steps_ += (long long)iters + 1;
    const LExit& ex = t.lir.exits[e];
    pc_ = ex.resumePC;
    if (logOn(2) && t.entries <= 5) {
        *log_ << "[DEOPT] Trace #" << t.id << " exit" << e << " after " << iters
              << " loop iterations -> resume interpreter at " << where(ex.resumePC);
        if (!ex.writes.empty()) {
            *log_ << " (restored";
            for (auto& w : ex.writes) *log_ << " R" << w.first << "=" << registers_[w.first];
            *log_ << ")";
        }
        *log_ << "\n";
        if (t.entries == 5) *log_ << "[DEOPT] (further entry/exit messages for trace #" << t.id << " suppressed)\n";
    }
    if (ex.isFuelExit) return false;
    return true;
}

RunResult VirtualMachine::run() {
    RunResult res;
    if (!loaded_) {
        res.status = RunStatus::RuntimeError;
        res.error = "program failed verification";
        for (const auto& d : loadDiags_)
            if (d.level == Diagnostic::Error) { res.error += ": " + d.message; break; }
        return res;
    }
    const int n = (int)program_.code.size();
    while (true) {
        if (pc_ >= n) { res.status = RunStatus::EndOfProgram; break; }
        if (steps_ >= cfg_.maxSteps) { res.status = RunStatus::StepLimit; break; }

        Instruction instr = code_[pc_];
        // ---- Patched loop header: enter the compiled trace ----
        if (instr.op == OP_TRACE_ENTER) {
            if (!recorder_.active()) {
                if (!enterTrace(*traceAt_[pc_])) { res.status = RunStatus::StepLimit; break; }
                continue;
            }
            instr = program_.code[pc_];  // recording another loop: interpret normally
        }
        const int currentPC = pc_;  // address of the instruction being executed
        pc_++;
        steps_++;
        stats_.interpreted++;
        bool taken = false;
        int32_t* R = registers_;

        switch (instr.op) {
            case LOAD_CONST: R[instr.arg1] = instr.arg2; break;
            case ADD_INT: R[instr.arg1] = wrapAdd(R[instr.arg2], R[instr.arg3]); break;
            case SUB_INT: R[instr.arg1] = wrapSub(R[instr.arg2], R[instr.arg3]); break;
            case MUL_INT: R[instr.arg1] = wrapMul(R[instr.arg2], R[instr.arg3]); break;
            case AND_INT: R[instr.arg1] = R[instr.arg2] & R[instr.arg3]; break;
            case MOV: R[instr.arg1] = R[instr.arg2]; break;
            case DIV_INT: {
                int32_t a = R[instr.arg2], b = R[instr.arg3];
                if (b == 0) {
                    res.status = RunStatus::RuntimeError;
                    res.error = "division by zero";
                    res.errorPC = currentPC;
                    if (recorder_.active()) recorder_.take();
                    return res;
                }
                R[instr.arg1] = (a == INT32_MIN && b == -1) ? INT32_MIN : a / b;
                break;
            }
            case JUMP_IF_LT:
                taken = R[instr.arg1] < R[instr.arg2];
                if (taken) pc_ = instr.arg3;
                break;
            case JUMP_IF_EQ:
                taken = R[instr.arg1] == R[instr.arg2];
                if (taken) pc_ = instr.arg3;
                break;
            case JUMP:
                taken = true;
                pc_ = instr.arg1;
                break;
            case PRINT_REG: emitPrint(instr.arg1, R[instr.arg1]); break;
            case HALT:
                if (recorder_.active()) {
                    int h = recorder_.headerPC();
                    recorder_.record(instr, currentPC, false);
                    onRecordAbort(h, recorder_.abortReason());
                }
                res.status = RunStatus::Halted;
                return res;
            case UNKNOWN_OP: {
                int& count = unknownWarnings_[currentPC];
                if (logOn(1) && count < 3)
                    *log_ << "[INTERPRETER] Ignored unknown opcode. Falling back to standard execution.\n";
                if (logOn(1) && count == 3)
                    *log_ << "[INTERPRETER] (further unknown-opcode warnings at " << where(currentPC) << " suppressed)\n";
                count++;
                break;
            }
            default:
                res.status = RunStatus::RuntimeError;
                res.error = "invalid opcode";
                res.errorPC = currentPC;
                return res;
        }

        // ---- Trace recorder ----
        bool closedNow = false;
        if (recorder_.active()) {
            int h = recorder_.headerPC();
            TraceRecorder::Status st = recorder_.record(instr, currentPC, taken);
            if (st == TraceRecorder::Status::Completed) {
                compileTrace(recorder_.take());
                closedNow = true;
            } else if (st == TraceRecorder::Status::Aborted) {
                onRecordAbort(h, recorder_.abortReason());
            }
        }

        // ---- Hot-loop profiler: count taken backward branches ----
        if (cfg_.jit && taken && !closedNow) {
            int target = (instr.op == JUMP) ? instr.arg1 : instr.arg3;
            if (target <= currentPC && !recorder_.active() && !traceAt_[target] && !blacklisted_[target]) {
                if (++loopCounters_[target] >= cfg_.hotThreshold) {
                    if (logOn(1)) {
                        *log_ << "[PROFILER] Hot loop detected at " << where(target) << "!\n";
                        *log_ << "[RECORDER] Starting trace recording...\n";
                    }
                    recorder_.start(target, cfg_.maxTraceLength);
                }
            }
        }
    }
    if (recorder_.active()) {
        int h = recorder_.headerPC();
        recorder_.take();
        onRecordAbort(h, "program ended while recording");
    }
    return res;
}

void VirtualMachine::printStats(std::ostream& os) const {
    os << "==================== VM statistics ====================\n";
    os << "  execution mode            : " << backendDescription() << ", -O" << cfg_.optLevel << "\n";
    os << "  instructions interpreted  : " << stats_.interpreted << "\n";
    os << "  traces compiled           : " << stats_.tracesCompiled << "\n";
    os << "  trace entries             : " << stats_.traceEntries << "\n";
    os << "  loop iterations in traces : " << stats_.traceIterations << "\n";
    os << "  trace exits (deopts)      : " << stats_.exits << "\n";
    os << "  recordings aborted        : " << stats_.recordAborts << "\n";
    os << "  loops blacklisted         : " << stats_.blacklisted << "\n";
    uint64_t total = stats_.interpreted + stats_.traceIterations;
    if (total)
        os << "  share of work in JIT code : " << std::fixed << std::setprecision(1)
           << 100.0 * (double)stats_.traceIterations / (double)total << "% of loop iterations+instructions\n";
    for (const auto& t : traces_) {
        os << "  trace #" << t->id << " @" << where(t->headerPC) << " [" << t->backendName
           << "]: recorded " << t->ssa.stats.recordedOps
           << " ops, loop body " << t->ssa.stats.loopOps << " ops, entries " << t->entries
           << ", iterations " << t->iterations << "\n";
        for (size_t e = 0; e < t->exitCounts.size(); e++)
            if (t->exitCounts[e])
                os << "      exit" << e << " -> pc " << t->lir.exits[e].resumePC
                   << (t->lir.exits[e].isFuelExit ? " (fuel)" : "") << " taken " << t->exitCounts[e] << "x\n";
    }
    os << "=======================================================\n";
}
