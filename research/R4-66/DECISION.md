# R66 — adaptive direct vertex pages, not promoted

R65's sixteen 1.5 MiB pages refused many busy reservations. R66 uses up to 128
size-rounded pages with the same 24 MiB total bound. Ready pages can grow;
pending fences never wait or overwrite. Native VAO references are restored before
reuse/deletion. CPU bounds and actual coherent-buffer growth are checked.

The 256-image native atlas fixture passes both variants. Its tiny workload uses
0.5 MiB instead of 24 MiB. That saving does NOT carry over to the actual level:
both R66 Heliopolis passages eventually reach the 24 MiB bound.

Four passes, same R66 auditor, Heliopolis 74–78%, full detail, no shake/noclip,
isolated mods, 120 FPS cap, no external sampler active during comparison:

| Order | Renderer | FPS | p95 ms | Worst ms |
|---|---|---:|---:|---:|
| 1 | R65 | 34.69 | 34.87 | 40.23 |
| 2 | R66 | 35.35 | 36.73 | 46.83 |
| 3 | R66 | 35.39 | 35.08 | 40.97 |
| 4 | R65 | 35.54 | 35.58 | 38.72 |

Busy refusals fall to 171/173 over the full R66 passages, with zero fence failures.
However FPS is effectively unchanged and some peaks regress. Keep R66 isolated;
the fixed-page R65 lineage remains the reference. These binaries predate R67's
context/cross-batch recovery fixes. No vanilla run or normal-promotion proof.

A separate R66 CPU diagnostic was added behind an explicit campaign, identified
auditor, frozen package hashes and sampling-session export. Performance sessions
keep that export inactive; comparison guards reject sampling. The sampler accepts
only the exact test executable and auditor SHA, uses read-only Windows snapshots,
and has a bounded observer budget. Twenty self snapshots/handle balance and eight
PE-unwind tests pass. The first live capture possibly overlapped a fixture build
and is unsuitable for time shares. A quiet repeat at 74.43–77.30% collects 189
leaves, no missing contexts/cleanup failures, 325.77 ms capture work, worst 4.07 ms.
These are sampled wall locations including waits, not inclusive CPU percentages.
Group movement, pre-visibility and atlas reindexing recur as leads for later work.
