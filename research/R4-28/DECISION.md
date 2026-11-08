# R4-28 - Actual isolated Grief visuals and Heliopolis baseline

Goal remains unachieved. These are section diagnostics, not full-level proof.
No main installation change, promotion, commit or push. Tests stopped on the
user's subsequent instruction; later work is source editing and analysis only.

## Controlled inputs

GD 2.2081, Geode 5.11.2, full detail, shake off, native collisions, no noclip.
Minimal node-ids dependency plus the independent auditor; the production V10
package exists only in the candidate fixture pass. Hardware reported Intel
Core i3-10110U (2 cores / 4 threads) and Intel UHD Graphics.

The new bounded-PE auditor r2 compiles/links and packages separately. Package
SHA256 7c363ad2b45d3d0d62bcfd4acf8f7c6ba9bc85c251498003a0c6d80dc5a82c31;
DLL 98bb02f2b2cd5968a830bf9197772f869ba0654867edb3dad23885568a125505.
Production V10 package remains 107b945ce4940d49670c1aab287411e4f10ebcf45ec52174651a0d876d7f2a71.
The launcher no longer overwrites the plan's auditor selection with the old
hardcoded V10 package. Both planned package hashes are checked before staging.
Real preflight and altered-byte rejection fixtures passed before the test stop.

## Grief auto 88952972, visual interval 24-40 percent

First cold launch exits 1 before reaching the harness; no measurement. All
92 protected profile files restore with verified hashes. Steam logs show that
the game was launched during client startup, before login completed. One allowed
startup retry with Steam initialized reaches 40 percent. This is consistent
with a startup timing race; it is not a demonstrated renderer crash cause.

Native: 2215 attempted/queued/measured frames. V10: 2329. Both have zero skipped,
invalid or pending images, zero deaths, zero inactive-core frames and completed
visual coverage. The GPU diagnostic fixture separately passed 257 readback/state
checks before launching GD. No recurring GL read errors in these isolated passes.

Both versions show 11 near-dark samples starting at 30.3931 percent. Their last
sample is 30.5057 / 30.5107 percent. Presence in both runs prevents attributing
that transition to Chlorium. It does not certify every dark image as intended.
At probes 24.903034 and 25.203314 percent the geometry appears consistent in two
inspected images; saw animation/particles differ. Core pixel mean absolute
channel differences are 1.6401 and 2.5196 / 255, so pixel identity is NOT proven.
Visual diagnostics are excluded from FPS comparisons.

Both provenances have identical game, level, auditor, common-mod and common-
settings hashes. Main installation unchanged; 92 files restored after each run.
Private raw evidence: build-research/runtime-canonical-v10-isolated/reports-88952972.

## Heliopolis auto 129598490, native performance interval 74-78 percent

No readback, personal mods or Chlorium production module. Target reached with
zero deaths/inactive frames; 214 included frames, one excluded boundary frame.
Average 38.6566 FPS, p99 34.4752 ms, worst 37.0026 ms; 212/214 exceed 16.6667 ms.
This plainly fails the 60 FPS objective. It is one native profiling pass, not
evidence of a candidate gain or an alternating paired comparison.

Mean timings: update 17.5293 ms, visibility 9.6975 ms, move 5.6914 ms, rotation
0.8596 ms, transform 0.00628 ms, Area 0.00409 ms. Timings may be nested and must
not be added. GPU timestamp span averages 23.2315 ms; that interval alone does
not distinguish GPU busy time from command-feeding gaps. Area-only optimization
cannot remove this measured frame deficit. Next code work targets diagnosis of
visibility/movement before selecting an actual replacement.

92 profile files restored; installed package unchanged. Raw evidence remains
under build-research/runtime-canonical-v10-isolated-performance/reports-129598490.
Orbit has no new run in this iteration. All-level, full-run, reproducible 60 FPS
and absence of renderer artifacts remain unverified.
