# R4-17 - Bounded persistent transfer source

**Isolated research. No speedup or whole-level visual success claimed.**

V8 complete-store reconstruction regressed on Grief. This competing protocol
preserves the canonical native VBO name and storage. A coherently mapped,
three-page upload ring copies only the dirty CPU range GPU-to-GPU into that
canonical store. No unchanged head/tail copying, per-draw allocation, temporary
ownership lease, CPU shadow comparison or custom draw/program/VAO setup.
The original native draw still supplies attributes, textures and final GL state.
Clean draws require no upload. The transfer must complete preparation before
clearing native dirty; refusal keeps the original dirty upload in place.

Fences close used pages. Reuse polls with timeout zero; an unavailable page
causes native fallback, not a CPU wait. Default storage is 3 x 1 MiB, with a
hard 3 x 8 MiB maximum. Context generations own resources and govern deletion.
Mapped native targets, active transform feedback, invalid ranges/contexts,
foreign native atlas/getter/bind callbacks and unsupported GL fall back.
No native object/group/gameplay preparation is removed by this transport.

1162 real GPU checks passed in the first fixture, including 256 randomized
partial byte comparisons, preserved canonical GPU-only bytes, pointer identity,
actual Cocos draw pixels, wrap and binding restoration. Forced unavailable-page
reuse is rejected; removing that guard makes the adversarial fixture fail.
The private mutant is not a packaged profile. These checks establish transfer
invariants, not a GD FPS improvement. The candidate and independent audit are
built with separately fingerprinted source archives for a fresh measured pair.

Primary API contract: coherent writes precede subsequent GL commands and pages
must not be overwritten while GPU reads remain in flight:
https://wikis.khronos.org/opengl/Buffer_Object_Streaming
https://wikis.khronos.org/opengl/GLAPI/glBufferStorage

V9 removes the stock-program restriction because it retains the exact original
draw and canonical vertex/index identities; it does not reinterpret attributes
or uniforms. Raw unregistered hooks remain outside the proved native contract.
No installation or promotion until supported by measured game results.


The first full-mod Grief 0-40% pass reached its target, zero deaths, 80357
transfers (1126264512 bytes), 18 native fallbacks, all 18 due to busy pages,
and zero inactive frames. It is **not a passing performance result**: the
legacy validator requires zero fallbacks and rejects the intended bounded
hybrid policy. Original reports are not rewritten. Both profiles restored.

A separate R2 audit explicitly validates typed V9 fallback accounting. Its
binary marker and source archive distinguish this policy from the legacy one.
13 CPU checks accept the known 18 accounted fallbacks only with the new policy;
missing typed state, unaccounted fallbacks, inactive renderer, zero work and
wrong ABI versions remain failures. The same measured frames retain native
fallback costs. A fresh native/candidate pair with this auditor is required.


## Measured R2 result: no promotion

Fresh full-mod Grief 0-40% pair with the typed auditor: 3805/3749 accepted
frames, zero deaths, target reached, zero inactive renderer frames. Native
57.539 FPS versus V9 56.626 FPS; p95 19.985/25.829 ms, p99 23.288/28.913 ms.
Native worst 999.632 ms versus V9 36.778 ms; the isolated native stall prevents
a stability gain claim. Slow-frame fractions 49.93% / 58.15%. Scene mean
8.911/10.680 ms. 79233 transfers, 1112737920 bytes, 15 accounted busy fallbacks.
All shared files restored, main package unchanged. Per-percent CSV retained.

The observed 1.6% mean regression is too small for single-pair causal attribution,
but there is no demonstrated benefit and tails are worse in this passage.
Do not promote; do not spend more GD runs tweaking upload transport alone.
Keep the coherent/fence and native authority components for future architectures.
The next architecture must remove native preparation work, not add transport
after it. Complete blackscreen freedom, 60 FPS everywhere and 3x remain unproved.
