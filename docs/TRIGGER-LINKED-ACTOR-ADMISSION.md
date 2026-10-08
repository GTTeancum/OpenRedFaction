# Authored linked-actor trigger admission

Written2026-10-08 after the14:00 build. Source reconstruction only; no tests,
fixtures, emulator or campaign traversal. Parent15:00 owns the next build.

## Concrete mission gap

The SP trigger inventory contains29 flag0x80 records across10 levels. The
scene's contact loop discarded every one before invoking its already-written
shared eligibility service. This made ordinary contact unable to start those
chains, even though directly firing their events could work. Examples include
L1S2 door trigger8635 and L12S1 trigger9697, whose links start Attack9698,
Teleport9711 and Set_AI_Mode9710. Later records include airlock interactions.

The blanket rejection is removed. All other source gates remain in the existing
shared contact path: object lifetime, actor filter, input, contact geometry,
contact delay, activation limit, disabled state, airlock and normal link dispatch.
No trigger is forced active and no actor is relabeled or replaced by its host.

## Original source and existing owner

RF.exe SHA-256:
b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.

At4c0843, trigger flag0x80 requires the contacting object to resolve through
426fc0. It calls4290d0 and rejects false. That predicate resolves the actor's
linked handle at+200 through426fc0 and requires486c90 of the linked entity to
be1. This is a real typed relationship, not a flag saying every vehicle may
activate the trigger. Absent/stale/nonentity links and ordinary on-foot actors
do not pass it.

The port's `rf_trigger_actor_resolve` already produces exactly these facts from
generation-qualified entity views: entity existence, linked-handle resolution
and linked class_type1. `rf_trigger_eligible` already applies them for0x80.
The actual possession/seat owners publish the player/NPC backlink and real
class use kind, so no new speculative trigger facts are needed. The repair
simply allows those existing owners to reach that existing gate. It adds no
saved state, allocation, vehicle behavior or gameplay-fixture shortcut.

Raw trigger fields[1]/fields[2] and projectile-only flag2 are separate concerns;
this change neither reinterprets nor bypasses them. Their admission is tracked
separately from this bounded linked-actor repair.
