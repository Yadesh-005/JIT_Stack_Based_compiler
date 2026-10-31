// trace_ir.h -- the three intermediate representations used by the JIT.
//
//   Level 1  Recorded TraceIR (register form)   produced by the TraceRecorder
//            -- the Review 2 IR, extended with guards for every branch
//               direction, MOV, PRINT and exit PCs for every guard.
//   Level 2  SSA TraceIR                         produced/optimised by optimizer.cpp
//            -- every value defined once; loop-carried registers become Phi
//               nodes; every guard owns a Snapshot used for deoptimisation.
//   Level 3  LIR (slot form)                     produced by lowerToLIR()
//            -- flat, backend-neutral instruction list over "slots" (a
//               spill area in memory) consumed by both the portable IR
//               executor and the native x86-64 / AArch64 code generators.
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------- Level 1
// The first four enumerators keep their Review 2 names and order.
enum IROpcode {
    IR_LOAD_CONST,  // arg1 = rd, arg2 = imm
    IR_ADD_INT,     // arg1 = rd, arg2 = ra, arg3 = rb
    IR_GUARD_LT,    // guard (ra <  rb); arg1 = ra, arg2 = rb, arg3 = exit pc
    IR_JUMP_LOOP,   // closes the loop;   arg1 = header pc
    IR_SUB_INT,
    IR_MUL_INT,
    IR_AND_INT,
    IR_MOV,         // arg1 = rd, arg2 = ra
    IR_PRINT,       // arg1 = ra
    IR_GUARD_GE,    // guard (ra >= rb)
    IR_GUARD_EQ,    // guard (ra == rb)
    IR_GUARD_NE,    // guard (ra != rb)
};

struct IROp {
    IROpcode op;
    int arg1;
    int arg2;
    int arg3;
    int pc;  // bytecode pc this op was recorded from
};

struct RecordedTrace {
    int headerPC = -1;
    std::vector<IROp> ops;  // ends with IR_JUMP_LOOP
};

std::string formatIROp(const IROp& op);

// ---------------------------------------------------------------- Level 2
enum class SOp : uint8_t {
    Const, LoadReg, Phi,
    Add, Sub, Mul, And,
    GuardLT, GuardGE, GuardEQ, GuardNE,
    Print,
    LoopStart, FuelCheck, Jump,
};

struct SInst {
    SOp op;
    int a = -1, b = -1;   // operand value ids (Phi: a = entry value, b = back-edge value)
    int32_t imm = 0;      // Const
    int reg = -1;         // LoadReg / Phi / Print: VM register number
    int snap = -1;        // guards and FuelCheck: snapshot index
    int pc = -1;          // originating bytecode pc
    bool dead = false;
    bool inLoop = false;  // true for instructions inside the loop body
};

// A snapshot records, for one guard, which VM registers must be rewritten
// (and with which SSA values) before the interpreter can resume at resumePC.
struct Snapshot {
    int resumePC = -1;
    bool isFuelExit = false;
    std::vector<std::pair<int, int>> regs;  // (vm register, value id)
};

struct OptStats {
    int recordedOps = 0;      // Level-1 ops (excluding JUMP_LOOP)
    int ssaBefore = 0;        // SSA ops emitted before DCE (excl. markers/consts)
    int preambleOps = 0;      // live ops in the preamble (peeled iteration)
    int loopOps = 0;          // live ops executed every iteration
    int loopGuards = 0;       // live guards executed every iteration
    int folded = 0;           // constant-folded / algebraically simplified ops
    int cseHits = 0;          // ops replaced by an existing equal value
    int hoisted = 0;          // loop ops replaced by a preamble value (LICM via peeling)
    int guardsRemoved = 0;    // redundant or constant-true guards removed
    int phisRemoved = 0;      // loop-carried registers proven invariant
    int dceRemoved = 0;       // dead ops removed
    int fixpointRounds = 0;
};

struct OptOptions {
    bool fold = true;
    bool cse = true;
    bool guardElim = true;
    bool dce = true;
    bool peel = true;  // loop peeling (Ardo, Bolz & Fijalkowski, DLS 2012)
    static OptOptions level(int o) {
        OptOptions x;
        if (o <= 0) { x.fold = x.cse = x.guardElim = x.dce = x.peel = false; }
        else if (o == 1) { x.peel = false; }
        return x;
    }
};

struct SSATrace {
    int headerPC = -1;
    std::vector<SInst> insts;
    std::vector<Snapshot> snaps;
    int loopStart = -1;  // index of the LoopStart marker
    OptStats stats;
};

std::string formatSSA(const SSATrace& t);

// ---------------------------------------------------------------- Level 3
struct LOperand {
    bool isImm = false;
    int32_t imm = 0;
    int slot = -1;
    static LOperand Imm(int32_t v) { LOperand o; o.isImm = true; o.imm = v; return o; }
    static LOperand Slot(int s) { LOperand o; o.slot = s; return o; }
};

enum class LOp : uint8_t {
    LoadReg,    // slot[dst] = regs[reg]
    Add, Sub, Mul, And,  // slot[dst] = A op B
    Copy,       // slot[dst] = A
    GuardLT, GuardGE, GuardEQ, GuardNE,  // if !(A cond B) -> exit
    Print,      // print(reg, A)
    LoopLabel,  // loop header label
    FuelCheck,  // if (--slot[0] == 0) -> exit
    JumpLoop,   // goto LoopLabel
};

struct LInst {
    LOp op;
    int dst = -1;
    LOperand a, b;
    int reg = -1;
    int exit = -1;
};

struct LExit {
    int resumePC = -1;
    bool isFuelExit = false;
    std::vector<std::pair<int, LOperand>> writes;  // regs[reg] = operand
};

struct LIRTrace {
    int headerPC = -1;
    std::vector<LInst> code;
    std::vector<LExit> exits;
    int numSlots = 1;  // slot 0 = fuel counter
};

LIRTrace lowerToLIR(const SSATrace& t);
std::string formatLIR(const LIRTrace& t);

// Runtime context shared by both backends (layout is fixed: the native code
// reads regs at offset 0 and slots at offset 8).
struct TraceContext {
    int32_t* regs;
    int32_t* slots;
    void* vm;  // opaque pointer handed back to the print helper
};
static_assert(sizeof(void*) == 8, "64-bit host required");
