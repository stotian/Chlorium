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
