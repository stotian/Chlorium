# R4-40 — Ordered native SSE2 arithmetic splice, isolated

The compiler reordered an intrinsic addition in the first broad-domain attempt:
case28767/state0 swapped which NaN payload survived in TL/BL.Y. The test failed;
that version never entered GD. OrderedKernel now emits a small owned leaf page
with the exact source/destination order for every scalar/packed operation.
It touches only volatile XMM0..5 and volatile argument registers. W-then-RX,
bounded code/data and explicit lifetime management remain mandatory.

1754528 raw arithmetic comparisons pass, including paired signed/payload NaNs,
infinities, subnormals, arbitrary bits and rounding/DAZ/FTZ/sticky settings.
Unsupported vertex modifiers and unmasked FP controls reject without mutation.

NativeSplice is tested in the actual complete Cocos method in an isolated
helper process, never the game. Whole function and exact14-byte CCSize body
hashes gate it. The latter is two integer field copies and a returned pointer:
those reads/writes are reproduced, not omitted. All three virtual callbacks,
parent composition, hidden/opacity branches, atlas update and child traversal
remain in their original method. Vertex-mod/unmasked cases take the original
instructions. No new GPU state or render ordering is introduced.

Initial splice after native XMM saves is exact but small/noisy. The revision
before those saves uses only volatile XMM registers, so the ten XMM saves and
restores are unnecessary on this path. The original fallback still executes
them. A nearby owned code region registers a zero-prologue chained unwind
entry using the ORIGINAL module base; its parent is the pre-spill native
fragment. RtlLookupFunctionEntry/RtlVirtualUnwind checks verify RSP/RIP, saved
RBX/RDI and live XMM6..15 at dispatcher, fallback and arithmetic PCs. Actual
hardware execution traps prove one fast and two fallback dispatches. Unknown
code, unwind owner, unavailable nearby memory or registration failure rejects.

Heliopolis/Orbit83/Grief64:1750592 complete native-method comparisons pass.
All29031 supported captured quads still match. Native function bytes restore
after every helper run. Atlas/children are absent in these closed fixtures;
full native object lifetime, exceptional-handler dispatch, installed mods and
in-game pixels/black screens are NOT established.

Full-method fixture retains the native callbacks, input objects and outputs.
Nine alternating Helio CPU batches,16 replays of21306 sprites each:
latest paired time ratio median0.98378, range0.94495–1.01104,7/9 wins.
Median replay1.21482ms native versus1.19839ms splice. Earlier placement had
ratio0.94723 but substantial variation and only about0.005ms difference between
unpaired replay medians; it is not stronger evidence. These are kernel/helper
measurements, not game frametimes. Every callback count and whole-object output
checksum agrees. Two latest batches regress: do not promote or claim FPS gain.

Decision: preserve the exact primitive and private proofs; no game build/test,
normal CMake profile, installation or release. Move to a larger GPU workload
reduction experiment instead of tuning this small arithmetic gain further.

Windows contracts: [x64 unwind/chained fragments](https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64),
[VirtualAlloc granularity](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc).
