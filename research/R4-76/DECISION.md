# R76 — native Follow in resident mixed programs

Research only. R75's successful phase-tagged Orbit diagnostic attributed all64
bounded first gaps to native processFollowActions. This revision adds its real
ordered movement operation to the GPU program, keeping all native CPU work.

Follow differs from moveObjects: enabled axes add even a zero delta, in Y then X
order. Skipping a zero changes signed-zero results. A two-bit axis mask preserves
native enable/lock behavior; stamp/history/dirty flags retain their native order.
The shared shader still executes at most64 steps per dispatch; longer programs
retain GPU state across bounded chunks. Rotation math is unchanged.

The owned oracle extracts the166-byte native Follow iteration from the pinned GD
image (SHA2562dd6f1e0c39ca2019ac14789194b93008e959ac9fe412db9ad08ae19fe98a051),
including its original section callback. No GD instructions are committed or
distributed.24,720 full48-byte native/GPU states match:192-step mixed programs,
four FP modes, enabled zero deltas, locked axes, duplicate command visits,
unequal/empty classes and four resident frames. The prior271,128 mixed native/GPU
states still match the updated shader. These are fixtures, not game FPS gains.

## Native observation boundary

One existing Cocos CCArray::count call at GD0x2308cb is redirected through a small
x64 gateway. It still invokes the original count exactly once, then snapshots
selected call-entry states using the live group array, native delta pair and
axis masks. Native Follow's arithmetic and callbacks execute unchanged. Member
order, duplicate continuation, owner/thread/UID, native code/import hashes,
private executable path and bounded row budgets are checked. Setup/output/fault
failures reject the capture. Movement observation allocation failures now remain
catchable and preserve the original movement call; OOM injection is not tested.

512 owned native-count gateway witnesses pass all arguments, return values,
four FP modes with accrued flags and LastError. Call-entry snapshots retain the
R74 limitation: they do not prove callbacks cannot mutate future members.
The full native/GPU prefix replay detects any observed position discrepancy.

CHLMIX76/format76/208-byte rows distinguish Follow from old trace formats. Its
candidate and independent auditor have separate R76 identities and typed stats;
the runner checks both and labels the display FOLLOW R76. R74/R72 data remain
readable, but their formats reject a disguised Follow opcode. Instrumented runs
are excluded from FPS comparisons even with only the R76 provenance flag set.

## Actual captured geometry

Orbit89–89.5 completed without deaths:5,056 operations on32 selected actors,
2,528 rotations and2,528 Follow visits,158 shared commands, zero discontinuities,
maximum96 steps/actor/frame. All32 programs qualify in one class. Owned replay
matches5,184 prefix/resident positions from one1,536-byte seed,12,640 command bytes
and632 index bytes.30 nonempty native quads match all96 bytes;34 retain native
fallbacks. No changed end-root positions, declined biases or quad mismatches.

Grief25–26 completed without deaths:32 native moves on16 selected actors,
six shared commands, no discontinuities. Three classes,160 prefix/resident
positions,768 seed bytes,480 command bytes and24 index bytes.30 nonempty native
quads match;two retain native fallback. No observed pose/geometry mismatch.

Helio74–74.5 also completed without deaths on the final native build:440 operations,
31 actors,32 commands, no discontinuities. One actor appears in only one frame
and remains native;30 resident actors in three classes match1,080 positions.
18 nonempty native quads match;43 records retain native fallback. No pose/quad
mismatch. Its final build also includes the catchable observation-allocation fix;
the earlier Orbit/Grief binaries precede that error-path-only change.

All three use isolated mods, full detail, no shake/noclip, requested120FPS. Each restores
and SHA-verifies92 shared profile files; the main installation is unchanged.
The code remains a sampled two-frame diagnostic and an owned GPU replay. It does
not replace live GD geometry or remove CPU readers, orientation/effects work,
callback dependencies or object publication. No game acceleration, universal
blackscreen fix, or normal activation is established.

Decision: retain isolated. Native Follow closes the observed Orbit gap without
invented before-states or per-operation position reseeding. Next, reduce renderer
transfer/preparation costs with retained GPU records and shared dynamic parameters;
CPU ownership may only change after separate reader/callback proof.
