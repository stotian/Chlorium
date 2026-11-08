# R4-29 - Bounded native thread-location diagnostic, not an optimizer

SOURCE ONLY after the user's test-stop instruction. No sampling attachment,
new GD launch or validation of the post-stop changes is authorized now.

Measured Heliopolis costs point to visibility/movement, whereas Area takes only
microseconds. This tool prepares a bounded way to locate the hot native code
without adding a hook to every object operation. It uses Windows thread-context
snapshots, not injected code or explicit SuspendThread/ResumeThread calls.
Only the exact existing GD fixture executable is accepted. Sampling is bounded;
observer cost, missing contexts and cleanup failures remain explicit.

The initial revision compiled and passed 20 self-context captures, exact handle
balance, continuing worker execution and rejection of the non-fixture host.
Those checks ran BEFORE the test stop. Existing process_sampler.exe predates
the later SHA256 receipts, exit-race handling and resolver changes. Do not treat
that old binary as validation of current sources. Current source is uncompiled.
No cross-process game sampling or resolved hotspot result exists yet.

Current sources bind a CSV to a diagnostic receipt and SHA256 fingerprints of
the actual fixture game/Cocos image paths. The offline resolver checks those
hashes and exact PE unwind bounds. It does not invent nearest-export function
names; unresolved addresses stay labelled address blocks. Leaf-location shares
are wall-clock samples including waits, not inclusive CPU percentages or FPS.
The new crypto/receipt/resolver paths require verification when tests resume.

The separate sampling auditor compilation was requested before the stop and
has completed, but it has not been packaged or launched. Its earlier binary
predates the V2 active-session handshake and is rejected by current launch code.
Requested external sampling excludes its frames
from performance statistics. The comparison script rejects sampling provenance.
These guard changes have not yet received their runtime validation. The normal
renderer build and user's installed package are untouched by this prototype.

Read-only inspector REWORK P2: accepting any fixture PID could contaminate an
ordinary FPS run. Current sources require the actual mapped auditor's exported
immutable sampling-session flag and an explicitly selected auditor DLL SHA256
before the first snapshot. The flag is checked again per sample; the audit uses
the same immutable flag for frame exclusion. PID, source images, CSV hash and
verified diagnostic session are bound in the receipt. The resolver requires it.
This is a SOURCE correction, not a verified fix: no compilation, attachment or
guard test followed the user's stop instruction. The targeted read-only follow-up
ACCEPT covers the session handshake in source, not executable validation. Its
minor exit-race note was addressed by moving the live export read into the same
process-exit handler as capture; this final edit remains uncompiled/unexecuted.
Disk fingerprints do not establish equality of runtime-patched instruction bytes.

Static navigation located the exact generated native bindings used here:
PlayLayer::updateVisibility at GD RVA 0x3af3a0 and GJBaseGameLayer::moveObjects
at 0x22dd50. The legacy Bismuth visibility rewrite is not an equivalence proof
for the current game; it omits native behavior and stays excluded. Native
timings identify the outer paths, not which inner leaf is dominant. No new
replacement is activated on the strength of source names or profiler scaffolding.

Static unwind ownership correction: moveObjects is split over five PE runtime
function entries (0x22dd50 through 0x22ded0). Treating each entry as an independent
function incorrectly hid its three call sites. Both offline tools now share
bounded chained/indirect unwind ownership. The full body retains CCArray::count,
virtual updateObjectSection (slot 0x490), then virtual
updateDisabledObjectsLastPos (slot 0x498). updateVisibility contains 73 static
call sites; these counts do not measure execution frequency or cost.
Evidence: private native-visibility-chained-callmap.local.json. Source-only
regression cases cover split ownership, cycles, conflicting flags, truncation
and overlaps; none have been executed since the stop instruction. A subsequent
source review added bounds for root unwind-code arrays and handler payloads;
that final parser revision has not been executed either.

A static vtable-candidate scan found the adjacent 0x498 target 0x3be80, a return
stub. Candidate adjacency alone does not identify every live object's vtable or
prove that mod callbacks are absent. No callback suppression or movement
reordering follows from this observation; the experimental loop keeps the call.

Windows API contracts consulted:
- [PssCaptureSnapshot](https://learn.microsoft.com/en-us/windows/win32/api/processsnapshot/nf-processsnapshot-psscapturesnapshot)
- [PSS_THREAD_ENTRY context lifetime](https://learn.microsoft.com/en-us/windows/win32/api/processsnapshot/ns-processsnapshot-pss_thread_entry)
- [PssFreeSnapshot local ownership](https://learn.microsoft.com/en-us/windows/win32/api/processsnapshot/nf-processsnapshot-pssfreesnapshot)
- [x64 unwind records and chained procedures](https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64)
