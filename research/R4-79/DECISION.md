# R79 — frame-local trace slicing

**Status: isolated prototype; not an FPS claim.** The R78 live compiler used
two full-history passes for each prepared frame, even though trace rows are
appended in nondecreasing frame order by R72. R79 uses binary bounds to pass
only the requested frame's contiguous rows to both compiler passes and to seed
the initial roster. This changes lookup work from O(all captured rows) per pass
to O(log all rows + rows in this frame).

The ordering premise is currently established by R72's two append sites:
each row receives the current monotonically increasing trace frame immediately
before `push_back`; the history is never reordered or cleared during capture.
R79 remains on a research branch and affects only the already isolated live-root
profile. A 50,002-row ordered history fixture verifies frame lookup and exact
compile output against the small reference trace.

**Evidence:** frame/source-contract/dirty-state checks pass, including a
50,002-row ordered history fixture that compares the sliced compile result with
the two-row reference. Both experimental Windows DLL profiles compile and link.
A targeted live Grief auto smoke (88952972, 25–26%) then passed with this code:
32 trace rows, two prepared frames, 12 submissions, 30 quads, 381,984 verified
bytes, zero failures/declines. The isolated test restored shared settings and
removed staged packages. This short segment does not measure the slicing benefit
at scale or prove full-level safety. No frame-time or FPS improvement is claimed.
If trace capture ever reorders or backfills rows, replace the bounds lookup with
a maintained frame index or reject that trace.
