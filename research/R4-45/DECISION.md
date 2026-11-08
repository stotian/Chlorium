# R4-45 — Society startup death (diagnostic only)

One-hour continuation began2026-11-04 16:58:37UTC. No normal renderer, collision,
input or damage behavior changed. Three thread-local execution breakpoints
observe native destroyPlayer/startGame/resetLevel for at most three seconds.
Pinned GD2.2081 PE/body hashes, debugger ownership, RF/overflow and cleanup are
retained. The shared watcher now records R8: the actual third Win64 argument.
An owned volatile indirect-call control verifies it without changing the native
return. Existing1,200-trap/XMM/overflow tests and read/write watcher tests pass.
The initial direct-call control failed; its volatile-call replacement establishes
the real ABI entry and passes. No game result was inferred from the failed test.

Society139648964, native path: first destroyPlayer receives a live level object
with ID8, type2, position/start-position(0,105). Native collision caller returns
to GD RVA0x214950; player death is reported at x2.0930007/y104.8569,0.0061235%.
The object pointer is matched against the live retained level array at the end
of that death frame, not dereferenced later from a stale event pointer.
All observed native bodies still match, no debugger conflict/lost ownership or
overflow. This establishes the immediate collision, not why the first attempt
starts there or whether the level intentionally relies on its first reset.

Controlled negative experiment: retain downloaded level ID139648964 instead of
the launcher's zero ID, keeping dontSave=true and all other rendering/physics
settings. The same death against the same object/coordinates persists. Reject
that proposed fix; the experimental opt-in defaults OFF. Different identity
policies cannot be compared as FPS evidence (seven comparison guard tests pass).

Both diagnostic passages reach15% after one death. Both are rejected for whole-
pass FPS/visual validation, and all diagnostic frames are excluded. No death is
hidden or reclassified as a successful attempt. Each restores/verifies92 shared
files and the main installed package. Native startup cause remains unresolved
upstream of the observed collision; no gameplay workaround or noclip is added.

Private evidence: r45-society-death-r1 and r45-society-source-id-r1 pass-capture
campaigns; source/package histories retain both built variants.

User clarification, 2026-11-04: Society intentionally kills the player immediately
on the first attempt. Treat that startup death as expected level behavior, not
an unresolved Chlorium defect. No collision/gameplay fix is required. Existing
diagnostic recordings remain excluded from FPS evidence; future paired runs must
apply the same first-attempt warm-up and measure the subsequent attempt.
