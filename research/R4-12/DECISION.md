# R4-12 — Causal group command IR and explicit deferred debt

**Decision: REWORK. No GD integration or FPS claim.**

The R4-11 measurements do not justify a math cache or pure matrix fusion as the
primary architecture. This prototype changes the work representation instead:
record a pure position-affine command once per group; merge relevant command
streams in global order when an object is read. Group membership changes first
materialize the object's prior state. Generational handles reject recycled
slots; checkpoint restore invalidates old world handles. Unknown capabilities
force a full barrier and return false for the producer to execute its native
fallback. A bounded command budget also forces an explicit full barrier.

This is a CLOSED MODEL of position-affine commands. It does not implement GD's
Area weights, rotation/scale bookkeeping, spatial sections, collision, native
callbacks, rendering, variable FP control/observation or mod compatibility.
Proving the model does not prove those missing native producer/reader contracts.
No raw GameObject pointer is consumed and this code is absent from mod targets.

## Evidence

`run-cpu.cmd`: **105,394 checks pass** against an eager model, including overlapping
groups, noncommuting command order, delayed reads, membership mutation, direct
writes, unknown observers, bounded storage, checkpoints, destruction and slot
reuse. Two deliberately broken variants are rejected: wrong cross-group order
and a barrier that drops pending work. Object allocation uses a free-slot stack,
not repeated linear scans of every live object.

Single timing demonstration, not a repeated benchmark: 32,768 objects, 64 group
events, 256 readers. Eager evaluation 6340.3 us; deferred reads 340.2 us; final
debt barrier **24868.3 us**. Before the barrier: 16,384 applied operations. After
settling: **2,097,152**, exactly the eager total. No work was magically removed
from the settled world. Timings include the model's allocation/lookup overhead.

The barrier exceeds 16.67 ms in this fixture. Promoting the sparse-read timing
alone would hide a worse hitch and would violate the actual stable-60 objective.
The next useful experiment must improve bounded debt settlement/publication
(bulk chronological/SIMD or versioned worker/GPU state), and prove native read
barriers. Keeping unknown readers safe is mandatory; dropping their updates is
not an optimization. No additional game launch was made for this model.
