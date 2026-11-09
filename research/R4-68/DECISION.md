# R68 — packed CPU mirror of shared group programs

Isolated arithmetic/data-layout experiment; no game hooks or skipped callbacks.
R64's class programs are reused. Actors are packed by joint program once, then
two SSE2 lanes keep positions in registers through the ordered command list.
Repeated visits/stamps and zero deltas are retained. Locked lanes and omitted
last-position conversions are masked before arithmetic, preventing extra FP
exceptions. Rejected programs preserve state and FP controls. Empty classes do
no writes. Bounds and the packet reader are shared with R64's GPU implementation.

Owned tests: 1,178 mixed/odd-size actors across nearest/DAZ/FTZ modes, all 1,536
object bytes compared after selective publication; native sticky FP state too.
The real two-frame packet adds 55,734 state comparisons and exact recorded GD
end-position witnesses. 82,739 logical visits become 55,551 packed state stores.
A mutating native callback deliberately disagrees with the independent packed
evaluation: callback/CPU-reader closure is still required before any live use.
The native fixture uses section=-1 and controlled callbacks, not real gameplay.

Nine alternating measured batches, 32 recurrences each, first batch excluded:

| Owned fixture operation | Median ms | Worst ms |
|---|---:|---:|
| Native closed movement loop | 1.418 | 1.546 |
| Packed kernel only | 0.254 | 0.328 |
| Kernel + copy + full owned-object scatter | 2.494 | 2.550 |
| Kernel + copy + moved fields only | 1.392 | 1.451 |

The earlier full scatter already lost (2.463 vs 1.349 ms). Publishing only fields
that native movement can change recovers parity, not a convincing complete gain.
Repeated closed calls omit visibility, lifetimes, section callbacks, competing
writers and class-rebuild costs. No ratio above is a GD FPS result.

Decision: retain packed owned-state representation as research; abandon full
CPU-object republication as an acceleration strategy. No runtime promotion.
Future gains require owned render state and a qualified native-reader boundary,
rather than a faster kernel followed by copying the entire object model back.
