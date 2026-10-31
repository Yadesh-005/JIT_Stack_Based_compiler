// assembler.cpp -- two-pass assembler for the textual bytecode format.
//
// Pass 1 collects label definitions and counts instructions so that forward
// references resolve. Pass 2 parses operands, checks operand counts and
// kinds, and reports *all* errors with line numbers instead of stopping at
// the first one.
#include "assembler.h"

#include <cctype>
#include <climits>
#include <fstream>
#include <sstream>

namespace {

std::string upper(std::string s) {
    for (char& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

std::string stripComment(const std::string& line) {
    size_t cut = line.size();
    for (size_t i = 0; i < line.size(); i++) {
        if (line[i] == ';' || line[i] == '#') { cut = i; break; }
        if (line[i] == '/' && i + 1 < line.size() && line[i + 1] == '/') { cut = i; break; }
    }
    return line.substr(0, cut);
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) a++;
    while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

bool isIdentStart(char c) { return std::isalpha((unsigned char)c) || c == '_' || c == '.'; }
bool isIdentChar(char c) { return std::isalnum((unsigned char)c) || c == '_' || c == '.'; }

bool isIdentifier(const std::string& s) {
    if (s.empty() || !isIdentStart(s[0])) return false;
    for (char c : s)
        if (!isIdentChar(c)) return false;
    return true;
}

// Parses a (possibly negative, possibly hex) integer that must fit in int32.
bool parseInt(const std::string& tok, long long& out, std::string& err) {
    if (tok.empty()) { err = "empty number"; return false; }
    size_t i = 0;
    bool neg = false;
    if (tok[0] == '-' || tok[0] == '+') { neg = tok[0] == '-'; i = 1; }
    if (i >= tok.size()) { err = "malformed number '" + tok + "'"; return false; }
    int base = 10;
    if (i + 1 < tok.size() && tok[i] == '0' && (tok[i + 1] == 'x' || tok[i + 1] == 'X')) { base = 16; i += 2; }
    if (i >= tok.size()) { err = "malformed number '" + tok + "'"; return false; }
    long long v = 0;
    for (; i < tok.size(); i++) {
        int d;
        char c = (char)std::tolower((unsigned char)tok[i]);
        if (c >= '0' && c <= '9') d = c - '0';
        else if (base == 16 && c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else { err = "malformed number '" + tok + "'"; return false; }
        v = v * base + d;
        if (v > 4294967296LL) { err = "number '" + tok + "' does not fit in 32 bits"; return false; }
    }
    if (neg) v = -v;
    if (v < INT32_MIN || v > INT32_MAX) {
        // Allow unsigned 32-bit hex constants such as 0xFFFFFFFF (= -1).
        if (base == 16 && !neg && v <= 0xFFFFFFFFLL) v = (int32_t)(uint32_t)v;
        else { err = "number '" + tok + "' does not fit in int32"; return false; }
    }
    out = v;
    return true;
}

struct Stmt {
    int line;
    std::string mnemonic;
    std::vector<std::string> operands;
};

}  // namespace

bool AssembleResult::ok() const {
    for (const auto& d : diagnostics)
        if (d.level == Diagnostic::Error) return false;
    return true;
}

std::string Program::labelAt(int pc) const {
    for (const auto& kv : labels)
        if (kv.second == pc) return kv.first;
    return "";
}

AssembleResult assemble(const std::string& source, const std::string& name) {
    AssembleResult res;
    res.program.name = name;
    auto error = [&](int line, const std::string& msg) {
        res.diagnostics.push_back({Diagnostic::Error, line, -1, msg});
    };

    // ---- Pass 1: tokenize, collect labels ----
    std::vector<Stmt> stmts;
    std::istringstream in(source);
    std::string raw;
    int lineNo = 0;
    while (std::getline(in, raw)) {
        lineNo++;
        std::string text = trim(stripComment(raw));
        // Labels: one or more "ident:" prefixes.
        while (true) {
            size_t colon = text.find(':');
            if (colon == std::string::npos) break;
            std::string label = trim(text.substr(0, colon));
            if (!isIdentifier(label)) {
                error(lineNo, "invalid label name '" + label + "'");
                text = trim(text.substr(colon + 1));
                continue;
            }
            std::string key = upper(label);
            if (res.program.labels.count(key))
                error(lineNo, "duplicate label '" + label + "'");
            else if (key.size() >= 2 && key[0] == 'R' &&
                     std::isdigit((unsigned char)key[1]))
                error(lineNo, "label '" + label + "' looks like a register name");
            else
                res.program.labels[key] = (int)stmts.size();
            text = trim(text.substr(colon + 1));
        }
        if (text.empty()) continue;
        Stmt s;
        s.line = lineNo;
        // Split on whitespace and commas.
        std::vector<std::string> toks;
        std::string cur;
        for (char c : text) {
            if (std::isspace((unsigned char)c) || c == ',') {
                if (!cur.empty()) toks.push_back(cur), cur.clear();
            } else {
                cur += c;
            }
        }
        if (!cur.empty()) toks.push_back(cur);
        s.mnemonic = upper(toks[0]);
        s.operands.assign(toks.begin() + 1, toks.end());
        stmts.push_back(s);
    }

    // ---- Pass 2: parse instructions ----
    for (const Stmt& s : stmts) {
        Instruction ins{UNKNOWN_OP, 0, 0, 0};
        Opcode op = opcodeFromName(s.mnemonic);
        bool bad = false;
        if (op == OPCODE_COUNT) {
            error(s.line, "unknown mnemonic '" + s.mnemonic + "'");
            bad = true;
        } else {
            const OpcodeInfo& info = opcodeInfo(op);
            ins.op = op;
            if ((int)s.operands.size() != info.numOperands) {
                std::ostringstream m;
                m << s.mnemonic << " expects " << info.numOperands << " operand"
                  << (info.numOperands == 1 ? "" : "s") << ", got " << s.operands.size();
                error(s.line, m.str());
                bad = true;
            } else {
                int* args[3] = {&ins.arg1, &ins.arg2, &ins.arg3};
                for (int i = 0; i < info.numOperands; i++) {
                    const std::string& tok = s.operands[i];
                    switch (info.kinds[i]) {
                        case OperandKind::Reg: {
                            std::string u = upper(tok);
                            long long r = -1;
                            bool digits = u.size() >= 2 && u.size() <= 4 && u[0] == 'R';
                            for (size_t k = 1; digits && k < u.size(); k++)
                                if (!std::isdigit((unsigned char)u[k])) digits = false;
                            if (digits) r = std::stoll(u.substr(1));
                            if (digits && r >= 0 && r < NUM_REGS) {
                                *args[i] = (int)r;
                            } else {
                                error(s.line, "invalid register '" + tok + "' (valid: R0-R15)");
                                bad = true;
                            }
                            break;
                        }
                        case OperandKind::Imm: {
                            long long v;
                            std::string err;
                            if (parseInt(tok, v, err)) *args[i] = (int)v;
                            else { error(s.line, err); bad = true; }
                            break;
                        }
                        case OperandKind::Target: {
                            std::string t = tok;
                            long long v;
                            std::string err;
                            if (!t.empty() && t[0] == '@') t = t.substr(1);
                            if (!t.empty() && (std::isdigit((unsigned char)t[0]) || t[0] == '-')) {
                                if (parseInt(t, v, err)) *args[i] = (int)v;
                                else { error(s.line, err); bad = true; }
                            } else {
                                auto it = res.program.labels.find(upper(t));
                                if (it == res.program.labels.end()) {
                                    error(s.line, "undefined label '" + tok + "'");
                                    bad = true;
                                } else {
                                    *args[i] = it->second;
                                }
                            }
                            break;
                        }
                        default: break;
                    }
                }
            }
        }
        (void)bad;
        res.program.code.push_back(ins);
        res.program.srcLine.push_back(s.line);
    }
    return res;
}

AssembleResult assembleFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        AssembleResult r;
        r.program.name = path;
        r.diagnostics.push_back({Diagnostic::Error, 0, -1, "cannot open file '" + path + "'"});
        return r;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return assemble(ss.str(), path);
}

Program programFromInstructions(const std::vector<Instruction>& code, const std::string& name) {
    Program p;
    p.name = name;
    p.code = code;
    for (size_t i = 0; i < code.size(); i++) p.srcLine.push_back((int)i + 1);
    return p;
}

std::string disassemble(const Program& p) {
    std::ostringstream os;
    for (size_t pc = 0; pc < p.code.size(); pc++) {
        std::string label = p.labelAt((int)pc);
        if (!label.empty()) os << label << ":\n";
        std::string text = "      " + formatInstruction(p.code[pc]);
        if (text.size() < 34) text.resize(34, ' ');
        os << text << " ; pc " << pc << "\n";
    }
    return os.str();
}

std::string formatDiagnostic(const Program& p, const Diagnostic& d) {
    std::ostringstream os;
    os << p.name;
    if (d.line > 0) os << ":" << d.line;
    else if (d.pc >= 0 && d.pc < (int)p.srcLine.size()) os << ":" << p.srcLine[d.pc];
    os << ": " << (d.level == Diagnostic::Error ? "error" : "warning") << ": " << d.message;
    if (d.pc >= 0) os << " [pc " << d.pc << "]";
    return os.str();
}
