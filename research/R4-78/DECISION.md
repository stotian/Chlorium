# R78 — verified canonical GPU handoff, GD validation pending

Research prototype, disabled outside its dedicated build. Not promoted.
Owned checks:128 complete images through the pinned native Cocos atlas function,
8 deliberately skipped GPU dispatches rejected by poison verification and repaired
by the full native fallback. All six SSBO ranges (including3), generic bindings,
program, LastError, FP state, pending GL error, canonical tail bytes and no-context
path pass. Both the poison itself and the GPU restoration are read back; a missing
GPU write cannot pass because native data happened to be present already.

RootRuntime retains positions between frames, bounded64-step dispatches without
per-chunk readback. Actual R76 input replays pass64 Orbit,32 Grief and60 Helio
positions over two frames with one seed. CPU frame checks cover continuity,
unsupported opcodes, duplicate identities/visits and command-table overflow.

The dedicated native/auditor R78 packages compile and pass packaging identity/CRC
checks. LiveDraw.cpp connects root programs and verified canonical writes to the
actual native atlas draw for the two-frame diagnostic window. All native CPU work
is retained. It checks current live descendant ownership/UID/index and native quad
bytes, restores GL state and forwards native drawing once. Unsupported cases remain
native. Its live GD validation has NOT run: quota reached3percent before launch.
Do not claim live success, continuous availability, a global blackscreen fix,
120FPS, or any FPS gain. The poison/readback profile is diagnostic and excluded
from performance comparisons independently of the parent flags.

Next: preflight then one isolated Grief25–26 diagnostic with --RotationTrace and
matching R78 packages. Require live_root_draw_r78 frames2, drawn_mask3, nonzero
actual draws/quads/verified_bytes and failures0, plus complete parent captures,
92-file restoration and unchanged main installation. If uncertain, retain native.
Do not promote or install into the main game. Assets resources/shaders/r78-*.comp
must match their research shader originals. Native/auditor profile hashes are in
build-research/{native,measurement}-live-root-r78/build-fingerprint.json and package
receipts. Builder outputs are private and not committed.
