# R64 — resident poses, shared movement programs

Research component, not an enabled game renderer. Native callbacks remain unchanged.

R63 found hundreds of movement commands per frame but at most 56 ordered visits
per observed object. R64 therefore bounds the global table at 8192 commands and
each object's shared program at 64 operations. Identical programs share one range;
duplicates, zero deltas, stamps and order are retained. Joint two-frame classes
keep the resident actor's class index stable. Only commands/ranges/indices upload
after the initial position seed. The same R62 compute shader produces native
24-byte vertices directly for the original Cocos graphics program.

`compile_trace.py` requires complete final observations and excludes actors with
unsupported roles, changed metadata, position discontinuities or semantic flags.
This observed subset is not proof that native CPU readers can be removed. The
first seed retains R42's domain. Recurrence additionally admits cancellation onto
the exact 2^-84 lattice; magnitude bounds reject unsupported future commands.

Owned Intel UHD checks: 80-command synthetic case, repeated membership and tiny
cancellation; then 27,867 actors/104 classes from the complete Heliopolis capture.
55,734 states and quads match native machine-code fixtures, and two nonempty
images match. The native final position witnesses also match. Geometry is
synthetic: these are not screenshots or FPS measurements from Heliopolis.
891,744 initial state bytes, 33,452 subsequent shared-program bytes, no position
uploads or CPU readbacks before drawing. This is a data-volume result only.

Invalid/oversized programs, unsupported rounding and missing contexts disarm
output before fallback; empty frames retain resident states. Foreign SSBO ranges,
program/VAO bindings, FP mode and LastError are preserved. Shader identity is pinned.
R62 regression: 266,304 states/quads and 128 images still match after sharing the
root-packet validator. Compiler and CPU contracts run in CI; owned GPU fixtures
require local GD/Cocos binaries and are not CI claims.

Decision: keep isolated. Native reader ownership, actual level geometry, hostile
GL error boundaries, live context/lifetime integration, fidelity and balanced
frametimes remain unqualified. No production package changed or speedup claimed.
