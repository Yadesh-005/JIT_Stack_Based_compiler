// recorder.h -- the trace recorder (Review 2 module, completed for Review 3).
//
// While active, the interpreter calls record() after executing each
// instruction. The recorder appends Level-1 TraceIR and turns every
// conditional branch into a guard on the direction that was actually taken,
// remembering where execution must resume if that guard later fails.
//
// A recording ends in one of these ways:
//   Completed  control returns to the trace header (loop closed)
//   Aborted    - trace longer than MAX_TRACE_LENGTH
//              - unsupported opcode (UNKNOWN_OP, DIV_INT) or HALT
//              - the loop exits while recording (header back-edge not taken)
//              - a backward branch to a different loop (inner loop)
//
// Review 2 defects fixed here: (1) recording previously continued past the
// end of the loop when the loop exited mid-recording, which also stopped any
// later loop from ever being traced; (2) LOAD_CONST, PRINT_REG and forward
// branches inside the loop body were silently not recorded, so the trace did
// not describe the loop's real semantics; (3) the length limit was only
// checked for ADD_INT.
#pragma once

#include <string>
#include "isa.h"
#include "trace_ir.h"

class TraceRecorder {
public:
    enum class Status { Continue, Completed, Aborted };

    void start(int headerPC, int maxLength);
    bool active() const { return active_; }
    int headerPC() const { return trace_.headerPC; }
    // Called after `in` (located at `pc`) has been executed by the interpreter.
    // `taken` is the branch outcome for conditional jumps.
    Status record(const Instruction& in, int pc, bool taken);
    const std::string& abortReason() const { return reason_; }
    // Number of ops recorded so far (excluding the closing guard/jump).
    int length() const { return bodyOps_; }
    RecordedTrace take();

private:
    Status abort(const std::string& why);
    Status push(IROpcode op, int a1, int a2, int a3, int pc);
    bool active_ = false;
    int maxLength_ = 50;
    int bodyOps_ = 0;
    RecordedTrace trace_;
    std::string reason_;
};
