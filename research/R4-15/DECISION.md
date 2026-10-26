# R4-15 — Native draw contract and GPU authority

**V7 remains isolated; not promoted.**

The actual supported Cocos draw body (RVA 0xa6350, FNV64 tail
c9d0f08451f64908) is the oracle, not a reconstructed glDrawElements call.
It leaves ARRAY/ELEMENT bindings zero and texture unit zero active. V5/V6
restored pre-draw bindings, a reproducible difference even when the current
pixel is identical. A native draw sentinel detects this contract defect.
V7 forwards the actual draw, temporarily leasing a fully uploaded owned VBO;
partial fallback hydrates the original native store. These mechanisms pass
postcondition and subrange fixtures, including exception unwinding.

A further negative witness prevents promotion: a clean native atlas may hold
GPU-only data differing from CPU staging. V7 overwrites it from CPU and makes
the original store dirty on release. This changes authority. The oracle
reproduces the difference; it is not proved to cause the observed GD blacks.
241 GPU checks passed at this stage, including explicit negative witnesses.

Grief 0–40%, full user mods/detail, no shake/noclip: both targets reached,
zero deaths, 3247/3426 visual samples, 90 shared files restored each pass.
Two unmatched full-dark episodes remain near 12.53% and 12.78%. A separate
native run contains the same 12.53% episode; undersampling prevents attribution.
This is neither a universal visual pass nor a measured FPS improvement.

Next protocol: canonical GPU replacement, retaining GPU-only bytes outside the
actual dirty CPU interval and using the original native draw (R4-16).
