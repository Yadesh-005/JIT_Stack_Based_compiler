// optimizer.h -- SSA construction and trace optimisation.
//
// buildOptimizedTrace() converts a recorded (register-form) trace into SSA
// form and optimises it in a single forward pass per region:
//
//   * copy propagation (MOV creates no instruction),
//   * constant folding and algebraic simplification (x+0, x*1, x*0, x-x, ...),
//   * common-subexpression elimination by hash-based value numbering,
//   * redundant-guard elimination,
//   * dead-code elimination (backward liveness, snapshots count as uses),
//   * LOOP PEELING (the Review 3 novelty, after Ardo, Bolz & Fijalkowski,
//     "Loop-Aware Optimizations in PyPy's Tracing JIT", DLS 2012):
//     the trace is emitted twice -- once as a preamble (first iteration) and
//     once as the loop body -- and the same forward optimisations run over
//     both. Because the value-numbering and guard tables built while
//     optimising the preamble are still visible when the loop body is
//     optimised, loop-invariant computations and guards in the body are
//     recognised as duplicates of preamble values and disappear from the
//     loop. Loop-invariant code motion therefore falls out of passes we
//     already had, without a dedicated LICM pass. A small fix-point removes
//     Phi nodes whose back-edge value equals their entry value (registers
//     that are re-assigned the same invariant value every iteration).
#pragma once

#include "trace_ir.h"

SSATrace buildOptimizedTrace(const RecordedTrace& raw, const OptOptions& opt);
