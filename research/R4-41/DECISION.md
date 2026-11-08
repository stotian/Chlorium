# R4-41 — Ordered opaque occlusion (isolated, not promoted)

Hypothesis: preserve native painter order, use an ordinal depth prepass to reject
fragments hidden by later fully opaque samples. Additive draws, unsupported MVP,
clipping/state and uncertain alpha must retain the native path. Alpha holes must
never write depth: a forced early-fragment-tests mutant demonstrates why.

Owned hidden GPU fixture: 129 exact RGBA8 cases, 68 direct fallbacks; forced
opaque classification, early transparent depth and zero-MVP mutants detected.
Integer-only UV and depth guards cover NaNs/Inf/subnormals/FP flags, 20,000 UV
queries and 100,000 exact depth bounds. Dense artificial overlap reduces main
samples 2,097,152 to 16,384. These are fixture results, not GD FPS.

Native Heliopolis 74–78 input census disproves whole-quad opacity usefulness:
zero whole opaque UV footprints in the observed batch set. Partial alpha-255
regions exist, so a second prototype samples/discards in the depth prepass.
Texture bytes are stable across both observed frames. Four textures, standard
native shader, no multisampling/depth/stencil and linear clamped sampling.

Actual batch framebuffer pair (private r41-helio-gpu-inputs-r4): 2,005 quads,
74.00149%, ordinal 8. Captured GPU VBO matches CPU/capture bytes exactly; IBO
matches native 16-bit triangles, attributes/uniforms are checked, and the actual
8,131-byte linked program binary reloads successfully in the helper.
Package SHA256 71ed1ad4e69aba85a141d0166134285806995ccb1b4c566654522fa529dcb8bf;
DLL 2bb9dab165a991b5952dfea411d60224f0bb903bc4e3e5367d4cbb2b57fd1c20.
Dataset 15a68ac0b84c523ae1b38c1993e581f1ac1bd8ad1b14ff07fdf99c1cf1dab798.

Negative fidelity result: helper baseline differs from actual native pixels in
204 bytes/138 pixels; candidate equals helper baseline exactly. Texture-target
linear/sRGB, dither and matching GL 4.6 probes do not resolve it. A hidden default
window probe is substantially worse and is discarded. This is NOT native pixel
parity and must not authorize activation or a fidelity claim.

Isolated unvalidated replay timing (nine alternating pairs, copies included):
native/candidate GPU medians 0.725669/1.93108 ms, paired ratio 2.62872, 0/9 wins;
wall medians 1.4079/2.7163 ms, paired ratio 1.92316, 0/9 wins. CPU issue ratio
1.84052. This is not a frame benchmark or a game performance estimate. The
additional prepass/copies lose despite good synthetic overlap results.

Decision: reject this alpha-prepass variant for the observed native batch class.
Keep research sources and negative evidence; no runtime optimized renderer,
native fallback replacement or normal-profile integration. The unresolved native
pixel gap remains explicit, rather than weakening its comparison threshold.

All native diagnostics reached 78% with zero deaths; each protected run restored
and verified 92 shared files and left the installed/normal package unchanged.
One cold Steam startup failed before the harness; a distinct recorded retry
succeeded. No diagnostic FPS is admissible as a performance comparison.
