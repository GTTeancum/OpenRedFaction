# Independent metadata source review

2026-10-10 UTC. Disposition: APPROVED, source-only, for this component. Runtime integration requires separate joint review. No build, syntax check or test was performed.

Exact artifacts:
- body-hit-metadata.patch SHA256 ba04e8881bee2efe8bc828af3e06f15dd9237d38c489f51830df4c7a72077205
- include/rf/entity_assets.h SHA256 3013e3566a2fd0b766f490d5e5efb8ddf275202eedadf5fc11305b66b2f2e451
- src/core/entity_assets.c SHA256 67449d4d0e0efa39be7f74a44fe4466e3fbb78c3e4994dbcfe32606b1a3b9c97

The additive rf_entity_class_spheres_resolve extracts the old physical builder's exact resolution loop: actual file order/model_index, packed posed center, existing humanoid X/Z centering, default scalar1, parameter sentinel-1 and unchanged SP override matching. It adds no general finite/positive selected_scalar requirement or radius>0 restriction to previously admitted classes. It publishes all eight initialized rows and count only after success. The existing physical builder projects the same center/radius/parameter/opaque fields for the admitted count and leaves unused caller rows unchanged. Existing argument/error conditions remain represented by the wrapper/resolver combination.

The new8-byte projectile-geometry read/load receipt is independent of primary, supply, reset and save layouts. Full ASCII-insensitive exact name matching is provided by the existing same() helper. It selects the first matching declaration, requires Velocity and Collision Radius once each, rejects malformed/missing/duplicate fields and nonpositive speed/negative radius, and uses the existing finite decimal lexer suitable for the Xbox library. Read errors preserve output; load owns and frees one budgeted weapons.tbl scratch allocation. No constant fallback or explosive-only parser is used.

The resolver has a512-byte local result. When called from the old physical-builder wrapper, the wrapper also holds512 bytes, adding approximately512 bytes transient stack over the prior builder; no permanent memory is added by these core functions. Scene metadata accounting and actual class/weapon qualification remain the runtime component's responsibility. Stronger mandatory scene admission must be limited to the approved guard1/guard2/medic1 and selected ordinary weapon descriptors, not imposed speculatively on unrelated classes or private restore candidates.
