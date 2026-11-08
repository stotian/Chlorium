# R4-48 — Native quad producers and CPU consumer inventory

No native work is suppressed. The inventory scripts read owned PE bytes only,
under below-normal process priority; they do not load or execute target images.
Private outputs: build-research/native-reader-inventory-r48-r1. No raw game
instructions, saves or credentials are exported into the repository.

The displacement scan covers220334 Cocos and1121372 GD instructions, with29/186
incompletely decoded ranges. Offsets are potential candidates, not type proof.
The local alias pass selects55 Cocos exports;36 remain unseeded. It distinguishes
address formation from actual reads and includes potential stack arguments.
An additional first-base binding pass selects443 GD void methods. Its two
this-relative candidates are NOT ABI/runtime-owner proofs. In particular, the
GJRobotSprite result cannot authorize a renderer or field-layout assumption.
Zero six-vector copies found in the scanned GD procedures does NOT prove there
are no inline getters, aliases, non-void or uncovered consumers.

## Concrete boundaries found by static inspection

- Cocos getQuad0x5dda0 copies all96 bytes; the SDK also defines it inline.
- CCSprite::draw0x8b960 passes positions/UV/color pointers to GL, including its
  sixth stack argument. Game CCSpriteWithHue::draw0x48c90 is another candidate.
- Batch appendChild0x8df50,insertChild0x8e7b0,insertQuadFromSprite0x8ea50 copy
  complete quads. Copy construction0xc93b0 and assignment0xcf510 do too.
- updateColor0x8d450 writes colors and tail-calls updateQuad0xa6fa0 with the full
  quad. A classified style event must keep the newer retained pose, not import
  potentially older CPU positions or reinterpret current visibility flags.
- setTextureRect0x8ce10 also writes positions and cannot be classified as style
  alone. Unclassified writers/readers must retain native ownership.

The complete95-byte updateQuad body updates total=max(total,index+1), performs
six forward16-byte load/store pairs, then sets dirty=1. NativeAtlasWrite is an
owned reference model preserving those metadata and overlap semantics, not an
installed replacement. Replacing it with a generic memcpy/memmove96 or ignoring
total/dirty would not establish the same behavior.

## Qualification, 2026-11-06

The owned native function comparison passes480 valid cases, including overlap,
unaligned sources, old total values and raw dirty bytes; invalid model inputs
reject before mutation. It loads only the owned Cocos DLL under SystemInputGuard,
never a GD process or Geode. This is not arbitrary callback/ABI ownership proof.
Static inventory controls pass for branch joins, volatile-register calls, GL's
stack argument, address-only LEA, copy recognition and binding selection.

R4-47 now separates classified style publication from pose updates, checks
producer rounding before admitting reconstructed geometry, and prepares owned
CPU quad snapshots from retained records while preserving reader MXCSR. These
snapshots perform no native writes or callback replacement. Their revision and
lifetime must still be current before a future native adapter uses them.

Decision: use these boundaries to design the producer/consumer adapter. Do not
claim complete CPU-reader closure, remove native quad preparation, or change
groups/Area/gameplay based on a static candidate inventory. All such closure
flags remain false. Unknown hooks, pointer escapes, inherited layouts, FP-status
observers and lifecycle transitions remain prerequisites for native deferral.
