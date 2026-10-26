# R4-21 - Isolate Cocos fixtures from the Geode proxy

KEEP the test-harness correction; no runtime renderer change or installation.

The reported "Unable to Load Geode" window was owned by chlorium_gpu_checks,
not GeometryDash. Its module inventory contained the game-folder XINPUT1_4
proxy and Geode. Cocos imports XINPUT1_4; sibling dependency resolution could
start Geode inside a process without a GD entry point.

SystemInputGuard pins the absolute Windows-system XInput library before Cocos
loads, rejects an already-loaded foreign basename, verifies path and handle,
and holds its reference until after Cocos unload. Every native fixture loading
Cocos uses this guard. No game DLL, setting, save or search-path setting changed.

Two separate child tests pass: rejection of a harmless foreign XInput DLL and
Cocos load/unload retaining the Windows library with Geode absent. The latter
fixture loads Cocos only; it never launches GD. Inspector: ACCEPT, no P0/P1.
Its P2 requests (final isolation assertion before success output and these
child regressions) are addressed. Module snapshots do not prove absence of
arbitrary transient/concurrent DLL injection.

The first guarded Area pair passed 327427 correctness checks. Dense medians
11965.9 / 12489.8 us still regress; this is a native fixture, not GD FPS.
The guarded GPU suite passed 1162 checks, including deliberate reproductions
of known legacy defects. That does not establish that blackscreens are fixed.
Native affine passed 192000 comparisons, 8 overlap cases and 3 compatibility
guards. Native matrix passed 853560 comparisons but explicitly permits 244447
FP-status differences: it is not a full-fidelity candidate.

The initial GPU pass was interrupted at the Geode modal and is invalid. Earlier
R4-20 measurements did not record loader isolation; their historical values
remain, with this qualification. A subsequent stage-audit pass completed
439387 checks, but overlapped the user's Steam-launched GD process: its timing
data are excluded. The subsequent GPU attempt was cancelled before execution;
it is not another pass. R4-11's caller was compiled, not re-benchmarked.

No GD launches are authorized now. Continue the synchronous owned-group
prototype using bounded functional checks; defer fresh performance comparisons
while the user's game is running. Do not promote this harness fix as an FPS gain.
