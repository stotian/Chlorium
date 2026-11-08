# R4-49 — Resident vertex reconstruction in the native rendering context

Explicit isolated pass-capture diagnostic only. No normal target, main installed
mod or gameplay callback changes. CHLORIUM_RESIDENT_NATIVE_REPLAY requires the
native/pixel replay chain, pinned local R4-33 vertex source, current package
marker and isolated mods. Other renderer/watch diagnostics cannot be combined.

R4-32 supplies the actual pre-draw sprite source records; the post-native GPU
VBO/IBO are authoritative witnesses. SpriteSources admits only exact shadow
matches in its bounded nearest domain; unsupported cases keep complete quads.
NativeReplay builds private resident geometry/parameters/order/fallback buffers
and its own reviewed-source program. The pinned standard Cocos fragment uses
color*texture, with no fixture-only factor. Native uniforms are copied by value;
no native program, atlas buffer or VAO is borrowed as owned storage.

Only NativeReplay can enter the renderer's same-target replay endpoint. In GD
it runs inside R4-43's save/baseline/candidate/restore transaction; failures after
submission never trigger an unprotected duplicate native blend. Comparison must
include an executed resident draw and positive reconstructed-quad count. All
diagnostic frames remain excluded from FPS evidence.

## Evidence so far, 2026-11-06

- R4-47 CPU contracts pass. Owned GPU fixtures pass15 exact-image cases,
  including original Cocos shader text, reconstructed geometry, native fallback,
  style-zero ONE/ZERO blending, lifecycle/updates and four foreign SSBO ranges.
- Shared R4-33 regression:25503 eligible actual-native/synthetic GPU quads,
  21676 captured-native matches,270 unsupported records, zero GPU mismatches;
  24 owned pixel cases and7558 detected opacity-mutant mismatches.
- Eight comparison guards and isolated launch guards pass. Resident readbacks
  cannot be presented as FPS comparisons, even with incorrectly omitted capture
  provenance. The first startup attempt exited before the audit harness and
  supplies no render evidence; its92 shared files were restored.
- Distinct warm-start Helio74-78 run reaches78 with zero deaths. Its actual
  580-quad batch reconstructs580 quads, zero fallback quads. Native baseline,
  resident candidate, before/after copies and final restoration all differ by
  ZERO observed RGBA bytes. renderer_comparison_passed=true; framework success.
  Audit package5b1d359f58c54aaa21604cd6df8df8a47ef07313fe928e0dec3a08a9aceb9b10;
  DLLdee2077ec0de2d817ab19a45db65bf99c43e4ad374484f419a71b535836c672c.
  Private campaign: runtime-canonical-v10-isolated-r49-resident-warm-r2-pass-capture.

This is native-context proof for ONE batch, not all effects, levels or frames.
The full native CPU preparation still executes; there is NO FPS gain claim.
Persistent updates across native frames, strict producer/CPU-reader ownership,
native group/Area migration and balanced full-run frametimes remain unfinished.
Nothing is promoted, installed into the main game, committed or published.


## R4-50 — two-snapshot retained cache, isolated diagnostic

The first captured frame seeds owned storage without framebuffer draws. The
second captured frame uses the same atlas address/UID and private GPU owner,
revalidates source shaders, publishes complete immutable sprite witnesses and
updates painter order. Absent sprites remain retained, not falsely destroyed;
capacity is bounded (at most16384), overflow/duplicates decline. A changed native
ID retires the old local token. Native preparation and all callbacks still run.
A failed snapshot disarms submission so a stale seed cannot be drawn.

Comparison requires seed success, two preparations, storage reuse, one resident
draw, reconstructed quads and exact R43 baseline/candidate/restoration. Per-lane
upload bytes refer ONLY to these private buffers, never saved native work or FPS.
The separate R50 marker prevents old one-shot R49 packages satisfying this test.

Owned GPU checks pass16 exact-image cases, including original Cocos shader,
zero uploads for an unchanged second snapshot, parameter-only uploads after a
pose change and a rejected truncated snapshot with no stale submission. CPU
contracts pass, including idempotent painter order with no revision/upload.

First native attempt r50-resident-cache-r1 reaches Helio78 but is UNQUALIFIED:
seed580 succeeds, frame2 has587 quads; stricter framebuffer guard declines before
replay because GL_BACK and GL_BACK_LEFT were compared without mono normalization.
No candidate draw, no FPS claim;92 shared files restored, main package unchanged.
Guard now normalizes only mono default attachments; distinct front/back still
reject. R43 passes14 owned image cases,9 fallbacks,1 detected mutant,2 default
attachment controls. Hidden default surfaces have no pixel-ownership guarantee:
the default controls prove preflight/state only, not image equivalence. HDR and
GL-error rejection remain mandatory; the actual GD rerun supplies image proof.


### R50 native result and rejected case

Helio74-78, r50-resident-cache-r2:580 seed to587 current quads,587 reconstructed,
zero fallback, two snapshots/same GPU owner, zero observed RGBA differences in
baseline/candidate/copies/restoration. Seed lanes74240/55680/2320/111360 bytes;
update lanes4096/30720/2348/0 bytes (geometry/parameters/order/fallback).
Society0-15, r50-resident-society-r3:420 reconstructed, zero private update
uploads, but FOUR differing pixel bytes. REJECT this path as a universal renderer.
Known initial death also rejects whole-run comparisons. R33 GPU/feedback oracle
matches all2730 captured Society quads;6557 eligible captured/synthetic quads,
zero GPU mismatches,24 owned pixel cases. Thus correct quad bytes alone do not
prove framebuffer equivalence after replacing the native graphics program.

## R51 — GPU vertex stream with the actual native linked program

NativeVertexStream uses the pinned vertex graph with rasterization disabled to
write private native-layout quad records on GPU. The subsequent draw uses an
owned VAO/index buffer and the actual native program/uniforms. No rewritten
fragment/projection program participates in this draw. Geometry/parameter/order
storage persists across snapshots. The stream owns a separate feedback object,
preserves foreign generic/indexed feedback bindings, and uses ContextEpoch before
resource deletion. A failed stream does not fall through to R50 or an unprotected
native redraw; R43 restores the target. A pending-deletion native program rejects.
The R51 environment flag/marker is explicit, opt-in, isolated and excluded from
performance comparisons even if other provenance flags are missing.

17 owned exact-image cases pass, including native program rendering and foreign
feedback state restoration; CPU,8 comparison guards and launch guards pass.
In native Society r51-native-stream-society-r1,420/420 reconstructed, zero pixel
mismatch/restoration mismatch and zero private second-snapshot uploads. The
VBO/IBO, before/after native images, linked native binary, reviewed source texts,
MVP/draw state and texture snapshots are IDENTICAL to the rejected R50 dataset.
This removes its observed4-byte mismatch, without proving which driver/compiler
operation in R50 produced it. The expected initial death remains one death:
whole-run success=false and no FPS/whole-run visual qualification is claimed.
Helio74-78 r51-native-stream-helio-r2 passes,587/587 reconstructed, exact observed
image/restoration, zero deaths and target reached.92 shared files restored after
each run; main installed package unchanged. Rendering audit4cfd0df62cbf4421db60f7638dcb7872839bf9bb75f6f13e4a5d0eb0f8d93ca4;
DLLfc125bd8ee39bbfcab4a9828437a73db8ffd67964f0aeccc41d3ded1122acee7.
Private receipts/artifacts:build-research/resident-native-replay/r51-proof.local.json
and r51-qualified-batches*. These are bounded batch proofs, NOT release packages.

Native CPU prep/callbacks still execute. The hybrid adds a GPU stream pass and
native readbacks in this diagnostic; FPS benefit is unproven and overhead can
regress performance. Other programs, effects, full runs, Grief and Orbit are not
qualified by these R51 tests. R50 stays quarantined; R51 is the retained prototype,
not activated normally. CPU-reader/FP/error/lifecycle closure remains false.
The census error-queue flag was corrected after these native runs: replay can
consume GL errors. This metadata-only change needs compilation/source-delta
checks, not an unchanged rendering test; native error transparency is NOT proven.
