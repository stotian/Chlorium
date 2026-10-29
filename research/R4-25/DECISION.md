# R4-25 - Direct identity CCNode specialization

NO-GO for runtime integration: mixed workloads regress. Keep only the isolated
prototype and evidence so the rejected assumption is not repeated.

The prototype directly materializes the native local matrix for dirty nodes
with zero rotations/skew/anchor, unit scales, no additional transform and no
ignore-anchor-position flag. Integer SIMD guards and copies preserve signed
zeros, NaN payloads and FP state. Unsupported or overlapping outputs call the
original helper. This has no cache, shared token, rendering or gameplay hook.

189571 fixture assertions pass against the complete fingerprinted installed
Cocos routine (RVA 0x37f0, 0x468 bytes, FNV 7abc292bc9db4855): entire node bytes,
output bits, return ABI, full MXCSR and errno. Includes 16 rounding/DAZ/FTZ
combinations, initially sticky flags, random/special positions, signed-zero
fields, unsupported states, overlapping outputs and adjacent output. A mutant
omitting the linear guard is rejected at assertion 11817. No GD is launched.

Earlier alternating measurements include dispatch/guard/fallback cost, 32
phases, nine paired batches. All-identity 4096-node median: 2633.3 to 1795.9 us
(31.8% reduction). Mixed 25% identity: 2843.8 to 3173.7 us (11.6% worse); 75%:
2667.0 to 2779.3 us (4.2% worse). These are fixture kernel times, not GD FPS.
Raw pairs and summary are retained. Alias checks were added after timing;
the active SIMD algorithm is unchanged. Noise is large; no repeated favorable
measurement is substituted for the original one.

The old Orbit pose census measured concat recipe reuse, not dirty identity-node
eligibility: 1084638 concat calls, 44344 sampled, 1040294 skipped by budget over
14 frames. It cannot justify assuming an all-identity stream. Production helper
vtable ownership, hook compatibility and actual hit distribution remain unproven.
No new production package or in-game performance claim comes from this slice.
