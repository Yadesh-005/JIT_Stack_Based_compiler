// optimizer.cpp -- SSA construction, forward optimisation, loop peeling, DCE.
#include "optimizer.h"

#include <map>
#include <set>
#include <tuple>

#include "isa.h"

namespace {

bool isGuard(SOp op) {
    return op == SOp::GuardLT || op == SOp::GuardGE || op == SOp::GuardEQ || op == SOp::GuardNE;
}

class Builder {
public:
    Builder(const OptOptions& o, SSATrace& t) : opt(o), tr(t) {
        for (int& e : env) e = -1;
    }

    const OptOptions& opt;
    SSATrace& tr;
    int env[NUM_REGS];
    bool inLoop = false;
    int curPC = -1;

    std::map<int32_t, int> consts;
    std::map<int, int> loadRegs;
    std::map<std::tuple<int, int, int>, int> cse;      // (op, a, b) -> value
    std::map<std::tuple<int, int, int>, bool> guards;  // (op, a, b) -> defined in loop?

    int emit(SInst s) {
        s.pc = curPC;
        s.inLoop = inLoop;
        tr.insts.push_back(s);
        return (int)tr.insts.size() - 1;
    }
    bool isConst(int v) const { return tr.insts[v].op == SOp::Const; }
    int32_t cval(int v) const { return tr.insts[v].imm; }

    int constant(int32_t v) {
        auto it = consts.find(v);
        if (it != consts.end()) return it->second;
        SInst s{SOp::Const};
        s.imm = v;
        bool saved = inLoop;
        inLoop = false;  // constants are not code; never "in" the loop
        int id = emit(s);
        inLoop = saved;
        consts[v] = id;
        return id;
    }
    int loadReg(int r) {
        auto it = loadRegs.find(r);
        if (it != loadRegs.end()) return it->second;
        SInst s{SOp::LoadReg};
        s.reg = r;
        int id = emit(s);
        loadRegs[r] = id;
        return id;
    }
    int get(int r) {
        if (env[r] < 0) env[r] = loadReg(r);
        return env[r];
    }

    int arith(SOp op, int a, int b) {
        if (opt.fold) {
            if (isConst(a) && isConst(b)) {
                int32_t x = cval(a), y = cval(b), r = 0;
                switch (op) {
                    case SOp::Add: r = wrapAdd(x, y); break;
                    case SOp::Sub: r = wrapSub(x, y); break;
                    case SOp::Mul: r = wrapMul(x, y); break;
                    case SOp::And: r = x & y; break;
                    default: break;
                }
                tr.stats.folded++;
                return constant(r);
            }
            auto c = [&](int v, int32_t k) { return isConst(v) && cval(v) == k; };
            int simplified = -1;
            switch (op) {
                case SOp::Add:
                    if (c(a, 0)) simplified = b;
                    else if (c(b, 0)) simplified = a;
                    break;
                case SOp::Sub:
                    if (c(b, 0)) simplified = a;
                    else if (a == b) simplified = constant(0);
                    break;
                case SOp::Mul:
                    if (c(a, 0) || c(b, 0)) simplified = constant(0);
                    else if (c(a, 1)) simplified = b;
                    else if (c(b, 1)) simplified = a;
                    break;
                case SOp::And:
                    if (c(a, 0) || c(b, 0)) simplified = constant(0);
                    else if (a == b) simplified = a;
                    else if (c(a, -1)) simplified = b;
                    else if (c(b, -1)) simplified = a;
                    break;
                default: break;
            }
            if (simplified >= 0) { tr.stats.folded++; return simplified; }
        }
        bool commutative = (op == SOp::Add || op == SOp::Mul || op == SOp::And);
        if (opt.cse) {
            if (commutative && a > b) std::swap(a, b);
            auto key = std::make_tuple((int)op, a, b);
            auto it = cse.find(key);
            if (it != cse.end()) {
                tr.stats.cseHits++;
                if (inLoop && !tr.insts[it->second].inLoop) tr.stats.hoisted++;
                return it->second;
            }
            SInst s{op};
            s.a = a;
            s.b = b;
            int id = emit(s);
            cse[key] = id;
            return id;
        }
        SInst s{op};
        s.a = a;
        s.b = b;
        return emit(s);
    }

    int makeSnapshot(int resumePC, bool fuel) {
        Snapshot sn;
        sn.resumePC = resumePC;
        sn.isFuelExit = fuel;
        for (int r = 0; r < NUM_REGS; r++) {
            int v = env[r];
            if (v < 0) continue;
            const SInst& d = tr.insts[v];
            if (d.op == SOp::LoadReg && d.reg == r) continue;  // register unchanged
            sn.regs.push_back({r, v});
        }
        tr.snaps.push_back(sn);
        return (int)tr.snaps.size() - 1;
    }

    void guard(SOp op, int a, int b, int exitPC) {
        if (opt.fold && isConst(a) && isConst(b)) {
            int32_t x = cval(a), y = cval(b);
            bool holds = op == SOp::GuardLT ? x < y : op == SOp::GuardGE ? x >= y
                       : op == SOp::GuardEQ ? x == y : x != y;
            if (holds) { tr.stats.guardsRemoved++; return; }
        }
        auto key = std::make_tuple((int)op, a, b);
        if (opt.guardElim) {
            auto it = guards.find(key);
            if (it != guards.end()) {
                tr.stats.guardsRemoved++;
                if (inLoop && !it->second) tr.stats.hoisted++;
                return;
            }
        }
        SInst s{op};
        s.a = a;
        s.b = b;
        s.snap = makeSnapshot(exitPC, false);
        emit(s);
        guards[key] = inLoop;
    }

    void replay(const RecordedTrace& raw) {
        for (const IROp& o : raw.ops) {
            curPC = o.pc;
            switch (o.op) {
                case IR_LOAD_CONST: env[o.arg1] = constant(o.arg2); break;
                case IR_ADD_INT: env[o.arg1] = arith(SOp::Add, get(o.arg2), get(o.arg3)); break;
                case IR_SUB_INT: env[o.arg1] = arith(SOp::Sub, get(o.arg2), get(o.arg3)); break;
                case IR_MUL_INT: env[o.arg1] = arith(SOp::Mul, get(o.arg2), get(o.arg3)); break;
                case IR_AND_INT: env[o.arg1] = arith(SOp::And, get(o.arg2), get(o.arg3)); break;
                case IR_MOV: env[o.arg1] = get(o.arg2); break;
                case IR_PRINT: {
                    SInst s{SOp::Print};
                    s.a = get(o.arg1);
                    s.reg = o.arg1;
                    emit(s);
                    break;
                }
                case IR_GUARD_LT: { int a = get(o.arg1), b = get(o.arg2); guard(SOp::GuardLT, a, b, o.arg3); break; }
                case IR_GUARD_GE: { int a = get(o.arg1), b = get(o.arg2); guard(SOp::GuardGE, a, b, o.arg3); break; }
                case IR_GUARD_EQ: { int a = get(o.arg1), b = get(o.arg2); guard(SOp::GuardEQ, a, b, o.arg3); break; }
                case IR_GUARD_NE: { int a = get(o.arg1), b = get(o.arg2); guard(SOp::GuardNE, a, b, o.arg3); break; }
                case IR_JUMP_LOOP: break;
            }
        }
        curPC = -1;
    }
};

// Registers read / written anywhere in the trace.
void scanRegs(const RecordedTrace& raw, bool read[NUM_REGS], bool written[NUM_REGS]) {
    for (int r = 0; r < NUM_REGS; r++) read[r] = written[r] = false;
    for (const IROp& o : raw.ops) {
        switch (o.op) {
            case IR_LOAD_CONST: written[o.arg1] = true; break;
            case IR_ADD_INT: case IR_SUB_INT: case IR_MUL_INT: case IR_AND_INT:
                read[o.arg2] = read[o.arg3] = true; written[o.arg1] = true; break;
            case IR_MOV: read[o.arg2] = true; written[o.arg1] = true; break;
            case IR_PRINT: read[o.arg1] = true; break;
            case IR_GUARD_LT: case IR_GUARD_GE: case IR_GUARD_EQ: case IR_GUARD_NE:
                read[o.arg1] = read[o.arg2] = true; break;
            case IR_JUMP_LOOP: break;
        }
    }
}

// One complete construction of the SSA trace for a given set of registers
// that are known to be loop-invariant (no Phi needed).
SSATrace buildOnce(const RecordedTrace& raw, const OptOptions& opt, const std::set<int>& invariant) {
    SSATrace tr;
    tr.headerPC = raw.headerPC;
    for (const IROp& o : raw.ops)
        if (o.op != IR_JUMP_LOOP) tr.stats.recordedOps++;
    Builder B(opt, tr);
    bool read[NUM_REGS], written[NUM_REGS];
    scanRegs(raw, read, written);

    // Entry block: load every register the trace reads (DCE drops unused ones).
    for (int r = 0; r < NUM_REGS; r++)
        if (read[r] || written[r]) B.get(r);

    // Preamble: one peeled iteration (only with loop peeling).
    if (opt.peel) B.replay(raw);
    int preEnd[NUM_REGS];
    for (int r = 0; r < NUM_REGS; r++) preEnd[r] = B.env[r];

    // Loop header.
    {
        SInst ls{SOp::LoopStart};
        B.curPC = raw.headerPC;
        tr.loopStart = B.emit(ls);
    }
    B.inLoop = true;
    std::vector<int> phis;
    for (int r = 0; r < NUM_REGS; r++) {
        if (!written[r]) continue;
        if (invariant.count(r)) { B.env[r] = preEnd[r]; continue; }
        SInst p{SOp::Phi};
        p.reg = r;
        p.a = preEnd[r];
        int id = B.emit(p);
        B.env[r] = id;
        phis.push_back(id);
    }
    {
        SInst f{SOp::FuelCheck};
        f.snap = B.makeSnapshot(raw.headerPC, true);
        B.emit(f);
    }
    B.replay(raw);
    for (int id : phis) tr.insts[id].b = B.env[tr.insts[id].reg];
    SInst j{SOp::Jump};
    B.emit(j);
    return tr;
}

void deadCodeElimination(SSATrace& tr, bool enabled) {
    const int n = (int)tr.insts.size();
    std::vector<bool> live(n, false);
    std::vector<int> work;
    auto mark = [&](int v) {
        if (v >= 0 && !live[v]) { live[v] = true; work.push_back(v); }
    };
    for (int i = 0; i < n; i++) {
        SOp op = tr.insts[i].op;
        if (!enabled || isGuard(op) || op == SOp::Print || op == SOp::LoopStart ||
            op == SOp::FuelCheck || op == SOp::Jump)
            mark(i);
    }
    while (!work.empty()) {
        int v = work.back();
        work.pop_back();
        const SInst& s = tr.insts[v];
        mark(s.a);
        mark(s.b);
        if (s.snap >= 0)
            for (auto& rv : tr.snaps[s.snap].regs) mark(rv.second);
    }
    for (int i = 0; i < n; i++) {
        if (live[i]) continue;
        tr.insts[i].dead = true;
        if (tr.insts[i].op != SOp::Const) tr.stats.dceRemoved++;
    }
}

}  // namespace

SSATrace buildOptimizedTrace(const RecordedTrace& raw, const OptOptions& opt) {
    std::set<int> invariant;
    SSATrace tr;
    for (int round = 1; round <= NUM_REGS + 1; round++) {
        tr = buildOnce(raw, opt, invariant);
        tr.stats.fixpointRounds = round;
        if (!opt.fold && !opt.cse) break;  // -O0: no phi simplification
        bool changed = false;
        for (const SInst& s : tr.insts) {
            if (s.op != SOp::Phi) continue;
            int self = (int)(&s - &tr.insts[0]);
            if (s.b == self || s.b == s.a) {
                invariant.insert(s.reg);
                changed = true;
            }
        }
        if (!changed) break;
    }
    tr.stats.phisRemoved = (int)invariant.size();
    for (const SInst& s : tr.insts)
        if (s.op != SOp::Const && s.op != SOp::LoopStart && s.op != SOp::Jump) tr.stats.ssaBefore++;
    deadCodeElimination(tr, opt.dce);
    for (const SInst& s : tr.insts) {
        if (s.dead || s.op == SOp::Const || s.op == SOp::LoopStart || s.op == SOp::Jump) continue;
        if (s.inLoop) {
            if (s.op == SOp::Phi || s.op == SOp::FuelCheck) continue;
            tr.stats.loopOps++;
            if (isGuard(s.op)) tr.stats.loopGuards++;
        } else if (s.op != SOp::LoadReg) {
            tr.stats.preambleOps++;
        }
    }
    return tr;
}
