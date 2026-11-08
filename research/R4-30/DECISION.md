# R4-30 - Shared ordered group-opacity prefixes

SOURCE PROTOTYPE ONLY. No compilation, test execution, GD launch, installation,
commit or push. The user's test stop remains in force. Absent from normal mod
targets; an explicit private diagnostic build profile is now wired in source.
That observer profile is intentionally BUILD-BLOCKED pending installed-loader
register compatibility. The standalone GroupOpacityProgram header is separate.

Native Heliopolis visibility previously averaged 9.6975 ms. Static inspection
of the measured GD image finds GameObject::groupOpacityMod at 0x19ccb0: for each
membership entry, fetch opacity via GJEffectManager::opacityModForGroup at
0x260b80, multiply in list order, compare zero >= product, return +0 early.
The getter clamps IDs to 0..9999, hashes and searches the live map, and returns
the action's current opacity or the native default. Neither leaf writes the
object/map. This does NOT measure their fraction of visibility CPU time.

Hypothesis: many visible objects share ordered group prefixes. Compile those
prefixes once, snapshot each distinct group's live value once per immutable
phase, evaluate each distinct prefix once, then publish each object's result.
Unlike reassociation or a sorted group-set key, this retains multiplication
order, repeated group IDs and native early-zero termination. The source keeps
the scalar SSE comparison: replacing it with a bit test would lose denormal
exception flags and DAZ's treatment of subnormals as zero during comparison.
Prefix products are discarded/recomputed each evaluation.

Restricted contract: input memberships and values are owned immutable snapshots;
opacity values are finite in [0,1], with -0 permitted; every FP exception is
masked; FP control stays constant and no observer can read intermediate FP flags
or object opacity during the phase. Rejection leaves caller outputs and MXCSR
unchanged, requiring native fallback. Inputs are copied before output writes.
Compilation and all storage allocation are outside evaluation. No color,
trigger, gameplay, GL or engine callback is removed by this standalone source.

Regression SOURCES cover ordered/overlapping/duplicate/clamped memberships,
changed snapshots, early zero, all rounding and DAZ/FTZ combinations, sticky FP
flags, rejection domains and overlapping input/output storage. They compare to
an eager SSE model, NOT the extracted native function. They have not run. The
4096-identical-object fixture expresses a structural hypothesis of four prefix
multiplications instead of 16384; neither execution nor timing is established.

Do not activate: actual prefix sharing, native leaf equivalence, input snapshot
cost, membership invalidation, native reader/producer barriers and external
hooks remain unproven. Program evaluation still visits every object to publish
an output. Mostly unique groups, early-zero-heavy cases, small scenes or costly
snapshot acquisition may regress. Preserve GameObject::colorForMode writes to
its scratch color; returning a memoized RGB alone is not equivalent.

One bounded read-only V6 inspector ACCEPT for this isolated source, with no
P0/P1 finding. Its NDEBUG concern is addressed by an explicit compile-time error
in the regression source; this edit remains uncompiled. Its allocation-budget
concern remains a prerequisite for any engine adapter: cap object, membership
and prefix counts before compilation, retaining native work on excess. Current
construction is restricted to caller-owned research inputs, not level loading.
This review is not execution, native equivalence or performance evidence.

When tests are authorized: compile checks with C++20, /fp:strict and assertions
enabled; compare against the native routine before running the shadow adapter.
Any later FPS claim must include snapshot, invalidation, publication and fallback
cost, with alternating game frametimes and visual checks on all required levels.

## Native observation adapter draft, not built or run

The separate --opacity-shadow manual profile compiles NativeOpacityShadow.cpp
with /fp:strict. Its two observer hooks forward every groupOpacityMod and
opacityModForGroup call exactly once. A single eligible visibility phase records
at most 512 completed object calls and 16384 getter observations in fixed storage.
No extra engine getters, object-field reads or pointer lifetime cache are added
by those hooks. Reentrancy, changing group values, mixed managers, changing FP
control and unsupported opacity values prevent an accepted comparison.

The bounded trace contains only getter prefixes actually executed by native GD,
including its early stops. It is NOT the complete membership list; it cannot
certify a persistent cache, future states, full-level equivalence or producer/
reader barriers. A truncated trace only describes its captured subset. Existing
sticky flags may mask exception differences; the receipt records flags observed
being raised from clear, not universal native FP coverage.

After native visibility returns, an eager SSE replay compares each recorded
result, lookup consumption and visible MXCSR with the actual native observations.
The prefix program is then compared with those results and with settled eager
flags. Eligibility inspection and post-phase analysis restore their caller's FP
state. RAII abandons an unfinished phase on native unwinding; diagnostic analysis
failure never unwinds into gameplay. A missing/partial receipt is failed evidence.
Receipt creation refuses to overwrite an existing file. Analysis initially masks
exceptions privately; replay only uses validated masked native controls. Capture
eligibility avoids extra percentage reads after the one capture attempt.
Construction receives at most the captured memberships, bounding node count by
16384 before map/program allocation. No optimized opacity is returned to GD.

Marker/profile matching, an explicit OpacityShadow switch, an isolated native
campaign plan and its package hashes are required by launch sources. Personal
mods, image capture, external sampling and FPS-comparison mode are rejected.
The auditor excludes every shadow-profile frame from FPS statistics; provenance
is marked and compare-auto-runs rejects it. Guard regression sources are added
but unexecuted. The existing authorized local runtime plan has NOT been changed.
No new diagnostic binary, receipt, native-parity result or game measurement exists.

## Native-register compatibility gate

Native groupOpacityMod keeps XMM1 (product) and XMM2 (zero) live across its getter
call at 0x19ccf6. Visibility keeps XMM3 live across its group-opacity calls.
Ordinary Win64 C++ ABI guarantees do not suffice to preserve these registers.
The local TulipHook 3.1.15 generator explicitly saves XMM1..3 before the detour
and restores those ORIGINAL values afterwards: src/generator/X64Generator.cpp,
preserveRegisters, preserveReturnRegisters, restoreReturnRegisters, handlerBytes.
The return-save step overwrites XMM0/RAX, not the saved XMM1..3 slots. This local
source is counter-evidence to a claim that the C++ detour necessarily clobbers
the native accumulator. The targeted read-only inspector withdrew that P1.

It does not prove that this generator is the handler used by the actual Geode
loader, or prove native call-site parity/FP preservation. Therefore the build
helper refuses --opacity-shadow before any build/staging work; the observer
header also has a hard compile error for its macro. These are deliberate gates,
not reported build failures: no compilation was attempted. No bypass flag is
provided. Verify the installed loader/handler contract and a native-register
oracle before removing these gates, then validate the observer before any
optimized GD path. Current runtime plan and installed mod remain unchanged.
The native initial/default opacity constant was statically read as 0x3f800000
at RVA 0x622c24; this data read did not execute the engine or regression cases.
