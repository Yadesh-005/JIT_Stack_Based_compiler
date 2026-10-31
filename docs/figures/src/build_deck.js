// Review 3 presentation -- BCSE307 A2, Team 6
const pptxgen = require("pptxgenjs");
const fs = require("fs");
const { applyTheme } = require("/mnt/skills/public/pptx/scripts/apply_theme.js");

const FIG = "/home/claude/proj/docs/figures/";
const bench = JSON.parse(fs.readFileSync("/home/claude/report/bench.json"));
const OUT = process.argv[2] || "/home/claude/report/Review3_Presentation_Team6.pptx";

const THEME = {
  name: "Team6 JIT",
  headFontFace: "Cambria",
  bodyFontFace: "Calibri",
  colors: {
    dk1: "16222E", lt1: "FFFFFF", dk2: "1F4E78", lt2: "EEF3F8",
    accent1: "2E74B5", accent2: "C00000", accent3: "1BAF7A", accent4: "EB6834",
    accent5: "6C7A89", accent6: "F2B134", hlink: "2E74B5", folHlink: "1F4E78",
  },
};
const HEX = THEME.colors;
const pres = new pptxgen();
pres.layout = "LAYOUT_WIDE"; // 13.33 x 7.5
pres.theme = { headFontFace: THEME.headFontFace, bodyFontFace: THEME.bodyFontFace };
pres.title = "Trace-Based JIT Compiler -- Review 3";
pres.author = "Team 6 (BCSE307 A2)";
const C = pres.SchemeColor;

// ------------------------------------------------------------ layouts
pres.defineSlideMaster({
  title: "DARK",
  background: { color: HEX.dk2 },
  objects: [],
  slideNumber: { x: 12.4, y: 7.0, w: 0.6, h: 0.3, fontSize: 10, color: "CADCFC" },
});
pres.defineSlideMaster({
  title: "CONTENT",
  background: { color: "FFFFFF" },
  objects: [
    { placeholder: { options: { name: "title", type: "title", x: 0.6, y: 0.35, w: 9.6, h: 0.85, fontFace: "Cambria",
      fontSize: 30, bold: true, color: HEX.dk2, valign: "middle", align: "left", margin: 0 }, text: "" } },
    { text: { text: "BCSE307 · A2 · Team 6 · Review 3", options: { x: 0.6, y: 7.02, w: 6, h: 0.3, fontSize: 10, color: "6C7A89", margin: 0 } } },
  ],
  slideNumber: { x: 12.4, y: 7.0, w: 0.6, h: 0.3, fontSize: 10, color: "6C7A89" },
});

const sections = {};
function slide(master, section) {
  if (!sections[section]) { pres.addSection({ title: section }); sections[section] = true; }
  return pres.addSlide({ masterName: master, sectionTitle: section });
}
function badge(s, text, dark = false) {
  s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x: 10.45, y: 0.47, w: 2.3, h: 0.5, rectRadius: 0.25,
    fill: { color: dark ? "2E74B5" : HEX.lt2 }, line: { color: dark ? "2E74B5" : HEX.lt2 }, objectName: "presenter badge" });
  s.addText(text, { x: 10.45, y: 0.47, w: 2.3, h: 0.5, align: "center", valign: "middle", fontSize: 12, bold: true,
    color: dark ? "FFFFFF" : HEX.dk2, isTextBox: true, margin: 0, objectName: "presenter" });
}
function title(s, t) { s.addText(t, { placeholder: "title" }); }
function card(s, x, y, w, h, head, body, opts = {}) {
  s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x, y, w, h, rectRadius: 0.08, fill: { color: opts.fill || HEX.lt2 },
    line: { color: opts.fill || HEX.lt2 }, objectName: "card " + head });
  s.addText(head, { x: x + 0.2, y: y + 0.15, w: w - 0.4, h: 0.45, fontSize: opts.headSize || 17, bold: true,
    color: opts.headColor || HEX.dk2, fontFace: "Calibri", isTextBox: true, margin: 0, valign: "top" });
  s.addText(body, { x: x + 0.2, y: y + 0.62, w: w - 0.4, h: h - 0.75, fontSize: opts.size || 14, color: HEX.dk1,
    isTextBox: true, margin: 0, valign: "top", paraSpaceAfter: 4 });
}
function bullets(items, size = 16) {
  return items.map((t, i) => ({ text: t, options: { bullet: true, breakLine: i < items.length - 1, fontSize: size, paraSpaceAfter: 6 } }));
}
function code(s, text, x, y, w, h, size = 11) {
  s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x, y, w, h, rectRadius: 0.05, fill: { color: "16222E" }, line: { color: "16222E" }, objectName: "code panel" });
  s.addText(text, { x: x + 0.15, y: y + 0.1, w: w - 0.3, h: h - 0.2, fontFace: "Consolas", fontSize: size, color: "E8EEF4",
    isTextBox: true, margin: 0, valign: "top" });
}
function stat(s, x, y, num, label, color) {
  s.addText(num, { x, y, w: 2.9, h: 0.95, fontSize: 48, bold: true, color: color || HEX.dk2, fontFace: "Cambria", isTextBox: true, margin: 0 });
  s.addText(label, { x, y: y + 0.95, w: 2.9, h: 0.7, fontSize: 13, color: "52514E", isTextBox: true, margin: 0, valign: "top" });
}

// ============================================================ 1 Title
let s = slide("DARK", "Introduction");
s.addText("BCSE307 — COMPILER DESIGN · PROJECT REVIEW 3", { x: 0.8, y: 0.7, w: 11, h: 0.4, fontSize: 14, color: "CADCFC", bold: true, isTextBox: true, margin: 0 });
s.addText("Trace-Based JIT Compiler for a Custom Bytecode Virtual Machine", { x: 0.8, y: 1.35, w: 11.5, h: 1.7, fontSize: 40, bold: true, color: "FFFFFF", fontFace: "Cambria", isTextBox: true, margin: 0, valign: "top" });
s.addText("Project A2 · Team 6 · Final demonstration and evaluation", { x: 0.8, y: 3.15, w: 11, h: 0.5, fontSize: 18, italic: true, color: "CADCFC", isTextBox: true, margin: 0 });
const members = [["Atul Vipin", "24BCE2465", "Lead / integration"], ["Emmanuel Joshy", "24BCE0916", "Front end / recorder"],
  ["Yadesh Waran M", "24BCE2402", "Optimiser / codegen"], ["Tanvi Gupta", "24BAI0254", "Deopt / testing"]];
members.forEach((m, i) => {
  const x = 0.8 + i * 3.0;
  s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x, y: 4.35, w: 2.75, h: 1.35, rectRadius: 0.08, fill: { color: "2A5F8F" }, line: { color: "2A5F8F" }, objectName: "member " + m[0] });
  s.addText([{ text: m[0], options: { bold: true, fontSize: 16, color: "FFFFFF", breakLine: true } },
    { text: m[1], options: { fontSize: 13, color: "CADCFC", breakLine: true } },
    { text: m[2], options: { fontSize: 13, color: "CADCFC" } }], { x: x + 0.2, y: 4.45, w: 2.4, h: 1.15, isTextBox: true, margin: 0, valign: "middle" });
});
s.addText("Faculty: Dr. Shalini L · School of Computer Science and Engineering, VIT · github.com/Yadesh-005/JIT", { x: 0.8, y: 6.3, w: 11.5, h: 0.4, fontSize: 13, color: "CADCFC", isTextBox: true, margin: 0 });
s.addNotes("Member 1 (Atul). Introduce the team and the project in one sentence: a VM that interprets bytecode, finds hot loops, records them, optimises them and compiles them to real ARM and x86 machine code, falling back safely to the interpreter. Total time for slides 1-5: about 4 minutes.");

// ============================================================ 2 Problem
s = slide("CONTENT", "Introduction");
title(s, "Problem, motivation and final objectives"); badge(s, "M1 · Atul Vipin");
s.addText([{ text: "Interpreters pay for fetch, decode and dispatch on every instruction. ", options: {} },
  { text: "A tracing JIT", options: { bold: true } },
  { text: " records the path a hot loop actually takes, compiles only that path, and guards every assumption so it can fall back to the interpreter.", options: {} }],
  { x: 0.6, y: 1.4, w: 5.6, h: 1.6, fontSize: 17, color: HEX.dk1, isTextBox: true, margin: 0, valign: "top" });
s.addText("Goal: identical results to pure interpretation, at native speed.", { x: 0.6, y: 3.1, w: 5.6, h: 0.6, fontSize: 17, italic: true, color: HEX.accent2, isTextBox: true, margin: 0 });
stat(s, 0.6, 4.0, "62×", "faster than the interpreter on Apple Silicon (B2)", HEX.dk2);
stat(s, 3.5, 4.0, "0", "mismatches in 7,040 differential runs on 2 CPUs", HEX.accent3);
const objs = [["O1", "Interpreter for custom bytecode", "Achieved"], ["O2", "Hot-loop detection (HOT_THRESHOLD = 3)", "Achieved"],
  ["O3", "Trace recording with guards and limits", "Achieved"], ["O4", "SSA TraceIR optimisation", "Achieved"],
  ["O5", "Native code generation (x86-64 + AArch64)", "Achieved"], ["O6", "Snapshots and deoptimisation", "Achieved"],
  ["O7", "Paper novelty: loop peeling (DLS 2012)", "Extension"], ["O8", "Robustness, reproducibility", "Extension"]];
s.addTable([[{ text: "", options: { fill: { color: HEX.dk2 } } }, { text: "Objective", options: { bold: true, color: "FFFFFF", fill: { color: HEX.dk2 } } },
  { text: "Status", options: { bold: true, color: "FFFFFF", fill: { color: HEX.dk2 } } }]].concat(
  objs.map(o => [{ text: o[0], options: { bold: true, color: HEX.dk2 } }, o[1], { text: o[2], options: { color: o[2] === "Achieved" ? "0B7A55" : HEX.accent1, bold: true } }])),
  { x: 6.7, y: 1.4, w: 6.0, colW: [0.6, 4.1, 1.3], fontSize: 13, fontFace: "Calibri", color: HEX.dk1, border: { type: "solid", pt: 0.5, color: "C8D3DE" }, rowH: 0.42 });
s.addNotes("Problem in one breath, then the objective matrix. Every objective from Review 1/2 is achieved; O7 and O8 go beyond the approved scope. Point at the two numbers: 62x speed-up and zero mismatches - speed and correctness are the two claims we will prove in the demo.");

// ============================================================ 3 Review 2 closure
s = slide("CONTENT", "Introduction");
title(s, "Review 2 feedback closure and final scope"); badge(s, "M1 · Atul Vipin");
const closure = [["Missing snapshot state (High risk)", "Every guard owns a snapshot; exit stubs restore registers"],
  ["Backend complexity (Medium risk)", "Narrow LIR + template encoders for 2 ISAs; IR fallback"],
  ["Optimiser pass (DCE, folding)", "SSA: fold, CSE, guard-elim, DCE, loop peeling"],
  ["Interpreter → native transition", "Loop header patched to TRACE_ENTER"],
  ["Multi-loop / nested coverage gaps", "Tests 06, 09, 11, 24 + random nested loops"]];
s.addTable([[{ text: "Review 2 item", options: { bold: true, color: "FFFFFF", fill: { color: HEX.dk2 } } },
  { text: "Closed by", options: { bold: true, color: "FFFFFF", fill: { color: HEX.dk2 } } }]].concat(closure),
  { x: 0.6, y: 1.45, w: 6.9, colW: [2.8, 4.1], fontSize: 13, color: HEX.dk1, border: { type: "solid", pt: 0.5, color: "C8D3DE" }, rowH: 0.5 });
card(s, 7.9, 1.45, 4.85, 4.9, "8 Review 2 defects found and fixed",
  bullets(["Recording ran past the loop end; later loops never traced", "LOAD_CONST, PRINT, forward branches not recorded",
    "Length limit checked only for ADD_INT", "Test case 8 never terminated", "Out-of-bounds register write (ASan)",
    "Signed-overflow UB (UBSan)", "Accidental permanent blacklist", "Recorded trace never used"], 13), { size: 13 });
s.addText("Each defect reproduced on the unchanged Review 2 code (legacy/review2/) and covered by a regression test.", { x: 0.6, y: 5.8, w: 6.9, h: 0.6, fontSize: 13, italic: true, color: "52514E", isTextBox: true, margin: 0 });
s.addNotes("Show that every Review 2 action item is closed, then the defects we found ourselves while finishing the project. If asked, the most important one is D1: when a loop became hot on its last iteration the old recorder kept recording into the next loop, so the next loop could never be compiled. Test 11 demonstrates the fix.");

// ============================================================ 4 Architecture
s = slide("CONTENT", "Design");
title(s, "Final architecture and execution workflow"); badge(s, "M1 · Atul Vipin");
s.addImage({ path: FIG + "architecture.png", x: 0.6, y: 1.3, w: 12.1, h: 12.1 * 1210 / 2365, objectName: "architecture diagram" });
s.addNotes("Walk the pipeline left to right, top then bottom: assembler and verifier, interpreter with profiler, recorder; then the JIT compiler on the bottom row from right to left: SSA plus loop peeling, forward optimisations, lowering, native codegen; code cache; native trace; exit stub that restores state and resumes interpretation (the red arrow). Colours show module ownership.");

// ============================================================ 5 Design decisions
s = slide("CONTENT", "Design");
title(s, "Key design decisions and compiler concepts"); badge(s, "M1–M3");
const dec = [["Three IR levels", "Recorded TraceIR → SSA (Phi, snapshots) → slot LIR"],
  ["SSA + forward passes", "Folding, CSE, guard elimination, DCE in one pass"],
  ["Loop peeling", "LICM from existing passes (Ardö et al., DLS 2012)"],
  ["Linear-scan allocation", "Loop-aware live intervals; 10 / 11 registers"],
  ["Own x86-64 + AArch64 encoders", "No LLVM dependency; µs compile time; W^X / MAP_JIT"],
  ["Lazy write-back + snapshots", "VM registers written only at exits"]];
dec.forEach((d, i) => card(s, 0.6 + (i % 3) * 4.1, 1.5 + Math.floor(i / 3) * 2.55, 3.85, 2.3, d[0], d[1], { size: 15 }));
s.addNotes("Six decisions, each with its trade-off. Trace-based instead of method-based keeps the optimiser on straight-line code. Own encoders instead of LLVM ORC: nothing to install, and the evaluator can see every byte we emit. Snapshots: the loop never touches the VM register file; only an exit writes it back.");

// ============================================================ 6 Module 1 design
s = slide("CONTENT", "Module 1");
title(s, "Module 1: front end, interpreter and recorder"); badge(s, "M2 · Emmanuel Joshy");
s.addText(bullets(["Two-pass assembler: labels, hex/negative immediates, every error with its line",
  "Verifier: register/target checks, CFG reachability, definitely-assigned data-flow analysis",
  "Profiler counts taken back-edges (currentPC fix from Review 2)",
  "Recorder turns each branch direction into a guard with an exit pc; 5 abort rules"], 15),
  { x: 0.6, y: 1.45, w: 5.4, h: 4.6, color: HEX.dk1, isTextBox: true, margin: 0, valign: "top" });
code(s, "case JUMP_IF_LT: case JUMP_IF_EQ:\n  if (taken) {\n    if (target <= pc && target != header)\n      return abort(\"inner loop\");\n    push(GUARD_LT/EQ a, b, exit = pc+1);\n    if (target == header) {\n      push(JUMP_LOOP); return Completed;\n    }\n  } else {\n    if (target == header)\n      return abort(\"loop exited\");   // D1 fix\n    push(GUARD_GE/NE a, b, exit = target);\n  }", 6.4, 1.45, 6.35, 4.6, 13);
s.addText("src/recorder.cpp · src/assembler.cpp · src/verifier.cpp", { x: 6.4, y: 6.2, w: 6.35, h: 0.4, fontSize: 12, color: "52514E", isTextBox: true, margin: 0 });
s.addNotes("Member 2 (Emmanuel), 4-5 minutes for slides 6-7. Explain the two-pass assembler and the must-analysis: IN is the intersection of predecessors' OUT, OUT is IN plus definitions. Then the recorder rule on the right: a taken branch to the header closes the loop; a not-taken back-edge means the loop exited while recording - the Review 2 bug.");

// ============================================================ 7 Module 1 demo
s = slide("CONTENT", "Module 1");
title(s, "Module 1 demo: valid and error inputs"); badge(s, "M2 · Emmanuel Joshy");
code(s, "$ ./build/jitvm tests/errors/e01_syntax_errors.jbc\n:3: error: unknown mnemonic 'LOAD'\n:4: error: ADD_INT expects 3 operands, got 2\n:5: error: invalid register 'R16' (valid: R0-R15)\n:6: error: number '99999999999' does not fit in 32 bits\n:7: error: undefined label 'nowhere'\n:9: error: duplicate label 'dup'          (exit 2)", 0.6, 1.45, 6.2, 2.6, 12);
code(s, "$ ./build/jitvm tests/programs/03_invalid_unknown_op.jbc\n[PROFILER] Hot loop detected at pc 3 (line 7)!\n[RECORDER] Unsupported opcode detected! Aborting\n[PROFILER] Hot loop detected at pc 3 (line 7)!\n[RECORDER] Unsupported opcode detected! Aborting\n[PROFILER] Loop at pc 3 blacklisted after 2 attempts", 0.6, 4.25, 6.2, 2.4, 12);
s.addText("Live commands", { x: 7.2, y: 1.45, w: 5.5, h: 0.45, fontSize: 18, bold: true, color: HEX.dk2, isTextBox: true, margin: 0 });
s.addText(bullets(["e01_syntax_errors.jbc — all 6 errors, line numbers", "--check 09_nested_loops.jbc — verifier + disassembly",
  "-q w01_warnings.jbc — read-before-write, unreachable", "03_invalid_unknown_op.jbc — abort, retry, blacklist",
  "11_loop_exit_during_recording.jbc — Review 2 bug fixed"], 15), { x: 7.2, y: 2.0, w: 5.5, h: 4.0, color: HEX.dk1, isTextBox: true, margin: 0, valign: "top" });
s.addNotes("Run the five commands on the right live. Point out that the assembler reports all errors at once, with exit code 2, and that UNKNOWN_OP never crashes anything: the recorder aborts twice and the loop is blacklisted while the interpreter keeps producing the right answer.");

// ============================================================ 8 Module 2 design
s = slide("CONTENT", "Module 2");
title(s, "Module 2: SSA optimiser with loop peeling"); badge(s, "M3 · Yadesh Waran M");
code(s, "-O1  loop body (no peeling)\n  v16 = Mul v3, v4     ; a*b  invariant\n  v17 = Add v1, v16\n  v19 = Mul v17, #3\n  v20 = Add v11, v19   ; acc += k\n  v21 = Add v1, v10    ; i += 1\n  GuardLT v21, v2\n  5 Phi nodes  ->  6 ops / iteration", 0.6, 1.45, 5.9, 3.5, 13);
code(s, "-O2  preamble + peeled loop\n  v9  = Mul v3, v4     ; computed once\n  v10 = Add v1, v9\n  v12 = Mul v10, #3\n  ---- loop header ----\n  v20 = Add v12, v18   ; acc += k\n  v21 = Add v1, v17    ; i += 1\n  GuardLT v21, v2\n  2 Phi nodes  ->  3 ops / iteration", 6.85, 1.45, 5.9, 3.5, 13);
s.addText([{ text: "How it works: ", options: { bold: true, color: HEX.dk2 } },
  { text: "emit the trace twice; while optimising the loop body the CSE and guard tables still contain the preamble's values, so invariant operations hash to them and disappear. A Phi fix-point removes registers that get the same value every iteration (R6, R7, R8).", options: {} }],
  { x: 0.6, y: 5.2, w: 12.15, h: 1.3, fontSize: 16, color: HEX.dk1, isTextBox: true, margin: 0, valign: "top" });
s.addNotes("Member 3 (Yadesh), 4-5 minutes for slides 8-9. This is the paper's novelty: loop-invariant code motion without writing a LICM pass. Explain why it is safe: the preamble dominates the loop, and a CSE hit needs identical SSA operands, which are invariant because loop-varying values are always fresh Phi values.");

// ============================================================ 9 Module 2 demo
s = slide("CONTENT", "Module 2");
title(s, "Module 2 demo: native code and integration"); badge(s, "M3 · Yadesh Waran M");
code(s, "AArch64 loop for  sum += i; i += 1; while (i < n)\nLOOP:\n  subs w22, w22, #1     ; fuel\n  b.eq exit1\n  add  w26, w23, w26    ; sum += i\n  add  w23, w24, w23    ; i += step\n  cmp  w23, w25\n  b.lt LOOP             ; loop inversion\n  b    exit2", 0.6, 1.45, 6.2, 3.6, 13);
code(s, "$ ./build/jitvm --compare bench/programs/b2_loop_invariant.jbc\ninterpreter : 0.6513 s  (HALTED)\nJIT         : 0.0105 s  (native AArch64, -O2)\nspeed-up    : 61.91x\nfinal registers and status IDENTICAL\n-> differential check PASSED", 0.6, 5.2, 6.2, 1.55, 11);
card(s, 7.2, 1.45, 5.55, 5.3, "From LIR to machine code", bullets(["Phi coalescing removes back-edge copies",
  "Linear-scan register allocation: loop-live values stay in registers", "Template encoders for AArch64 and x86-64",
  "Peepholes: compute into the destination register, loop inversion", "W^X memory; Apple MAP_JIT + pthread_jit_write_protect_np",
  "Self-test in a forked child: on failure, IR executor fallback"], 15), { size: 15 });
s.addNotes("Run --dump-asm on b5_polynomial and --compare on b2 live. The loop on the left is seven instructions; every value is in a register. Mention the fork-isolated self-test: if native code were ever wrong on a machine, the VM silently switches to the portable executor instead of crashing.");

// ============================================================ 10 End-to-end demo
s = slide("DARK", "Demonstration");
s.addText("Complete end-to-end live demonstration", { x: 0.8, y: 0.55, w: 11.5, h: 0.8, fontSize: 32, bold: true, color: "FFFFFF", fontFace: "Cambria", isTextBox: true, margin: 0 });
const stages = [["1", "Environment", "make, --backend-info"], ["2", "Normal case", "01_valid_stable_loop"],
  ["3", "Boundary", "07 limit, 09 nested, 22 deopt"], ["4", "Failure", "e01, e02, e03 ÷0, e04 loop"],
  ["5", "Internals", "IR dumps -O1 vs -O2, machine code"], ["6", "Evaluation", "make test, jit_bench"], ["7", "Evaluator input", "--compare on their program"]];
stages.forEach((st, i) => {
  const x = 0.8 + i * 1.73;
  s.addShape(pres.shapes.OVAL, { x: x + 0.45, y: 1.75, w: 0.75, h: 0.75, fill: { color: i === 6 ? HEX.accent2 : HEX.accent1 }, line: { color: "FFFFFF", width: 1.5 }, objectName: "stage " + st[0] });
  s.addText(st[0], { x: x + 0.45, y: 1.75, w: 0.75, h: 0.75, align: "center", valign: "middle", fontSize: 20, bold: true, color: "FFFFFF", isTextBox: true, margin: 0 });
  s.addText(st[1], { x, y: 2.7, w: 1.65, h: 0.5, align: "center", fontSize: 15, bold: true, color: "FFFFFF", isTextBox: true, margin: 0 });
  s.addText(st[2], { x, y: 3.2, w: 1.65, h: 1.3, align: "center", fontSize: 12, color: "CADCFC", isTextBox: true, margin: 0, valign: "top" });
});
s.addText("./demo.sh runs this sequence with a pause after every step", { x: 0.8, y: 4.9, w: 11.5, h: 0.5, fontSize: 16, italic: true, color: "CADCFC", isTextBox: true, margin: 0 });
s.addText("Every member operates the system; the evaluator chooses the final input.", { x: 0.8, y: 5.5, w: 11.5, h: 0.5, fontSize: 18, bold: true, color: "FFFFFF", isTextBox: true, margin: 0 });
s.addNotes("All members, at least 8 minutes. Follow docs/DEMO_SCRIPT.md. Stage 7: ask the evaluator for a loop, type it into demo_input.jbc and run --compare, then -vv --stats and --dump-ir on it.");

// ============================================================ 11 Testing
s = slide("CONTENT", "Evaluation");
title(s, "Test strategy and test-suite coverage"); badge(s, "M4 · Tanvi Gupta");
stat(s, 0.6, 1.4, "210", "checks pass on Apple Silicon and on x86-64 (183 suite + 27 CLI)", HEX.dk2);
stat(s, 0.6, 3.15, "3,520", "differential runs per machine: JIT output = interpreter output", HEX.accent3);
stat(s, 0.6, 4.9, "2,800", "of them on 400 random programs × 7 JIT configurations", HEX.accent1);
const layers = [["Unit", "assembler, verifier, recorder, optimiser, deopt (122 checks)"], ["Golden", "24 programs vs. expected output files"],
  ["Differential", "program × 2 backends × 3 levels × 5 thresholds"], ["Fuzz", "random nested loops, if/else, PRINT, DIV, UNKNOWN_OP"],
  ["CLI", "exit codes 0-5, diagnostics, --compare"], ["Sanitizers", "ASan + UBSan clean; -Wall -Wextra -pedantic -Wconversion"]];
s.addTable([[{ text: "Layer", options: { bold: true, color: "FFFFFF", fill: { color: HEX.dk2 } } }, { text: "What it checks", options: { bold: true, color: "FFFFFF", fill: { color: HEX.dk2 } } }]].concat(
  layers.map(l => [{ text: l[0], options: { bold: true, color: HEX.dk2 } }, l[1]])),
  { x: 4.2, y: 1.45, w: 8.55, colW: [1.7, 6.85], fontSize: 15, color: HEX.dk1, border: { type: "solid", pt: 0.5, color: "C8D3DE" }, rowH: 0.62 });
s.addText("Positive, negative, boundary, stress and regression cases — tests/, results/test_log_*.txt", { x: 4.2, y: 6.2, w: 8.55, h: 0.4, fontSize: 13, italic: true, color: "52514E", isTextBox: true, margin: 0 });
s.addNotes("Member 4 (Tanvi), 4-5 minutes for slides 11-14. The interpreter is the reference semantics; the JIT must be observationally identical. Different hot thresholds start recording at different iterations, so the matrix exercises different trace-entry states. Run make test live; it ends with TOTAL 183 passed and CLI tests 27 passed.");

// ============================================================ 12 Results (native chart)
s = slide("CONTENT", "Evaluation");
title(s, "Results: speed-up over the interpreter"); badge(s, "M4 · Tanvi Gupta");
const bnames = ["b1_sum_loop", "b2_loop_invariant", "b3_nested_loops", "b4_fibonacci_mod", "b5_polynomial", "b6_bounds_checked", "b7_branchy", "b8_div_not_jitable"];
const blabels = ["B1 sum", "B2 invariant", "B3 nested", "B4 Fibonacci", "B5 polynomial", "B6 bounds", "B7 branchy", "B8 DIV"];
const a = bench.AArch64;
s.addChart(pres.charts.BAR, [
  { name: "IR executor -O2", labels: blabels, values: bnames.map(n => a[n]["JIT IR-executor -O2"]) },
  { name: "Native -O1", labels: blabels, values: bnames.map(n => a[n]["JIT native -O1"]) },
  { name: "Native -O2 (peeling)", labels: blabels, values: bnames.map(n => a[n]["JIT native -O2"]) }],
  { x: 0.6, y: 1.35, w: 8.4, h: 5.4, barDir: "col", barGrouping: "clustered", chartColors: ["2A78D6", "EB6834", "1BAF7A"],
    valAxisLogScaleBase: 10, valAxisMinVal: 0.5, valAxisMaxVal: 100, valAxisLabelColor: "52514E", catAxisLabelColor: "16222E",
    valAxisLabelFontSize: 11, catAxisLabelFontSize: 11, valAxisLabelFontFace: "+mn-lt", catAxisLabelFontFace: "+mn-lt",
    valGridLine: { color: "E4E3DF", size: 0.75 }, catGridLine: { style: "none" }, showLegend: true, legendPos: "t", legendFontSize: 12,
    legendFontFace: "+mn-lt", showTitle: true, title: "Apple Silicon, median of 7 runs (log scale, ×)", titleFontSize: 13, titleColor: "16222E", titleFontFace: "+mn-lt",
    showValue: false, valAxisTitle: "speed-up (×)", showValAxisTitle: true, valAxisTitleFontSize: 11, valAxisTitleColor: "52514E" });
card(s, 9.35, 1.45, 3.4, 5.25, "Reading the chart", bullets(["Native JIT: 10–62× on loop benchmarks (x86-64: 14–75×)",
  "Peeling: B2 33× → 62×, nothing else changes", "B7 branchy 1.8×: side exit every other iteration", "B8 DIV 0.94×: blacklisted, only profiling cost",
  "All 80 measurements produced identical output"], 14), { size: 14 });
s.addNotes("Explain the three bars. Native code gives the order-of-magnitude gain; loop peeling doubles it exactly where loop-invariant work exists (B2) and changes nothing where it does not. Be honest about B7 and B8 - they motivate future work. x86 numbers are in the report and in results/bench_x86_64.csv.");

// ============================================================ 13 Limitations
s = slide("CONTENT", "Evaluation");
title(s, "Limitations, failure cases and trade-offs"); badge(s, "M3 & M4");
card(s, 0.6, 1.45, 3.9, 4.9, "Limitations", bullets(["One linear trace per loop: no side traces / trace trees", "Outer loop with an inner loop stays interpreted",
  "Integer ISA, 16 registers, no memory or calls", "Template code generation, no scheduling"], 14), { size: 14 });
card(s, 4.75, 1.45, 3.9, 4.9, "Failure handling", bullets(["Errors → diagnostics + exit codes 2 / 3 / 4", "Infinite loop inside native code → fuel exit",
  "Unsupported op → abort, retry, blacklist", "Bad native code → forked self-test → IR executor"], 14), { size: 14 });
card(s, 8.9, 1.45, 3.85, 4.9, "Trade-offs measured", bullets(["Fuel check: 1 instruction per iteration buys a safe step limit",
  "Profiling costs ~6 % on non-JIT-able loops", "Phi coalescing: removing it halved loop speed", "No LLVM: no dependency, but simpler code"], 14), { size: 14 });
s.addNotes("Members 3 and 4. Limitations come with their measured impact, not just a list. A good viva point: we measured our own simplifications too - removing Phi coalescing made the Apple Silicon loop twice as slow, so it stayed.");

// ============================================================ 14 Repository
s = slide("CONTENT", "Evaluation");
title(s, "Repository, build and reproducibility"); badge(s, "M4 · Tanvi Gupta");
code(s, "xcode-select --install   # once, macOS\ncd ~/Desktop/\"compiler JIT project\"\nmake                     # build all\nmake test                # 183 + 27 checks\nmake bench               # results/*.csv\n./demo.sh                # guided demo\n./build/jitvm --help", 0.6, 1.45, 6.3, 3.1, 13);
code(s, "src/          VM, front end, recorder, optimiser, backends\ntests/        test_main.cpp, programs/, expected/, errors/\nbench/        benchmark harness + 8 programs\nlegacy/       Review 2 prototype (unchanged)\ndocs/         figures, demo script, viva prep, contributions\nresults/      test + benchmark logs (AArch64, x86-64)", 0.6, 4.75, 6.3, 2.0, 11);
card(s, 7.3, 1.45, 5.45, 5.3, "Reproducibility", bullets(["C++17 + POSIX only: no third-party libraries", "Same sources build with Apple clang, g++ and clang",
  "Logs for both architectures checked into results/", "Charts regenerated by results/plot_results.py", "README: build, ISA, CLI, exit codes, limitations",
  "Review 3 report (.docx/.pdf) and this deck in the same folder"], 15), { size: 15 });
s.addNotes("Show the folder in Finder and the README. For the evaluator's reproducibility check: make clean && make && make test.");

// ============================================================ 15 Contribution
s = slide("CONTENT", "Closing");
title(s, "Member-wise technical contribution"); badge(s, "All members");
const contrib = [["Atul Vipin · M1", "Lead / integrator", "VM state machine, code cache (header patching), executable memory + self-test, CLI, build, demo"],
  ["Emmanuel Joshy · M2", "Front end / analysis", "ISA, assembler, verifier (CFG + data-flow), interpreter, profiler, recorder"],
  ["Yadesh Waran M · M3", "Core / back end", "SSA optimiser + loop peeling, LIR, linear-scan allocation, x86-64 + AArch64 codegen"],
  ["Tanvi Gupta · M4", "Testing / release", "Snapshots + deoptimisation, test suite + fuzzer, benchmarks, docs, report"]];
contrib.forEach((c, i) => {
  const x = 0.6 + (i % 2) * 6.15, y = 1.45 + Math.floor(i / 2) * 2.6;
  card(s, x, y, 5.95, 2.35, c[0], [{ text: c[1], options: { bold: true, color: HEX.accent1, breakLine: true } }, { text: c[2], options: {} }], { size: 15 });
});
s.addText("Interfaces: RecordedTrace (M2 → M3) · LIRTrace (M3 → back ends) · Snapshot / LExit (M3 ↔ M4) · VMConfig (M1)", { x: 0.6, y: 6.55, w: 12.15, h: 0.4, fontSize: 13, italic: true, color: "52514E", isTextBox: true, margin: 0 });
s.addNotes("Each member states in one sentence what they own and which file to open. Evidence: docs/CONTRIBUTION_LOG.md and the repository history.");

// ============================================================ 16 Innovation
s = slide("CONTENT", "Closing");
title(s, "Innovation, extensions and publication potential"); badge(s, "M1 · Atul Vipin");
s.addChart(pres.charts.BAR, [
  { name: "-O0", labels: ["12_invariant", "21_folding", "20_guards", "B2", "B6", "B5"], values: [6, 6, 5, 8, 7, 9] },
  { name: "-O1", labels: ["12_invariant", "21_folding", "20_guards", "B2", "B6", "B5"], values: [6, 3, 4, 8, 5, 9] },
  { name: "-O2 peeling", labels: ["12_invariant", "21_folding", "20_guards", "B2", "B6", "B5"], values: [3, 3, 4, 3, 5, 9] }],
  { x: 0.6, y: 1.35, w: 6.6, h: 5.3, barDir: "col", barGrouping: "clustered", chartColors: ["2A78D6", "EB6834", "1BAF7A"],
    showValue: true, dataLabelPosition: "outEnd", dataLabelFontSize: 10, dataLabelColor: "16222E", dataLabelFontFace: "+mn-lt",
    valAxisLabelColor: "52514E", catAxisLabelColor: "16222E", valAxisLabelFontFace: "+mn-lt", catAxisLabelFontFace: "+mn-lt",
    valAxisLabelFontSize: 11, catAxisLabelFontSize: 11, valGridLine: { color: "E4E3DF", size: 0.75 }, catGridLine: { style: "none" },
    showLegend: true, legendPos: "t", legendFontSize: 12, legendFontFace: "+mn-lt", showTitle: true, title: "Operations per loop iteration",
    titleFontSize: 13, titleColor: "16222E", titleFontFace: "+mn-lt", valAxisMaxVal: 10 });
card(s, 7.55, 1.45, 5.2, 2.55, "Beyond the approved scope", bullets(["Loop peeling from Ardö, Bolz & Fijałkowski (DLS 2012)",
  "Two native ISAs incl. Apple Silicon MAP_JIT", "Linear-scan register allocation, Phi coalescing"], 14), { size: 14 });
card(s, 7.55, 4.2, 5.2, 2.45, "Next steps / publication", bullets(["Side traces and trace trees; allocation removal",
  "Workshop paper: \"Loop peeling in a minimal tracing JIT: a two-ISA evaluation\""], 14), { size: 14 });
s.addNotes("Member 1. The chart shows where each optimisation level removes work. Our adaptation of the paper uses Phi nodes and a fix-point instead of PyPy's jump arguments. A small reproducible artefact that isolates peeling, register allocation and micro-architectural effects on two ISAs is publishable as a workshop or student-research paper.");

// ============================================================ 17 Conclusion
s = slide("DARK", "Closing");
s.addText("Conclusion", { x: 0.8, y: 0.6, w: 11.5, h: 0.8, fontSize: 34, bold: true, color: "FFFFFF", fontFace: "Cambria", isTextBox: true, margin: 0 });
const concl = [["All 8 objectives", "achieved, including two extensions beyond the approved scope"],
  ["From recorder to JIT", "the Review 2 trace is now optimised, compiled to ARM/x86 code and deoptimised safely"],
  ["Correct by construction", "0 mismatches in 7,040 differential runs; 210 / 210 checks on two CPUs"],
  ["Key learning", "test infrastructure first, then optimise; measure every change, including simplifications"]];
concl.forEach((c, i) => {
  const y = 1.7 + i * 1.2;
  s.addShape(pres.shapes.OVAL, { x: 0.8, y: y + 0.08, w: 0.5, h: 0.5, fill: { color: HEX.accent3 }, line: { color: HEX.accent3 }, objectName: "check " + i });
  s.addText("✓", { x: 0.8, y: y + 0.08, w: 0.5, h: 0.5, align: "center", valign: "middle", fontSize: 18, bold: true, color: "FFFFFF", isTextBox: true, margin: 0 });
  s.addText([{ text: c[0] + " — ", options: { bold: true, color: "FFFFFF" } }, { text: c[1], options: { color: "CADCFC" } }],
    { x: 1.55, y, w: 11, h: 0.7, fontSize: 19, isTextBox: true, margin: 0, valign: "middle" });
});
s.addNotes("All members: one sentence each on what they learned. Then invite questions.");

// ============================================================ 18 References
s = slide("CONTENT", "Closing");
title(s, "References and acknowledgements"); badge(s, "M4 · Tanvi Gupta");
s.addText([
  "H. Ardö, C. F. Bolz, M. Fijałkowski, \"Loop-aware optimizations in PyPy's tracing JIT,\" DLS 2012, pp. 63–72.",
  "V. Bala, E. Duesterwald, S. Banerjia, \"Dynamo,\" PLDI 2000.  A. Gal, C. Probst, M. Franz, \"HotpathVM,\" VEE 2006.",
  "A. Gal et al., \"Trace-based just-in-time type specialization for dynamic languages,\" PLDI 2009.",
  "C. F. Bolz et al., \"Tracing the meta-level: PyPy's tracing JIT,\" ICOOOLPS 2009; \"Allocation removal…,\" PEPM 2011.",
  "M. Poletto, V. Sarkar, \"Linear scan register allocation,\" ACM TOPLAS 21(5), 1999.",
  "U. Hölzle, C. Chambers, D. Ungar, \"Debugging optimized code with dynamic deoptimization,\" PLDI 1992.",
  "Aho, Lam, Sethi, Ullman, Compilers: Principles, Techniques, and Tools, 2nd ed., 2006.",
  "Intel 64 and IA-32 SDM; Arm ARM + AAPCS64; Apple \"Porting JIT compilers to Apple silicon\".",
].map((t, i, arr) => ({ text: t, options: { bullet: { type: "number" }, breakLine: i < arr.length - 1, fontSize: 14, paraSpaceAfter: 6 } })),
  { x: 0.6, y: 1.45, w: 12.15, h: 4.3, color: HEX.dk1, isTextBox: true, margin: 0, valign: "top" });
s.addText("No third-party code or libraries beyond the C++ standard library and POSIX. AI assistance (Claude) was used during development and documentation; the team reviewed, tested and owns all submitted work.",
  { x: 0.6, y: 5.9, w: 12.15, h: 0.8, fontSize: 13, italic: true, color: "52514E", isTextBox: true, margin: 0 });
s.addNotes("Ethical acknowledgement of reused resources, as the guidelines require. Full reference list is Section 14 of the report.");

// ============================================================ Backup slides
s = slide("CONTENT", "Backup");
title(s, "Backup: deoptimisation restores the exact state"); badge(s, "Backup · M4");
code(s, "$ ./build/jitvm -vv tests/programs/22_side_exit_restores_state.jbc\n[PROFILER] Hot loop detected at pc 6 (line 10)!\n[RECORDER] Trace recording complete! Captured 7 IR instructions.\n[BACKEND] Trace #0 compiled with AArch64 (316 bytes of machine code), 5 exits.\n[EXEC] Entering trace #0 at pc 6 (line 10)\n[DEOPT] Trace #0 exit3 after 72 loop iterations -> resume interpreter at pc 12\n        (restored R0=77 R3=149226 R4=-2926 R5=5929)", 0.6, 1.45, 12.15, 2.7, 13);
s.addText(bullets(["Each guard owns a snapshot: (VM register → SSA value) + resume pc", "Exit stub stores those values into the VM register file, saves the fuel counter, returns the exit index",
  "Inside the loop the VM register file is never written — only at exits", "Verified against the interpreter at -O0/-O1/-O2 on both backends"], 16),
  { x: 0.6, y: 4.4, w: 12.15, h: 2.3, color: HEX.dk1, isTextBox: true, margin: 0, valign: "top" });
s.addNotes("Backup for viva questions on deoptimisation.");

s = slide("CONTENT", "Backup");
title(s, "Backup: benchmark table (median of 7 runs)"); badge(s, "Backup · M4");
const hdr = ["Benchmark", "AArch64 IR -O2", "AArch64 native -O1", "AArch64 native -O2", "x86-64 native -O1", "x86-64 native -O2"].map(t => ({ text: t, options: { bold: true, color: "FFFFFF", fill: { color: HEX.dk2 } } }));
const xr = bench.x86_64;
const rowsB = bnames.map((n, i) => [blabels[i], a[n]["JIT IR-executor -O2"].toFixed(1) + "×", a[n]["JIT native -O1"].toFixed(1) + "×",
  { text: a[n]["JIT native -O2"].toFixed(1) + "×", options: { bold: true } }, xr[n]["JIT native -O1"].toFixed(1) + "×", { text: xr[n]["JIT native -O2"].toFixed(1) + "×", options: { bold: true } }]);
s.addTable([hdr].concat(rowsB), { x: 0.6, y: 1.45, w: 12.15, colW: [2.15, 2.0, 2.0, 2.0, 2.0, 2.0], fontSize: 15, color: HEX.dk1, align: "center",
  border: { type: "solid", pt: 0.5, color: "C8D3DE" }, rowH: 0.5 });
s.addText("Speed-up relative to the interpreter on the same machine. Raw data: results/bench_AArch64.csv, results/bench_x86_64.csv.", { x: 0.6, y: 6.25, w: 12.15, h: 0.4, fontSize: 13, italic: true, color: "52514E", isTextBox: true, margin: 0 });
s.addNotes("Backup: complete numbers for both machines.");

s = slide("CONTENT", "Backup");
title(s, "Backup: the bytecode language"); badge(s, "Backup · M2");
code(s, "      LOAD_CONST R0, 0        ; i\n      LOAD_CONST R1, 1\n      LOAD_CONST R2, 1000000\n      LOAD_CONST R3, 0        ; sum\nloop: MUL_INT    R4, R0, R0\n      ADD_INT    R3, R3, R4\n      ADD_INT    R0, R0, R1\n      JUMP_IF_LT R0, R2, loop\n      PRINT_REG  R3\n      HALT", 0.6, 1.45, 5.6, 4.0, 14);
s.addTable([[{ text: "Opcodes", options: { bold: true, color: "FFFFFF", fill: { color: HEX.dk2 } } }, { text: "JIT", options: { bold: true, color: "FFFFFF", fill: { color: HEX.dk2 } } }],
  ["LOAD_CONST, ADD/SUB/MUL/AND_INT, MOV", "compiled"], ["JUMP_IF_LT, JUMP_IF_EQ", "guards"], ["JUMP", "straightened"], ["PRINT_REG", "helper call"],
  ["DIV_INT, UNKNOWN_OP", "interpreter only"], ["HALT", "ends recording"]],
  { x: 6.6, y: 1.45, w: 6.15, colW: [4.1, 2.05], fontSize: 15, color: HEX.dk1, border: { type: "solid", pt: 0.5, color: "C8D3DE" }, rowH: 0.52 });
s.addText("Registers R0–R15 · 32-bit wrap-around · labels, @index or numeric targets · comments ; # //", { x: 0.6, y: 5.7, w: 12.15, h: 0.5, fontSize: 14, color: "52514E", isTextBox: true, margin: 0 });
s.addNotes("Backup: use this program as a ready-made evaluator input if needed (save it as demo_input.jbc).");

pres.writeFile({ fileName: OUT }).then(async () => {
  await applyTheme(OUT, THEME);
  console.log("written", OUT);
});
