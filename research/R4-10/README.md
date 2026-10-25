# R4-10 — Native shader parity and baseline migration

Hypothesis: substituting a guessed shader/MVP in the V4 atlas bridge can destroy
the native render contract. Preserve the actual active program, uniforms,
native 96-byte quad data and read-only native index buffer instead.

V5 retains native preparation and replaces only eligible atlas submissions.
Own VBO/VAO, exact index subranges, context epochs, binding restoration and
foreign atlas-hook fallback remain mandatory. Unknown engine binaries fall
back. No gameplay or Area scheduling changes are introduced in this renderer.

Evidence: 218 GPU checks, including 32 exact pixel-parity scenarios; 257
readback/state checks. A zero/guessed MVP mutation is detected as black.
Heliopolis auto 129598490, 74–78%, full user mods, full detail, shake off,
noclip off: 189 native and 183 V5 valid observations, zero deaths, complete
target visual coverage, no full blackout or additional classified anomaly.
One left-side anomaly occurs in both runs at matching progress. V5 reports
5837 active submission frames and zero fallbacks. This is a targeted visual
diagnostic, not a performance benchmark or proof for the complete level.

An earlier attempt had a pre-existing GL error on its first target sample and
was rejected. The audit now records/drains errors only before target coverage;
errors inside coverage still invalidate it. Their original cause is unknown.

Decision: keep V5 as the requested experimental baseline, archive alpha.6.
No universal blackscreen fix, stable 60 FPS or ×3 FPS claim. Major native
Area/Transform loops remain the next performance target. Bismuth's supplied
2.2081 package is a reference only; it was never installed or executed.

Raw private game/configuration data and binary artifacts stay local. Aggregate
evidence and immutable binary hashes are recorded in evidence.json.
