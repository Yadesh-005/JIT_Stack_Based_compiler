// recorder.cpp -- trace recording rules.
#include "recorder.h"

#include <sstream>

void TraceRecorder::start(int headerPC, int maxLength) {
    active_ = true;
    maxLength_ = maxLength;
    bodyOps_ = 0;
    trace_ = RecordedTrace();
    trace_.headerPC = headerPC;
    reason_.clear();
}

RecordedTrace TraceRecorder::take() {
    active_ = false;
    RecordedTrace t = trace_;
    trace_ = RecordedTrace();
    return t;
}

TraceRecorder::Status TraceRecorder::abort(const std::string& why) {
    active_ = false;
    reason_ = why;
    trace_ = RecordedTrace();
    return Status::Aborted;
}

TraceRecorder::Status TraceRecorder::push(IROpcode op, int a1, int a2, int a3, int pc) {
    trace_.ops.push_back({op, a1, a2, a3, pc});
    bodyOps_++;
    if (bodyOps_ > maxLength_) {
        std::ostringstream m;
        m << "max trace length (" << maxLength_ << ") exceeded";
        return abort(m.str());
    }
    return Status::Continue;
}

TraceRecorder::Status TraceRecorder::record(const Instruction& in, int pc, bool taken) {
    if (!active_) return Status::Continue;
    const int header = trace_.headerPC;
    switch (in.op) {
        case LOAD_CONST: return push(IR_LOAD_CONST, in.arg1, in.arg2, 0, pc);
        case ADD_INT: return push(IR_ADD_INT, in.arg1, in.arg2, in.arg3, pc);
        case SUB_INT: return push(IR_SUB_INT, in.arg1, in.arg2, in.arg3, pc);
        case MUL_INT: return push(IR_MUL_INT, in.arg1, in.arg2, in.arg3, pc);
        case AND_INT: return push(IR_AND_INT, in.arg1, in.arg2, in.arg3, pc);
        case MOV: return push(IR_MOV, in.arg1, in.arg2, 0, pc);
        case PRINT_REG: return push(IR_PRINT, in.arg1, 0, 0, pc);

        case JUMP_IF_LT:
        case JUMP_IF_EQ: {
            const bool lt = in.op == JUMP_IF_LT;
            const int target = in.arg3;
            if (taken) {
                if (target <= pc && target != header)
                    return abort("backward branch to another loop (inner loop at pc " + std::to_string(target) + ")");
                // Guard that the branch is taken again; on failure resume at the fall-through.
                trace_.ops.push_back({lt ? IR_GUARD_LT : IR_GUARD_EQ, in.arg1, in.arg2, pc + 1, pc});
                if (target == header) {
                    trace_.ops.push_back({IR_JUMP_LOOP, header, 0, 0, pc});
                    active_ = false;
                    return Status::Completed;
                }
                bodyOps_++;
                if (bodyOps_ > maxLength_) return abort("max trace length (" + std::to_string(maxLength_) + ") exceeded");
                return Status::Continue;
            }
            if (target == header) return abort("loop exited while recording");
            // Guard that the branch is NOT taken; on failure resume at the target.
            trace_.ops.push_back({lt ? IR_GUARD_GE : IR_GUARD_NE, in.arg1, in.arg2, target, pc});
            bodyOps_++;
            if (bodyOps_ > maxLength_) return abort("max trace length (" + std::to_string(maxLength_) + ") exceeded");
            return Status::Continue;
        }

        case JUMP: {
            const int target = in.arg1;
            if (target == header) {
                trace_.ops.push_back({IR_JUMP_LOOP, header, 0, 0, pc});
                active_ = false;
                return Status::Completed;
            }
            if (target <= pc)
                return abort("backward jump to another loop (pc " + std::to_string(target) + ")");
            return Status::Continue;  // unconditional forward jump: straightened away
        }

        case HALT: return abort("HALT reached while recording");
        case DIV_INT: return abort("DIV_INT is not supported by the JIT (interpreter only)");
        default: return abort("unsupported opcode " + std::string(opcodeInfo(in.op).name));
    }
}
