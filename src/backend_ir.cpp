// backend_ir.cpp -- portable back end: executes the LIR directly.
//
// This backend needs no machine-code generation, so it works on every CPU.
// It is used automatically when the native backend is unavailable (or with
// --backend=ir). It is faster than the bytecode interpreter because the
// trace is already optimised and the loop never touches the VM register
// file, but slower than native code because every LIR op is still dispatched.
#include "backend.h"

#include "isa.h"

namespace {

// Compilation for this backend is one simple step: every operand becomes an
// index into one array S. Slots keep their index; each constant gets an extra
// slot (filled once, here), so an operation is always S[dst] = S[a] op S[b]
// with no "is it a constant?" test at run time.
struct XOp {
    LOp op;
    int dst, a, b;  // indices into S
    int reg;        // VM register (LoadReg / Print)
    int exit;       // exit index (guards / FuelCheck)
};

class IRCompiledTrace : public CompiledTrace {
public:
    explicit IRCompiledTrace(const LIRTrace& l) : L(l), S(l.numSlots, 0) {
        auto index = [this](const LOperand& o) {
            if (!o.isImm) return o.slot;
            S.push_back(o.imm);  // constant slot
            return (int)S.size() - 1;
        };
        for (const LInst& in : L.code) {
            XOp x{in.op, in.dst, 0, 0, in.reg, in.exit};
            if (in.op != LOp::LoadReg && in.op != LOp::LoopLabel && in.op != LOp::FuelCheck && in.op != LOp::JumpLoop) {
                x.a = index(in.a);
                if (in.op != LOp::Copy && in.op != LOp::Print) x.b = index(in.b);
            }
            if (in.op == LOp::LoopLabel) loopIdx = code.size();
            code.push_back(x);
        }
        for (auto& e : L.exits)
            for (auto& w : e.writes) exitSrc.push_back({w.first, index(w.second)});
        // exitSrc is laid out exit by exit; remember where each exit starts.
        size_t k = 0;
        for (auto& e : L.exits) { exitStart.push_back(k); k += e.writes.size(); }
        exitStart.push_back(k);
    }
    const char* kind() const override { return "IR executor"; }

    int run(TraceContext& c) override {
        int32_t* s = S.data();
        const int32_t* r = c.regs;
        s[0] = c.slots[0];  // fuel
        size_t i = 0;
        for (;;) {
            const XOp& x = code[i];
            switch (x.op) {
                case LOp::LoadReg: s[x.dst] = r[x.reg]; break;
                case LOp::Copy:    s[x.dst] = s[x.a]; break;
                case LOp::Add:     s[x.dst] = wrapAdd(s[x.a], s[x.b]); break;
                case LOp::Sub:     s[x.dst] = wrapSub(s[x.a], s[x.b]); break;
                case LOp::Mul:     s[x.dst] = wrapMul(s[x.a], s[x.b]); break;
                case LOp::And:     s[x.dst] = s[x.a] & s[x.b]; break;
                // A guard that fails leaves the trace through its exit.
                case LOp::GuardLT: if (!(s[x.a] <  s[x.b])) return takeExit(c, x.exit); break;
                case LOp::GuardGE: if (!(s[x.a] >= s[x.b])) return takeExit(c, x.exit); break;
                case LOp::GuardEQ: if (!(s[x.a] == s[x.b])) return takeExit(c, x.exit); break;
                case LOp::GuardNE: if (!(s[x.a] != s[x.b])) return takeExit(c, x.exit); break;
                case LOp::Print:   jitPrintHelper(&c, x.reg, s[x.a]); break;
                case LOp::LoopLabel: break;
                case LOp::FuelCheck: if (--s[0] == 0) return takeExit(c, x.exit); break;
                case LOp::JumpLoop:  i = loopIdx; continue;   // back to the loop header
            }
            i++;
        }
    }

private:
    // Deoptimisation: write the exit's snapshot back into the VM registers.
    int takeExit(TraceContext& c, int e) {
        for (size_t k = exitStart[e]; k < exitStart[e + 1]; k++) c.regs[exitSrc[k].first] = S[exitSrc[k].second];
        c.slots[0] = S[0];  // remaining fuel, so the VM can count iterations
        return e;
    }
    LIRTrace L;
    std::vector<int32_t> S;                    // slots followed by constants
    std::vector<XOp> code;
    std::vector<std::pair<int, int>> exitSrc;  // (VM register, index into S)
    std::vector<size_t> exitStart;
    size_t loopIdx = 0;
};

}  // namespace

std::unique_ptr<CompiledTrace> compileIRBackend(const LIRTrace& lir) {
    return std::unique_ptr<CompiledTrace>(new IRCompiledTrace(lir));
}
