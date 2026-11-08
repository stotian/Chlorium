# R4-27 - Cumulative scales inside a command stamp

KEEP ISOLATED. Evidence is insufficient for the normal mod. No GD launch,
installation, publication, commit or push for this continuation after the user's
new preservation instruction. Reference HEAD is 5678dcc on research/area-pose-v6;
main remains 182a83d. Target remains GD 2.2081 / Geode 5.11.2, v0.3.0-alpha.6.

## Change and exact restricted contract

The existing executor now materializes a positive axis scale as well as identity.
The scale program's explicit cumulative instantiation carries the prior compiled
scale when command stamps are equal. A newer stamp starts from one after the
native reset. The previous strict instantiation remains the default; its tests
still reject equal stamps. Shared comparison/benchmark code was moved into
ScaleFixture.hpp without duplicating its assertions.

All input and output scales must remain powers of two from 1/16 to 16. Source,
delta and accumulated scale offsets remain exact multiples of 1/16 in this
bounded domain; resetting an offset restores one exactly. Commands are copied
before any writes. Active members are unique and initially identity; their
first command stamp must be newer than their recorded stamp. Unknown callbacks,
unsupported controls, decreasing stamps or unbounded cumulative scales reject
before mutation. Rejection is a caller-level requirement to retain native work;
no runtime adapter or native-loop suppression is installed.

Membership, linear fields, helper topology, tables and lifetimes remain owned
for the synchronous sequence. Only the existing fixture observers may modify
positions or sticky FP flags. The actual move/reset calls, callback ordering,
scale writes and section publication remain per object. No object traversal is
eliminated. This ownership contract is not established for real GD or other mods.

## Evidence and negative results

454314 assertions including the alternating benchmark pass against the runtime-
extracted native routine. Coverage includes same/new stamps, all 81 scale pairs,
four DAZ/FTZ settings, sticky flags, command aliasing into an overwritten helper
cache, old center stamp/offset reset, position quantization boundaries, null and
skipped members, stamp endpoints, and 4096 objects over 64 phases. The borrowed-
command mutant fails at assertion 111057. Shared regressions pass: R4-20 327427,
R4-22 26054, strict R4-26 110202. Helpers' compared fields/topology are those in
Node.math(); arbitrary Cocos object internals are not claimed covered.

Nine alternating CPU batches include copying, validation, compilation and work.
4096/16: paired program/native median 0.790, range 0.698-1.140, 8/9 wins.
4096/64: median 0.730, range 0.675-1.157, 8/9 wins. One dense phase loses 9/9
(median 1.418). Raw batch times and peaks are retained. These are kernel timings,
not gameplay frametimes; individual regressions disallow normal activation.

One read-only inspector ACCEPT for the closed functional scope. P2: benchmark
clears tracking even on equal stamps, whereas functional cases also preserve it.
That performance distribution remains unmeasured; no rerun was substituted for
the original evidence. Dense intermediate snapshots and actual GD barriers are
unproven. The exact benchmark inputs archive was frozen before compilation;
all 25 source hashes and its ZIP hash were rechecked unchanged afterwards.

## Isolated test preparation

The existing ModProfile helper now supports an explicit ID allowlist, checked
before excluding the Chlorium experimental variable. Preflight accepts only
node-ids; the staged common profile accepts node-ids and the auditor. Unexpected
packages reject rather than being deleted. Native/candidate preflights share
the minimal profile, dependency/settings hashes and a distinct isolated campaign
path. Personal-mod mode is rejected by the current local authorized plan.
Real PrepareOnly calls and profile fixtures pass without a GD process or run
directory. Installed Chlorium's SHA256 and compiled canonical source/header/
resource/DLL hashes remain unchanged. No in-game visual, blackscreen, crash or
frametime gate has been passed by this prototype.
