// backend_native.cpp -- native machine-code generation for x86-64 and AArch64.
//
// Code model (both architectures):
//   * Callee-saved registers hold the slot base (spill area for SSA values),
//     the VM register-file base, the context and the fuel counter.
//   * A linear-scan register allocator (allocateRegisters) assigns machine
//     registers to slots; slots that do not get one live in memory.
//     Constants become immediates.
//   * Each LIR instruction is translated by a fixed template (one small
//     function per instruction kind); the listing shown by --dump-asm is
//     produced by the same code, so it always matches the executed bytes.
//
// How to read the encoders: every instruction is a fixed bit pattern from
// the Intel SDM / Arm ARM with register numbers and offsets OR-ed into
// fields. Example (AArch64): ADD w9, w9, w10 = 0x0B000000 | (10<<16) | (9<<5) | 9.
//   * Every guard is a compare + conditional branch to an out-of-line exit
//     stub. The stub writes the snapshot's registers back into the VM
//     register file (deoptimisation) and returns the exit index.
//   * Executable memory is W^X: written while RW, then flipped to RX
//     (Linux / Intel macOS), or allocated with MAP_JIT and toggled with
//     pthread_jit_write_protect_np on Apple Silicon.
#include "backend.h"

#include <algorithm>
#include <cstdio>
#include <initializer_list>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <sstream>

#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <libkern/OSCacheControl.h>
#include <pthread.h>
#endif

#include "isa.h"

#if defined(__x86_64__) || defined(_M_X64)
#define JIT_ARCH_X64 1
#elif defined(__aarch64__) || defined(__arm64__)
#define JIT_ARCH_A64 1
#endif

namespace {

// ------------------------------------------------------------ exec memory
class ExecMemory {
public:
    ~ExecMemory() {
        if (mem_) munmap(mem_, size_);
    }
    bool init(const std::vector<uint8_t>& code, std::string& why) {
        long page = sysconf(_SC_PAGESIZE);
        size_ = ((code.size() + page - 1) / page) * page;
        if (size_ == 0) size_ = page;
#if defined(__APPLE__) && defined(JIT_ARCH_A64)
        void* p = mmap(nullptr, size_, PROT_READ | PROT_WRITE | PROT_EXEC,
                       MAP_PRIVATE | MAP_ANON | MAP_JIT, -1, 0);
        if (p == MAP_FAILED) { why = "mmap(MAP_JIT) failed"; return false; }
        pthread_jit_write_protect_np(0);
        std::memcpy(p, code.data(), code.size());
        pthread_jit_write_protect_np(1);
        sys_icache_invalidate(p, code.size());
#else
        void* p = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        if (p == MAP_FAILED) { why = "mmap failed"; return false; }
        std::memcpy(p, code.data(), code.size());
        if (mprotect(p, size_, PROT_READ | PROT_EXEC) != 0) {
            munmap(p, size_);
            why = "mprotect(PROT_EXEC) failed";
            return false;
        }
        __builtin___clear_cache((char*)p, (char*)p + code.size());
#endif
        mem_ = p;
        return true;
    }
    void* ptr() const { return mem_; }

private:
    void* mem_ = nullptr;
    size_t size_ = 0;
};

// ------------------------------------------------------------ emitter base
struct Emitter {
    std::vector<uint8_t> buf;
    std::vector<std::pair<size_t, std::string>> asmText;  // (offset, mnemonic)
    size_t mark = 0;
    void begin() { mark = buf.size(); }
    void note(const std::string& s) { asmText.push_back({mark, s}); }
    void u8(uint8_t b) { buf.push_back(b); }
    void u32(uint32_t v) { for (int i = 0; i < 4; i++) buf.push_back((uint8_t)(v >> (8 * i))); }
    void u64(uint64_t v) { for (int i = 0; i < 8; i++) buf.push_back((uint8_t)(v >> (8 * i))); }
    void patch32(size_t at, uint32_t v) { for (int i = 0; i < 4; i++) buf[at + i] = (uint8_t)(v >> (8 * i)); }
    uint32_t read32(size_t at) const {
        return (uint32_t)buf[at] | (uint32_t)buf[at + 1] << 8 | (uint32_t)buf[at + 2] << 16 | (uint32_t)buf[at + 3] << 24;
    }
    std::string listing() const {
        std::ostringstream os;
        for (size_t k = 0; k < asmText.size(); k++) {
            size_t start = asmText[k].first;
            size_t end = (k + 1 < asmText.size()) ? asmText[k + 1].first : buf.size();
            os << "  " << std::hex << std::setw(4) << std::setfill('0') << start << ":  ";
            std::ostringstream hex;
            for (size_t b = start; b < end && b < start + 12; b++)
                hex << std::setw(2) << std::setfill('0') << std::hex << (int)buf[b] << " ";
            if (end - start > 12) hex << "..";
            os << std::left << std::setw(38) << std::setfill(' ') << hex.str() << std::right
               << std::dec << asmText[k].second << "\n";
        }
        return os.str();
    }
};

// ------------------------------------------------------------ register allocation
// Linear-scan register allocation (Poletto & Sarkar, TOPLAS 1999) over the
// linear LIR of one trace.
//   * Live interval of a slot = [first def/use, last def/use], where the
//     snapshot writes of an exit count as uses at the guard that owns it.
//   * Loop awareness: a slot that is live on entry to the loop (defined
//     before LOOP and used at or after it -- Phi slots and loop invariants)
//     must stay live for the whole loop, so its interval is extended to the
//     back-edge.
//   * Intervals are visited in start order; registers of expired intervals
//     are reused; when none is free, the interval that ends furthest away is
//     spilled to its memory slot (classic heuristic).
// Returns, for every slot, an index into the architecture's register pool or -1.
std::vector<int> allocateRegisters(const LIRTrace& L, int K) {
    const int n = (int)L.code.size();
    std::vector<int> start(L.numSlots, -1), end(L.numSlots, -1);
    auto touch = [&](int slot, int pos) {
        if (slot <= 0) return;  // slot 0 = fuel, handled separately
        if (start[slot] < 0) start[slot] = pos;
        end[slot] = pos;
    };
    int loopStart = -1, loopEnd = -1;
    for (int i = 0; i < n; i++) {
        const LInst& in = L.code[i];
        if (in.op == LOp::LoopLabel) loopStart = i;
        if (in.op == LOp::JumpLoop) loopEnd = i;
        if (!in.a.isImm) touch(in.a.slot, i);
        if (!in.b.isImm) touch(in.b.slot, i);
        touch(in.dst, i);
        if (in.exit >= 0)
            for (const auto& w : L.exits[in.exit].writes)
                if (!w.second.isImm) touch(w.second.slot, i);
    }
    if (loopStart >= 0 && loopEnd >= 0)
        for (int s = 1; s < L.numSlots; s++)
            if (start[s] >= 0 && start[s] < loopStart && end[s] >= loopStart) end[s] = loopEnd;

    std::vector<int> order;
    for (int s = 1; s < L.numSlots; s++) if (start[s] >= 0) order.push_back(s);
    std::stable_sort(order.begin(), order.end(), [&](int x, int y) { return start[x] < start[y]; });
    std::vector<int> regOf(L.numSlots, -1);
    std::vector<int> active;  // slots currently holding a register
    std::vector<bool> freeReg(K, true);
    for (int s : order) {
        // Expire intervals that ended at or before this start (an operand
        // that dies here may share its register with this instruction's result).
        for (size_t k = 0; k < active.size();) {
            if (end[active[k]] <= start[s]) { freeReg[regOf[active[k]]] = true; active.erase(active.begin() + k); }
            else k++;
        }
        int r = -1;
        for (int i = 0; i < K; i++) if (freeReg[i]) { r = i; break; }
        if (r >= 0) {
            freeReg[r] = false;
            regOf[s] = r;
            active.push_back(s);
            continue;
        }
        // Spill the active interval that ends last (if it outlives this one).
        size_t victim = 0;
        for (size_t k = 1; k < active.size(); k++) if (end[active[k]] > end[active[victim]]) victim = k;
        if (!active.empty() && end[active[victim]] > end[s]) {
            int v = active[victim];
            regOf[s] = regOf[v];
            regOf[v] = -1;
            active[victim] = s;
        }
    }
    return regOf;
}

#if defined(JIT_ARCH_X64)
// ============================================================ x86-64
// rbx = slots base, r12 = VM register file, r15d = fuel counter,
// [rsp] = TraceContext*. Allocatable pool (10): ebp, r13d, r14d, r8d-r11d,
// ecx, esi, edi (the caller-saved ones are saved around helper calls).
// Scratch: eax, edx.
struct X64 : Emitter {
    enum { EAX = 0, ECX = 1, EDX = 2, EBX = 3, ESP = 4, EBP = 5, ESI = 6, EDI = 7, R12 = 12, R15 = 15 };
    const int cacheRegs[10] = {5, 13, 14, 8, 9, 10, 11, 1, 6, 7};
    std::vector<int> regOf;  // slot -> machine register or -1

    static std::string r32(int r) {
        static const char* n[16] = {"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
                                    "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d"};
        return n[r];
    }
    int cached(const LOperand& o) const { return (!o.isImm && o.slot >= 0) ? regOf[o.slot] : -1; }
    int cachedSlot(int slot) const { return slot >= 0 ? regOf[slot] : -1; }
    static std::string mem(int slot) { return "[rbx+" + std::to_string(slot * 4) + "]"; }

    void rex(int reg, int rm) {
        uint8_t r = (uint8_t)(0x40 | ((reg >> 3) << 2) | (rm >> 3));
        if (r != 0x40) u8(r);
    }
    // op r/m32(register rm), r32(reg)
    void opRR(std::initializer_list<uint8_t> op, int reg, int rm) {
        rex(reg, rm);
        for (uint8_t b : op) u8(b);
        u8((uint8_t)(0xC0 | ((reg & 7) << 3) | (rm & 7)));
    }
    // op with [base + disp32] memory operand
    void opRM(std::initializer_list<uint8_t> op, int reg, int base, int32_t disp) {
        rex(reg, base);
        for (uint8_t b : op) u8(b);
        u8((uint8_t)(0x80 | ((reg & 7) << 3) | (base & 7)));
        if ((base & 7) == 4) u8(0x24);
        u32((uint32_t)disp);
    }
    void movImm(int r, int32_t v) {
        begin();
        rex(0, r);
        u8((uint8_t)(0xB8 + (r & 7)));
        u32((uint32_t)v);
        note("mov " + r32(r) + ", " + std::to_string(v));
    }
    // Loads an operand into machine register r.
    void load(int r, const LOperand& o) {
        int c = cached(o);
        if (o.isImm) movImm(r, o.imm);
        else if (c >= 0) { begin(); opRR({0x89}, c, r); note("mov " + r32(r) + ", " + r32(c) + "   ; s" + std::to_string(o.slot)); }
        else { begin(); opRM({0x8B}, r, EBX, o.slot * 4); note("mov " + r32(r) + ", " + mem(o.slot) + "   ; s" + std::to_string(o.slot)); }
    }
    // Stores eax into a slot (register or memory).
    void storeEax(int slot) {
        int c = cachedSlot(slot);
        begin();
        if (c >= 0) { opRR({0x89}, EAX, c); note("mov " + r32(c) + ", eax   ; s" + std::to_string(slot)); }
        else { opRM({0x89}, EAX, EBX, slot * 4); note("mov " + mem(slot) + ", eax   ; s" + std::to_string(slot)); }
    }
    size_t jcc(uint8_t cc, const char* name, const std::string& target) {
        begin(); u8(0x0F); u8(cc);
        size_t at = buf.size();
        u32(0);
        note(std::string(name) + " " + target);
        return at;
    }
    size_t jmp(const std::string& target) {
        begin(); u8(0xE9);
        size_t at = buf.size();
        u32(0);
        note("jmp " + target);
        return at;
    }
    void bindRel32(size_t at, size_t target) { patch32(at, (uint32_t)((int64_t)target - (int64_t)(at + 4))); }
    void raw(std::initializer_list<uint8_t> bytes, const std::string& text) {
        begin();
        for (uint8_t b : bytes) u8(b);
        note(text);
    }

    // r = r op b   (b: immediate, cached register or memory slot)
    void applyOp(int r, LOp op, const LOperand& b) {
        const char* nm = op == LOp::Add ? "add" : op == LOp::Sub ? "sub" : op == LOp::Mul ? "imul" : "and";
        begin();
        int cb = cached(b);
        if (b.isImm) {
            if (op == LOp::Mul) { rex(r, r); u8(0x69); u8((uint8_t)(0xC0 | ((r & 7) << 3) | (r & 7))); }
            else { rex(0, r); u8(0x81); u8((uint8_t)(0xC0 | ((op == LOp::Add ? 0 : op == LOp::Sub ? 5 : 4) << 3) | (r & 7))); }
            u32((uint32_t)b.imm);
            note(std::string(nm) + " " + r32(r) + ", " + (op == LOp::Mul ? r32(r) + ", " : "") + std::to_string(b.imm));
        } else if (cb >= 0) {
            if (op == LOp::Mul) opRR({0x0F, 0xAF}, r, cb);
            else opRR({(uint8_t)(op == LOp::Add ? 0x01 : op == LOp::Sub ? 0x29 : 0x21)}, cb, r);
            note(std::string(nm) + " " + r32(r) + ", " + r32(cb) + "   ; s" + std::to_string(b.slot));
        } else {
            if (op == LOp::Mul) opRM({0x0F, 0xAF}, r, EBX, b.slot * 4);
            else opRM({(uint8_t)(op == LOp::Add ? 0x03 : op == LOp::Sub ? 0x2B : 0x23)}, r, EBX, b.slot * 4);
            note(std::string(nm) + " " + r32(r) + ", " + mem(b.slot) + "   ; s" + std::to_string(b.slot));
        }
    }
    // dst = a op b, computed directly in dst's register when it has one.
    void binary(const LInst& in) {
        LOperand a = in.a, b = in.b;
        int rd = cachedSlot(in.dst);
        if (rd >= 0 && cached(b) == rd) {
            if (in.op != LOp::Sub) std::swap(a, b);  // commutative: rd already holds b
            else rd = -1;                             // rd = a - rd: go through eax
        }
        if (rd >= 0) {
            if (cached(a) != rd) load(rd, a);
            applyOp(rd, in.op, b);
        } else {
            load(EAX, a);
            applyOp(EAX, in.op, b);
            storeEax(in.dst);
        }
    }
    void compare(const LOperand& a, const LOperand& b) {
        int ra = cached(a);
        if (ra < 0) { load(EAX, a); ra = EAX; }
        int cb = cached(b);
        begin();
        if (b.isImm) {
            if (ra == EAX) u8(0x3D);
            else { rex(0, ra); u8(0x81); u8((uint8_t)(0xF8 | (ra & 7))); }
            u32((uint32_t)b.imm);
            note("cmp " + r32(ra) + ", " + std::to_string(b.imm));
        } else if (cb >= 0) { opRR({0x39}, cb, ra); note("cmp " + r32(ra) + ", " + r32(cb)); }
        else { opRM({0x3B}, ra, EBX, b.slot * 4); note("cmp " + r32(ra) + ", " + mem(b.slot)); }
    }

    bool compile(const LIRTrace& L, std::string& why) {
        (void)why;
        regOf = allocateRegisters(L, 10);
        for (int& r : regOf) if (r >= 0) r = cacheRegs[r];
        raw({0x53}, "push rbx");
        raw({0x55}, "push rbp");
        raw({0x41, 0x54}, "push r12");
        raw({0x41, 0x55}, "push r13");
        raw({0x41, 0x56}, "push r14");
        raw({0x41, 0x57}, "push r15");
        raw({0x48, 0x83, 0xEC, 0x08}, "sub rsp, 8             ; keep 16-byte alignment");
        raw({0x48, 0x89, 0x3C, 0x24}, "mov [rsp], rdi         ; TraceContext*");
        raw({0x4C, 0x8B, 0x27}, "mov r12, [rdi]         ; VM registers");
        raw({0x48, 0x8B, 0x5F, 0x08}, "mov rbx, [rdi+8]       ; slots");
        raw({0x44, 0x8B, 0x3B}, "mov r15d, [rbx]        ; fuel");
        size_t loopLabel = 0;
        std::vector<std::pair<size_t, int>> exitFixups;
        for (size_t k = 0; k < L.code.size(); k++) {
            const LInst& in = L.code[k];
            // The last guard directly before the back-edge can be fused with it.
            const bool fuseBackEdge = k + 1 < L.code.size() && L.code[k + 1].op == LOp::JumpLoop;
            switch (in.op) {
                case LOp::LoadReg: {
                    int c = cachedSlot(in.dst);
                    if (c >= 0) { begin(); opRM({0x8B}, c, R12, in.reg * 4); note("mov " + r32(c) + ", [r12+" + std::to_string(in.reg * 4) + "]   ; R" + std::to_string(in.reg)); }
                    else {
                        begin(); opRM({0x8B}, EAX, R12, in.reg * 4); note("mov eax, [r12+" + std::to_string(in.reg * 4) + "]   ; R" + std::to_string(in.reg));
                        storeEax(in.dst);
                    }
                    break;
                }
                case LOp::Copy: {
                    int c = cachedSlot(in.dst);
                    if (c >= 0) load(c, in.a);
                    else { load(EAX, in.a); storeEax(in.dst); }
                    break;
                }
                case LOp::Add: case LOp::Sub: case LOp::Mul: case LOp::And:
                    binary(in);
                    break;
                case LOp::GuardLT: case LOp::GuardGE: case LOp::GuardEQ: case LOp::GuardNE: {
                    compare(in.a, in.b);
                    // Condition codes for "condition holds" and "condition fails".
                    uint8_t ccTrue, ccFail; const char *nTrue, *nFail;
                    switch (in.op) {
                        case LOp::GuardLT: ccTrue = 0x8C; nTrue = "jl"; ccFail = 0x8D; nFail = "jge"; break;
                        case LOp::GuardGE: ccTrue = 0x8D; nTrue = "jge"; ccFail = 0x8C; nFail = "jl"; break;
                        case LOp::GuardEQ: ccTrue = 0x84; nTrue = "je"; ccFail = 0x85; nFail = "jne"; break;
                        default: ccTrue = 0x85; nTrue = "jne"; ccFail = 0x84; nFail = "je"; break;
                    }
                    if (fuseBackEdge) {
                        // Loop inversion: "if (cond) goto LOOP; goto exit" -- one branch per iteration.
                        bindRel32(jcc(ccTrue, nTrue, "LOOP"), loopLabel);
                        exitFixups.push_back({jmp("exit" + std::to_string(in.exit)), in.exit});
                        k++;  // the JumpLoop has been emitted
                    } else {
                        exitFixups.push_back({jcc(ccFail, nFail, "exit" + std::to_string(in.exit)), in.exit});
                    }
                    break;
                }
                case LOp::Print: {
                    load(EDX, in.a);
                    raw({0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53, 0x51, 0x56, 0x57, 0x50},
                        "push r8..r11, rcx, rsi, rdi, rax  ; caller-saved (8 pushes keep alignment)");
                    begin(); u8(0xBE); u32((uint32_t)in.reg); note("mov esi, " + std::to_string(in.reg));
                    raw({0x48, 0x8B, 0x7C, 0x24, 0x40}, "mov rdi, [rsp+64]      ; TraceContext*");
                    begin(); u8(0x48); u8(0xB8); u64((uint64_t)(uintptr_t)&jitPrintHelper); note("mov rax, jitPrintHelper");
                    raw({0xFF, 0xD0}, "call rax");
                    raw({0x58, 0x5F, 0x5E, 0x59, 0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58},
                        "pop rax, rdi, rsi, rcx, r11..r8");
                    break;
                }
                case LOp::LoopLabel:
                    loopLabel = buf.size();
                    asmText.push_back({buf.size(), "LOOP:"});
                    break;
                case LOp::FuelCheck:
                    raw({0x41, 0xFF, 0xCF}, "dec r15d               ; fuel");
                    exitFixups.push_back({jcc(0x84, "je", "exit" + std::to_string(in.exit)), in.exit});
                    break;
                case LOp::JumpLoop:
                    bindRel32(jmp("LOOP"), loopLabel);
                    break;
            }
        }
        // Exit stubs: deoptimisation = write snapshot registers back, save fuel, return index.
        std::vector<size_t> exitPos(L.exits.size());
        std::vector<size_t> toEpilogue;
        for (size_t e = 0; e < L.exits.size(); e++) {
            exitPos[e] = buf.size();
            asmText.push_back({buf.size(), "exit" + std::to_string(e) + ":                 ; resume pc " +
                                               std::to_string(L.exits[e].resumePC)});
            for (const auto& w : L.exits[e].writes) {
                load(EAX, w.second);
                begin(); opRM({0x89}, EAX, R12, w.first * 4); note("mov [r12+" + std::to_string(w.first * 4) + "], eax   ; R" + std::to_string(w.first));
            }
            raw({0x44, 0x89, 0x3B}, "mov [rbx], r15d        ; save fuel");
            begin(); u8(0xB8); u32((uint32_t)e); note("mov eax, " + std::to_string(e) + "             ; exit index");
            toEpilogue.push_back(jmp("epilogue"));
        }
        size_t epi = buf.size();
        asmText.push_back({buf.size(), "epilogue:"});
        raw({0x48, 0x83, 0xC4, 0x08}, "add rsp, 8");
        raw({0x41, 0x5F}, "pop r15");
        raw({0x41, 0x5E}, "pop r14");
        raw({0x41, 0x5D}, "pop r13");
        raw({0x41, 0x5C}, "pop r12");
        raw({0x5D}, "pop rbp");
        raw({0x5B}, "pop rbx");
        raw({0xC3}, "ret");
        for (auto& f : exitFixups) bindRel32(f.first, exitPos[f.second]);
        for (size_t at : toEpilogue) bindRel32(at, epi);
        return true;
    }
};
using NativeEmitter = X64;
#endif

#if defined(JIT_ARCH_A64)
// ============================================================ AArch64
// x19 = slots base, x20 = VM register file, x21 = TraceContext*,
// w22 = fuel counter. Allocatable pool (11): w23-w28 (callee-saved) and
// w11-w15 (caller-saved, saved around helper calls). Scratch: w9, w10.
// Register 31 = wzr for zero.
struct A64 : Emitter {
    std::vector<int> regOf;
    void ins(uint32_t w, const std::string& text) { begin(); u32(w); note(text); }
    static std::string w(int r) { return r == 31 ? "wzr" : "w" + std::to_string(r); }
    static uint32_t pairOp(uint32_t base, int imm7, int rt2, int rn, int rt) {
        return base | (((uint32_t)imm7 & 0x7Fu) << 15) | ((uint32_t)rt2 << 10) | ((uint32_t)rn << 5) | (uint32_t)rt;
    }
    int cachedSlot(int slot) const { return slot >= 0 ? regOf[slot] : -1; }

    void movImm(int rd, int32_t v) {
        uint32_t u = (uint32_t)v;
        ins(0x52800000u | ((u & 0xFFFFu) << 5) | rd, "movz " + w(rd) + ", #" + std::to_string(u & 0xFFFFu));
        if (u >> 16)
            ins(0x72A00000u | ((u >> 16) << 5) | rd, "movk " + w(rd) + ", #" + std::to_string(u >> 16) + ", lsl #16");
    }
    void movReg(int rd, int rm) { ins(0x2A0003E0u | ((uint32_t)rm << 16) | rd, "mov " + w(rd) + ", " + w(rm)); }
    void ldrW(int rt, int base, int index, const std::string& comment) {
        ins(0xB9400000u | ((uint32_t)index << 10) | (base << 5) | rt,
            "ldr " + w(rt) + ", [x" + std::to_string(base) + ", #" + std::to_string(index * 4) + "]   ; " + comment);
    }
    void strW(int rt, int base, int index, const std::string& comment) {
        ins(0xB9000000u | ((uint32_t)index << 10) | (base << 5) | rt,
            "str " + w(rt) + ", [x" + std::to_string(base) + ", #" + std::to_string(index * 4) + "]   ; " + comment);
    }
    // Returns a register holding the operand (materialising into `scratch` if needed).
    int use(const LOperand& o, int scratch) {
        if (o.isImm) {
            if (o.imm == 0) return 31;  // wzr
            movImm(scratch, o.imm);
            return scratch;
        }
        int c = cachedSlot(o.slot);
        if (c >= 0) return c;
        ldrW(scratch, 19, o.slot, "s" + std::to_string(o.slot));
        return scratch;
    }
    // Moves an operand into a specific register.
    void loadInto(int rd, const LOperand& o) {
        if (o.isImm) { movImm(rd, o.imm); return; }
        int c = cachedSlot(o.slot);
        if (c >= 0) { if (c != rd) movReg(rd, c); return; }
        ldrW(rd, 19, o.slot, "s" + std::to_string(o.slot));
    }
    size_t bcond(int cond, const char* name, const std::string& target) {
        size_t at = buf.size();
        ins(0x54000000u | (uint32_t)cond, std::string("b.") + name + " " + target);
        return at;
    }
    size_t b(const std::string& target) {
        size_t at = buf.size();
        ins(0x14000000u, "b " + target);
        return at;
    }
    void bindBcond(size_t at, size_t target) {
        int32_t imm19 = (int32_t)(((int64_t)target - (int64_t)at) / 4);
        patch32(at, (read32(at) & 0xFF00001Fu) | (((uint32_t)imm19 & 0x7FFFFu) << 5));
    }
    void bindB(size_t at, size_t target) {
        int32_t imm26 = (int32_t)(((int64_t)target - (int64_t)at) / 4);
        patch32(at, 0x14000000u | ((uint32_t)imm26 & 0x3FFFFFFu));
    }

    bool compile(const LIRTrace& L, std::string& why) {
        if (L.numSlots > 4000) { why = "trace needs more than 4000 slots"; return false; }
        static const int pool[11] = {23, 24, 25, 26, 27, 28, 11, 12, 13, 14, 15};
        regOf = allocateRegisters(L, 11);
        for (int& r : regOf) if (r >= 0) r = pool[r];
        // Frame: x29,x30 | x19,x20 | x21,x22 | x23,x24 | x25,x26 | x27,x28  (96 bytes)
        ins(pairOp(0xA9800000u, -12, 30, 31, 29), "stp x29, x30, [sp, #-96]!");
        ins(pairOp(0xA9000000u, 2, 20, 31, 19), "stp x19, x20, [sp, #16]");
        ins(pairOp(0xA9000000u, 4, 22, 31, 21), "stp x21, x22, [sp, #32]");
        ins(pairOp(0xA9000000u, 6, 24, 31, 23), "stp x23, x24, [sp, #48]");
        ins(pairOp(0xA9000000u, 8, 26, 31, 25), "stp x25, x26, [sp, #64]");
        ins(pairOp(0xA9000000u, 10, 28, 31, 27), "stp x27, x28, [sp, #80]");
        ins(0x910003FDu, "mov x29, sp");
        ins(0xAA0003F5u, "mov x21, x0            ; TraceContext*");
        ins(0xF9400014u, "ldr x20, [x0]          ; VM registers");
        ins(0xF9400413u, "ldr x19, [x0, #8]      ; slots");
        ins(0xB9400276u, "ldr w22, [x19]         ; fuel");
        size_t loopLabel = 0;
        std::vector<std::pair<size_t, int>> exitFixups;
        std::vector<std::pair<size_t, int>> exitBranches;  // unconditional b -> exit
        for (size_t k = 0; k < L.code.size(); k++) {
            const LInst& in = L.code[k];
            // The last guard directly before the back-edge can be fused with it.
            const bool fuseBackEdge = k + 1 < L.code.size() && L.code[k + 1].op == LOp::JumpLoop;
            switch (in.op) {
                case LOp::LoadReg: {
                    int c = cachedSlot(in.dst);
                    if (c >= 0) ldrW(c, 20, in.reg, "R" + std::to_string(in.reg));
                    else {
                        ldrW(9, 20, in.reg, "R" + std::to_string(in.reg));
                        strW(9, 19, in.dst, "s" + std::to_string(in.dst));
                    }
                    break;
                }
                case LOp::Copy: {
                    int c = cachedSlot(in.dst);
                    if (c >= 0) loadInto(c, in.a);
                    else strW(use(in.a, 9), 19, in.dst, "s" + std::to_string(in.dst));
                    break;
                }
                case LOp::Add: case LOp::Sub: case LOp::Mul: case LOp::And: {
                    int ra = use(in.a, 9);
                    int rb = use(in.b, 10);
                    int c = cachedSlot(in.dst);
                    int rd = c >= 0 ? c : 9;
                    uint32_t base; const char* nm;
                    switch (in.op) {
                        case LOp::Add: base = 0x0B000000u; nm = "add"; break;
                        case LOp::Sub: base = 0x4B000000u; nm = "sub"; break;
                        case LOp::Mul: base = 0x1B007C00u; nm = "mul"; break;
                        default: base = 0x0A000000u; nm = "and"; break;
                    }
                    ins(base | ((uint32_t)rb << 16) | ((uint32_t)ra << 5) | (uint32_t)rd,
                        std::string(nm) + " " + w(rd) + ", " + w(ra) + ", " + w(rb) + "   ; s" + std::to_string(in.dst));
                    if (c < 0) strW(9, 19, in.dst, "s" + std::to_string(in.dst));
                    break;
                }
                case LOp::GuardLT: case LOp::GuardGE: case LOp::GuardEQ: case LOp::GuardNE: {
                    int ra = use(in.a, 9);
                    int rb = use(in.b, 10);
                    ins(0x6B00001Fu | ((uint32_t)rb << 16) | ((uint32_t)ra << 5), "cmp " + w(ra) + ", " + w(rb));
                    // Condition codes for "condition holds" and "condition fails".
                    int cTrue, cFail; const char *nTrue, *nFail;
                    switch (in.op) {
                        case LOp::GuardLT: cTrue = 0xB; nTrue = "lt"; cFail = 0xA; nFail = "ge"; break;
                        case LOp::GuardGE: cTrue = 0xA; nTrue = "ge"; cFail = 0xB; nFail = "lt"; break;
                        case LOp::GuardEQ: cTrue = 0x0; nTrue = "eq"; cFail = 0x1; nFail = "ne"; break;
                        default: cTrue = 0x1; nTrue = "ne"; cFail = 0x0; nFail = "eq"; break;
                    }
                    if (fuseBackEdge) {
                        // Loop inversion: "b.cond LOOP; b exit" -- one branch per iteration.
                        bindBcond(bcond(cTrue, nTrue, "LOOP"), loopLabel);
                        exitBranches.push_back({b("exit" + std::to_string(in.exit)), in.exit});
                        k++;
                    } else {
                        exitFixups.push_back({bcond(cFail, nFail, "exit" + std::to_string(in.exit)), in.exit});
                    }
                    break;
                }
                case LOp::Print: {
                    loadInto(2, in.a);
                    movImm(1, in.reg);
                    ins(0xAA1503E0u, "mov x0, x21");
                    uint64_t addr = (uint64_t)(uintptr_t)&jitPrintHelper;
                    ins(0xD2800010u | (uint32_t)((addr & 0xFFFF) << 5), "movz x16, #lo16(jitPrintHelper)");
                    ins(0xF2A00010u | (uint32_t)(((addr >> 16) & 0xFFFF) << 5), "movk x16, #.., lsl #16");
                    ins(0xF2C00010u | (uint32_t)(((addr >> 32) & 0xFFFF) << 5), "movk x16, #.., lsl #32");
                    ins(0xF2E00010u | (uint32_t)(((addr >> 48) & 0xFFFF) << 5), "movk x16, #.., lsl #48");
                    ins(0xD100C3FFu, "sub sp, sp, #48        ; save caller-saved x11-x15");
                    ins(pairOp(0xA9000000u, 0, 12, 31, 11), "stp x11, x12, [sp]");
                    ins(pairOp(0xA9000000u, 2, 14, 31, 13), "stp x13, x14, [sp, #16]");
                    ins(0xF90013EFu, "str x15, [sp, #32]");
                    ins(0xD63F0200u, "blr x16");
                    ins(0xF94013EFu, "ldr x15, [sp, #32]");
                    ins(pairOp(0xA9400000u, 2, 14, 31, 13), "ldp x13, x14, [sp, #16]");
                    ins(pairOp(0xA9400000u, 0, 12, 31, 11), "ldp x11, x12, [sp]");
                    ins(0x9100C3FFu, "add sp, sp, #48");
                    break;
                }
                case LOp::LoopLabel:
                    loopLabel = buf.size();
                    asmText.push_back({buf.size(), "LOOP:"});
                    break;
                case LOp::FuelCheck:
                    ins(0x710006D6u, "subs w22, w22, #1      ; fuel");
                    exitFixups.push_back({bcond(0x0, "eq", "exit" + std::to_string(in.exit)), in.exit});
                    break;
                case LOp::JumpLoop:
                    bindB(b("LOOP"), loopLabel);
                    break;
            }
        }
        std::vector<size_t> exitPos(L.exits.size());
        std::vector<size_t> toEpilogue;
        for (size_t e = 0; e < L.exits.size(); e++) {
            exitPos[e] = buf.size();
            asmText.push_back({buf.size(), "exit" + std::to_string(e) + ":                 ; resume pc " +
                                               std::to_string(L.exits[e].resumePC)});
            for (const auto& wr : L.exits[e].writes)
                strW(use(wr.second, 9), 20, wr.first, "R" + std::to_string(wr.first));
            ins(0xB9000276u, "str w22, [x19]         ; save fuel");
            movImm(0, (int32_t)e);
            toEpilogue.push_back(b("epilogue"));
        }
        size_t epi = buf.size();
        asmText.push_back({buf.size(), "epilogue:"});
        ins(pairOp(0xA9400000u, 10, 28, 31, 27), "ldp x27, x28, [sp, #80]");
        ins(pairOp(0xA9400000u, 8, 26, 31, 25), "ldp x25, x26, [sp, #64]");
        ins(pairOp(0xA9400000u, 6, 24, 31, 23), "ldp x23, x24, [sp, #48]");
        ins(pairOp(0xA9400000u, 4, 22, 31, 21), "ldp x21, x22, [sp, #32]");
        ins(pairOp(0xA9400000u, 2, 20, 31, 19), "ldp x19, x20, [sp, #16]");
        ins(pairOp(0xA8C00000u, 12, 30, 31, 29), "ldp x29, x30, [sp], #96");
        ins(0xD65F03C0u, "ret");
        for (auto& f : exitFixups) bindBcond(f.first, exitPos[f.second]);
        for (auto& f : exitBranches) bindB(f.first, exitPos[f.second]);
        for (size_t at : toEpilogue) bindB(at, epi);
        return true;
    }
};
using NativeEmitter = A64;
#endif

#if defined(JIT_ARCH_X64) || defined(JIT_ARCH_A64)
class NativeCompiledTrace : public CompiledTrace {
public:
    typedef int (*EntryFn)(TraceContext*);
    bool build(const LIRTrace& L, std::string& why) {
        NativeEmitter em;
        if (!em.compile(L, why)) return false;
        if (!mem_.init(em.buf, why)) return false;
        fn_ = (EntryFn)mem_.ptr();
        size_ = em.buf.size();
        listing_ = em.listing();
        return true;
    }
    int run(TraceContext& ctx) override { return fn_(&ctx); }
    const char* kind() const override { return nativeArchName(); }
    size_t codeSize() const override { return size_; }
    std::string listing() const override { return listing_; }

private:
    ExecMemory mem_;
    EntryFn fn_ = nullptr;
    size_t size_ = 0;
    std::string listing_;
};
#endif

// Print sink used by the self-test (ctx->vm == nullptr).
long long g_selfTestSink = 0;

// Builds a small loop exercising every LIR opcode family, runs it through
// the native backend and compares against a C++ reference.
bool runSelfTestInProcess() {
    LIRTrace L;
    L.headerPC = 0;
    auto S = [](int s) { return LOperand::Slot(s); };
    auto I = [](int v) { return LOperand::Imm(v); };
    // s1=i (R0), s2=n (R1), s3=acc (R2); s4=phi i, s5=phi acc
    LInst x;
    x = LInst(); x.op = LOp::LoadReg; x.dst = 1; x.reg = 0; L.code.push_back(x);
    x = LInst(); x.op = LOp::LoadReg; x.dst = 2; x.reg = 1; L.code.push_back(x);
    x = LInst(); x.op = LOp::LoadReg; x.dst = 3; x.reg = 2; L.code.push_back(x);
    x = LInst(); x.op = LOp::Copy; x.dst = 4; x.a = S(1); L.code.push_back(x);
    x = LInst(); x.op = LOp::Copy; x.dst = 5; x.a = S(3); L.code.push_back(x);
    x = LInst(); x.op = LOp::LoopLabel; L.code.push_back(x);
    x = LInst(); x.op = LOp::FuelCheck; x.exit = 0; L.code.push_back(x);
    x = LInst(); x.op = LOp::Add; x.dst = 6; x.a = S(4); x.b = I(1); L.code.push_back(x);
    x = LInst(); x.op = LOp::Mul; x.dst = 7; x.a = S(6); x.b = I(-7); L.code.push_back(x);
    x = LInst(); x.op = LOp::Sub; x.dst = 8; x.a = S(7); x.b = S(2); L.code.push_back(x);
    x = LInst(); x.op = LOp::And; x.dst = 9; x.a = S(8); x.b = I(0x7FFFF); L.code.push_back(x);
    x = LInst(); x.op = LOp::Add; x.dst = 10; x.a = S(5); x.b = S(9); L.code.push_back(x);
    x = LInst(); x.op = LOp::Print; x.reg = 0; x.a = S(6); L.code.push_back(x);
    x = LInst(); x.op = LOp::GuardNE; x.a = S(6); x.b = I(1000000); x.exit = 1; L.code.push_back(x);
    x = LInst(); x.op = LOp::GuardGE; x.a = S(6); x.b = I(-5); x.exit = 1; L.code.push_back(x);
    x = LInst(); x.op = LOp::GuardEQ; x.a = S(2); x.b = S(2); x.exit = 1; L.code.push_back(x);
    x = LInst(); x.op = LOp::Copy; x.dst = 4; x.a = S(6); L.code.push_back(x);
    x = LInst(); x.op = LOp::Copy; x.dst = 5; x.a = S(10); L.code.push_back(x);
    x = LInst(); x.op = LOp::GuardLT; x.a = S(4); x.b = S(2); x.exit = 2; L.code.push_back(x);
    x = LInst(); x.op = LOp::JumpLoop; L.code.push_back(x);
    L.numSlots = 11;
    for (int e = 0; e < 3; e++) {
        LExit ex;
        ex.resumePC = e;
        ex.isFuelExit = (e == 0);
        ex.writes = {{0, S(4)}, {2, S(5)}};
        L.exits.push_back(ex);
    }
    std::string why;
    std::unique_ptr<CompiledTrace> t = compileNativeBackend(L, why);
    if (!t) return false;
    int32_t regs[NUM_REGS] = {0};
    regs[1] = 100;
    std::vector<int32_t> slots(L.numSlots, 0);
    slots[0] = 1000;
    TraceContext ctx{regs, slots.data(), nullptr};
    g_selfTestSink = 0;
    int e = t->run(ctx);
    // Reference computation.
    int32_t acc = 0;
    long long sink = 0;
    for (int32_t i = 1; i <= 100; i++) {
        acc = wrapAdd(acc, wrapSub(wrapMul(i, -7), 100) & 0x7FFFF);
        sink += i;
    }
    return e == 2 && regs[0] == 100 && regs[2] == acc && g_selfTestSink == sink && slots[0] == 900;
}

int g_nativeState = -1;  // -1 unknown, 0 unavailable, 1 available
std::string g_nativeStatus;

}  // namespace

extern "C" void jitPrintHelper(TraceContext* ctx, int32_t reg, int32_t value);
void vmPrintFromTrace(void* vm, int32_t reg, int32_t value);  // defined in vm.cpp

extern "C" void jitPrintHelper(TraceContext* ctx, int32_t reg, int32_t value) {
    if (ctx->vm) vmPrintFromTrace(ctx->vm, reg, value);
    else g_selfTestSink += value;
}

const char* nativeArchName() {
#if defined(JIT_ARCH_X64)
    return "x86-64";
#elif defined(JIT_ARCH_A64)
    return "AArch64";
#else
    return "unsupported";
#endif
}

std::unique_ptr<CompiledTrace> compileNativeBackend(const LIRTrace& lir, std::string& why) {
#if defined(JIT_ARCH_X64) || defined(JIT_ARCH_A64)
    std::unique_ptr<NativeCompiledTrace> t(new NativeCompiledTrace());
    if (!t->build(lir, why)) return nullptr;
    return std::unique_ptr<CompiledTrace>(t.release());
#else
    why = "no native code generator for this CPU architecture";
    (void)lir;
    return nullptr;
#endif
}

bool nativeBackendAvailable() {
    if (g_nativeState >= 0) return g_nativeState == 1;
#if !(defined(JIT_ARCH_X64) || defined(JIT_ARCH_A64))
    g_nativeState = 0;
    g_nativeStatus = "unsupported CPU architecture";
    return false;
#else
    const char* env = std::getenv("JITVM_NO_NATIVE");
    if (env && *env && *env != '0') {
        g_nativeState = 0;
        g_nativeStatus = "disabled by JITVM_NO_NATIVE";
        return false;
    }
    std::fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        // Child: any crash here is contained.
        _exit(runSelfTestInProcess() ? 0 : 1);
    }
    bool ok;
    if (pid < 0) {
        ok = runSelfTestInProcess();  // cannot fork: test in-process
    } else {
        int status = 0;
        waitpid(pid, &status, 0);
        ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
        if (!ok) {
            g_nativeStatus = WIFSIGNALED(status)
                ? std::string("self-test crashed with signal ") + std::to_string(WTERMSIG(status))
                : "self-test produced wrong results";
        }
    }
    g_nativeState = ok ? 1 : 0;
    if (ok) g_nativeStatus = std::string("native ") + nativeArchName() + " backend (self-test passed)";
    return ok;
#endif
}

std::string nativeBackendStatus() {
    nativeBackendAvailable();
    return g_nativeStatus;
}
