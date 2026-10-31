// assembler.h -- Front end: textual bytecode (.jbc) -> Program.
//
// Grammar (one statement per line, case-insensitive mnemonics):
//   line      := [label ':']* [instruction] [comment]
//   instruction := MNEMONIC [operand {',' operand}]
//   operand   := register | immediate | target
//   register  := 'R' 0..15
//   immediate := ['-'] decimal | ['-'] '0x' hex      (must fit in int32)
//   target    := label | '@' index | index
//   comment   := ';' ... | '#' ... | '//' ...
#pragma once

#include <map>
#include <string>
#include <vector>
#include "isa.h"

struct Diagnostic {
    enum Level { Error, Warning } level;
    int line;          // 1-based source line, 0 if not applicable
    int pc;            // instruction index, -1 if not applicable
    std::string message;
};

struct Program {
    std::string name;                     // file name (for diagnostics)
    std::vector<Instruction> code;
    std::vector<int> srcLine;             // srcLine[pc] = source line
    std::map<std::string, int> labels;    // label -> pc
    std::string labelAt(int pc) const;    // "" if none
};

struct AssembleResult {
    Program program;
    std::vector<Diagnostic> diagnostics;
    bool ok() const;
};

AssembleResult assemble(const std::string& source, const std::string& name = "<input>");
AssembleResult assembleFile(const std::string& path);

// Builds a Program directly from an instruction vector (used by the C++
// regression tests that were written as Instruction arrays in Review 2).
Program programFromInstructions(const std::vector<Instruction>& code, const std::string& name);

std::string disassemble(const Program& p);
std::string formatDiagnostic(const Program& p, const Diagnostic& d);
