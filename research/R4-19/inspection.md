# Independent inspector - one bounded read-only pass

VERDICT: ACCEPT for the stated isolated fixture and tested domain.

No P0/P1 demonstrated. The inspector checked source independence of the native
oracle, Win64 argument layout, bounded growth path and the R4-18 counterexample.
It did not execute the tests or inspect proprietary bytes.

P2 findings and subsequent targeted work:

- Callback records originally omitted other objects' non-position fields and
  tracking entries. Records now capture every object, normalized layer state,
  tracking identity/order and pointer validity. A temporary slot corruption
  restored before final comparison is rejected specifically by event equality.
- Static jumps originally needed only an in-range target. Targets must now be
  members of the decoded instruction-start set.
- The requested relocation inventory now classifies every direct call,
  virtual callback and RIP operand in all three executed bodies. The full
  transform inventory retains unresolved calls as barriers.
- A decreasing sequence including INT_MAX -> INT_MIN was added under all 16
  SSE controls.

After these changes: 376652 checks pass and three mutants fail as intended.
No second agent pass was run; the fixes received targeted native checks.
The verdict does not authorize GD integration, skipped native calls or an FPS
claim. Real section callbacks and the broader Area reader set remain open.
