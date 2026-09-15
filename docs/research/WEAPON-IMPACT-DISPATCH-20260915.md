# Original weapon impact dispatch (2026-09-15)

**Configured impact clips are simultaneous ordered components, not randomly selected variants.** Original4c8a10 loops over every configured handle and invokes4c16e0 for each.

## Executable evidence

`tools/verify_weapon_impact_dispatch.py` executes complete original4c8a10 against synthetic weapon records, replacing only child effect4c16e0 with an argument recorder returning-1. Twenty cases use weapon indices0/7, counts-1/0/1/2/3 and radius arrays(0,1.5,3)/(-1,.125,0). Every case passes. SHA256 and raw calls are in `impact-dispatch.json`.

- Weapon record stride1360. Count is85d158+stride*index; handles start85d15c; corresponding radius floats start85d180 (handle-relative+24hex).
- Every positive-count slot calls, in increasing index order, even if a handle is-1 or the previous child returned-1. Counts<=0 call nothing.
- Eight child arguments are handle, entity_base, optional position, hit point, stored radius, parent handle, hit normal,1. The last1 enables the downstream effect's audio path.
- Zero and negative *stored* radii pass unchanged; there is no wrapper clamp, replacement radius, filtering or RNG. This does not imply negative radii should be accepted from malformed input.

Recommend treating an authored list as all components to dispatch, preserving order and independent child failure behavior. Do not choose one random impact name. Radius fallback at runtime should not silently replace a stored zero based on this wrapper.

## Boundaries

No parser or weapon initialization was executed, so absent `$Impact Vclips Radius` defaults remain unproven here. Original4c16e0 is not executed by this harness, and may have its own randomness or child-type radius behavior. Raw retained decompilation shows its code-explosion branch forwards param5 to48e640, but that path was only inspected, not counted as executable proof. Full valid-effect visuals, sound ordering, room lookup and damage are outside scope.

Read-only community weapon.h at local/alpine-reference supplied the4c8a10 address/signature and capacity3; implementation evidence above comes from original executable instructions. No third-party code was copied into the port.
