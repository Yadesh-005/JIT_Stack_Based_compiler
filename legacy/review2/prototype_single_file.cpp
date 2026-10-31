#include <map>
#include <iostream>
#include <vector>

// 1. Define the Opcodes
// These are the basic instructions your VM understands.
enum Opcode {
    LOAD_CONST,
    ADD_INT,
    JUMP_IF_LT,
    PRINT_REG,
    HALT,
    UNKNOWN_OP
};

// Instruction format (Fixed-width)
// Keeps parsing simple by ensuring every instruction has the same layout.
struct Instruction {
    Opcode op;
    int arg1;
    int arg2;
    int arg3;
};

// --- TRACE IR DEFINITIONS ---
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

// 2. Set up the VM State

class VirtualMachine {
private:
    int registers[16] = {0};
    int pc = 0;
    std::vector<Instruction> program;

    std::map<int, int> loopCounters; // Tracks how many times a line is the target of a jump
    const int HOT_THRESHOLD = 3;     // The limit before a loop is considered "hot"

    // --- TRACE RECORDER VARIABLES ---
    bool isRecording = false;           // Tracks if we are currently recording a trace
    int traceHeaderPC = -1;             // The line number where the loop starts
    std::vector<IROp> currentTrace;     // The actual recorded TraceIR list
    const int MAX_TRACE_LENGTH = 50;    // Safety limit to prevent infinite recording
public:
    // Loads the bytecode array into the VM's memory
    void loadProgram(const std::vector<Instruction>& prog) {
        program = prog;
        pc = 0; // Reset Program Counter to the start
        for(int i = 0; i < 16; i++) registers[i] = 0;
        loopCounters.clear();
        isRecording = false;
        currentTrace.clear();
    }

    // 3. The Fetch-Decode-Execute Loop
    void run() {
        bool running = true;

        while (running && pc < program.size()) {
            Instruction instr = program[pc];

            // FIX: capture the address of the instruction we are about to execute
            // BEFORE advancing pc. Backward-branch detection must be judged against
            // where the jump instruction itself lives, not where execution has
            // already moved to after the increment below.
            int currentPC = pc;
            pc++;

            // DECODE & EXECUTE
            switch (instr.op) {
                case LOAD_CONST:
                    // LOAD_CONST R0, 5
                    registers[instr.arg1] = instr.arg2;
                    break;

                case ADD_INT:
                    registers[instr.arg1] = registers[instr.arg2] + registers[instr.arg3];

                    // --- RECORDER LOGIC ---
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

                        // PROFILER LOGIC
                        // FIX: compare against currentPC (the jump's own address), not
                        // pc (which already points past it). This is what actually
                        // makes "backward branch" mean "target is at or before this
                        // instruction," instead of "target is before the next one."
                        if (targetPC <= currentPC) {
                            // FIX: only count this as a fresh hit when we are not
                            // already recording a trace for this same target, so the
                            // counter doesn't keep climbing past HOT_THRESHOLD while
                            // a trace for it is in progress.
                            if (!(isRecording && targetPC == traceHeaderPC)) {
                                loopCounters[targetPC]++;
                            }

                            if (loopCounters[targetPC] == HOT_THRESHOLD && !isRecording) {
                                std::cout << "[PROFILER] Hot loop detected at line " << targetPC << "!\n";
                                std::cout << "[RECORDER] Starting trace recording...\n";
                                isRecording = true;
                                traceHeaderPC = targetPC; // Remember where the loop starts
                                currentTrace.clear();
                            }
                        }

                        // --- RECORDER LOGIC FOR BRANCHES ---
                        if (isRecording) {
                            // If we jump back to the start of our trace, the loop is closed!
                            if (targetPC == traceHeaderPC) {
                                // Add a guard to verify the loop condition is still true
                                currentTrace.push_back({IR_GUARD_LT, instr.arg1, instr.arg2, 0});
                                // Add a terminator to close the loop
                                currentTrace.push_back({IR_JUMP_LOOP, targetPC, 0, 0});

                                std::cout << "[RECORDER] Trace recording complete! Captured "
                                          << currentTrace.size() << " IR instructions.\n";
                                isRecording = false; // Turn off recording
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
};

// 4. Run a Hardcoded Test Program
int main() {
    VirtualMachine vm;

    // We will build a simple loop conceptually identical to:
    // int R0 = 0;
    // int R1 = 1;
    // int R2 = 5;
    // while (R0 < R2) {
    //     R0 = R0 + R1;
    //     print(R0);
    // }

    // FIX: the previous test program never included a PRINT_REG instruction
    // (despite the comment above promising print(R0) each pass), and its
    // dummy-math instructions fed R1's growth back into R0, causing R0 to
    // blow past R2 in only 2 taken jumps -- never reaching HOT_THRESHOLD (3).
    // As a result neither PRINT_REG output nor [PROFILER]/[RECORDER] messages
    // ever appeared, regardless of the profiler logic fix above. This version
    // uses a fixed increment so the loop takes at least HOT_THRESHOLD jumps,
    // and adds PRINT_REG so R0's progress is actually visible.
    std::vector<Instruction> testProgram = {
    {LOAD_CONST, 0, 0, 0}, // Line 0: R0 = 0 (Counter)
    {LOAD_CONST, 1, 1, 0}, // Line 1: R1 = 1 (Increment)
    {LOAD_CONST, 2, 10, 0},// Line 2: R2 = 10 (Limit)

    // --- LOOP START ---
    {ADD_INT, 0, 0, 1},
    {PRINT_REG,0,0,0},   // Line 3: R0 = R0 + R1
    {JUMP_IF_LT, 0, 2, 3}, // Line 4: If R0 < R2, jump to Line 3
    // --- LOOP END ---

    {HALT, 0, 0, 0}// 6
};


    std::cout << "Starting VM Execution...\n";
    std::cout << "------------------------\n";

    vm.loadProgram(testProgram);
    vm.run();

    std::cout << "------------------------\n";
    std::cout << "VM Execution Finished.\n";

    return 0;
}
