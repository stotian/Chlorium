# R78 P0 — live integration and canonical VBO fallback

**Status: P0 implemented, locally verified, and live-smoke verified; experimental only.** R78 remains
behind the dedicated `--live-root-r78` profile. The normal CMake configuration is
unchanged and continues to use the canonical renderer.

## Call path and state contract

The R78 source path is `build_quad_candidate.py --live-root-r78` →
`R4-57/ContinuousRenderer.cpp` → `R4-72/RotationTrace.cpp::livePrepare` →
`R4-78/LiveDraw.cpp::prepare` → `CanonicalWrite::prepare` → the native Cocos
`drawNumberOfQuads` call. Only a drawn, standard-texture, full-atlas range with
valid buffers enters this path. Partial ranges and unsupported paths retain the
existing native/direct route.

On a verified GPU write, the native Cocos draw runs once with the canonical VBO
visible and atlas dirtiness suppressed; successful return is then accounted by
`submitted()`. On `Status::native`, the atlas is marked dirty before the native
draw, so Cocos refreshes canonical `m_pQuads` even if the atlas began clean. This
also repairs poison left by a failed dispatch/readback. No GL error is consumed to
infer draw success; accounting means the native draw call returned.

## Evidence

- Local Windows fixture: 128 full-image comparisons through the pinned native
  Cocos draw function; all images matched exactly, including the native-only
  middle quad. Eight deliberately denied GPU dispatches fell back and restored
  canonical VBO bytes.
- Covered clean/dirty success and failure, failure before and after GPU mutation,
  partial draw, contextless decline, repeated failure followed by success,
  consecutive batches, single native submission, GL bindings/state and pending
  error preservation. The R78 fixture script also runs the frame and source
  contract checks.
- R78 native and measurement DLL builds: compile and link passed for the
  experimental profiles. A targeted live smoke on Grief auto `88952972`,
  section 25–26%, reached the requested section and exercised both R78 frames:
  `drawn_mask=3`, 12 native submissions, 30 quads, 381,984 verified bytes,
  zero failures and zero declines. The previous run exposed a shader resource
  path mismatch; both shader loads now use Geode's packaged resource root.
  Diagnostic instrumentation records the failing stage/message if setup fails.
  The isolated test restored the user's settings byte-for-byte and removed its
  staged packages.
- CI now
  runs portable dirty-state and source-contract checks; the hidden-context Cocos
  fixture depends on local SDK/Cocos binaries and is not a CI test.
- Independent one-pass critical review: **ACCEPT**, no P0/P1 findings.
- `git diff --check`: passed. Live validation is limited to the smoke section
  above; it does not prove full-level visual fidelity or stability.
  FPS improvement: **NOT MEASURED**.

## Limits and decision

This proves the bounded fixture contract and that the experimental path can
prepare and submit canonical geometry in a live level section. It does not prove
compatibility with all atlases/mods, full-level stability, black-screen
elimination, or performance. Keep R78 experimental and out of the normal build
until broader balanced validation passes.
