// isa.cpp -- opcode metadata table and instruction formatting.
#include "isa.h"
#include <sstream>

namespace {
using K = OperandKind;
const OpcodeInfo kInfo[OPCODE_COUNT] = {
    /* LOAD_CONST */ {"LOAD_CONST", 2, {K::Reg, K::Imm, K::None}, true, false},
    /* ADD_INT    */ {"ADD_INT", 3, {K::Reg, K::Reg, K::Reg}, true, false},
    /* JUMP_IF_LT */ {"JUMP_IF_LT", 3, {K::Reg, K::Reg, K::Target}, false, true},
    /* PRINT_REG  */ {"PRINT_REG", 1, {K::Reg, K::None, K::None}, false, false},
    /* HALT       */ {"HALT", 0, {K::None, K::None, K::None}, false, false},
    /* UNKNOWN_OP */ {"UNKNOWN_OP", 0, {K::None, K::None, K::None}, false, false},
    /* SUB_INT    */ {"SUB_INT", 3, {K::Reg, K::Reg, K::Reg}, true, false},
    /* MUL_INT    */ {"MUL_INT", 3, {K::Reg, K::Reg, K::Reg}, true, false},
    /* AND_INT    */ {"AND_INT", 3, {K::Reg, K::Reg, K::Reg}, true, false},
    /* MOV        */ {"MOV", 2, {K::Reg, K::Reg, K::None}, true, false},
    /* JUMP       */ {"JUMP", 1, {K::Target, K::None, K::None}, false, true},
    /* JUMP_IF_EQ */ {"JUMP_IF_EQ", 3, {K::Reg, K::Reg, K::Target}, false, true},
    /* DIV_INT    */ {"DIV_INT", 3, {K::Reg, K::Reg, K::Reg}, true, false},
};
const OpcodeInfo kInvalid = {"<invalid>", 0, {K::None, K::None, K::None}, false, false};
}  // namespace

bool isValidOpcode(int op) { return op >= 0 && op < OPCODE_COUNT; }

const OpcodeInfo& opcodeInfo(Opcode op) {
    return isValidOpcode(op) ? kInfo[op] : kInvalid;
}

Opcode opcodeFromName(const std::string& upperName) {
    for (int i = 0; i < OPCODE_COUNT; i++)
        if (upperName == kInfo[i].name) return (Opcode)i;
    return OPCODE_COUNT;
}

std::string formatInstruction(const Instruction& in) {
    const OpcodeInfo& info = opcodeInfo(in.op);
    std::ostringstream os;
    os << info.name;
    const int args[3] = {in.arg1, in.arg2, in.arg3};
    for (int i = 0; i < info.numOperands; i++) {
        os << (i == 0 ? " " : ", ");
        switch (info.kinds[i]) {
            case OperandKind::Reg: os << "R" << args[i]; break;
            case OperandKind::Imm: os << args[i]; break;
            case OperandKind::Target: os << "@" << args[i]; break;
            default: break;
        }
    }
    return os.str();
}
