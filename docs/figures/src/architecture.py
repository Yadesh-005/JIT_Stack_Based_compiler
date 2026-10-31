import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch
plt.rcParams["font.family"] = "DejaVu Sans"
fig, ax = plt.subplots(figsize=(15, 7.6), dpi=200)
ax.set_xlim(0, 7.0); ax.set_ylim(0, 3.75); ax.axis("off")
W, H = 0.98, 0.56
X = [0.6, 1.75, 2.9, 4.05, 5.2, 6.35]; TOP, MID, BOT = 2.85, 1.72, 0.66
C = {"fe": "#dbe8f5", "jit": "#e2efda", "ex": "#fde9d9", "src": "#f2f2f2"}
def group(x0, y0, x1, y1, title):
    ax.add_patch(FancyBboxPatch((x0, y0), x1-x0, y1-y0, boxstyle="round,pad=0,rounding_size=0.06",
                 fc="none", ec="#2e74b5", lw=1.3, ls=(0, (4, 2))))
    ax.text(x0+0.06, y1-0.04, title, ha="left", va="top", fontsize=10.5, weight="bold", color="#1f4e78")
def box(x, y, text, fc, dashed=False):
    ax.add_patch(FancyBboxPatch((x-W/2, y-H/2), W, H, boxstyle="round,pad=0,rounding_size=0.07",
                 fc=fc, ec="#1f4e78", lw=1.4, ls="--" if dashed else "-"))
    lines = text.split("\n")
    ax.text(x, y+0.11, lines[0], ha="center", va="center", fontsize=10, weight="bold", color="#111")
    ax.text(x, y-0.08, "\n".join(lines[1:]), ha="center", va="center", fontsize=8.2, color="#333", linespacing=1.15)
def arrow(pts, label=None, lpos=None, color="#404040", dashed=False, ha="center"):
    for i in range(len(pts)-1):
        last = i == len(pts)-2
        ax.annotate("", xy=pts[i+1], xytext=pts[i],
                    arrowprops=dict(arrowstyle="-|>" if last else "-", color=color, lw=1.4,
                                    ls="--" if dashed else "-", shrinkA=0, shrinkB=0, mutation_scale=14))
    if label:
        ax.text(lpos[0], lpos[1], label, fontsize=8.3, color=color, ha=ha, va="center",
                bbox=dict(fc="white", ec="none", pad=0.6))
group(0.04, 2.38, 3.48, 3.33, "① Front end  — Member 2")
group(3.52, 2.38, 6.96, 3.33, "② Runtime: interpreter, profiler, recorder  — Members 2 & 1")
group(2.34, 0.22, 6.96, 2.26, "③ JIT compiler  — Member 3")
group(0.04, 0.22, 2.30, 2.26, "④ Execution & deopt  — Members 1 & 4")
box(X[0], TOP, "program.jbc\ntext bytecode", C["src"])
box(X[1], TOP, "Assembler\ntwo-pass, labels,\nline diagnostics", C["fe"])
box(X[2], TOP, "Verifier\nCFG reachability,\ndefinitely-assigned", C["fe"])
box(X[3], TOP, "Interpreter\nfetch–decode–execute", C["fe"])
box(X[4], TOP, "Profiler\ncounts taken back-edges,\nhot at HOT_THRESHOLD=3", C["fe"])
box(X[5], TOP, "Trace Recorder\nguard per branch; aborts\n→ retry, then blacklist", C["fe"])
box(X[5], MID, "SSA + Loop peeling\nArö et al., DLS 2012".replace("Arö","Ardö"), C["jit"])
box(X[4], MID, "Forward opts\nfold · CSE · guard-elim\nphi fix-point · DCE", C["jit"])
box(X[3], MID, "Lower to LIR\nphi coalescing", C["jit"])
box(X[2], MID, "Native codegen\nlinear-scan RA\nx86-64 / AArch64, W^X", C["jit"])
box(X[3], BOT, "IR executor\nportable fallback", C["jit"], dashed=True)
box(X[1], MID, "Code cache\nheader patched to\nTRACE_ENTER", C["ex"])
box(X[0], MID, "Native trace\nruns until a guard\nfails (fuel check)", C["ex"])
box(X[0], BOT, "Exit stub (deopt)\nrestore snapshot,\nresume at pc", C["ex"])
hw = W/2
for a, b in [(0, 1), (1, 2), (2, 3)]:
    arrow([(X[a]+hw, TOP), (X[b]-hw, TOP)])
arrow([(X[3]+hw, TOP+0.08), (X[4]-hw, TOP+0.08)])
arrow([(X[4]+hw, TOP+0.09), (X[5]-hw, TOP+0.09)])
arrow([(X[5]-hw, TOP-0.12), (X[4]+hw, TOP-0.12)], dashed=True)
arrow([(X[5], TOP-H/2), (X[5], MID+H/2)], "loop closed:\nLevel-1 TraceIR", (X[5]+0.08, (TOP+MID)/2), ha="left")
for a, b in [(5, 4), (4, 3), (3, 2), (2, 1), (1, 0)]:
    arrow([(X[a]-hw, MID), (X[b]+hw, MID)])
arrow([(X[3], MID-H/2), (X[3], BOT+H/2)], "self-test failed /\nJITVM_NO_NATIVE", (X[3]+0.08, (MID+BOT)/2), dashed=True, ha="left")
arrow([(X[3]-hw, BOT), (X[1], BOT), (X[1], MID-H/2)], dashed=True)
arrow([(X[0], MID-H/2), (X[0], BOT+H/2)], "guard\nfails", (X[0]+0.08, (MID+BOT)/2), ha="left")
red = "#c00000"
arrow([(X[0]-hw, BOT), (0.02, BOT), (0.02, 3.52), (X[3], 3.52), (X[3], TOP+H/2)], color=red)
ax.text(1.9, 3.52, "resume interpretation (VM registers restored from the snapshot)", fontsize=8.5, color=red,
        ha="center", va="center", bbox=dict(fc="white", ec="none", pad=0.6))
plt.savefig("../architecture.png", bbox_inches="tight", facecolor="white")
