# R4-33 — Resident native geometry and animated parameters

KEEP ISOLATED. No normal renderer, installed package or gameplay path uses this
prototype. GD 2.2081 / Geode 5.11.2, branch research/area-pose-v6, HEAD 5678dcc.

## Hypothesis and implementation

R4-32 showed stable local geometry/UVs despite frequent matrix changes. Instead
of another whole-quad upload protocol, this experiment reconstructs positions
on the GPU from resident local rectangles and per-sprite parameters. A separate
64-byte geometry lane, 48-byte parameter lane and native-order slot indirection
are replayed across two actual captured frames. Changed geometry/UVs invalidate
their resident entries; unsupported sprites retain complete native quads.

The CPU oracle executes the actual owned Cocos updateTransform body with closed
fixture callbacks and no atlas/children. It gates timestamp, image size and the
entire 0x719-byte body hash. SystemInputGuard pins the system input library so
this helper cannot accidentally load Geode. This tests its arithmetic branch,
not arbitrary native callbacks, parent composition or complete GD ownership.

The native arithmetic graph uses offsetPosition directly, does NOT subtract
anchor again, negates C then subtracts its rounded product, and preserves the
native corner-specific addition order. Real opacity at byte 0x149, visibility
and inherited hiding can zero positions. Vertex modifiers/DontDraw reject.

Eligible arithmetic operands are signed zero or finite magnitudes in
[2^-40,2^32], with finite Z and masked FP exceptions. Local sums have nonzero
minimum 2^-63; products have minimum 2^-103, hence cancellation cannot create
a nonzero result smaller than 2^-126. A constructed case reaches FLT_MIN exactly.
Out-of-domain/unmasked controls reject without changing output or FP state.
GPU eligibility assumes nearest rounding. CPU comparisons additionally cover
all four roundings and all four DAZ/FTZ settings, including sticky FP state.

## Evidence and critique

- Heliopolis 74%, Grief 24/32/64%, Orbit 83%, Society 0% have complete two-frame
  V3 captures. Supported coordinates match the actual captured native quads.
  CPU fixtures add 4097 deterministic adversarial cases. Compute and resident
  vertex feedback compare all 96 bytes/quad; output is poisoned before execution.
- Resident replay preserves actual resource identities across changing native
  atlas slots and checks every quad of both frames, including native fallbacks.
  Reversed order indirection is checked separately without rebuilding geometry.
- Five transition cases cover becoming unsupported, becoming supported, changed
  UV/colors, changed rectangle and zero opacity. Review found and fixed a missing
  initialization when a previously supported entry became a zero native quad.
- 24 exact RGBA8 pixel scenarios use actual-Cocos fixture geometry, patterned
  alpha textures, blend modes, culling, clipping, camera shear and subranges.
  An intentionally zero MVP must differ from the visible result. These are
  offscreen fixture pixels, NOT full GD visual/black-screen validation.
- Actual driver: Intel UHD Graphics, OpenGL 4.3.0, build 31.0.101.2141.
- Heliopolis second-frame replay sends geometry10112 + parameters516240 +
  order42836 bytes, versus a THEORETICAL full-quad volume1028064. These are
  counted fixture uploads, not measured savings in GD's actual upload stream.
  CPU encoding/map scans, geometry seeding, retirement and GPU work also cost.
- The isolated alternating timing check retains the same position arithmetic
  and output checksum in BOTH modes. Nine measured batches of16 iterations per
  mode: direct median0.584625ms, resident0.694806ms. Thus this version REGRESSES
  the closed fixture when native preparation is retained. Raw CPU issue/GPU
  draw/batch times: build-research/sprite-corner-oracle/
  helio-resident-timed-sink.local.json.kernel.csv. NOT gameplay frametimes/FPS.

Capture runs restored92 shared files with SHA checks and left the main package
unchanged. Orbit had two deaths before completion; Society reached15% after one
startup death. Both are explicitly invalid performance/visual comparisons.
Society139648964 was added to the exact audited allowlists at the user's request.
Normal package SHA remains107b945ce4940d49670c1aab287411e4f10ebcf45ec52174651a0d876d7f2a71.

## Decision and next discriminant

Do not promote or claim any global gain. Less upload traffic alone is inadequate;
the shader adds work while native per-object preparation is still paid. No live
sprite callback, dirty propagation, atlas preparation, group/Area operation or
CPU pose is skipped. Persistent production lifetime management, GL state/uniform
borrowing, effects, other mods, engine CPU consumers and balanced game frametimes
remain unproven. R4-34 investigates actual CPU reads before considering a render-
only lane; direct field consumers must not observe stale native quad positions.

Build with build.cmd / build-gpu.cmd. gpu_corner_checks.exe takes owned Cocos DLL,
V3 capture, corners.comp, resident.vert and a NEW receipt path; --timings is an
explicit optional kernel diagnostic. DLL search PATH must include the isolated
fixture for GLEW. No normal package or Documents export was generated here.
