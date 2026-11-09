# R73 — GPU transfer into authoritative native vertex storage

Rejected research profile. R67 normally writes both the coherent staging page
and canonical native VBO from the CPU. R73 instead copies the staging page into
the original canonical VBO on GPU, then executes the original native draw using
that same canonical identity. It avoids the second CPU upload and the temporary
vertex-attribute rebinding. Original capacity/tail, indices, shaders, draw order,
callbacks and CPU preparation remain native. Partial/clean/unsupported draws and
busy/missing-context pages retain native fallback. No CPU waits are introduced.

The persistent/coherent source visibility and GPU-copy permissions follow the
[Khronos ARB_buffer_storage contract](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_buffer_storage.txt).
Pages are fenced after copy/draw before reuse; the canonical buffer stays current
for additional native consumers. This changes transfer mechanics, not geometry.

Owned checks pass256 complete alpha images, exactly one GPU copy and zero duplicate
CPU uploads per admitted draw, preserved canonical identity/foreign copy bindings,
unused tail preservation, subsequent native GPU consumers, mapped-store rejection,
missing-copy fallback, foreign/missing contexts, pending GL errors and unwinding.
R67 default and R66 adaptive regressions each pass256 images as well.

Same-auditor, isolated, full-detail/no-shake120-cap Helio74–78 BAAB campaign:

| Variant | FPS | p95 ms | p99 ms | worst ms |
|---|---:|---:|---:|---:|
| R73 B1 |32.48|42.01|43.27|44.15|
| R67 A1 |35.15|35.65|37.92|99.14|
| R67 A2 |35.66|34.82|36.30|37.11|
| R73 B2 |33.90|41.57|43.09|44.58|

All four runs complete without deaths or invalid coverage, same game/level/mod
profile/auditor/settings, unchanged per-variant hashes.92 shared profile files
restored each time; main installation unchanged. Paired per-percent median p95
also regresses on all four bins: R73/R67=39.53/35.24,42.71/36.41,41.40/33.36,
28.11/25.89ms. The99ms R67 outlier is retained, not hidden; it does not offset the
consistent p95 regression and lower average FPS of the new transfer.

Decision: abandon normal activation and further level-performance campaigns for
this path. A GPU queue dependency cost is plausible, not measured causation.
Keep the small isolated prototype and negative evidence; no global/vanilla gain,
120FPS, image equivalence of whole levels or zero-black-screen claim.
Receipt: build-research/r73-helio-balanced.local.json.
