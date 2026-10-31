// backend.h -- code generation back ends for optimised traces.
//
//   IR backend      portable: executes the LIR directly (works everywhere).
//   Native backend  emits real machine code (x86-64 or AArch64) into
//                   executable memory and calls it through a function pointer.
//                   Selected automatically when the host is supported and a
//                   start-up self-test (run in a forked child process, so a
//                   crash cannot take down the VM) succeeds.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "trace_ir.h"

class CompiledTrace {
public:
    virtual ~CompiledTrace() = default;
    // Executes the trace. ctx.slots must point at numSlots() int32 values,
    // slot 0 holding the remaining loop-iteration budget (fuel). Returns the
    // index of the exit that was taken; the exit has already written its
    // registers back into ctx.regs.
    virtual int run(TraceContext& ctx) = 0;
    virtual const char* kind() const = 0;
    virtual size_t codeSize() const { return 0; }
    virtual std::string listing() const { return ""; }
};

std::unique_ptr<CompiledTrace> compileIRBackend(const LIRTrace& lir);
// Returns nullptr (and sets why) if native code cannot be produced.
std::unique_ptr<CompiledTrace> compileNativeBackend(const LIRTrace& lir, std::string& why);

const char* nativeArchName();     // "x86-64", "AArch64" or "unsupported"
bool nativeBackendAvailable();    // arch supported, not disabled, self-test passed
std::string nativeBackendStatus();  // human-readable reason

// Called by both backends for IR PRINT; forwards to the VM's output stream.
extern "C" void jitPrintHelper(TraceContext* ctx, int32_t reg, int32_t value);
