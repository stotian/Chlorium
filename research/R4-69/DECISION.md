# R69 — native position-reader evidence

Diagnostic only, built on the R67 renderer with authoritative native vertex
storage. No native movement, rotations, section updates or callbacks are removed.
Four hardware data breakpoints observe X/Y of two live decoration actors on the
owner thread, for two render frames. The first starts inside a move call and is
partial. A helper suspends the owner only while it waits at explicit safe points;
debug registers are restored and verified before removing the exception handler.
Other debug ownership, foreign processes, incompatible native code, incomplete
windows, stale actor/layer identity and overflow reject the evidence. Irrecoverable
debug-state restoration terminates only the hard-pinned private probe process.
It is never compiled into the normal R67 candidate. All capture frames are
excluded from performance evidence; comparison tools reject this provenance.

Owned checks: 48 arm/disarm cycles in four DAZ/FTZ modes, 192 exact accesses,
LastError/FP preservation, duplicate/alignment/nested-owner guards and no residual
traps after disarm. Four malformed-file regression groups and six runner
combination guards pass. Both diagnostic profiles compile, link and package.

Private Heliopolis 129598490 capture, 74.0015–74.5%, Chlorium active, full detail,
no shake/noclip, isolated mods: 368 accesses, two actors, two consecutive frames,
no overflow or recovery error. All sites resolve inside the pinned GD image.
92 shared profile files restored and verified; main installation unchanged.

| Native procedure owner | Observed accesses |
|---|---:|
| GJBaseGameLayer::processRotationActions, 0x22b7f0 | 160 |
| GJBaseGameLayer::moveObjects, 0x22dd50 | 102 |
| GJBaseGameLayer::updateObjectSection, 0x227f50 | 100 |
| GameObject::getObjectTextureRect, 0x1978f0 | 4 |
| GJBaseGameLayer::addToSection, 0x226500 | 2 |

Names match exact generated binding addresses; offline instruction decoding uses
bounded PE unwind fragments and reports post-access trap addresses. This is
selected-field evidence, not inclusive timing, a control-flow proof, every actor's
reader closure, or a frame-rate result. Private trace and decoded evidence:
`build-research/r69-helio-readers1.local.json` and the protected campaign receipts.

Decision: keep the diagnostic isolated. A move-only GPU replacement cannot assume
these actors are CPU-independent. Inspect the native unchanged-section path next:
it has no callbacks/writes when both exact cell indices agree, and was observed
repeatedly after native movement and rotations. Any shortcut needs native byte/FP
equivalence and conflict guards before live use. No normal renderer promotion.
