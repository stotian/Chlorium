# R4-31 — Persistent packet rebuild planning (isolated CPU prototype)

120 FPS is the new target: 8.333333 ms per actual presentation interval.
No game speedup, rendered-pixel parity or GPU correctness is claimed here.
The normal renderer and the installed mod do not use this prototype.

## Hypothesis and scope

V6–V9 changed transfer protocols while retaining native per-object preparation.
The measured native Heliopolis visibility cost was ~9.70 ms, already above the
new whole-frame budget. Persistent geometry plus dependency invalidation could
avoid rebuilding unaffected render packets, rather than copying all current
quads and searching for differences. These measurements do not identify the
share that this prototype could actually remove.

Sodium retains section data and cached multi-draw batches in regions; its 0.9.2
allocator reduces buffer allocations/resizes. Bismuth's current scheduler also
caches sorted batches keyed by blend state and prepares GPU buffers lazily.
Concepts only: no Sodium source was copied or license changes introduced.

- [Sodium persistent region state, pinned source](https://github.com/CaffeineMC/sodium/blob/148151cf25c56f8b670986b348dcc589278c867e/common/src/main/java/net/caffeinemc/mods/sodium/client/render/chunk/region/RenderRegion.java)
- [Sodium allocator change](https://github.com/CaffeineMC/sodium/commit/5c44e4f6e14590681d47d39cec4b11260e2425b5)
- [Bismuth scheduler, pinned source](https://github.com/iliasHDZ/Bismuth/blob/70815d2540fe797bbe251a7f1452e876e4d0449b/src/renderer/WorkGroupScheduler.cpp)

## Implemented contract

PacketRebuildPlan compiles reverse dependencies once. A changed input queues
only its dependent packets; repeated changes coalesce without allocations in
the rendering loop. Monotonic revisions reject obsolete work. Owner identities
also reject work from a destroyed scene reconstructed at the same address.
Topology compilation failure disables the entire custom plan, including its old
topology. Packet/edge/input limits bound construction; allocation failure leaves
the plan disabled and must be caught by a future adapter before drawing.

Submission requires a current visibility revision, complete coverage evidence,
strictly increasing native packet ordinals, and every visible packet published
at its requested revision. Failure means ONE native pass, decided before any
custom draw. No packet sorting by texture, transparent reordering, gameplay
callback suppression, visibility heuristics or GPU waiting is implemented.

## Self-critique and corrections

1. Address identity alone allowed old scene jobs to match reused storage.
   Added saturating atomic owner identities and a placement-reconstruction case.
2. A failed repeated upload could leave the same revision marked ready.
   Failure now withdraws readiness and requeues the packet.
3. Per-batch vector allocation undermined the persistence goal.
   Caller-reserved output now consumes pending work without allocating;
   insufficient capacity leaves the queue and output unchanged.
4. Old visibility lists could omit newly affected packets. Submission now
   requires the current dependency revision, in addition to coverage proof.
5. Treating every group transform as a geometry rebuild would reproduce the
   dense animated-scene bottleneck. The composition check separates resident
   geometry and parameter-cell plans: a parameter change queues one parameter
   upload, zero geometry jobs, and blocks custom submission until BOTH lanes
   are ready. This establishes scheduling only, not equivalence of a GPU
   transform to GD's native effects. Unknown/deforming effects still require
   geometry invalidation or native fallback; never relabel them as rigid motion.

CPU checks compile and pass with assertions enabled. They cover pending work,
duplicate dependencies, stale jobs/topologies, unknown inputs, upload failure,
owner reuse, ordering and full-pass fallback. A synthetic 100000-packet graph
with 100 disjoint input classes queues exactly 1000 packets for one changed
input. This is an operation-count fixture, NOT 100x faster rendering or a
frametime measurement. The dense case coalesces 10000 repeated invalidations.

## Missing evidence / decision

Keep isolated. No GD adapter, buffers, shader execution or actual rendering is
included. An adapter must track ALL geometry/material/Area/group/camera/pass
dependencies, rebuild topology after membership/order changes, and prove exact
native coverage. Unknown effects must select the native pass. Immutable input
snapshots must match build tickets. Uploads must use separate staged allocations;
checking a ticket after overwriting a live allocation cannot undo corruption.
Actual allocation ownership/fences, final GL preflight, asynchronous job lifetime,
driver errors, pixels, black screens and total cost remain unverified.
Context, storage or shader recreation must invalidate all affected resources
even when numeric GL names are reused. The coverage boolean is a caller
obligation, not evidence generated by this class.

The prototype therefore cannot be activated in the normal version. Next useful
integration is a capture-only adapter for one fully owned native pass, proving
coverage and dependency invalidation before any draw replacement.
