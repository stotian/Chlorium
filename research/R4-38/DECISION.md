# R4-38 — Native movement memory lookahead, rejected

Hypothesis: scattered GameObject fields stall the movement loop; preloading the
section/position/stamp cache lines16 objects ahead might hide those misses.
The exact GD2.2081 moveObjects body is copied into owned RW-then-RX memory.
All arithmetic, native branches, per-object virtual callbacks, count updates
and the final callback remain. No GD process, live patch or game adapter exists.
An inserted preload checks the captured array end and a null future pointer.
RAX is dead at this point on all native paths; original load/test and flags run.

3360 initial comparisons pass, then6720 for trampoline and inline variants:
all4096 object bytes, native layer bytes, callback order/intermediate snapshots,
null termination, empty/null arrays, callback changes to future positions/order/
stamps/count and all four rounding/DAZ/FTZ/sticky states. The source body hash is
4a7b0d8f19100568. These closed callbacks are fixtures, not a proof of arbitrary
native object lifetime, installed mods, protected pages or unwind safety.

Self-critique: the first prototype adds two jumps per object. A second clone
inlines the preload and relocates all four native branches crossing it. The
same functional oracle passes. Neither version eliminates any native work.

Nine alternating measured batches,128 complete loop calls per batch, warmup
excluded. Prefetch/native paired median time ratios (less than1 would win):

| Layout/count | Trampoline | Inline |
| --- | ---: | ---: |
| Contiguous256 |1.364|1.262|
| Permuted256 |1.419|1.287|
| Contiguous4096 |1.250|1.217|
| Permuted4096 |1.104|1.168|

Each variant loses all36 paired cases. Raw timings are private in
build-research/movement-prefetch/alternating-r1.local.csv and
alternating-inline-r2.local.csv. These are owned CPU loops, not game frametimes.

Decision: abandon both variants. No GD integration, normal build, installation
or game comparison is justified. Preserve negative evidence; do not keep adding
prefetch switches. Next useful work must reduce computation/representation cost,
rather than add speculative reads to the same scattered traversal.
