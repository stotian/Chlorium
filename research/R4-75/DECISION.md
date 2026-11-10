# R75 — native end-frame geometry and bounded root programs

Research only, based on R67; no native CPU operation or callback is removed.
The collector records selected root states, complete 96-byte native sprite quads,
their matrix/rect/style sources and ownership identities at two frame ends.
Formats remain separate: CHLMIX74 roots and CHLGEO75 geometry. Matching candidate
and auditor identities, pinned game/Cocos bodies, private path, owner thread,
live object lookup and bounded buffers are required. Diagnostic runs are excluded
from performance comparisons even when only their specific R74/R75 flag is set.

The owned emitter uses GPU root positions plus a bit-qualified translation bias;
it preserves the native quad for every refused pose/geometry. Qualified slots do
not receive native XY vertices through the fallback buffer. Complete output quads
must match all 96 bytes, and a nonempty GPU-qualified quad is required for success.
Matrices, styles, command inputs and CPU readers still remain native.

## Real Orbit evidence, 10 November 2026

The first 128-actor attempt exceeded 65,536 records. Its two frames contained up
to 374 operations/actor; the partial stream and missing second geometry snapshot
are rejected. Raising the cap or ignoring the missing records is not a fix.

The revised collector bounds selection to 32 actors (at most 16 movement samples),
and installs its section/movement observer hooks only during the two-frame window.
Hooks are enabled/disabled at Director boundaries with state preservation and
rollback on setup failure. This removes observer detours outside capture; it is
not a measured FPS improvement. The actor selection can fill with rotations before
a dense movement call, so this is not guaranteed broad movement coverage.

Orbit 89.071% yielded 5,504 closed rotations on 32 actors, 172 distinct commands,
64 geometry rows, 12 native corner matches and 24 unsupported corner sources.
The independent auditor reported complete root/geometry captures with zero reasons
or missing actors. However, there were 3,328 inter-operation position discontinuities
and no captured movement rows. All 32 resident programs are rejected by continuity
validation. Unknown/interleaved native transforms must be identified; the guard
must not be weakened. The whole run died at 89.879% and 89.888% and therefore failed
the campaign. Offline arithmetic data can be investigated, but the strict audited
analysis rejects this run as completed visual/performance evidence.

The earlier Helio attempt died before its capture threshold; no data was collected.
The first lazy-observer Helio run completed, but all 32 selected actors were
invisible. Its emitter correctly failed with zero GPU quads and 64 native fallbacks.
New R75 actors are now admitted only while visible, nonhidden and natively batched;
already selected identities remain tracked even if their visibility later changes.
This is a geometry-qualified sample, not representative full-level coverage.

## Real Helio geometry, corrected sample

The corrected run completed 74–74.5%, without deaths. Independent root/geometry
audits and strict offline provenance analysis passed: 526 native operations
(279 movements, 247 rotations), 29 actors, 42 distinct commands, no discontinuities,
at most 22 steps/actor/frame. Its 58 native geometry records contained 50 exact
native corner matches and eight unsupported sources.

Owned GPU replay compiled three joint classes: 1,334 prefix/resident positions,
zero mismatches, one 1,392-byte position seed, 3,360 command bytes, 168 index bytes.
From these resident GPU poses, the emitter reproduced 18 nonempty native quads
exactly over all 96 bytes. Forty other records used their native fallback, with
zero mismatches, changed end-root poses or declined translation biases. Thus the
actual geometry component has a positive bounded witness. Its output is not yet
substituted into live GD draws; CPU preparation/readers remain unchanged.

All completed campaigns restored and SHA-verified 92 shared profile files, with
the main installation unchanged. No universal blackscreen or FPS claim follows.

## Long-program fixture

`R4-74/joint_replay.cpp` dispatches ordered programs in chunks of at most 64 steps,
retaining GPU state between chunks. Full ranges and ordering across chunk boundaries
are checked first; 8,192 steps/actor remains the fixture ceiling. The shader's
64-step admission is unchanged. Command/index uploads and per-chunk readbacks are
verification overhead, not a proposed live streaming implementation.

`R4-74/chunk_checks.cpp` compares 1,540 complete 48-byte states to the pinned native
movement/rotation routines: 192-step programs, duplicate commands, empty/unequal
classes, boundary prefixes, four FP modes and four resident frames with one seed.
Zero mismatches; malformed ranges and cross-chunk disorder reject before dispatch.
This synthetic result is not a game speed measurement. Local GPU checks require
the existing native rotation oracle object and pinned game/Cocos files.

Decision: keep isolated. Helio's qualified geometry is exact in owned replay,
Orbit's resident ownership remains incomplete, and no CPU work or game frametime
has been saved by this component. The normal renderer is not promoted.
