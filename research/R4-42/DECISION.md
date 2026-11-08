# R4-42 — Shared ordered movement program on GPU

Keep isolated. The shader runs an ordered program once per group combination,
in parallel across its members. It keeps every double-precision addition and
the native float conversion for lastPosition; summing deltas first is unsafe.
Resident 32-byte state is separate from the common command/range/index buffers.
No geometry, physics, callback or native visibility path is replaced in GD.

Owned native reference: exact GD 2.2081 moveObjects body (R4-38 hash gate), with
closed count/section/end callbacks. 2,163,720 intermediate prefix states match
GPU output bit-for-bit, including stamps, last positions, dirty flags, signed
zero, x locks, decoration flags and four masked-nearest DAZ/FTZ combinations.
36 arithmetic rejections pass. Four hostile callbacks modifying a future object
produce differences as expected: arithmetic eligibility is NOT ownership proof.
FP status, arbitrary observers, all object bytes and CPU reader closure are not
reproduced by GPU execution. This is not a native method replacement.

Arithmetic domain: <=64 commands, <=64 combinations, bounded finite doubles
(zero or magnitude 2^-32 through 2^32), nearest rounding/masked exceptions.
The smallest input lattice is 2^-84; <=64 additions cannot generate subnormals
or overflow in either native double arithmetic or the float conversion here.
NaN/Inf, unknown state bits, invalid ranges and reordered/duplicate indices
reject before dispatch. GL_ARB_gpu_shader_fp64 is required; no precision downgrade.

Owned alternating kernel measurements (nine pairs each, not game frames):

| Objects | Combinations | CPU median ms | GPU wall median ms | Paired ratio | Wins |
| --- | --- | --- | --- | --- | --- |
| 65 | 1 | 0.0193 | 0.3314 | 17.4145 | 0/9 |
| 65 | 16 | 0.0063 | 0.3072 | 48.7846 | 0/9 |
| 4096 | 1 | 4.5366 | 0.8206 | 0.173676 | 9/9 |
| 4096 | 16 | 1.7646 | 0.5175 | 0.271620 | 9/9 |

These use 64 commands; GPU command upload/dispatch/synchronization is included,
initial residency and CPU publication/readback are excluded. Native callbacks
are owned fixture callbacks, not arbitrary game/mod behavior. No FPS multiplier
or production activation follows from this favorable large-fixture result.

## Actual workload discriminant

Explicit two-frame native movement census, Heliopolis 74–78, full detail/120 cap,
no shake/noclip/personal mods. Guard checks reject absent marker, wrong profile
and combined diagnostics before staging. Owned census checks cover budgets,
null stop, invalid reads, pointer/owner/frame gates, FP/LastError and disabled path.
The native original method is called once per observed invocation, unchanged.
No per-object retention or live dereferencing after the original call.

Observed complete frames: 696/986 calls, 32,849/34,816 member visits; 467/678 calls
have zero members. Largest group is 3,290 members and appears once per frame.
Maximum repeated array+UID+membership fingerprint is 7/10 calls, not 64. Only
7/10 visits have negative section indices, so eliding native section processing
would be unjustified for nearly all observed members. Membership hashes are
diagnostic fingerprints, not collision-free dependency/identity proofs.

Thus the favorable 64-command dense fixture is NOT a representative performance
claim for these frames. Most work cannot simply be queued to GPU while retaining
native CPU visibility/section callbacks. The next GPU discriminant is reducing
native draw work in the same actual context/framebuffer, without duplicating
the whole native preparation path or accepting the R4-41 pixel calibration gap.

Package 5b4a086ba70e3ea0707d96c2d7e5aa210e33cec93cb014483fd39b81c254806e;
DLL 73b0d27db55c1c52fe86b64c46ceabe6cab56c8b91abbb0c802865a83c4ab7e6.
Reached78%, zero deaths, 92 shared files restored/SHA verified, installed and
normal reference packages unchanged. No screenshot, publication or promotion.
