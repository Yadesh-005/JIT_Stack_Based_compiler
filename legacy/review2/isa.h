#pragma once

enum Opcode { LOAD_CONST, ADD_INT, JUMP_IF_LT, PRINT_REG, HALT, UNKNOWN_OP };


struct Instruction {
    Opcode op;
    int arg1;
    int arg2;
    int arg3;
};


enum IROpcode {
    IR_LOAD_CONST,
    IR_ADD_INT,
    IR_GUARD_LT,   // A safety check: ensures Register A is still < Register B
    IR_JUMP_LOOP   // Indicates the end of the recorded loop
};

struct IROp {
    IROpcode op;
    int arg1;
    int arg2;
    int arg3;
};
