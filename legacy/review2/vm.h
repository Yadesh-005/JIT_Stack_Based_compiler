#include <vector>
#include <map>
#include "isa.h"

class VirtualMachine {
private:
    int registers[16];
    int pc;
    std::vector<Instruction> program;

    std::map<int, int> loopCounters;
    const int HOT_THRESHOLD;


    bool isRecording;
    int traceHeaderPC;
    std::vector<IROp> currentTrace;
    const int MAX_TRACE_LENGTH;

public:
    VirtualMachine();


    void loadProgram(const std::vector<Instruction>& prog);


    void run();
};
