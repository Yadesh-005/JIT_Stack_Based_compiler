| Benchmark | Interpreter | JIT IR-executor -O2 | JIT native -O0 | JIT native -O1 | JIT native -O2 |
|---|---|---|---|---|---|
| b1_sum_loop | 0.279 s (1x) | 0.498 s (0.56x) | 0.0156 s (18x) | 0.0157 s (18x) | 0.0157 s (18x) |
| b2_loop_invariant | 0.644 s (1x) | 0.337 s (1.9x) | 0.0204 s (32x) | 0.0198 s (33x) | 0.0105 s (61x) |
| b3_nested_loops | 0.124 s (1x) | 0.182 s (0.68x) | 0.00747 s (17x) | 0.00743 s (17x) | 0.00744 s (17x) |
| b4_fibonacci_mod | 0.434 s (1x) | 0.628 s (0.69x) | 0.0417 s (10x) | 0.0418 s (10x) | 0.0418 s (10x) |
| b5_polynomial | 0.593 s (1x) | 0.579 s (1x) | 0.0227 s (26x) | 0.0218 s (27x) | 0.0218 s (27x) |
| b6_bounds_checked | 0.54 s (1x) | 0.405 s (1.3x) | 0.0259 s (21x) | 0.0215 s (25x) | 0.0215 s (25x) |
| b7_branchy | 0.217 s (1x) | 0.336 s (0.65x) | 0.121 s (1.8x) | 0.121 s (1.8x) | 0.121 s (1.8x) |
| b8_div_not_jitable | 0.0731 s (1x) | 0.0773 s (0.94x) | 0.0773 s (0.94x) | 0.0773 s (0.94x) | 0.0774 s (0.94x) |
