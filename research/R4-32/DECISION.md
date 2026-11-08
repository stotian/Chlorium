# R4-32 — Bounded native pass capture

Hypothesis: stable local geometry can remain resident while matrices/colors
change. Whole-atlas slot caching may invalidate too much when native visibility
adds/removes sprites. This diagnostic measures that distinction without
replacing a draw, a native callback or a gameplay update.

Two consecutive frames only; at most 2048 atlas draws and 65536 quads per frame.
Data buffers are reserved before observation; overflow is explicit incomplete
coverage. Serialization happens after the target section, not during gameplay.
The observer calls each original batch/atlas draw once. It reads atlas bytes,
sprite IDs, matrices, rectangles, controls and ordering. No screenshots, GL
queries/writes or framebuffer reads. All frames of this profile are excluded
from FPS comparison. Explicit native/isolated profile, package markers/hashes,
plan switch and comparison rejection remain mandatory.

## Evidence and self-review

- CPU bounds/byte-copy tests pass; parser tests reject malformed/truncated data.
  Capture flags cannot count as performance evidence. Real launcher preflights
  and both package/flag mismatch guards pass without launching a game.
- First game attempt correctly stopped at the measurement auditor's prohibition
  on atlas hooks. A narrowly compiled diagnostic exception now permits only the
  exact atlas observer in native mode with production absent. Its truth table
  is checked; normal measurement profiles retain the prohibition.
- Heliopolis native 74–78 completed without death with V1 capture. Two frames
  at 74.00447/74.05817: 22 draws each, 10755/10919 quads, all mapped to native
  sprites, zero unknown draws and frame errors. 4,339,144-byte private capture,
  SHA256 c0051b2af64a81094b356df5bec4e33f11d3c32076817ab11041f6c7a4f19cab.
- Strict call-size alignment compared only 831 quads: ten calls changed size.
  Identity alignment instead retained 10690 sprites, with 229 additions and
  65 removals, zero ambiguous keys, and unchanged retained painter order.
  All retained local geometry/UVs were unchanged; 7965 matrices and 1711 colors
  changed. This is two-frame data sharing, NOT an FPS or speedup measurement.
- Review fixed typed aliasing (byte copy now), null arithmetic for empty draws,
  scene/context changes, TLS batch ownership and deferred serialization.

## Why V2 is needed

The actual Cocos updateTransform at RVA 0x8d570 also branches on byte 0x149.
V2 incorrectly identified it as m_bUnkBool2; compile-time layout checks and the
native arithmetic oracle disproved that mapping. It is CCSprite::_realOpacity,
whereas m_bUnkBool2 is at 0x139. V1/V2 cannot prove complete reconstruction inputs.
V3 captures the correct byte as nonzero/zero (not a C++ bool alias), with ABI
assertions. V2 had added native visibility, parent UID, native MXCSR, texture bounds
and current root GameObject metadata/groups. The parser retains V1 as explicitly
incomplete branch evidence; legacy capture binaries cannot be launched as normal
measurement. Root lookup is a bounded setup-time registry with UID checks and
no native getter calls. Root metadata is observational, not proof that native
updates may be skipped; anchor/trigger dependencies remain unproven.

The same native body uses offsetPosition directly: anchor is already represented
in the matrix. It also negates C then subtracts the rounded product. Assuming a
generic affine formula or subtracting anchor again would be incorrect. The
next discriminant is a real native Cocos arithmetic oracle, followed by an
isolated GPU result comparison. No new rendering path is activated here.

V3 Heliopolis two-frame inputs matched21306/21306 supported native quads after
the arithmetic domain was widened for small near-90-degree rotation terms.
R4-33 retains native CPU/GPU comparisons and the negative performance result.
Further captures target Grief32/64 and Society opening0%; all data is private
diagnostic evidence, never a performance comparison. Orbit and Society runs
contain deaths and remain rejected by the performance/visual completion gate.

Limits: actual GL state/uniforms, CPU consumers of sprite data, root dependency
closure, effects, fidelity, black screens and total cost are not proven. Current
normal build, main installation and Documents archive remain unchanged.
