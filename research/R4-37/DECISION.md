# R4-37 — Actual native opacity invocation census

No native code or renderer is replaced. Two thread-local execution breakpoints
count groupOpacityMod and opacityModForGroup entries for one interval between
the two bounded native pass captures. Full native function hashes and the exact
GD 2.2081 PE identity are checked before arming and after stopping. The explicit
pass-capture diagnostic/profile and matching package marker are mandatory;
simultaneous quad read watches are rejected. No results count as FPS evidence.

The shared R4-34 watch preserves inactive debug slots and detects changed slot
ownership. Execution traps set RF even after the bounded sample log fills;
per-slot counts continue. The original data-watch regression passes. An owned
RX-page oracle passes 1200 alternating entry/return traps, XMM1..3 and return
parity, log overflow, count reset, rearm, cleanup and LastError preservation.

Heliopolis auto129598490 reached78%, zero deaths, full detail/no shake/no noclip.
The one sampled inter-frame interval has exactly92 group-product calls and92
group-value calls, no sample truncation, debugger conflict or lost ownership.
70 product callers return to0x3af8aa;22 to0x3af9fe. All92 value callers return
to0x19ccfb. These are actual native entry return PCs, not unwound-stack guesses.
This is one interval, not a level-wide bound or an execution-time measurement.

Auditor package SHA256:
8d0c2e1d97367e8376e037a3a8fba53c9dbe1de9aa71407fdf57a2939042b31b.
DLL2497e1756d36fae50853f57c82b82c2846f63609a2811ba90eb42a8098205f58.
Private raw receipt is in runtime-canonical-v10-isolated-r37-helio-invocations-
pass-capture/reports-129598490. All92 protected files restored with SHA checks;
main installed package unchanged. No screenshot, commit, publication or export.

Decision: do not develop the opacity rewrite further on this evidence. The
engine already shares that work heavily; the existing sampled movement and
visibility paths justify investigating memory/section preparation instead.
The counter remains diagnostic-only. It proves neither visual equivalence nor
absence of black screens, and permits no normal-version activation.
