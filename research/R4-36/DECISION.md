# R4-36 — Native group hashes, live values

EXPERIMENTAL / NOT PROMOTED. Normal candidate, main installation and native
rendering remain intact. No opacity-value cache, callback suppression or LDM.

## Bottleneck and exact transformation

The pinned GD getter at0x260b80 clamps a signed group ID to0..9999, calculates
four FNV-1a byte stages, traverses its live native hash table and loads the
current float bits. Repeating that integer hash is unnecessary. Retaining live
table/value reads avoids the mutation/invalidation problems of opacity snapshots.

For clamped x, its high two bytes are zero. With unsigned64 arithmetic modulo
2^64, P=1099511628211 and L[k]=(14695981039346656037 XOR k)*P:

    hash(x) = (L[x & 255] XOR (x >> 8)) * P^3

The compact table occupies2048 bytes. All10000 possible clamped hashes are
checked exhaustively. An80-KiB full-hash variant is retained only for the CPU
comparison; the runtime candidate uses the compact table.

HashSubstitution.hpp emits the SAME block for the isolated oracle and runtime.
It replaces only0x2b..0x68 of this190-byte getter. The clamp, list/bucket traversal,
live value load and default return remain native. All native opacity products,
early-zero branches, rendering, triggers and gameplay callbacks remain native.
The caller-home scratch write is retained. Compact hashing must restore RAX=0
before the original missing-key path; omitting it would dereference a small
integer instead of returning the sentinel/default. No FP instruction is added.

## Evidence / self-review

The standalone oracle maps the owned executable as SEC_IMAGE_NO_EXECUTE data,
checks PE timestamp/size and complete getter/product hashes, then copies only
the two leaf/closed routines to a private RW-then-RX page. It does not load or
launch GD, Geode, Cocos, or user save data. The only RIP constants/direct call
between those routines are relocated inside that owned page.

42,438 controlled evaluations cover raw return bits, full MXCSR, errno, all
four roundings/DAZ/FTZ combinations, clear/set sticky flags, signed zero, NaNs
(including signaling payloads), infinities, subnormals, clamped IDs, collisions,
rehashing, missing keys, erase/reinsert and live value changes. Complete native
group products are compared too. The shared R4-35 caller proves preservation
of XMM1/2/3. Its original standalone regression also passes after the new entry.

Initial9 rotating batches of262144 calls per variant showed a lower compact
kernel median in both collision-heavy and wider-table cases. These are CPU
kernel measurements, NOT gameplay speedups. Raw times/digests are private in
build-research/native-opacity-hash/timings-r1/r2.local.csv. The final verdict must
use raw game frametimes and peaks with repeated same-setting passes.

## Isolated runtime candidate

NativeOpacityHashPatch.cpp is compiled only by --opacity-hash into
build-research/native-opacity-hash-r36. It requires the exact native image/body
and minimal mod allowlist. Unsupported/modified code retains the original.
The existing enabled setting restores the original block when disabled.
ChloriumOpacityHashActiveR36 verifies the whole installed getter body; a new
independent auditor requires that proof throughout a candidate run. No renderer
is replaced. The R4-30 observer/cache gate remains blocked and unused.

Candidate package7f0723fe5bb1be4a0f7ea587e88b5f85e51ff30b96edc1aaaa0118d02efccb64,
DLL906660c945c0b669871d28933df9d0470953026442bb0d68fad1c7c82832e560.
Same-version reference package107b945ce4940d49670c1aab287411e4f10ebcf45ec52174651a0d876d7f2a71.
Auditor package9b90d5df65d3f0dfe6389c124e120d9cf86b47ab31652518961581c87ceaaf67.

Runtime protocol: Heliopolis74–78, N/A/B/B/A/N (absent/reference/R36), fresh
processes, same auditor/level/settings/minimal mods,120 FPS cap, full detail,
shake off, collisions intact, at most two deaths. Each pass must restore the
shared profile. HUD distinguishes SANS CHLORIUM / AVEC CHLORIUM REF / R36.
Comparison rejects mismatched or diagnostic provenance, incomplete/disturbed
coverage, missing per-percent samples, deaths and inactive patches. Even a
completed campaign remains one level with two passes per variant; it cannot
prove a global gain or authorize normal activation on Orbit/Grief/Society.
Performance runs do not measure black screens; no claim of their absence.

Implementation limits: the prefix substitution is specific to the pinned
integer-key getter, not arbitrary hashes. Full native code/constant proof and
new balanced game measurements are required after any modification. Later
foreign mid-function patches are unsupported; the health check invalidates
evidence if they change the body. The normal package and Documents archive
are not replaced and no commit/push/publication is performed.

[Microsoft SEC_IMAGE_NO_EXECUTE contract](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createfilemappinga).

## Runtime result / rejection

All6 passes reached78% without death and restored92 files each. N1 nevertheless
lost focus for195 target frames: the entire absent/present N arm is INVALID.
The comparator refused it. N2 is not substituted or used alone to claim a gain.
The contiguous A1/B1/B2/A2 reference-only block is independently balanced and
passes the same configuration/coverage checks. Production version is verified
from the actual presence event; the mod-profile inventory deliberately excludes
the experimental variable and cannot establish that version by itself.

Reference run medians versus compact R36:28.9162 versus27.8109 FPS;
p9548.6450 versus47.3231ms; p9952.1290 versus51.4527ms;
worst53.3377 versus58.0562ms. This does NOT justify activation: mean is worse,
peaks are worse and neither comes close to120. Available physical memory during
the target was724–763MiB and reference passes vary too; causal attribution of
the small differences remains uncertain. Data is not silently corrected for it.

A further CPU-only variant factors the same integer identity into two multiplies
without a lookup table.56,584 controlled evaluations pass. It is not consistently
better than the compact variant in the kernel (wider-table ratio to native0.978,
against0.850 compact in that run). No GD build/test of the factored variant is
justified yet. The explicit runtime selector is not exposed by the build helper.
Both approaches remain isolated. No normal package, install, push or publication.
Next priority: count real native work before another replacement; native
visibility already caches some group combinations, so fixture duplication is
not evidence that the engine repeats that entire demand.
