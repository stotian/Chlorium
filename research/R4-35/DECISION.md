# R4-35 — Installed-loader register contract, owned functions only

KEEP DIAGNOSTIC ONLY. R4-30's native opacity observer remains blocked; no native
getter, gameplay function or normal renderer was changed by this experiment.

## Necessary prerequisite

Native GD keeps volatile XMM registers live across some getter calls. Reading
Tulip's source was insufficient to establish the installed handler's behavior.
This experiment asks the actual Geode loader to hook two functions owned by the
auditor, using Thiscall float-return signatures with one/two pointer/int args.

RegisterProbe emits a bounded4-KiB fixture with a normal Win64 caller, native
targets and deliberately hostile detours. The caller fills XMM1/2/3 with three
128-bit sentinels, calls the target and records all12 words plus the return.
The hostile detours overwrite all three registers. Native returns are1/7,
detour returns2/8. This proves that a detour ran rather than silently bypassing
the hook. Both hooks are detached and native return/register behavior rechecked.

The page is written as RW, then changed to RX and its instruction cache flushed.
Standalone controls check both signatures, exact return bits and poison
detection before attempting a loader experiment. After handing addresses to
Tulip, one4-KiB page is retained until process exit because the loader can retain
handler addresses. No game address is patched. FP state/LastError are restored.

Explicit CHLORIUM_OWNED_REGISTER_ORACLE=1 is accepted only by the isolated
pass-capture auditor with its compiled marker. Other modes and old binaries
reject before staging; guard tests use a fixture WITHOUT a GD executable.
The one-shot oracle never enables automatically in normal builds.

## Actual result

The installed loader preserved every word of XMM1/2/3 for BOTH signatures while
the hostile detour return was observed. Detachment restored native behavior.
Standalone controls and the runtime receipt pass. Package0198307657a0cfc0d19724460a3871cd003eae796b5107f31497191c3e64595f,
DLL8c7c6c4463077b8ec7eef27bd47950afd7f4a027b0b6e24c0c82d4e8d7414a9b.
Fixture Geode.dll SHA checked around the run:
0c34f50454c102637038cae48c8c7dd685da3582fe474358024b1eb6833684d3.
This file hash is not a live module-memory snapshot.

The surrounding Society0–15 diagnostic reached15% after one initial death.
That GAME passage is invalid for performance/visual comparison; the isolated
owned-call register result is a separate, local prerequisite. All2071 observed
frames are excluded from FPS evidence.92 shared files restored/SHA verified,
main installed mod unchanged, no screenshot or publication.

The extra atlas observation found only1 dirty draw among18 in EACH captured
opening frame. Most native geometry is already resident here. A renderer that
resends/recomputes those clean quads adds work; it must not assume full native
uploads from object counts. This does not locate Society's actual CPU bottleneck.

## Limits / decision

This resolves the owned-hook register prerequisite, not full native call-site
equivalence. Actual getter side effects, live-register requirements at each
native caller, reentrancy, FP/errno state, later mods and native prefix parity
still require checking before enabling R4-30's observer or its cache. The
compile-time/build-helper blocks remain intact. No FPS gain or absence of black
screens is claimed. Next work is native call-site proof and a bounded group
snapshot comparison, not enabling a faster path from this receipt alone.

Private evidence is in build-research/runtime-canonical-v10-isolated-register-v1-society0-r1-pass-capture/.
Source archives/package history preserve the exact compiled inputs. The normal
candidate remains107b945ce4940d49670c1aab287411e4f10ebcf45ec52174651a0d876d7f2a71.
