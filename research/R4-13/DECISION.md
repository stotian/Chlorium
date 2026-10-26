# R4-13 — Bounded cohort compilation and SIMD settlement

**Keep as a research component. Native GD reader/callback contracts unproven.**

R4-12's merge-per-object barrier hid a large deferred debt. This revision builds
cohorts from exact membership/cursor signatures, compiles each ordered command
stream once and replays it over the cohort. Cross-object work may be batched
only in the closed, callback-free position model. Each object's command order
and float operand/operation order stay unchanged; effects are not precomposed.
SIMD handles four independent positions. Scalar/merge paths remain available.
An empty barrier is constant-time. Compilation stops after 65,536 logical
command references; the remaining state settles with the reference path.
Already completed cohorts retain consumed cursors until that fallback finishes.

106,212 model checks pass. In addition to the earlier membership, checkpoint,
lifetime and read tests, this covers a forced compilation-budget fallback,
no double application, SIMD tails, zeros, NaN/Inf, four rounding modes and all
DAZ/FTZ combinations. Corrupt ordering, dropped barriers and corrupted SIMD
results are rejected. Masked FP flags match scalar execution in the tested
fixed-control domain. Arbitrary FP observers/control changes remain unmodelled.

Nine-batch matched comparison, rotated variant order, alternating eager/IR:
32,768 objects and 2,228,224 operations. Merge debt median 22,115.9 us versus
5644.8 us for SIMD cohorts. Total SIMD median 5834.0 us, maximum 6918.4 us.
The matched eager model is 5411.7 us: **cohorts do not beat eager when the whole
world must be settled**, despite greatly reducing the original barrier cost.
The 4096-object case and scalar cohorts are retained in timings.csv. Setup and
checkpoint restoration are outside timings; every final object is compared.

No GD FPS, GPU, native Area semantics or blackscreen claim follows from this
model. Its useful result is bounded, ordered debt settlement with a sparse-read
path, not a shipped speedup. A real producer still needs complete native fields,
read barriers, lifetime semantics and publication coverage before activation.
