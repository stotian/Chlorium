# R4-44 â€” Skip exactly zero fragment output (isolated)

Hypothesis: in a standard premultiplied blend pass with depth/stencil disabled,
an exactly zero RGBA output leaves destination color unchanged. Discarding it
after the native sample/multiply may save blend/color-buffer traffic. Alpha zero
alone is insufficient because RGB can be nonzero. The texture lookup remains
before the branch, preserving implicit derivatives.

The prototype links the existing compiled native vertex shader with a fragment
shader differing only in the zero-output branch. Native positions, texture,
blend state and draw order remain. MVP/sampler uniforms are copied to the owned
program, and current program/index binding restored after drawing. Unknown
source/attributes/uniforms, compile/link failure or a changed context/program
retain the native path. No normal renderer uses this class.

Owned replay of the actual R4-43 Heliopolis dataset: candidate equals the helper
baseline byte-for-byte. The helper still differs194 bytes from the actual native
target; its exit is intentionally failure. Thus this is NOT native fidelity or
game FPS proof. Nine alternating kernel pairs: GPU-span medians0.730815/0.676367ms,
paired ratio0.928026 (9/9 wins); CPU issue0.0628/0.0727ms, ratio1.21980 (0/9 wins);
wall1.3553/1.306ms, paired ratio0.966689 (8/9 wins). GPU spans can include command
feeding gaps; initial program linking is outside timing, per-draw uniform reads,
uploads and program switching included. No 7% whole-game improvement is claimed.

An explicit same-target native diagnostic is required before drawing a fidelity
conclusion. The alternate replay path has executed/declined checks and restores
native pixels/bindings in the owned fixture. Runtime result is recorded in the
session journal when available; no activation without balanced game frametimes
and visual coverage on Orbit, Grief and Heliopolis.


## Actual-target result

Explicit native Helio74-78 diagnostic completed with zero deaths. Same2,009-quad
batch class: native baseline,977 point removals, zero-output-discard variant and
final restoration each differ by ZERO bytes. The alternate program compiled,
linked and drew (`zero_fragment_gate=drawn`), so this is not a skipped-path pass.
Private GPU copies before/after also match exactly. This proves one actual batch,
not whole-level/all-shader equivalence or stable frame pacing.

Auditor package0642f54d5864420319914d6f9708bbf562495a047dc6122b111f36202b165e08;
DLLab89cf62609fdf0b2f16dd9f52eecfa65378172ba1b5df44c7461a538d845935.
All92 shared files restored/hash verified; main installed and normal packages
unchanged. Guard tests reject missing replay/pixel scope and the older R4-43
binary before staging. Final owned replay suite14 cases/5 fallbacks/one mutant
passes; executed and declined alternate paths preserve final image/bindings.

Decision: retain this shader prototype isolated for further measurements.
No normal-profile activation, install, push or FPS claim. Missing: balanced
alternating actual frame timings, worst spikes, Orbit/Grief coverage, multiple
native passes, all effect/shader contracts and personal-mod profile validation.


## Society auto139648964 — follow-up requested by user

Only diagnostic selection changed: minimum batch256 instead of1024, because
Society opening's largest normal-blend batch has420 quads. Shader unchanged.
Explicit actual-target test0-15%:420 quads,5 point removals, native baseline,
zero-fragment program (`drawn`), private copies and final restoration:ZERO bytes
different. Exported standalone replay also matches actual native pixels exactly.
Nine kernel pairs: GPU-span medians1.35854/1.18391ms, paired0.873165 (9/9 wins);
wall1.9465/1.8102ms, paired0.929823 (8/9); CPU issue0.083/0.1047ms,
paired1.244706 (0/9). These are local replay costs, NOT FPS/whole-frame gains.

Both the shader diagnostic and the native reference without these captures reach
15% after one startup death at0.0061235116%, x2.0930007/y104.8569. The reference
has no shader prototype installed. This reproduces the failure without the
prototype but does not establish its root cause. Both complete-run performance
comparisons are rejected; no FPS validation or all-level visual certification.

Auditor79ed4b98409781442d99b921f1a2b85c3e20e614dc65c694214037bb272e53b7;
DLLac39164b1bd548028a0b9dd0a349b4cd9b72cb98128fad33d3947b6d25146614.
92 shared files restored/verified after EACH run; main installed and normal
reference unchanged. Prototype stays isolated; no production promotion.
