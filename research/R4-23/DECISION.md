# R4-23 - Installable canonical-core candidate V10

KEEP as an offline-checked candidate, not a demonstrated GD performance win.
Version v0.3.0-alpha.6, Windows x64, GD 2.2081, Geode 5.11.2. Main installation
unchanged; no GD launch or in-game validation authorized in this step.

## Concrete compatibility repair

The default target no longer links NativeQuadRenderer, Shader, Buffer or common
renderer code. It registers no atlas/batch draw hooks, owns no private GL
resources and performs no per-draw scan, copy, restore or transport operation.
Cocos retains its exact submission, dirty subrange uploads, canonical GPU-only
data and GL postconditions. Thus the V5 mechanisms reproduced by the existing
native-Cocos oracle cannot execute in this candidate. This does not attribute
every reported dark frame to Chlorium or prove all native/mod interactions.

The active optimization reuses repeated quotients in the existing pure native
Area routine. A complete engine-body fingerprint is required. All three source
instructions are checked before modifying an owned plan, and intermediate
bytes are retained. One 87-byte contiguous Geode patch replaces three separate
patch objects; installation/toggling cannot leave a partially enabled set at
the Chlorium level. Toggle failures are logged without claiming rollback.
The internal atomicity/failure behavior of Geode's memory writer is untested.
No triggers, object callbacks, collisions, timing or simulation steps are removed.
The existing detached-PlayLayer pause guard remains.

This is intentionally a small native baseline, not the old math cache or an
integration of the unmeasured R4-22 group program. It removes known defective
and expensive replacement work while retaining a separately checked exact
optimization. It has no new spatial renderer or proven large FPS multiplier.

## Evidence and provenance

- 1619360 result-bit/FP-status comparisons against the complete runtime-extracted
  Area routine: all rounding modes, four DAZ/FTZ modes, NaN/Inf/subnormal inputs,
  integer boundaries and initially set sticky flags. Exceptions remain masked;
  actual unmasked traps are not validated. The original timing loop was skipped.
- Invalid/short rewrite plans reject before changing their input. Bytes between
  substitutions remain identical. The old test's missing test-game path was
  corrected to read the installed executable; this does not launch it.
- The actual normal PowerShell packager accepts V10 in an isolated marker-only
  fixture and rejects V5, mixed V5/V10 and incomplete V10. Rejected inputs leave
  its prior package unchanged. Those synthetic files are never loaded as DLLs.
- The serial manual build reuses fingerprinted SDK/static dependencies and
  preserves its exact inputs archive. Compile/link pass, SDK warnings retained.
  Packaged DLL: 82944 bytes versus 282112 bytes in the installed V5. File size
  is not a RAM, frametime or FPS measurement.
- Actual candidate ZIP CRC, manifest/version/settings, licenses, x64 PE DLL
  characteristics, backend markers and embedded-DLL/source-archive hashes pass.
  Only the enabled setting remains in the package. Old replacement-auditor
  snapshot exports are absent, so they cannot certify V10 as an active V5/V9.

The first CMake configuration failed because a temporary source was removed
during configuration. The build started against that stale configuration was
cancelled. After freezing sources, fresh CMake configuration succeeds. Detailed
final build status and hashes are in evidence.json; interrupted logs remain
private and are not counted as successful builds.

Inspector initially REWORK: P1 contradictory V5/V10 packager checks, P2 partial
rollback across three patch objects. Both corrected and covered as described;
one targeted independent follow-up ACCEPT for these fixes. It did not execute
tests, validate the final DLL or establish in-game performance.

## Delivery and next gate

The versioned candidate is copied separately to Documents/chlorium. No file in
the Steam game, Geode mods/settings or shared saves is changed. Sources are
pushed to the research branch; main remains unchanged.

Subsequently the user authorized game tests on 2026-10-26. The in-game gate is: loading alongside
the user's mods, targeted blackscreen/visual comparison, and full-cost frametime
measurements. No GD run is started by building, packaging or these checks.
Stable 60 FPS, disappearance of every blackscreen and 3x remain unachieved.

Runtime follow-up: four full-mod passes on Grief and Heliopolis reach their
targets with zero deaths and active code. Performance remains inadequate;
Heliopolis regresses in its ordered pair. Do not install or promote as an FPS
upgrade. The native visual-diagnostic pass also fails before a finished report.
Detailed results follow in the single experiment journal; blackscreen freedom
is not established by the performance-only passes.

The bounded PE auditor was compiled after these runs and has not been run in GD.
Its source archive overwrote the exact old auditor source archive; the actual
runtime DLL/package hashes remain recorded, but that old source ZIP is unavailable.
Build/package scripts now preserve previous evidence before overwriting it.
The real source exporter also accepts V10 and rejects V5/mixed/incomplete binaries
before changing the destination; the packaging fixture exercises that path.
