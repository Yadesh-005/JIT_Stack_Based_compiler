#!/usr/bin/env python3
"""plot_results.py -- regenerate the report figures from results/*.csv.

usage: python3 results/plot_results.py      (needs matplotlib)
writes docs/figures/speedup.png and docs/figures/loop_body_ops.png
"""
import csv
import os

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
FIG = os.path.join(HERE, "..", "docs", "figures")
os.makedirs(FIG, exist_ok=True)
plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 10})
INK, MUTED, GRID = "#0b0b0b", "#52514e", "#e4e3df"
SERIES = ["#2a78d6", "#eb6834", "#1baf7a"]  # categorical slots 1-3 (validated all-pairs)

LABELS = {
    "b1_sum_loop": "B1 sum loop", "b2_loop_invariant": "B2 loop-invariant", "b3_nested_loops": "B3 nested loops",
    "b4_fibonacci_mod": "B4 Fibonacci", "b5_polynomial": "B5 polynomial", "b6_bounds_checked": "B6 bounds checks",
    "b7_branchy": "B7 branchy", "b8_div_not_jitable": "B8 DIV (not JIT-able)",
}


def load(arch):
    rows = {}
    with open(os.path.join(HERE, f"bench_{arch}.csv")) as f:
        for r in csv.DictReader(f):
            rows.setdefault(r["benchmark"], {})[r["configuration"]] = float(r["speedup_vs_interpreter"])
    return rows


def speedup_chart():
    configs = [("JIT IR-executor -O2", "IR executor -O2"), ("JIT native -O1", "native -O1 (no peeling)"),
               ("JIT native -O2", "native -O2 (loop peeling)")]
    arches = [("AArch64", "Apple Silicon (AArch64)"), ("x86_64", "Intel Xeon (x86-64)")]
    fig, axes = plt.subplots(1, 2, figsize=(12, 5.2), dpi=200, sharey=True, gridspec_kw={"wspace": 0.08})
    for ax, (arch, title) in zip(axes, arches):
        data = load(arch)
        names = list(LABELS)
        h = 0.26
        for i, (cfg, lab) in enumerate(configs):
            ys = [k + (i - 1) * h for k in range(len(names))]
            vals = [data[n][cfg] for n in names]
            ax.barh(ys, vals, height=h - 0.03, color=SERIES[i], label=lab, zorder=3)
            if i == 2:
                for y, v in zip(ys, vals):
                    ax.text(v * 1.08, y, f"{v:.1f}×", va="center", fontsize=8, color=INK)
        ax.set_xscale("log")
        ax.axvline(1, color=MUTED, lw=1, ls="--", zorder=2)
        ax.set_xlim(0.5, 150)
        ax.set_xticks([0.5, 1, 2, 5, 10, 20, 50, 100])
        ax.set_xticklabels(["0.5×", "1×", "2×", "5×", "10×", "20×", "50×", "100×"], color=MUTED)
        ax.set_yticks(range(len(names)))
        ax.set_yticklabels([LABELS[n] for n in names], color=INK)
        ax.grid(axis="x", color=GRID, zorder=0)
        for s in ("top", "right", "left"):
            ax.spines[s].set_visible(False)
        ax.spines["bottom"].set_color(GRID)
        ax.tick_params(length=0)
        ax.set_title(title, loc="left", fontsize=11, weight="bold", color=INK)
        ax.set_xlabel("speed-up vs. interpreter (log scale, median of 7 runs)", color=MUTED)
    axes[0].invert_yaxis()  # shared y: invert once
    axes[0].legend(loc="lower center", bbox_to_anchor=(1.02, -0.3), ncol=3, frameon=False)
    fig.subplots_adjust(wspace=0.08)
    fig.savefig(os.path.join(FIG, "speedup.png"), bbox_inches="tight", facecolor="white")


def ops_chart():
    rows = list(csv.DictReader(open(os.path.join(HERE, "loop_body_ops.csv"))))
    fig, ax = plt.subplots(figsize=(9, 4.2), dpi=200)
    w = 0.26
    levels = [("O0", "-O0 (SSA only)"), ("O1", "-O1 fold / CSE / guard-elim / DCE"), ("O2", "-O2 + loop peeling")]
    for i, (k, lab) in enumerate(levels):
        xs = [j + (i - 1) * w for j in range(len(rows))]
        vals = [int(r[k]) for r in rows]
        ax.bar(xs, vals, width=w - 0.03, color=SERIES[i], label=lab, zorder=3)
        for x, v in zip(xs, vals):
            ax.text(x, v + 0.15, str(v), ha="center", fontsize=8, color=INK)
    ax.set_xticks(range(len(rows)))
    ax.set_xticklabels([r["program"] for r in rows], rotation=12, color=INK, fontsize=9)
    ax.set_ylabel("operations executed per loop iteration", color=MUTED)
    ax.grid(axis="y", color=GRID, zorder=0)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    ax.tick_params(length=0)
    ax.legend(frameon=False, loc="upper center", bbox_to_anchor=(0.5, 1.13), ncol=3, fontsize=9)
    ax.set_ylim(0, 10.5)
    fig.tight_layout()
    fig.savefig(os.path.join(FIG, "loop_body_ops.png"), bbox_inches="tight", facecolor="white")


if __name__ == "__main__":
    speedup_chart()
    ops_chart()
    print("figures written to", os.path.normpath(FIG))
