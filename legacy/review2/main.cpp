#include <iostream>
#include <vector>
#include <string>
#include "vm.h"

void runCase(const std::string& label, std::vector<Instruction> prog) {
    std::cout << "===== " << label << " =====\n";
    VirtualMachine vm;
    vm.loadProgram(prog);
    vm.run();
    std::cout << "\n";
}

int main() {
    // 1. VALID / STABLE
    std::vector<Instruction> validStableLoop = {
        {LOAD_CONST, 0, 0, 0},
        {LOAD_CONST, 1, 1, 0},
        {LOAD_CONST, 2, 10, 0},
        {ADD_INT, 0, 0, 1},
        {JUMP_IF_LT, 0, 2, 3},
        {HALT, 0, 0, 0}
    };

    // 2. BELOW-THRESHOLD
    std::vector<Instruction> belowThresholdLoop = {
        {LOAD_CONST, 0, 0, 0},
        {LOAD_CONST, 1, 1, 0},
        {LOAD_CONST, 2, 3, 0},
        {ADD_INT, 0, 0, 1},
        {JUMP_IF_LT, 0, 2, 3},
        {HALT, 0, 0, 0}
    };

    // 3. INVALID / UNSUPPORTED OPCODE
    std::vector<Instruction> invalidInstructionLoop = {
        {LOAD_CONST, 0, 0, 0},
        {LOAD_CONST, 1, 1, 0},
        {LOAD_CONST, 2, 20, 0},
        {ADD_INT, 0, 0, 1},
        {UNKNOWN_OP, 0, 0, 0},
        {JUMP_IF_LT, 0, 2, 3},
        {HALT, 0, 0, 0}
    };

    // 4. TRACE LENGTH LIMIT
    std::vector<Instruction> traceLimitLoop;
    traceLimitLoop.push_back({LOAD_CONST, 0, 0, 0});
    traceLimitLoop.push_back({LOAD_CONST, 1, 1, 0});
    traceLimitLoop.push_back({LOAD_CONST, 2, 200, 0});
    int loopHeaderIdx = (int)traceLimitLoop.size();
    traceLimitLoop.push_back({ADD_INT, 0, 0, 1});
    for (int i = 0; i < 55; i++) {
        traceLimitLoop.push_back({ADD_INT, 3, 3, 3});
    }
    traceLimitLoop.push_back({JUMP_IF_LT, 0, 2, loopHeaderIdx});
    traceLimitLoop.push_back({HALT, 0, 0, 0});

    // Execute Tests
    runCase("1. VALID: validStableLoop", validStableLoop);
    runCase("2. BELOW-THRESHOLD: belowThresholdLoop", belowThresholdLoop);
    runCase("3. INVALID: invalidInstructionLoop", invalidInstructionLoop);
    runCase("4. TRACE-LIMIT: traceLimitLoop", traceLimitLoop);

    return 0;
}
