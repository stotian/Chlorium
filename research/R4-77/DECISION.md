# R77 — retained geometry and exact shared parameters

Owned GPU prototype; not linked into GD. R75's emitter resent64 geometry bytes,
48 parameter bytes and96 fallback bytes per record/frame, including zeros for
qualified GPU records. R77 retains geometry, shares bit-identical48-byte parameter
records and uploads only changed contiguous ranges. Native fallback quads are
packed separately, so qualified records no longer transfer dummy fallback data.
Current values determine classes; there is no future-frame information, numeric
approximation, signed-zero normalization or NaN-payload merging.

The shader emits directly into its owned canonical vertex buffer. No intermediate
GPU copy is needed; the same buffer name survives frame-size changes. Verification
reads all96 bytes through its vertex-buffer binding. Every refused pose/geometry
still has its exact native quad, and an all-fallback result still fails qualification.
The R75 native-row admission is shared, and its three original real-data replays
were rerun unchanged after extraction. Root positions remain resident across frames.

## Critical CPU revision

Review found that rebuilding a tree of classes every frame is unacceptable at
large diversity:65,536 actors/unique parameters measured27.05ms median,36.67ms
worst averaged batch. This would consume more than the120FPS budget by itself.
Classification now falls back to direct identity slots after256 distinct keys;
the cache retains prior groups while validating current members bit-for-bit.
Uniform changes update one shared value. Divergence/admission changes rebuild;
poor sharing stays on the direct route. Invalid updates preserve the last frame.

The controlled CPU benchmark uses65,536 fully prepared records, nine batches of
eight iterations; it excludes their production, GPU work and game callbacks:

| Diversity | Initial/rebuild median | Retained median | Worst retained batch |
|---|---:|---:|---:|
|16 values|2.34ms|0.411ms|0.482ms|
|65,536 values|4.00ms|0.452ms|0.603ms|

Rebuilds remain materially expensive. A live adapter must use producer events and
handle changing populations without assuming the steady-state cost. Direct slots
may retain less compression after values converge; correctness takes priority.

CPU assertions cover exact classes, signed zeros, raw payloads, uniform updates,
divergence, changing admission, invalid-input preservation, the direct route,
oversized inputs and sparse ranges including growth/shrink boundaries.

## Real captured inputs, measured driver upload calls

Same R76 captures/root programs and complete output-byte checks as the R75 replay:

| Capture, two frames | R75 emitter inputs | R77 uploaded bytes | Reduction | GPU/native fallback quads |
|---|---:|---:|---:|---:|
|Orbit89–89.5|13,312|8,272|37.86%|30/34|
|Grief25–26|6,656|2,048|69.23%|30/2|
|Helio74–74.5|12,688|9,140|27.96%|18/43|

All96-byte quads and prior prefix/resident positions still match. Helio grows
from30 to31 geometry records and reallocates twice; the late root remains native.
Numbers count actual glBufferSubData payloads, including cold geometry and packed
fallbacks. They exclude common root seeds/commands, allocation cost, CPU encoding,
GL state management and verification readbacks. Reduced bytes are not game FPS.

Decision: retain isolated. The output buffer is owned by the fixture, not a live
GD atlas. GPU state restoration, driver-failure recovery, producer/lifetime
invalidation and continuous game costs remain to be qualified before integration.
All native CPU preparation/readers remain present. No normal activation or global
blackscreen/performance claim is justified.
