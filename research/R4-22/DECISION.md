# R4-22 - Bounded synchronous owned-group program

KEEP the functional research prototype. Performance unmeasured; no runtime
integration, installation or GD launch. The rejected R4-20 single-phase
candidate is not promoted by this experiment.

## Hypothesis and implemented slice

Repeated eligibility scans dominated a prior R4-20 diagnostic, whose loader
isolation was unverified (R4-21). Test the architectural question directly:
validate once for a bounded, synchronous sequence rather than caching a token
across arbitrary callbacks, frames or checkpoint changes.

OwnedIdentityProgram copies at most 64 commands before writes, validates the
current world and callback tables, compiles the constant decomposition once,
then runs the same executor as the single-phase reference. Every call validates
anew. This removes repeated validation/compilation calls within the sequence;
it does not remove native object visits, moves, resets or publication points.
Validation, compilation and execution now share code with R4-20. The center
must satisfy the same identity/bounded-offset domain as members. Empty-world
fixtures explicitly initialize that center domain.

## Closed-world stability argument and limits

- Member identities, skip flags, source rotations/scales, callback tables,
  helper topology and lifetimes cannot change during this call. These are
  fixture preconditions, not a proven capability contract for real GD.
- Actual permitted callback implementations only record snapshots, update
  another owned position by .125, or clear sticky FP flags. They cannot write
  certified membership/linear fields. Unknown callback/table entries reject
  before mutation. No user callback or yield is inserted between steps.
- Stamps are nondecreasing signed integers. An increase after the first step
  must clear the active tracking count. Each distinct owned object resets at
  most once per stamp; duplicate members cannot add unbounded registrations.
  Initial count + members + center must fit the preallocated slots. No growth
  is executed. Equal-stamp clears may drop registrations exactly as native does.
- Initial positions are bounded by 2^40, offsets by 2^20 and linears are unit.
  At most 64 identity recenterings and bounded observer additions stay far
  below float overflow; generated offsets may exceed the initial bound, so
  later phases intentionally do not reapply the initial eligibility test.
  The native arithmetic order and movement operands remain per object.
- FP rounding/masks/DAZ/FTZ controls remain fixed. Recipe compilation restores
  errno and all incoming MXCSR state. Precision flags are published at the
  existing per-object point after the move callback, including flag clearers.

## Evidence

26053 positive assertions compare full object/center/layer/tracking/helper
state and final FP controls/flags/errno. Small worlds retain every callback
snapshot over up to 64 phases, four DAZ/FTZ modes, boundary/subnormal positions,
duplicate/null/skipped membership, blocked X, equal/negative/boundary stamps,
position-writing and flag-clearing observers. Dense 64/4096-object sequences
compare final state and callback counts over 16 phases, with snapshots disabled;
they do not prove every dense intermediate state or real section-handler work.

Rejection tests preserve state before any write: center NaN offset/non-unit
scale, member rotation/foreign pointer, getter/callback replacement, helper
skew, coherent full tracking, decreasing stamp, missing clear, unknown observer,
unsupported FP controls, empty/oversized program and active invocation.
After an accepted sequence, changed group metadata must be rejected anew.

The aliased-command fixture stores commands in owned position bytes. Native
reset overwrites command two during step one; only the prior immutable copy
can match the reference. Deliberately borrowing input fails at assertion 25971.
Bypassing validation fails at assertion 1. These are intentional nonzero exits.

The refactored R4-20 path still passes 327427 native checks. Its stage-audit
variant compiles; it was not re-benchmarked. Inspector ACCEPT for this closed
fixture, no P0/P1. Its P2 capacity-test flaw is corrected: coherent count 60
rejects, count 59 reaches exactly 64 registrations, then the next stamp reuses
the register without growth. These added tests and dense checks were run after
inspection; no second agent pass was requested for a P2 coverage correction.

## Decision

Functional equivalence is supported only in this fixture. No elapsed-time,
frametime or FPS gain is claimed. The user's Steam game remains open: no fresh
performance measurements are taken concurrently. Next discriminating evidence
is an alternating native/single-phase/program full-cost comparison on a quiet
machine, including program copy, validation, compilation and publication.
Even a positive fixture result cannot justify suppressing real GD callbacks
without establishing their write/lifetime barriers.

## 2026-10-29 - Full-cost isolated measurements

The machine had no GD process. Nine batches rotate native, validated single
phase and synchronous program order. Timers include command copying, eligibility,
constant compilation, preparation, all object work and publication; world
construction is outside all three. Final complete normalized state, callback
counts/order and full MXCSR/errno agree; 712179 total assertions pass. Dense
intermediate callback snapshots remain disabled.

4096 objects/16 phases: medians 26969.3 / 22022.5 / 12315.6 us. The ratio of
medians (2.19x native/program) overstates the typical paired result: paired
program/native median 0.609, range 0.362-0.873, nine wins of nine. At 64 phases
the paired median is 0.682, range 0.353-0.916, nine wins. One dense phase loses
all nine pairs (paired median 1.490, range 1.355-1.843). Raw rows retained.

KEEP as evidence that synchronous ownership can amortize repeated guards; no
universal speedup or GD integration. The real producer/callback/lifetime contract
and a sufficiently long safe sequence must be established before deployment.
