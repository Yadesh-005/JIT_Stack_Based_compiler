#include "vm.h"
#include <iostream>

// Initialize constants in constructor
VirtualMachine::VirtualMachine() : HOT_THRESHOLD(3), MAX_TRACE_LENGTH(50) {
    for(int i = 0; i < 16; i++) registers[i] = 0;
    pc = 0;
    isRecording = false;
    traceHeaderPC = -1;
}

void VirtualMachine::loadProgram(const std::vector<Instruction>& prog) {
    program = prog;
    pc = 0;
    for(int i = 0; i < 16; i++) registers[i] = 0;
    loopCounters.clear();
    isRecording = false;
    currentTrace.clear();
}

void VirtualMachine::run() {
    bool running = true;
    while (running && pc < program.size()) {
        Instruction instr = program[pc];

        // Capture the address of the instruction before advancing pc.
        int currentPC = pc;
        pc++;

        switch (instr.op) {
            case LOAD_CONST:
                registers[instr.arg1] = instr.arg2;
                break;

            case ADD_INT:
                registers[instr.arg1] = registers[instr.arg2] + registers[instr.arg3];
                if (isRecording) {
                    currentTrace.push_back({IR_ADD_INT, instr.arg1, instr.arg2, instr.arg3});
                    if (currentTrace.size() >= MAX_TRACE_LENGTH) {
                        std::cout << "[RECORDER] Max trace length reached! Aborting trace safely.\n";
                        isRecording = false;
                    }
                }
                break;

            case JUMP_IF_LT:
                if (registers[instr.arg1] < registers[instr.arg2]) {
                    int targetPC = instr.arg3;
                    bool justStartedRecording = false;

                    if (targetPC <= currentPC) {
                        if (!(isRecording && targetPC == traceHeaderPC)) {
                            loopCounters[targetPC]++;
                        }

                        if (loopCounters[targetPC] == HOT_THRESHOLD && !isRecording) {
                            std::cout << "[PROFILER] Hot loop detected at line " << targetPC << "!\n";
                            std::cout << "[RECORDER] Starting trace recording...\n";
                            isRecording = true;
                            traceHeaderPC = targetPC;
                            currentTrace.clear();
                            justStartedRecording = true;
                        }
                    }

                    // Evaluate closure only on a backward jump that arrives on a LATER pass.
                    if (isRecording && !justStartedRecording) {
                        if (targetPC == traceHeaderPC) {
                            currentTrace.push_back({IR_GUARD_LT, instr.arg1, instr.arg2, 0});
                            currentTrace.push_back({IR_JUMP_LOOP, targetPC, 0, 0});
                            std::cout << "[RECORDER] Trace recording complete! Captured "
                                      << currentTrace.size() << " IR instructions.\n";
                            isRecording = false;
                        }
                    }
                    pc = targetPC;
                }
                break;

            case PRINT_REG:
                std::cout << "Register R" << instr.arg1 << " = " << registers[instr.arg1] << "\n";
                break;

            case HALT:
                running = false;
                break;

            default:
                if (isRecording) {
                    std::cout << "[RECORDER] Unsupported opcode detected! Aborting trace recording.\n";
                    isRecording = false;
                }
                std::cout << "[INTERPRETER] Ignored unknown opcode. Falling back to standard execution.\n";
        }
    }
}
