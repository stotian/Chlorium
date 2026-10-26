# CHLORIUM-CRITICAL-INSPECTOR

One bounded read-only independent pass: **REWORK**, no execution or native-byte
inspection by the agent. One targeted follow-up: **ACCEPT for the two fixes**.
The full experiment was not independently re-audited after those corrections.

- P1: a replaced layer callback entry could be accepted while the candidate
  called its supported spy directly. The candidate now checks all supported
  layer section/disabled and object scale/rotation entries before mutation.
  Four distinct replacement tests assert unchanged objects, layer, center,
  tracking, helper fields, errno, FP state and callback counts.
- P2: different foreign tracking pointers both normalized to -2. Every slot
  must now be null or owned; callback snapshots and final comparisons require
  validity, not only equal normalized integers. A foreign-slot rejection test
  verifies no publication or state mutation.
- Missing coverage requested by the inspector was added: boundary/subnormal
  values, resetVisual=false and preexisting sticky flags. All three mutants
  have completed and fail intentionally.

Post-fix evidence: 327427 native checks. The targeted reviewer saw the preceding 322043-check version; the final change expands only boundary flag inputs. Zero-delta mutation fails at check 82,
missing FP flags at 2, missing callback at 410. Final full-cost dense pair is
15675.3 us native / 19767.2 us candidate. No production/FPS conclusion follows
from the oracle spies. The candidate remains rejected for promotion.
