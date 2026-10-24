# 24 October 2026 - Isolated Area decomposition reuse (unvalidated)

Extend exact atan2f reuse to two verified Area decomposition call sites, with
separate phase caches/policies, combined failure shutdown and optional sampled
verification. Keep native updates, rendering and collisions. Require an isolated
alpha.605 package and a new revision marker after the source-only pause guard.
The attempted runtime verification crashed before any cache phase, in a pause
request from Click Assist on a detached PlayLayer. Add an experimental guard
against the null parent dereference. This guard is not compiled or executed
after the user's test-stop request. No FPS gain or visual fix is claimed;
installed alpha.6 remains unchanged. See docs/AREA-MATH-20261024.md.

# 24 October 2026 - Isolated exact affine point experiment

Add an SSE2 two-component native affine helper at one verified rotation call
site. Preserve double precision and native per-axis operation order; retain
native handling for nonfinite inputs and unmasked exceptions. Validate module
bodies/imports, preserve later import/entry owners, and isolate packaging as
alpha.604. Revision 2 removes per-object diagnostic counters from its measured
path; the native call site's disjoint stack ranges justify omitting alias
checks there. General checked use retains native aliasing side effects.
192000 result/FP comparisons, 8 overlap cases and 3 compatibility guards pass.
Three short Grief runs, including the uncounted revision, show no convincing
FPS gain. Neither experiment is installed as the normal mod.
See docs/SODIUM-RESEARCH-20261024.md for final measurements and limits.
Install only two reviewed community skills from a pinned Superpowers revision.

# 23 October 2026 - Rigorous private visual diagnostics V2

Reject invalid GPU reads instead of treating zero-filled samples as black.
Inspect partial dark regions, flag repeated signatures as candidates and drain
pending reads before judging coverage. Verify real mod inventories and reject
ambiguous run sets. Temporarily disable masking debug overlays and restore the
original settings. Add paired percentage probes with actual position/time and
explicit shared references for fragmented blink episodes. 157 GPU/state checks,
CPU signature cases and report guard checks pass. Ten valid game runs retain
the two initial partial alerts and their targeted review; a one-frame blink
difference remains a comparison limit. Normal alpha.6 rendering is unchanged.
See VALIDATION-VISUELLE-V2-20261023.md. Stable 60 FPS is not achieved.

# 0.2.0-alpha.6 - Native GPU transfers, black-screen mitigation candidate

Remove the atlas upload cache and every one of its lifecycle/draw hooks from
normal builds. Native draws now always own their original dirty flag, storage
and complete GPU upload. Retain exact native math reuse. The retired cache is
restricted to audit builds; packaging requires a native-transfer identity and
rejects the cache implementation in normal packages.

This removes Chlorium's geometry-upload suppression path, whose storage
assumptions cannot cover arbitrary external GL writes. It is a conservative
mitigation, not a demonstrated fix for every reported black screen. After
runtime testing was reauthorized, paired full-mod runs of Grief 0-40% and full
Heliopolis completed without deaths or crashes. Every detected dark episode
with alpha.6 also occurred without Chlorium nearby; no additional episode was
detected in these runs. Stable 60 FPS is not achieved. Installed locally with
the previous alpha.5 package backed up. See VALIDATION-ALPHA6-20261023.md.

Add full-stack diagnostic support with exact installed-package hashes,
verified active-mod inventory, temporary full-detail/input-assist safeguards,
and restoration of the original settings. Keep private settings and logs out
of Git. Accept historical pretty JSON inventory records without losing data.

# 23 October 2026 - Repository and diagnostic tooling

Add private-repository documentation, version records, exact upstream licence
notice, full GPL text, dependency notices and source-only Git rules. Normal
alpha.5 code is unchanged; its release package adds notices and the project URL
while retaining the measured DLL byte for byte.

In-game tests now enforce full detail and no camera shake, display the quality
settings, and distinguish section completion, full completion and deaths.
Reports combine multiple levels and compare dark episodes only across verified
conditions. Fix experimental version labels rejected by Geode. Matrix v3 has
no eligible call on Grief 22-40%; no performance improvement is claimed there.

# 0.2.0-alpha.5 - Bound cache cost in dense scenes

Bound optional atlas comparisons, copies and shadow allocation per draw and
frame. Oversized or over-budget uploads use the original native draw after
invalidating their stale shadow. Clean draws bypass cache bookkeeping. Repeated
dense changes back off progressively (120 to 1920 eligible uploads).

Reset transform caches with generation tags instead of clearing 1024 entries
per phase, defer FP control checks on empty phases, and reassess reuse throughout
each phase. Native rendering, effects and collisions remain authoritative.

Private benchmarks can now measure only a requested percentage section and
stop at its end. The level reaches it through normal gameplay. A completed
section is reported separately from a completed level. These changes require
targeted before/after measurement; no universal FPS guarantee.

# 0.2.0-alpha.4 - Exact transform math candidate

Reuse exact CRT atan2f results for identical native affine coefficients within
one transform phase. Match both input float bit patterns and floating point
controls; preserve signed zeros and bypass exceptional/underflow cases.
Stop caching a phase with little observed reuse. Native trigger execution,
object history, sections and collisions still run unchanged. The import patch
requires the verified GD 2.2081 body and original CRT import. This candidate
requires in-game verification; isolated math speedups are not FPS guarantees.

# 0.2.0-alpha.3 - Storage invalidation and reproducible diagnostics

Invalidate atlas byte shadows on native mapBuffers storage replacement, even
when the CPU pointer, capacity and GPU buffer names remain equal. A live GPU
regression reproduces stale data in alpha.2 (expected 77, observed 102) and
passes in alpha.3 (77). This fixes a demonstrated cache error; the cause of
every reported intermittent black screen is not yet established.

Keep at most one contiguous partial upload instead of fragmenting one native
upload into multiple driver calls. Exact-byte range regressions pass.

Private tests now show AVEC/SANS CHLORIUM and measured FPS. The authorized
levels are Orbit auto 133260800, Heliopolis auto 129598490 and Grief auto
88952972. Asynchronous presentation capture records dark episodes separately
from ordinary performance runs. Native comparisons distinguish dark level
effects from unexplained differences; no noclip or synthetic input.

# 0.2.0-alpha.2 - Native-compatible candidate

Keep native scene preparation, effects and drawing. Disable the incomplete
static renderer and its sprite-tracking hooks in normal builds. Add bounded
native atlas byte shadows, exact changed-range planning and conservative
engine/hook guards. These changes are under runtime verification; no stable
60 FPS or universal visual-parity claim.

Separate the collision-preserving auto-level benchmark from normal packages.
Record frame/GPU timings by percentage, exclude paused/unfocused/dead/capture
frames, and stop after at most two deaths. Normal package checks reject audit
DLLs and experimental static rendering.

# 0.2.0-alpha.1 - Chlorium

Independent fork of Bismuth, targeting GD 2.2081 / Geode 5.11.2 on Windows x64.
Fresh optimized build with corrected localized Ninja header dependency tracking.
Removed unsafe difference/overdraw debug scene hooks. Added actual GPU shader
regressions. Corrected shader include lifetimes, hue wrapping, culling bounds,
buffer synchronization, native text visibility, group rotation/reset handling,
resource cleanup, blending and graphics-state restoration. Cached shader
programs across level loads; disabled intrusive profiling by default.

New Chlorium branding; original Bismuth attribution preserved. This is still
experimental: no universal FPS or 2.2 visual-fidelity guarantee.
