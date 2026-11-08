# R4-39 — Packed four-corner arithmetic, bounded prototype

Keep isolated. Four corners of one sprite use packed SSE2 without crossing any
native callback barrier. Original R4-33 eligibility and masked-exception guards
remain; rejection leaves bytes and FP state unchanged. Signed zero, corner
order, Z, colors and UVs are checked against the actual pinned Cocos method.

Heliopolis, Orbit83 and Grief64 captures plus the deterministic arithmetic
corpus pass1296384 native comparisons across rounding/DAZ/FTZ/sticky settings.
All29031 supported captured quads match. This is arithmetic/capture evidence,
not a full-level visual, black-screen, crash or game-performance validation.

Heliopolis CPU kernel,9 alternating paired batches: packed/scalar paired median
ratio0.9541, range0.9003–1.1237,6/9 wins. Scalar helper includes its domain checks;
this comparison is NOT against the complete native callback and shows no global
FPS gain. The variation and regressing batches do not justify game promotion.

The prototype is unchanged after these results; private source-r1.zip preserves
the three R4-39 inputs. Next experiment investigates exact operand ordering for
all masked floating-point inputs so a native arithmetic splice can avoid the
ten separate float-bound checks. Native virtual calls, parent composition,
atlas updates and children must remain. No normal build or installation change.
