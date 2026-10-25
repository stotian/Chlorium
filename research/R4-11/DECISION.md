# R4-11 — Phase programs versus helper-node fusion

**Decision: REWORK. No runtime accelerator promoted.**

Hypothesis A: compile repeated linear poses once in an immutable phase, then
evaluate object translations separately. The snapshot program removes repeated
composition and two native atan2 calls per repeated recipe. It exits compilation
after 1024 inputs if fewer than 25% were reused. Overflow always stays native.
Full FP-status/errno observation between operations is outside this cache
contract; it must not replace GD callbacks merely because its output bits match.

Hypothesis B: replace the temporary helper-node construction with a fused pure
pose pipeline, without any recipe lookup or dependence on duplicate poses.
The prototype models only zero anchor/skew/additional-transform helper nodes.
Raw rotations are inputs, not quantized angles. It calls the exact native CRT
trigonometry and preserves the actual Cocos/GD float operation order.
Neither prototype reads or writes real GameObject state or suppresses callbacks.

## Evidence

`run-cpu.cmd`: **1,048,611 checks passed**. Native Cocos concatenation and the
actual GD decomposition block are fingerprinted, privately copied/relocated
into RX pages. A separate Cocos helper-node oracle checks 65,536 fused poses.
Four rounding modes and DAZ/FTZ combinations, signed zeros, reflections and
negative/zero scales are exercised. Matrix/pose bits and uncached FP flags
match in the tested domains. Two initial fixture defects (CRT loading and
shadow space for the copied block) were corrected; they were not GD test crashes.
`--mutant` corrupts one coefficient and was rejected by the native comparison.

The latest seven-batch, alternated snapshot timings are in `timings.csv`.
The recipe program helps heavily repeated inputs. The fully unique case, which
initially regressed severely, is near native after early exit; short-run timing
variation does not establish an advantage there. These timings include oracle
wrapper overhead, exclude real callbacks/state publication, and are **not FPS**.
The no-cache fused pipeline takes 2544.2 us versus 2708.1 us for 16,384 inputs
in a separate nine-batch alternated fixture. Both paths time the same validated
C++ decomposition, excluding the copied-block wrapper. This approximately 6%
kernel saving excludes helper-state publication and native object callbacks;
it is insufficient evidence for a dominant architectural gain.

A bounded passive census forwards every native concatenation/callback. At most
4096 calls per rendered frame receive recipe classification; the remaining
calls are counted only. Two distinct launches, no periodic captures:

- Full user mods: process exited during loading, before the target; **invalid**
  for pose, blackscreen and FPS conclusions. Cause unknown. Low available RAM
  was logged but is not a demonstrated cause. Historical queued WER reports
  were checked and excluded as evidence about this process.
- Isolated native profile: Orbit auto 133260800 reached 97%, full detail,
  shake off, noclip off, zero deaths. Fourteen frame buckets, **1,084,638 actual
  concatenations** and 23,830 transformAreaObjects calls. Of **44,344 classified
  calls**, 13,579 repeated within their phase (30.62%); 1,040,294 calls were
  outside the classification budget. Zero table overflows or nested barriers.

The sample is a bounded prefix, not a distribution claim for the unclassified
calls. Inclusive timings are perturbed diagnostics, not a performance baseline.
Both launches restored and hash-verified all 90 protected profile files; the
installed V5 package is unchanged. No blackscreen readback was performed.

## Next discriminating experiment

Pursue **causal group programs that eliminate repeated object traversals**.
Do not install the recipe cache or pure matrix fusion as the primary solution.
Keep the validated kernels as exact reference operations for a deferred/group
representation. Decode the full native Area loop and preserve its world
writes/callback order. Prove read barriers, membership/lifetime changes and
helper-state publication before replacing a native loop. Then compare
actual frametimes against native on all three authorized levels. The current
work proves none of the required stable-60-FPS or universal blackscreen gates.
