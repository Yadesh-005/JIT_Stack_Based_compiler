// isa.h -- Bytecode instruction set of the custom register VM.
//
// BCSE307 Compiler Design, Project A2 (Team 6)
// Trace-Based JIT Compiler for a Custom Bytecode Virtual Machine
//
// The first six opcodes are kept in exactly the order used in Review 2 so
// that the Review 2 regression programs keep their meaning. Review 3 adds the
// opcodes needed for realistic loops (SUB/MUL/AND/MOV, unconditional and
// equality jumps) and one interpreter-only opcode (DIV_INT) that the JIT
// deliberately does not support, so that the "not JIT-able" fallback path can
// be demonstrated with a legitimate instruction.
#pragma once

#include <cstdint>
#include <string>

constexpr int NUM_REGS = 16;

enum Opcode : int {
    LOAD_CONST = 0,  // LOAD_CONST rd, imm          rd = imm
    ADD_INT,         // ADD_INT    rd, ra, rb       rd = ra + rb   (32-bit wrap)
    JUMP_IF_LT,      // JUMP_IF_LT ra, rb, target   if ra < rb: pc = target (signed)
    PRINT_REG,       // PRINT_REG  ra               prints "Register Ra = <value>"
    HALT,            // HALT                        stop execution
    UNKNOWN_OP,      // UNKNOWN_OP                  sentinel: skipped by the interpreter
    // ---- added in Review 3 ----
    SUB_INT,         // SUB_INT    rd, ra, rb       rd = ra - rb   (32-bit wrap)
    MUL_INT,         // MUL_INT    rd, ra, rb       rd = ra * rb   (32-bit wrap)
    AND_INT,         // AND_INT    rd, ra, rb       rd = ra & rb
    MOV,             // MOV        rd, ra           rd = ra
    JUMP,            // JUMP       target           pc = target
    JUMP_IF_EQ,      // JUMP_IF_EQ ra, rb, target   if ra == rb: pc = target
    DIV_INT,         // DIV_INT    rd, ra, rb       rd = ra / rb (runtime error if rb == 0)
    OPCODE_COUNT
};

// Fixed-width instruction format (unchanged from Review 2).
struct Instruction {
    Opcode op;
    int arg1;
    int arg2;
    int arg3;
};

// Operand kinds, used by the assembler, the verifier and the disassembler.
enum class OperandKind { None, Reg, Imm, Target };

struct OpcodeInfo {
    const char* name;
    int numOperands;
    OperandKind kinds[3];
    bool writesArg1;   // arg1 is a destination register
    bool isBranch;     // arg (last) is a jump target
};

const OpcodeInfo& opcodeInfo(Opcode op);
bool isValidOpcode(int op);
// Returns OPCODE_COUNT if the mnemonic is unknown.
Opcode opcodeFromName(const std::string& upperName);
std::string formatInstruction(const Instruction& in);

// Wrapping 32-bit arithmetic shared by the interpreter, the constant folder
// and (implicitly) the native backends, so all three agree bit-for-bit.
inline int32_t wrapAdd(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
inline int32_t wrapSub(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }
inline int32_t wrapMul(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }
