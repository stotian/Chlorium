# R4-14 — Adaptive transport of authoritative native quads

The active V5 bridge copies and compares every native quad before deciding
what to upload. On dense animation this duplicates large CPU memory passes.
The V6 candidate keeps native preparation, shader/uniforms, painter order,
attributes, borrowed index store, context epochs and restoration unchanged.

Eight record samples select transport cost only. Dense frames upload the
complete current native prefix directly, with no full CPU shadow copy. Stable
transitions seed a complete shadow; sparse frames still compare every record.
Sampling never authorizes ignoring a source record. New storage and failed
writer calls invalidate the shadow. Bounds/storage size are checked; native
atlas dirty state is retained for fallback. No orphaning or persistent mapping
is introduced in this experiment.

The candidate is isolated behind CHLORIUM_NATIVE_ADAPTIVE_UPLOAD_V6 and the
private --adaptive-upload profile. It is not enabled in the default build and
has not replaced the installed V5. Version: v0.3.0-alpha.2 research. Optional
audit counters distinguish direct, seed and sparse calls and actual record
comparisons/copies/uploads.

2530 CPU checks pass, including unsampled mutations, prefix growth/shrink,
storage replacement and writer failure recovery. The corruption that ignores
an unsampled record is rejected. 219 real GPU checks pass; the existing 32
native pixel-parity scenarios now exercise all three adaptive upload modes.
These are correctness checks, not proof of a GD FPS gain. Grief auto 0–40%
paired visual validation reached 40% in both modes with all user mods, full
detail, shake off, no noclip, zero deaths. 3621/3406 visual samples, complete
readback coverage. The comparator reports two unmatched full-dark episodes
near 12.8–13.0% and one right-dark episode near 27.25%. These are unresolved,
not proven renderer faults: the native trace has gaps/pulse sampling issues.
No FPS comparison is valid in this visual diagnostic. Both passes restored
90 shared files and left the installed V5 package unchanged.

**Do not promote V6.** The later actual Cocos oracle reproduces a GL
postcondition defect in V5/V6 despite current-draw pixel equality (R4-15).
