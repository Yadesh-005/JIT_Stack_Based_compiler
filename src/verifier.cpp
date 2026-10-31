// verifier.cpp -- structural checks plus two classic data-flow analyses.
#include "verifier.h"

#include <cstdint>
#include <deque>
#include <sstream>

std::vector<int> cfgSuccessors(const Program& p, int pc) {
    const Instruction& in = p.code[pc];
    switch (in.op) {
        case HALT: return {};
        case JUMP: return {in.arg1};
        case JUMP_IF_LT:
        case JUMP_IF_EQ: return {pc + 1, in.arg3};
        default: return {pc + 1};
    }
}

namespace {
uint32_t usesMask(const Instruction& in) {
    const OpcodeInfo& info = opcodeInfo(in.op);
    const int args[3] = {in.arg1, in.arg2, in.arg3};
    uint32_t m = 0;
    for (int i = 0; i < info.numOperands; i++) {
        if (info.kinds[i] != OperandKind::Reg) continue;
        if (i == 0 && info.writesArg1) continue;
        m |= 1u << args[i];
    }
    return m;
}
uint32_t defsMask(const Instruction& in) {
    return opcodeInfo(in.op).writesArg1 ? (1u << in.arg1) : 0u;
}
}  // namespace

std::vector<Diagnostic> verifyProgram(const Program& p) {
    std::vector<Diagnostic> out;
    auto err = [&](int pc, const std::string& m) { out.push_back({Diagnostic::Error, 0, pc, m}); };
    auto warn = [&](int pc, const std::string& m) { out.push_back({Diagnostic::Warning, 0, pc, m}); };
    const int n = (int)p.code.size();
    if (n == 0) {
        err(-1, "program is empty");
        return out;
    }

    // ---- 1. Structural checks ----
    bool structuralOk = true;
    for (int pc = 0; pc < n; pc++) {
        const Instruction& in = p.code[pc];
        if (!isValidOpcode(in.op)) {
            std::ostringstream m;
            m << "invalid opcode value " << (int)in.op;
            err(pc, m.str());
            structuralOk = false;
            continue;
        }
        const OpcodeInfo& info = opcodeInfo(in.op);
        const int args[3] = {in.arg1, in.arg2, in.arg3};
        for (int i = 0; i < info.numOperands; i++) {
            if (info.kinds[i] == OperandKind::Reg && (args[i] < 0 || args[i] >= NUM_REGS)) {
                std::ostringstream m;
                m << info.name << ": register operand " << i + 1 << " is R" << args[i]
                  << " (valid: R0-R15)";
                err(pc, m.str());
                structuralOk = false;
            }
            if (info.kinds[i] == OperandKind::Target && (args[i] < 0 || args[i] >= n)) {
                std::ostringstream m;
                m << info.name << ": jump target " << args[i] << " is outside the program (0-"
                  << n - 1 << ")";
                err(pc, m.str());
                structuralOk = false;
            }
        }
        if (in.op == UNKNOWN_OP)
            warn(pc, "UNKNOWN_OP is not executable; the interpreter will skip it and the JIT will refuse to trace through it");
        if (in.op == JUMP && in.arg1 == pc)
            warn(pc, "JUMP to itself is an infinite loop (protected only by the step limit)");
    }
    if (!structuralOk) return out;

    // ---- 2. Reachability over the CFG ----
    std::vector<bool> reach(n, false);
    std::deque<int> work{0};
    reach[0] = true;
    bool fallsOff = false;
    while (!work.empty()) {
        int pc = work.front();
        work.pop_front();
        for (int s : cfgSuccessors(p, pc)) {
            if (s >= n) { fallsOff = true; continue; }
            if (!reach[s]) { reach[s] = true; work.push_back(s); }
        }
    }
    for (int pc = 0; pc < n;) {
        if (reach[pc]) { pc++; continue; }
        int start = pc;
        while (pc < n && !reach[pc]) pc++;
        std::ostringstream m;
        if (pc - 1 == start) m << "instruction " << start << " is unreachable";
        else m << "instructions " << start << "-" << pc - 1 << " are unreachable";
        warn(start, m.str());
    }
    if (fallsOff) warn(n - 1, "control can fall off the end of the program (treated as an implicit HALT)");
    bool hasHalt = false;
    for (const auto& in : p.code) hasHalt |= (in.op == HALT);
    if (!hasHalt) warn(-1, "program contains no HALT instruction");

    // ---- 3. Definitely-assigned analysis (forward, must, intersection) ----
    // IN[0] = {}, IN[s] = AND over predecessors of OUT[p], OUT = IN | DEF.
    std::vector<uint32_t> inSet(n, 0xFFFFu), outSet(n, 0xFFFFu);
    inSet[0] = 0;
    std::vector<std::vector<int>> preds(n);
    for (int pc = 0; pc < n; pc++)
        if (reach[pc])
            for (int s : cfgSuccessors(p, pc))
                if (s < n) preds[s].push_back(pc);
    bool changed = true;
    while (changed) {
        changed = false;
        for (int pc = 0; pc < n; pc++) {
            if (!reach[pc]) continue;
            uint32_t i = (pc == 0) ? 0u : 0xFFFFu;
            for (int q : preds[pc]) i &= outSet[q];
            if (pc == 0) i = 0;  // the entry has no definitions, whatever its preds
            uint32_t o = i | defsMask(p.code[pc]);
            if (i != inSet[pc] || o != outSet[pc]) { inSet[pc] = i; outSet[pc] = o; changed = true; }
        }
    }
    uint32_t reported = 0;
    for (int pc = 0; pc < n; pc++) {
        if (!reach[pc]) continue;
        uint32_t undef = usesMask(p.code[pc]) & ~inSet[pc] & ~reported;
        for (int r = 0; r < NUM_REGS; r++) {
            if (undef & (1u << r)) {
                std::ostringstream m;
                m << "R" << r << " may be read before it is written (registers start at 0)";
                warn(pc, m.str());
                reported |= 1u << r;
            }
        }
    }
    return out;
}
