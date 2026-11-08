# R4-43 — Same-target differential replay and point compaction

Keep the diagnostic, do not activate point compaction. R4-41's separate-context
reconstruction differed from the actual game by 194/204 bytes. This experiment
saves/restores the actual color target on GPU and compares each replay there,
with the native program, attributes, vertex buffer and triangle order retained.
It does not replace a normal draw path, object update, trigger or gameplay step.

Before any diagnostic native target write, both private color copies must match the native
before/after RGBA8 images exactly. Unsupported state, targets, pack buffers,
formats and storage reject. Restoration is checked after the experiment, with
a native redraw recovery path. Depth/stencil are untouched. Two private targets
and the optional index buffer are released in the owning context. The diagnostic
adds a native replay/draw counter increment; its frames are excluded from FPS.

Owned checks: 12 RGB/RGBA/linear/sRGB cases, later extended to 14 with alternate
draw executed/declined paths. Five unsafe/native fallbacks, poisoned compaction
mutant detected; final RGBA and touched GL bindings match. No GL errors in the
owned fixture. Nonfinite/large positions, restart, smoothing, active feedback,
non-default pack state and uncertain shader contracts are conservative cases.

Native Heliopolis 74–78: one 2,009-quad batch at frame7294, 74.00447%, ordinal8.
977 point quads removed, zero different bytes for before/after copies, native
baseline replay, compacted drawing and final native restoration. This is a
non-vacuous same-batch result, not whole-level visual certification. Attached
shader source checks are not a universal proof of linked executable identity.
Package26eccb456e048f038d9ba7f9d654d3cc20adfabfaa8fa06a0178e1449b3e3422;
DLL8e15445df824b28bdbfe34ede33ddc97db1d247cda4d2f4dc5f2e6bca3776730.
Target78 reached, zero deaths, 92 shared files restored/hash verified; installed
and normal packages unchanged. No screenshot or production installation.

Negative isolated timing on the earlier 2,005-quad dataset: removed974 points,
GPU span paired ratio1.00320 (3/9 wins), CPU issue1.57014 (0/9), wall1.01966
(4/9). GPU medians0.726416/0.729653ms; wall1.3702/1.404ms. CPU scan/index upload
is included; allocation is outside the loop. Native-helper image calibration
still differs204 bytes, so the helper exits failure despite candidate=helper
baseline. These are unvalidated replay kernel times, never game frametimes.

Decision: reject point compaction as a performance feature on this evidence.
Retain the actual-target verification boundary for further GPU experiments.
