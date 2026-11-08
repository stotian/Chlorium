# R4-34 — Native quad CPU-read diagnostic

KEEP DIAGNOSTIC ONLY. No CPU calculations or native callbacks are suppressed.
R4-33's resident renderer was slower with native preparation retained. Before
retiring any preparation, find actual CPU consumers instead of guessing that
sprite quads are render-only. This does not establish global dependency closure.

## Implementation / safety review

Four thread-local hardware data breakpoints observe the X coordinates of one
leaf decoration sprite for one interval between captured frames. The diagnostic
does not retain the sprite or dereference its saved pointer later; the second
capture must see the same pointer/UID. Missing identity invalidates observation.
Bounds:512 events and16 raw stack words/event. Raw stack candidates are NOT an
unwound call stack. Overflow, another debugger or changed ownership invalidate
coverage. Existing enabled debug slots/single-step reject before changes.

Initial experiment changing debug registers in a software-exception context
reported zero hits; that approach was rejected. A helper now briefly suspends
the calling thread, uses Get/SetThreadContext, verifies the installed registers,
and resumes it. Cleanup preserves foreign slots that changed ownership and
preserves LastError. Standalone checks cover reads/writes, slot selection,
unwatched memory, cleanup, overflow and foreign ownership. The handler allocates
nothing, calls no Windows API and changes no watched memory. Stack reads stay
inside the captured committed stack interval and skip watched words.

Observation is explicitly enabled by CHLORIUM_QUAD_READ_WATCH=1, only in the
isolated pass-capture auditor. Runtime guards reject other modes/old binaries.
All its frames remain excluded from FPS comparisons. A process-lifetime bounded
observation object avoids waiting for a helper under DLL teardown; normal frame
end/finish explicitly remove the observation. No normal renderer uses it.

- [Microsoft SetThreadContext contract](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadcontext)
- [Microsoft GetThreadContext contract](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadcontext)

## Actual Heliopolis evidence

Two diagnostic runs74–78 reached the target without deaths; each restored92
shared files/SHA values and left the main installed package unchanged. The V3
observer package is711f036c560623fb5ca13a3bc21a8b17e938816034f06d0f769b43f1d246ebb0,
DLLc3086add148146c8ffd0b3e23991a2d8288e8894be968c9d62055cfd7a6ee5c8.

One moving child sprite was seen again, with no overflow or debugger conflict.
The resolved15 events are4 Cocos writes,4 Cocos reads and7 VCRUNTIME reads.
The writes belong to updateTransform; Cocos reads belong to updateQuad. That
exported leaf lacks unwind metadata, so the resolver accepts its bounded
straight-line instructions through RET, rejecting calls/jumps/interrupts.
VCRUNTIME raw stack candidates include the auditor, consistent with its capture
comparison; this is not a proven unwind or evidence excluding other readers.

Each resolved memory instruction ends at the captured post-instruction PC and
matches bytes read from the live process AT FINISH. This is not an atomic code
snapshot at event time. Module files/hashes and auditor package SHA are bound
to the private receipt. Unknown/changed code remains unclassified. Resolver
checks reject bad slot masks, non-diagnostic evidence, shifted instruction ends,
changed bytes and unbounded/branching leaf procedures.

The accompanying atlas dirty sidecar is also observed before each original
draw. Some native draws already have clean, resident atlas data. Therefore the
R4-33 full-quad byte reference is theoretical, NOT automatically the native
transfer volume. Dirty observations themselves do not measure GL upload bytes.

## Decision

Keep this instrument isolated. Main-thread observations of four coordinates on
one sprite cannot authorize skipping all sprite/atlas work or logical object
updates. The concrete next boundary is the render-data copy to updateQuad,
not GameObject positions used by visibility/anchors/gameplay. A deferred native
atlas mirror would need flush barriers for ALL CPU reads/mutations and a native
fallback before submission; current code does not claim that ownership.

Private evidence: build-research/runtime-canonical-v10-isolated-reads-v3-helio-r1-pass-capture/
and build-research/helio-native-read-sites-r2.local.json. No screenshots or FPS
gain claims. The normal package and current installation remain unchanged.
