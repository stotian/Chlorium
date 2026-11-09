# R70 — fused movement and unchanged-section classification

Owned experiment only; not linked into any game profile. R69 found repeated
native section classifications. `SameCell.hpp` computes the exact native two-cell
decision in SSE2, under bounded normal operands and masked exceptions. Rejected
speculation restores FP flags. `PacketClassifier` caches scales only between
native callbacks and requires inexact already set before speculative conversion;
it refreshes after every native fallback. Negative native section indices retain
the original immediate no-op. No native changed-section callback is replaced.

`SectionOracle.hpp` copies the pinned installed native arithmetic/branch body
into owned W-then-RX memory; its changed branch ends at a counter witness. It does
not model native changed-section mutations. `MovePacket.hpp` reuses the existing
native movement field helper and preserves member order, duplicates, first-null
termination, command stamps and terminal callbacks in the complete fixture.

30,345 classifier decisions/FP states pass, including exceptional/unsupported
coordinates, clamping boundaries and rounding/DAZ/FTZ modes. 198,380 complete owned
movement visits match all 1,536 object bytes and native counters. The final
classifier skips 191,016 proven no-op paths in that controlled fixture.

Negative timing results on 32,768 owned objects, nine alternating batches:

| Prototype | Native median/worst ms | Fused median/worst ms |
|---|---:|---:|
| Per-object speculative FP restoration | 2.149 / 2.879 | 2.262 / 3.141 |
| Packet-scoped classification controls | 2.081 / 2.403 | 2.101 / 4.266 |

Each sample includes a complete closed movement call, not only classification.
The fixture omits real changed-section mutations, competing writers and runtime
hook overhead; it is not a GD frame-time result. Savings do not offset the extra
checks and scattered object memory. Decision: abandon runtime integration of this
shortcut. Retain the isolated counterexample and native-equivalence checks.
No game test, normal activation, or claimed FPS gain follows from this result.
