# R71 — ordered GPU translation and native root rotation

Owned arithmetic component, not linked into a game renderer. R69 proves that a
move-only resident pose cannot assume CPU-reader independence. This experiment
extends the shared per-class command architecture to mixed ordered translations
and the native root-position part of rotation actions. Sprite orientation,
oriented boxes, section callbacks and gameplay remain native/unqualified.

48-byte resident states include native last-position/stamp/dirty/finish bits and
the fixed position offsets. 80-byte shared commands contain translation or an
affine matrix, center, claimed movement, stamp and finish flag. Ordered per-actor
programs retain duplicates, locked-X translation and empty programs. A compute
dispatch applies the program; state ping-pong allows recurrence without position
uploads. No host-pointer ownership or live callback suppression is installed.

Independent reference: `prepare_oracle.py` verifies the actual GD image and a
327-byte arithmetic fragment from processRotationActions. The build places that
fragment in an owned ABI-correct assembly fixture, relocating only its two calls
to the original Cocos point constructor and pinned affine helper. Raw GD code
stays in ignored local build output. The existing exact native movement oracle
provides translation. This is not execution of a complete game rotation phase.

271,128 complete native/GPU states match byte-for-byte on the Intel UHD, across
four DAZ/FTZ modes, 65 prefixes of a 64-command mixed table, 17 program classes,
duplicates, empty programs, decoration/stamp/lock/finish variants, large positions,
and four resident recurrences. The strengthened programs cover all 64 commands.
A native cancellation witness rejects replacing the original double subtract/add
sequence by a simpler assignment. Cocos affine arithmetic uses double products
and ordered sums, rather than the float expression one might expect.

These are synthetic arithmetic fixtures. `fixtureInput` only bounds structure and
finite inputs; it does not prove recurrence, subnormal, parent, lifetime, FP-status
or global native-reader safety. There are no level screenshots, FPS results or
normal promotion claims. GPU readbacks are test assertions only. Next evidence:
actual native rotation parameters and immediate post-arithmetic position witnesses.
Keep isolated until full geometry, readers, fallback and game frametimes qualify.
