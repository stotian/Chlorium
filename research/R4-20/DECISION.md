# R4-20 - Complete native loop and compiled identity helpers

**Reject the candidate for runtime promotion. Keep the native loop oracle.**

The experiment executes the complete supported `transformAreaObjects` body,
native `getRealPosition`, R4-19's reset/move routines and actual Cocos CCNodes
in an isolated process. Twenty-one relocations are explicit. Native function
bodies, Cocos helper bodies and constants are fingerprinted. Private fixture
section/disabled/visual callbacks expose state; real collision/render callback
implementations are not substituted or claimed proven. No GD launch occurred.

## Concrete work removed, within a closed domain

The candidate supports only identity-linear decoration objects, zero anchors,
known native helper classes, bounded finite inputs and nearest rounding with
four DAZ/FTZ combinations. Source/group ownership and callback capabilities are
closed to this fixture. Unknown observers, foreign getters, nonidentity scale
or rotation, unsupported helpers and FP controls reject before publication.

Object getters, command-stamped reset/move and section publication remain
native. Per-object temporary helper setters/matrix construction are replaced by
materializing their verified fields/caches. Constant linear decomposition is
compiled once per phase. Its masked FP flags are published after the native
move callback, not prematurely; flag-clearing observers require republication.
The program retains the actual per-object translation operand and operation
order. It does not replace all operands with a shared group delta.

For example, center X=16777216 and object X=.25, unit scale and zero offsets:
the native float recentering changes object X to 0 and stores offset -.25.
An identity-looking matrix is not proof that the native operation is inert.

327427 checks pass: complete object bytes, normalized tracking/layer ownership,
helper math/cache/reference fields, callback-visible snapshots, FP flags and
errno. Three phases per fuzz case exercise cached helper state, reused tracking,
null termination, skipped objects, duplicate members, cross-object writes and
flag-clearing observers. Three mutations are rejected (logs alongside sources).
The inspector found an unsupported callback-table acceptance bug and foreign
tracking pointer normalization gap. Both are fixed and have before-write rejection
tests; the single targeted follow-up accepts those fixes. Boundary/subnormal
values, resetVisual=false and preexisting sticky flags are now exercised.
Per-process node IDs/arrival IDs are normalized; this is not a complete proof
of every Cocos field or arbitrary mod observability.

## Full cost, not a kernel-only gain

Nine batches alternate native/candidate order; each batch executes 16 phases.
World/node construction is outside both timed paths. Validation, compilation,
publication and native writes are inside the candidate timing. Final state is
compared over every object, including the 4096-object fixture. Native execution
uses relocated calls and fixture callback spies, so these are not GD FPS or
native production frametimes. Intermediate callback snapshots use four objects;
the large benchmark disables captures on both paths and compares final state.

| Dense variant, 4096 objects | Native median us | Candidate median us |
|---|---:|---:|
| Constant decomposition only | 10180.6 | 13952.0 |
| Direct helper materialization | 13208.1 | 16805.9 |
| Conditional FP publication | 15187.1 | 21175.1 |
| Before inspection fixes | 19041.6 | 24560.8 |
| Final callback/slot guards and boundary tests | 15675.3 | 19767.2 |

These are distinct chronological pairs, not directly comparable across machine
load/frequency changes. Every dense candidate regressed within its own pair.
Final maxima: 19134.0 / 23885.3 us. The 4-object final medians are 12.6 / 14.5 us.
No variant is installed or presented as an FPS improvement. First variant
sources are frozen privately (archive hash in evidence.json); original logs
remain under build-research. The conditional-flags CSV was retained from its
tool output after the working CSV was overwritten by the final pair.

The next architectural question is whether an owned group program can remove
repeated eligibility traversals with a demonstrated mutation/invalidation
contract. A cheap counter or user-declared token alone cannot certify real GD
objects against unknown hooks, checkpoints or membership changes. Do not repeat
this helper specialization in GD hoping the measured regression disappears.

## Discriminating stage diagnostic

One separately instrumented build, still passing 327427 native checks, records
integer clock spans around validation, constant compilation, preparation,
object execution and publication. This is diagnostic instrumentation, not a
repeat intended to find a favorable speedup. Raw per-batch rows are retained.
For 4096 objects/16 phases, validation's median share of measured candidate
time is 42.12%; its median cost is 5499.5 us versus 7383.8 us in the object loop.
Constant compilation costs only 19.2 us (stage medians are not additive pairs).

This evidence motivates a bounded synchronous group program that validates an
owned membership/linear footprint once, then executes several phases while
known callbacks cannot invalidate that footprint. No such stability certificate
has been implemented or measured here. Unknown callbacks, lifetime/checkpoint
changes or a phase that can write the certified fields must break the program.
Removing guards from arbitrary GD memory would not constitute that proof.

## Loader-isolation qualification (R4-21)

The historical runs above did not verify XInput/Geode loader isolation.
R4-21 fixes this harness defect; do not treat those timings as an isolated
production reference. The first guarded dense pair still regresses:
11965.9 / 12489.8 us median. The guarded stage rerun overlapped the user game,
so its timing values are excluded. No runtime candidate is promoted.
