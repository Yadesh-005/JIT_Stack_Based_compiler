// trace_ir.cpp -- pretty printers for the three IR levels and SSA -> LIR lowering.
#include "trace_ir.h"

#include <sstream>

namespace {
const char* irName(IROpcode op) {
    switch (op) {
        case IR_LOAD_CONST: return "IR_LOAD_CONST";
        case IR_ADD_INT: return "IR_ADD_INT";
        case IR_GUARD_LT: return "IR_GUARD_LT";
        case IR_JUMP_LOOP: return "IR_JUMP_LOOP";
        case IR_SUB_INT: return "IR_SUB_INT";
        case IR_MUL_INT: return "IR_MUL_INT";
        case IR_AND_INT: return "IR_AND_INT";
        case IR_MOV: return "IR_MOV";
        case IR_PRINT: return "IR_PRINT";
        case IR_GUARD_GE: return "IR_GUARD_GE";
        case IR_GUARD_EQ: return "IR_GUARD_EQ";
        case IR_GUARD_NE: return "IR_GUARD_NE";
    }
    return "?";
}
const char* sName(SOp op) {
    switch (op) {
        case SOp::Const: return "Const";
        case SOp::LoadReg: return "LoadReg";
        case SOp::Phi: return "Phi";
        case SOp::Add: return "Add";
        case SOp::Sub: return "Sub";
        case SOp::Mul: return "Mul";
        case SOp::And: return "And";
        case SOp::GuardLT: return "GuardLT";
        case SOp::GuardGE: return "GuardGE";
        case SOp::GuardEQ: return "GuardEQ";
        case SOp::GuardNE: return "GuardNE";
        case SOp::Print: return "Print";
        case SOp::LoopStart: return "LoopStart";
        case SOp::FuelCheck: return "FuelCheck";
        case SOp::Jump: return "Jump";
    }
    return "?";
}
const char* lName(LOp op) {
    switch (op) {
        case LOp::LoadReg: return "load_reg";
        case LOp::Add: return "add";
        case LOp::Sub: return "sub";
        case LOp::Mul: return "mul";
        case LOp::And: return "and";
        case LOp::Copy: return "copy";
        case LOp::GuardLT: return "guard_lt";
        case LOp::GuardGE: return "guard_ge";
        case LOp::GuardEQ: return "guard_eq";
        case LOp::GuardNE: return "guard_ne";
        case LOp::Print: return "print";
        case LOp::LoopLabel: return "LOOP:";
        case LOp::FuelCheck: return "fuel_check";
        case LOp::JumpLoop: return "jump LOOP";
    }
    return "?";
}
bool isGuardS(SOp op) {
    return op == SOp::GuardLT || op == SOp::GuardGE || op == SOp::GuardEQ || op == SOp::GuardNE;
}
}  // namespace

std::string formatIROp(const IROp& o) {
    std::ostringstream os;
    os << irName(o.op);
    switch (o.op) {
        case IR_LOAD_CONST: os << " R" << o.arg1 << ", " << o.arg2; break;
        case IR_ADD_INT: case IR_SUB_INT: case IR_MUL_INT: case IR_AND_INT:
            os << " R" << o.arg1 << ", R" << o.arg2 << ", R" << o.arg3; break;
        case IR_MOV: os << " R" << o.arg1 << ", R" << o.arg2; break;
        case IR_PRINT: os << " R" << o.arg1; break;
        case IR_GUARD_LT: case IR_GUARD_GE: case IR_GUARD_EQ: case IR_GUARD_NE:
            os << " R" << o.arg1 << ", R" << o.arg2 << "  (exit -> pc " << o.arg3 << ")"; break;
        case IR_JUMP_LOOP: os << " -> pc " << o.arg1; break;
    }
    return os.str();
}

std::string formatSSA(const SSATrace& t) {
    std::ostringstream os;
    auto val = [&](int v) {
        std::ostringstream o;
        if (v < 0) o << "?";
        else if (t.insts[v].op == SOp::Const) o << "#" << t.insts[v].imm;
        else o << "v" << v;
        return o.str();
    };
    auto snap = [&](int s) {
        std::ostringstream o;
        const Snapshot& sn = t.snaps[s];
        o << "snap" << s << " -> pc " << sn.resumePC << " {";
        for (size_t i = 0; i < sn.regs.size(); i++)
            o << (i ? ", " : "") << "R" << sn.regs[i].first << "=" << val(sn.regs[i].second);
        o << "}";
        return o.str();
    };
    for (size_t i = 0; i < t.insts.size(); i++) {
        const SInst& s = t.insts[i];
        if (s.dead || s.op == SOp::Const) continue;
        if (s.op == SOp::LoopStart) { os << "  ---- loop header (pc " << t.headerPC << ") ----\n"; continue; }
        os << "  ";
        switch (s.op) {
            case SOp::LoadReg: os << val((int)i) << " = LoadReg R" << s.reg; break;
            case SOp::Phi: os << val((int)i) << " = Phi R" << s.reg << " [entry " << val(s.a) << ", back " << val(s.b) << "]"; break;
            case SOp::Add: case SOp::Sub: case SOp::Mul: case SOp::And:
                os << val((int)i) << " = " << sName(s.op) << " " << val(s.a) << ", " << val(s.b); break;
            case SOp::Print: os << "Print R" << s.reg << " = " << val(s.a); break;
            case SOp::FuelCheck: os << "FuelCheck   " << snap(s.snap); break;
            case SOp::Jump: os << "Jump -> loop header"; break;
            default:
                if (isGuardS(s.op)) os << sName(s.op) << " " << val(s.a) << ", " << val(s.b) << "   " << snap(s.snap);
                break;
        }
        os << "\n";
    }
    return os.str();
}

LIRTrace lowerToLIR(const SSATrace& t) {
    LIRTrace L;
    L.headerPC = t.headerPC;
    const int n = (int)t.insts.size();
    std::vector<int> slotOf(n, -1);
    int next = 1;  // slot 0 = fuel
    for (int i = 0; i < n; i++) {
        const SInst& s = t.insts[i];
        if (s.dead) continue;
        switch (s.op) {
            case SOp::LoadReg: case SOp::Phi: case SOp::Add: case SOp::Sub: case SOp::Mul: case SOp::And:
                slotOf[i] = next++;
                break;
            default: break;
        }
    }
    // ---- Phi coalescing (removes back-edge copies) ----
    // Like writing "i = i + 1" in place: if a loop variable's old value (the
    // Phi p) is never read after its new value b is computed, b can simply
    // reuse p's slot, and the copy "p <- b" at the end of the loop vanishes.
    // "Never read after" includes later operands, snapshot entries and the
    // back-edge copies of other Phis.
    {
        std::vector<int> backCount(n, 0);
        for (int i = 0; i < n; i++)
            if (!t.insts[i].dead && t.insts[i].op == SOp::Phi && t.insts[i].b >= 0) backCount[t.insts[i].b]++;
        for (int p = 0; p < n; p++) {
            const SInst& ph = t.insts[p];
            if (ph.dead || ph.op != SOp::Phi) continue;
            int b = ph.b;
            if (b < 0 || b == p || backCount[b] != 1) continue;
            const SInst& bd = t.insts[b];
            if (bd.dead || !bd.inLoop || bd.op == SOp::Phi || bd.op == SOp::Const || bd.op == SOp::LoadReg) continue;
            bool readLater = false;
            for (int q = 0; q < n && !readLater; q++) {
                const SInst& s = t.insts[q];
                if (s.dead) continue;
                if (s.op == SOp::Phi && s.b == p) readLater = true;  // read by another Phi's back-edge copy
                if (q <= b) continue;
                if (s.a == p || (s.op != SOp::Phi && s.b == p)) readLater = true;
                if (s.snap >= 0)
                    for (auto& rv : t.snaps[s.snap].regs)
                        if (rv.second == p) readLater = true;
            }
            if (!readLater) slotOf[b] = slotOf[p];
        }
    }
    auto opnd = [&](int v) {
        return t.insts[v].op == SOp::Const ? LOperand::Imm(t.insts[v].imm) : LOperand::Slot(slotOf[v]);
    };
    std::vector<int> exitOf(t.snaps.size(), -1);
    auto exitFor = [&](int snap) {
        if (exitOf[snap] >= 0) return exitOf[snap];
        LExit e;
        e.resumePC = t.snaps[snap].resumePC;
        e.isFuelExit = t.snaps[snap].isFuelExit;
        for (auto& rv : t.snaps[snap].regs) e.writes.push_back({rv.first, opnd(rv.second)});
        L.exits.push_back(e);
        return exitOf[snap] = (int)L.exits.size() - 1;
    };
    std::vector<int> livePhis;
    for (int i = 0; i < n; i++)
        if (!t.insts[i].dead && t.insts[i].op == SOp::Phi) livePhis.push_back(i);

    for (int i = 0; i < n; i++) {
        const SInst& s = t.insts[i];
        if (s.dead) continue;
        LInst li;
        switch (s.op) {
            case SOp::Const: case SOp::Phi: continue;
            case SOp::LoadReg:
                li.op = LOp::LoadReg; li.dst = slotOf[i]; li.reg = s.reg; break;
            case SOp::Add: case SOp::Sub: case SOp::Mul: case SOp::And:
                li.op = s.op == SOp::Add ? LOp::Add : s.op == SOp::Sub ? LOp::Sub : s.op == SOp::Mul ? LOp::Mul : LOp::And;
                li.dst = slotOf[i]; li.a = opnd(s.a); li.b = opnd(s.b); break;
            case SOp::GuardLT: case SOp::GuardGE: case SOp::GuardEQ: case SOp::GuardNE:
                li.op = s.op == SOp::GuardLT ? LOp::GuardLT : s.op == SOp::GuardGE ? LOp::GuardGE
                      : s.op == SOp::GuardEQ ? LOp::GuardEQ : LOp::GuardNE;
                li.a = opnd(s.a); li.b = opnd(s.b); li.exit = exitFor(s.snap); break;
            case SOp::Print:
                li.op = LOp::Print; li.reg = s.reg; li.a = opnd(s.a); break;
            case SOp::LoopStart:
                for (int p : livePhis) {
                    LInst c;
                    c.op = LOp::Copy; c.dst = slotOf[p]; c.a = opnd(t.insts[p].a);
                    L.code.push_back(c);
                }
                li.op = LOp::LoopLabel;
                break;
            case SOp::FuelCheck:
                li.op = LOp::FuelCheck; li.exit = exitFor(s.snap); break;
            case SOp::Jump: {
                // Parallel copy phi <- back-edge value. If a source is itself a
                // phi slot being overwritten, go through temporaries.
                bool conflict = false;
                auto needsCopy = [&](int p) {
                    int src = t.insts[p].b;
                    if (src == p) return false;           // Phi unchanged
                    LOperand o = opnd(src);
                    return o.isImm || o.slot != slotOf[p];  // coalesced -> no copy
                };
                for (int p : livePhis) {
                    int src = t.insts[p].b;
                    if (needsCopy(p) && t.insts[src].op == SOp::Phi && !t.insts[src].dead) conflict = true;
                }
                std::vector<int> tmp;
                for (int p : livePhis) {
                    if (!needsCopy(p)) continue;
                    LInst c;
                    c.op = LOp::Copy;
                    c.a = opnd(t.insts[p].b);
                    if (conflict) { c.dst = next++; tmp.push_back(c.dst); }
                    else c.dst = slotOf[p];
                    L.code.push_back(c);
                }
                if (conflict) {
                    size_t k = 0;
                    for (int p : livePhis) {
                        if (!needsCopy(p)) continue;
                        LInst c;
                        c.op = LOp::Copy; c.dst = slotOf[p]; c.a = LOperand::Slot(tmp[k++]);
                        L.code.push_back(c);
                    }
                }
                li.op = LOp::JumpLoop;
                break;
            }
        }
        L.code.push_back(li);
    }
    L.numSlots = next;
    return L;
}

std::string formatLIR(const LIRTrace& t) {
    std::ostringstream os;
    auto op = [](const LOperand& o) {
        std::ostringstream x;
        if (o.isImm) x << "#" << o.imm; else x << "s" << o.slot;
        return x.str();
    };
    for (const LInst& i : t.code) {
        if (i.op == LOp::LoopLabel) { os << "LOOP:\n"; continue; }
        os << "  " << lName(i.op);
        switch (i.op) {
            case LOp::LoadReg: os << " s" << i.dst << " <- R" << i.reg; break;
            case LOp::Add: case LOp::Sub: case LOp::Mul: case LOp::And:
                os << " s" << i.dst << " <- " << op(i.a) << ", " << op(i.b); break;
            case LOp::Copy: os << " s" << i.dst << " <- " << op(i.a); break;
            case LOp::GuardLT: case LOp::GuardGE: case LOp::GuardEQ: case LOp::GuardNE:
                os << " " << op(i.a) << ", " << op(i.b) << "  else exit" << i.exit; break;
            case LOp::Print: os << " R" << i.reg << " = " << op(i.a); break;
            case LOp::FuelCheck: os << "  else exit" << i.exit; break;
            default: break;
        }
        os << "\n";
    }
    for (size_t e = 0; e < t.exits.size(); e++) {
        const LExit& x = t.exits[e];
        os << "exit" << e << ": resume pc " << x.resumePC << (x.isFuelExit ? " (fuel)" : "") << " {";
        for (size_t k = 0; k < x.writes.size(); k++)
            os << (k ? ", " : "") << "R" << x.writes[k].first << "=" << op(x.writes[k].second);
        os << "}\n";
    }
    return os.str();
}
