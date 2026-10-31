| Benchmark | Interpreter | JIT IR-executor -O2 | JIT native -O0 | JIT native -O1 | JIT native -O2 |
|---|---|---|---|---|---|
| b1_sum_loop | 0.281 s (1x) | 0.373 s (0.75x) | 0.0108 s (26x) | 0.0128 s (22x) | 0.0109 s (26x) |
| b2_loop_invariant | 0.533 s (1x) | 0.255 s (2.1x) | 0.0211 s (25x) | 0.0212 s (25x) | 0.00712 s (75x) |
| b3_nested_loops | 0.105 s (1x) | 0.131 s (0.8x) | 0.00589 s (18x) | 0.00503 s (21x) | 0.00636 s (16x) |
| b4_fibonacci_mod | 0.35 s (1x) | 0.577 s (0.61x) | 0.0186 s (19x) | 0.0241 s (15x) | 0.0258 s (14x) |
| b5_polynomial | 0.522 s (1x) | 0.51 s (1x) | 0.0235 s (22x) | 0.0224 s (23x) | 0.0208 s (25x) |
| b6_bounds_checked | 0.521 s (1x) | 0.321 s (1.6x) | 0.0181 s (29x) | 0.0138 s (38x) | 0.0136 s (38x) |
| b7_branchy | 0.19 s (1x) | 0.264 s (0.72x) | 0.111 s (1.7x) | 0.105 s (1.8x) | 0.109 s (1.7x) |
| b8_div_not_jitable | 0.0769 s (1x) | 0.0738 s (1x) | 0.0678 s (1.1x) | 0.0668 s (1.2x) | 0.0687 s (1.1x) |
