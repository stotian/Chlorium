# R4-47 — Event-driven retained renderer, NOT activated

Direction requested: concentrate on the full rendering rewrite rather than
small arithmetic/shader tweaks. Scope: a scene authoritative for its own render
records, a persistent GPU endpoint, and deferred checks. No native hook changed,
normal package rebuilt, installation modified, game opened, or test executed.
The user's latest restriction forbids tests while they are playing.

## Hypothesis and implemented boundary

R4-33 reconstructed exact bounded sprite corners but rescanned two captured
frames and retained native preparation; its fixture regressed. Repeating that
architecture cannot establish a new engine. R4-47 instead accepts creation,
replacement, parameter, deletion and painter-order notifications directly.
It performs no per-frame full-scene comparison, quad reconstruction or resource
map scan. Static geometry/UV, dynamic parameters, order and native-quad fallback
have separate persistent lanes. It reuses R4-33's shader/record layout and the
existing DirtyStateUploads/ContextEpoch helpers rather than inventing new math.

An unchanged acknowledged scene performs no upload work in synchronize().
That is a source-level property, NOT a measured runtime result. Dirty block
coalescing may send unchanged initialized records; scattered changes can still
produce a broad upload. Dense animation still changes many parameter records.

Handles carry scene identity and slot generation. Deleting an ordered sprite
blocks rendering until a fresh order is supplied. Unknown/invalid producer data
invalidates the scene; it is not silently ignored. Complete native quads remain
available for unsupported arithmetic/custom geometry. Fallback colors/UV are
derived from that same quad. Switching scenes, sinks or context generations
requires a full seed. Failed/exceptional uploads retain pending work; no draw
can use a partially acknowledged version. Reentrant producers cannot commit.

The GPU endpoint owns its program, buffers, index topology and VAO. It restores
SSBO indexed ranges 0..3 (the older general guard omitted binding3), generic
storage binding, VAO, program, texture0 and active texture. ContextEpoch gates
resource destruction to avoid deleting recycled names in a replacement context.
An explicitly designated offscreen texture target is required. Unsupported
depth/stencil/feedback/query paths decline before submission. Post-submission
errors have a DISTINCT result: issuing native drawing immediately would duplicate
blending. A live adapter needs verified target restoration before retrying.

## Static adversarial review and corrections

- Reusing storage for another scene could otherwise leave unmarked old data:
  a sink must also acknowledge the scene identity before incremental updates.
- A native fallback's previous parameter colors/UV could disagree with its quad:
  they are now copied from the complete fallback on every replacement.
- Slot deletion/reuse could leave order entries naming a new owner: generation
  validation and an invalidated order prevent submission until repaired.
- Clearing dirties for one GPU consumer could hide changes from another:
  changing the acknowledged consumer triggers a full seed.
- Native redraw after an uncertain draw could blend twice: failure results are
  explicitly separated before/after submission; no live adapter exists yet.
- Shader allocation error text must not be accessed through a pointer evaluated
  before initialize mutates its string. Deferred GPU checks now sequence these.
- Order membership initially cleared a capacity-sized reference array on every
  reorder. Revision stamps now update only listed draw occurrences, retaining
  duplicate occurrences without an extra whole-scene clearing pass.
- Matching CPU/GPU revision numbers alone do not prove the context still exists:
  the renderer's acknowledgement also verifies its context-generation sentinel.

## Status and next required work

Code written; NOT compiled or tested. CPU contract scenarios and an owned GPU
pixel/lifecycle fixture are supplied but deliberately not executed. build.cmd
only compiles them; running the resulting executables is a separate action.
The record declaration extraction changes R4-33 research includes only; its
regression suite must also be rerun after tests are authorized.

Storage is bounded to65,536 slots per scene, not an entire arbitrary-sized level.
All four lanes are allocated at capacity, including the fallback lane even when
unused. CPU/GPU memory overhead is therefore real; spatial regions and compact
fallback storage are still absent. Resource generations prevent stale reuse but
do not make this allocation strategy a measured performance improvement.
Uploads use ordinary synchronized glBufferSubData, not unsynchronized mapped
memory. Command ordering protects earlier draws during slot reuse; driver stalls
remain possible and have not been measured.

This is the retained data/draw foundation, NOT a completed replacement for GD.
There is no installed native producer hook, callback/CPU-reader ownership proof,
group or Area migration, complete native shader/effect bridge, spatial selection,
checkpoint contract or full-game performance evidence. The fixture fragment shader remains
an owned test shader, not an implementation of every native effect. Native GL
error state and post-draw rollback require a separate integration contract.

Keep isolated. Next: connect authoritative render-only producers under shadow
comparison, then transfer ownership only across proven CPU-read barriers. Reuse
the existing group/Area research when moving shared pose calculations; do not
replace it with an unchecked reassociation of floating-point operations. The
normal renderer stays the reference until balanced in-game frametimes and native
pixel/effect coverage justify promotion. No FPS gain or black-screen fix claimed.

## 2026-11-05 — Native source notification adapter, still unexecuted

SpriteSources accepts complete synchronous SpriteRecord/native-quad pairs using
the existing capture schema. Native addresses are opaque identity values, never
dereferenced. Creation/destruction and native IDs map to generation-safe scene
handles; unknown producers, duplicate births and stale IDs invalidate the scene.
Every creation also returns a unique lifetime token, required by publication,
destruction and painter order. Address/ID equality alone cannot authorize a late
notification after both values have been reused. Deferred controls cover this.
Creation after an ordered deletion remains possible before the order is repaired.

Encoding is shared with R4-33; supported records encode geometry/UV, colors and
pose without copying the native positions into their GPU lanes. The bridge is
explicitly SHADOW: it reconstructs CPU corners, compares all96 bytes against the
observed authoritative quad, and restores incoming MXCSR. Missing quadMatches,
unsupported arithmetic or a mismatch selects the complete native quad. This CPU
comparison adds work; it is NOT an optimized native replacement or evidence that
an earlier native calculation can be removed. Native hooks are still absent.

Scene.update compares only the notified record's lanes. Identical notifications
do not advance its revision or trigger uploads; pose, UV, color, rectangle and
fallback changes mark only affected storage. The normal path is unchanged.

Deferred CPU scenarios cover source encoding, notifications, native mismatch,
fallbacks, stale IDs and address reuse. The owned GPU fixture additionally routes
source publications through the retained renderer and switches between scenes.
Neither suite has been compiled or executed. No game, UI, screenshot, installation
or package changes. Producer coverage and liveness remain contracts the future
hook adapter must establish; source records alone cannot prove those contracts.

## Reviewed native shader text bridge, still uncompiled

NativeShaderSources admits only the pinned R4-33 resident vertex (2056 bytes,
FNV59251a3c19c8d368) and the captured standard Cocos vertex/fragment texts
(709/a2318967aee60148 and516/a9c8e0b64e142972). It keeps the resident position
graph, uses native varying names, and converts the reviewed fragment's legacy
declarations/output/texture call to GLSL430 syntax. The fragment expression stays
vertexColor * texture; the fixture-only native_effect multiplication is absent.
Renderer.initializeReviewed owns the resulting linked program and requires its
MVP/sampler uniforms. Unknown texts decline before GL allocation in this mode.

This covers ONE reviewed standard program. It is not a generic shader translator,
support for every2.2 effect, or proof of the native executable currently linked.
GLSL version/interpolation/compiler changes may change pixels. R4-41's historic
native/helper pixel gap is NOT resolved by this source transformation. Offscreen
target, state guards, fallback and no-live-hook restrictions remain unchanged.

The two previously captured source texts and input provenance were extracted
under build-research/retained-scene-r47/native-reference.{vert,frag}; this was file
inspection only, not a test. The optional GPU fixture arguments accept those
files and prepare changed-source rejection plus equivalent owned-pixel scenarios.
They have NOT been compiled/executed. Native linked-program/framebuffer parity
and all other native programs remain unverified. No FPS or visual success claim.


## Qualification update 2026-11-06

The earlier not-compiled status above is historical. R47 CPU contracts now pass;
16 owned GPU image cases pass with the pinned original Cocos shader texts,
state restoration, style/fallback updates and cross-snapshot private storage
reuse. R49 native one-shot Helio580/580 reconstructs exactly on the observed
batch. See R49/DECISION for native cache qualification and limits. None of this
removes native CPU preparation or proves full-level effects/performance parity.
