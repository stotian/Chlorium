# R4-46 — Native atlas callback under an owned fragment program

Keep isolated. R4-44 demonstrated a zero-output-discard shader, but its direct
draw did not establish a complete native handoff. Program.withNativeDraw now
temporarily binds the owned shader, copies native MVP/sampler and calls the
original atlas operation ONCE. Native texture lookup, dirty handling, upload,
vertex attributes, indices and draw/statistics remain native. No CPU quads,
atlas storage or object/gameplay updates are replaced or suppressed.

Preconditions: exact Cocos2.2081 PE and atlas body beyond the owned entry detour;
native CCTexture2D::getName must be the exact four-byte integer getter. Isolated
mods only; foreign atlas/getter/helper hooks reject. Standard reviewed shaders,
sampler0, original program/context, native vertex layout and the existing render
state contracts are retained. The original program is borrowed for the bounded
probe: persistent use still needs lifetime/relink/generation ownership proof.

CPU state is part of the boundary: shader preparation preserves incoming MXCSR
and LastError. Those values are restored BEFORE the native callback; its outgoing
values survive shader restoration, including an exception. Wrong sampler and
unprepared cases invoke no callback and preserve state. The owned program must
actually be bound before a callback can be reported as executed.

Owned native Cocos oracle: atlas/texture byte fixtures call the actual full
native atlas routine and the native integer texture getter, with genuine GL
buffers. Dirty upload runs and clears its flag on both paths. Native/candidate
images match the actual captured420-quad Society batch exactly. Exactly-once,
callback exception, direct-vs-wrapped FP/LastError and foreign virtual-callback
rejection controls pass. No Geode/game process is loaded by this helper.

Latest nine-pair isolated replay (not frame timings): GPU-span medians1.27837 /
1.11162ms, paired ratio0.861340,9/9 wins; wall1.9793/1.8078ms, ratio0.918878,
9/9 wins. CPU issue0.0786/0.0917ms, ratio1.17312,1/9 wins. Native uploads and
per-call state guards are included; program creation/residency is outside. GPU
spans may include command-feeding gaps. No whole-game FPS gain follows.

Actual-target probes: Society opening420 quads, Heliopolis around74%585 quads,
Society around10%474 quads. Owned shader executed through exactly ONE native
atlas callback; baseline/alternate/copies/final restoration each differ by ZERO
bytes. The474-quad lot has no removable points, so the shader proof does not
depend on point compaction. Helio reaches78 with zero deaths; both Society
passes reach15 after their unchanged startup death and remain invalid whole-
pass FPS comparisons.92 shared files restored after every run; main untouched.

Validation correction: complete CPU capture alone cannot make the renderer probe
pass. Missing requested shader execution, multiple native calls, any mismatch,
incomplete baseline or failed restoration now fail the final diagnostic while
retaining its raw evidence. Owned poisoned-compaction and declined-alternate
controls verify rejection. Active occlusion/primitive/transform-feedback queries
reject replay before target writes; a live occlusion-query control passes.
The owned suite passes14 image cases,6 fallbacks and the deliberate bad mutant.

No normal activation, installation, commit, push or publication. Still missing:
independent full-pass FPS/frametime A/B runs, complete effect/pass coverage,
program/context lifecycle and personal-mod profile validation. This bounded
handoff is a tested building block, not a persistent production renderer.


Final renderer-gate end-to-end run: Heliopolis74-78,580-quad lot, exactly one
native handoff, requested shader bound/executed, zero different bytes throughout.
`renderer_comparison_passed=true`, full diagnostic exits success, target78 with
zero deaths. Package793466ee285b27570ac1a4298a1c6258328a096d94666cd916a49d5a8af8a834;
DLLab1931a7612738eba4124d1d51eeffc94898a23c722fe69ae023422ff1e3f8ed.
This remains a diagnostic, with281/281 observed frames excluded from FPS.

User clarification, 2026-11-04: Society's immediate first-attempt death is expected
level behavior, not a renderer defect. It does not change the diagnostic FPS
exclusions above. Future A/B runs must use the same warm-up/reset policy and
measure the following attempt; no gameplay workaround is needed.
