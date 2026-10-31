// verifier.h -- static analysis of a bytecode Program before execution.
//
// Errors (program is rejected):  empty program, invalid opcode, register
//   operand outside R0-R15, jump target outside the program.
// Warnings (program still runs): UNKNOWN_OP present, unreachable code
//   (CFG reachability), control falling off the end, registers that may be
//   read before being written (forward "definitely-assigned" data-flow
//   analysis), trivial self-loops.
#pragma once

#include <vector>
#include "assembler.h"

std::vector<Diagnostic> verifyProgram(const Program& p);

// Successor pcs in the control-flow graph (a successor equal to
// code.size() means "falls off the end").
std::vector<int> cfgSuccessors(const Program& p, int pc);
