# R4-16 — Canonical GPU replacement, native draw

**REJECT V8 transport for promotion. Keep contract oracles. No universal visual claim.**

Native quads become authoritative only for the exact dirty draw subrange.
Clean atlas calls go directly through the original draw. For dirty calls a
separate spare VBO is orphaned, the exact CPU interval is uploaded, and the
unchanged head/tail is copied from the canonical native store GPU-to-GPU.
Only after preparation is complete does ownership exchange permanently:
native atlas owns the new complete store; the cache owns the previous one.
The actual native call retains shader, attributes, indices, painter order,
cache interactions and GL postconditions. No CPU readback or shadow comparison.

This competes with V7 temporary leases, which lose GPU authority. V5/V6's
manual draw is also rejected because its GL postconditions differ. The V8
protocol avoids both reproduced defects in the local supported Cocos oracle.
636 real GPU checks pass, including canonical GPU-only head/tail preservation,
repeated ownership exchange, alias/budget rejection and native partial draws.
128 randomized dirty intervals match a full byte oracle; a mapped canonical
store is rejected. This is buffer fidelity, not whole-scene image parity.
Buffer regression tests verify alias/mapped rejection and exactly-once deletion;
41 buffer, 18 rendering and 7 geometry regression checks pass. Native V8 builds.

Storage is bounded to 32 MiB per spare. Mapped stores, immutable spare storage,
active transform feedback, unsupported GL (<4.4), invalid bounds/context and
foreign atlas/getter/texture-bind Geode hooks select native fallback. The exact
native texture getter body is checked before moving uploads ahead of it.
Raw unregistered GL hooks and code outside this supported engine contract are
not established compatible. VBO numeric identity changes: third-party code that
retains external references needs a separate ownership contract. No gain is
inferred from reduced CPU copying: GPU copies and driver queries have costs.
The V6 audit's direct/uploaded counters mean actual dirty subrange uploads;
its sparse counter means clean native draws in V8; copied_records remains CPU
copies (zero), not GPU copy bytes. Original installed V5 remains unchanged.

In-game paired results are recorded in evidence.json after run restoration.
The PowerShell 5 startup attempt failed before gameplay and restored all 90
shared files; the guarded retry uses PowerShell 7. Entry points now require 7.


Grief full-mod 0–40%: 3182/2971 visual samples, zero deaths, both targets
reached and profiles restored. Six unmatched full-dark episodes in this pair
all have supporting native intervals in the earlier native reference corpus.
No episode tolerance was changed. Attribution remains unresolved across runs.
Heliopolis full-mod reference is invalid: an 11.43 s wall-clock interruption
(with only ~0.42 s game CPU time) skipped the required probes. No verdict.
An isolated 74–78% pair reached its target with 212/202 samples and zero
additional dark episodes. This does not establish full-mod compatibility.

The visual comparator originally required a full-mod debug-disable request
even in isolated runs. Its only exception now accepts the exact audited V8
production/audit package hashes with only loader, node-ids, audit and optional
production loaded. Unknown binaries, external loaded mods and mismatched
profiles remain rejected; adversarial tests cover these cases. Source HUD is
outside the measured ROI. Existing episode and coverage checks are unchanged.


Orbit isolated 83–100% reaches the requested window with 292/234 visual
samples, zero deaths. One unmatched single full-dark frame at 97.3372% remains
unresolved: the native trace has only one sample in 97.0–97.7%, at 97.4133%.
Do not call this a visual pass. All passes restore 90 shared profile files.

## Actual performance pair — reject

Grief 0–40%, all user mods, no visual readback/probes, full detail, no shake,
no noclip, zero deaths, active candidate with zero inactive renderer frames.
3926/2792 accepted samples: native 59.118 FPS, V8 43.911 FPS; p95 20.217/31.922
ms, p99 21.973/39.114 ms, worst 70.878/1783.909 ms. Native 2024 and V8 1804
frames exceed 16.667 ms (51.55% / 64.61% of accepted frames). Mean drawScene
7.826/21.106 ms; update 3.820/5.802 ms. Per-percent results retain every slow
section in performance-grief.csv. No claim of a controlled 3x or of meeting 60.
A single ordered pair and background/RAM variation limit attribution; it is
already enough to reject this candidate. V8 has 114608 process page faults
versus 4154 natively; that counter does not distinguish disk-backed faults.

The complete-store rebuild/orphan/copy protocol adds work to native preparation
and cannot be retained on the installed path. Its value is the native-contract
oracle and authority witnesses. Next candidate must preserve the canonical
store while avoiding per-draw allocation and unchanged-range GPU copies; a
bounded persistent transfer ring is a competing architecture, not a proven fix.
The initial performance startup requested probes without visual mode and was
rejected before gameplay. Profile restored; retry excludes probes by construction.
