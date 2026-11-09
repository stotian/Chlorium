# R65 — direct coherent vertex pages

Experimental input backend. Original Cocos draw/program/texture/index order and
all native object preparation remain. This is not the R64 group-offload engine.

R61 reconstructed already-prepared native geometry, dispatched compute and saved
the framebuffer for each committed batch. R65 instead copies the authoritative
dirty CPU quad prefix once into a coherent mapped vertex page. The native atlas
body executes once against that page. No compute shader, framebuffer backup,
GPU-to-GPU copy, conditional rendering or extra draw is involved.

The reviewed atlas body, pure texture getter, isolated mod graph, owner thread,
CPU range and canonical VBO mapping/size are prerequisites. Clean draws retain
GPU authority and fall back. The existing NativeVertexLease restores canonical
identity and dirty recovery; attribute pointers are also restored to the
canonical VBO. Partial native fallbacks keep dirty until a full canonical upload.
Busy pages decline without waiting. Sixteen fixed 1.5 MiB pages cap allocation
at 24 MiB. Fences are placed after the real draw; a failed fence poisons reuse.

Owned Intel UHD checks: 256 full image comparisons using the actual native atlas,
alpha blending and partial ranges; exact single native/getter execution; native
post-draw bindings and VAO pointer ownership. Pending GL errors survive. Mapped
canonical storage declines, unwinding restores ownership, busy reuse is refused,
completed pages wrap, absent contexts decline. This proves the owned draw contract,
not every level/effect or 120 FPS. Contract reference:
[ARB_buffer_storage](https://raw.githubusercontent.com/KhronosGroup/OpenGL-Registry/main/extensions/ARB/ARB_buffer_storage.txt).

First private Heliopolis test reached 78%, full detail/no shake/no noclip, active
backend, zero fence/GL failures reported. At 74–78%, 162 accepted frame intervals:
29.10 FPS, p95 44.84 ms, worst 52.56 ms. Native update ~19.03 ms and visibility
~7.96 ms remain; CPU work has not been removed. This single run is not a gain claim.
35,097 busy-page refusals over the full passage expose a pool-size limitation.

The first untimed R61 control was invalid: its new package omitted resident.vert
and committed zero batches. It is excluded. Packaging now shares its shader
requirement between writing and verification; the repaired control's exact frozen
shader is checked before launch. Both candidates use one independent auditor,
explicit labels and no detailed renderer timing. Shared comparison guards retain
all prior exclusions; five additional tests protect balanced loaded/loaded runs.

Decision: retain isolated pending replicated in-game frame distributions and
level visual diagnostics. No native-only run, main-install change or release.

## R67 recovery corrections

An unrelated-current-context fixture initially fails: the adapter queries a
native GLEW buffer before checking its page's context generation. Move page/context
qualification before native buffer queries; rejected reservations are fenced.
The test then passes with zero native buffer dispatches in the foreign context.

A second actual-native witness interleaves an accelerated full draw, an external
native partial draw and a full native fallback. Without recovery, the partial draw
clears atlas-wide dirty and the next image uses stale canonical vertices. A shared
PartialDirtyRecovery scope now covers owner-thread native calls outside the batch
after a lease, under the same closed callback graph. The existing partial-dirty
rule is reused. The witness reproduces the wrong image without that scope and
matches the full native image with it. Native callbacks/draw counts remain intact.

Full-only qualification was added: a partial upload does not make the remaining
CPU quad prefix authoritative. Partial draws remain native.

A third witness then FAILED: an additional native GL draw using the restored
canonical VAO read old vertices even though the accelerated image was correct.
Dirty recovery alone cannot protect native GPU consumers outside the atlas.
R67 therefore retains the canonical full native upload before its owned-page draw.
Successful calls restore the native clean flag; exceptions still retain recovery.
The live partial-dirty workaround is no longer needed because canonical storage
is never deferred. Its historical counterexample remains in the owned tests.
The subsequent-consumer witness now passes, together with 256 alpha images,
foreign-context, partial-decline, pending-error and unwind checks. An explicit
native-authority export reports one canonical upload per committed draw. Packager
and runner reject earlier R67 binaries without that identity/telemetry.

R67 is separately fingerprinted; R65/R66 FPS results describe their frozen earlier
binaries and are not relabelled as current R67 performance proof. Retaining the
canonical upload adds work; its actual cost must be measured in the paired campaign.
