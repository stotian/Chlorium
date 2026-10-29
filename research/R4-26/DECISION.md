# R4-26 - Owned uniform scale program

KEEP as a closed native-checked prototype; no GD hook or installation.

The shared identity executor gains an explicitly selected scale-update path.
The old identity instantiation excludes it at compile time. The new program
copies at most 64 commands, validates ownership once, and compiles the native
linear concat/decomposition once per phase rather than once per member. Native
position/reset operations, helper publication, scale fields, callbacks and
section order remain per object. FP flags are published after native movement,
including when a known observer cleared them. No native object visit is removed.

Domain: initially identity objects, unique active members, first stamp newer
than every member, strictly increasing later stamps with cleared tracking,
positive power-of-two scales from 1/16 to 16, fixed masked nearest SSE controls.
The native reset restores scale one exactly in this dyadic domain. Commands,
membership, linears, callback tables and lifetimes remain synchronously owned;
only the fixture's known position-writing/flag-clearing observers are allowed.
This contract is not established for actual GD or third-party hooks.

110202 final assertions pass: all 81 scale pairs, four DAZ/FTZ controls, sticky
flags, float-quantization boundary and large positions, visual reset enabled
and disabled, position-writing and flag-clearing callbacks, null/skipped
members, stamp endpoints, old center stamp/offset reset, and 4096 members over
64 phases. Full callback snapshots cover small worlds; dense worlds compare
final state and callback counts only. Unsupported inputs reject before writes.
An input aliased into a native helper's cache is overwritten by the first phase;
the owned copy survives. Borrowing that input instead fails at assertion 3.
Shared executor regressions: R4-20 327427 and R4-22 26054 checks pass.

Nine alternating batches include copy, guards, compilation and execution;
construction is outside both paths. For 4096 objects/16 phases, medians are
19694.6 / 14960.5 us; paired program/native median 0.759, range 0.682-0.820,
9/9 wins. At 64 phases paired median 0.660, range 0.591-0.791, 9/9 wins. One
dense phase loses 8/9 pairs (median 1.453). Raw rows retained. Benchmark ran
before additional rejection/center checks; the active algorithm was unchanged.
These are perturbed-machine fixture kernel times, not FPS or a stable multiplier.

Read-only inspector ACCEPT for closed scope; center-reset coverage P2 added
and checked. Dense intermediate state, real producer/reader barriers and
cross-mod ownership remain unproven. Same-stamp cumulative scales are explicitly
rejected here; extending those semantics is a distinct experiment, not an
assumption that separate physics steps can be combined safely.
