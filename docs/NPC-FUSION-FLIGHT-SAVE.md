# NPC Fusion paid-flight persistence

Source-written against parent baseline `df67cf8a`; staged externally during the
October 12 18:00 parent build. Compilation, native save/load and resumed impact
behavior are unverified. No tests, new gameplay fixtures, builds, emulator runs,
campaign routes, images, HDD operations or real-repository edits were performed.

## Bounded working behavior

The ordinary composed RFWC3 save can now retain an accepted NPC shoulder-cannon
shell alongside player Fusion shells. The existing four shared slots, remaining
double-precision lifetime, position, velocity, render basis, liquid-query state,
accepted raw damage and source role survive. No ammunition is charged, attack
replayed, timer restarted or presentation emitted by restore. NPC inventory,
ordinary fire cadence, reload and forced-shot ownership remain in RFNC; all of
its existing admission gates still apply.

A paid flight does not require its source to keep holding the shoulder cannon
or remain alive. A still-registered, RFNC-admitted corpse is supported. An
already removed/retired source is deliberately unsupported: this flight owner
has no historical-source receipt, so save refuses rather than attributing it to
the player, to another generation or to a newly constructed actor.

## Source basis and explicit port policy

`docs/NPC-SHOULDER-CANNON-FIRST-PASS.md` records the installed weapon metadata and
original binary evidence already used by the working attack consumer:

- RF.exe `004c8bd7..004c8be9` multiplies nonplayer SP primary damage by the
  descriptor's AI scale. The loaded shoulder-cannon definition supplies
  `1000 * 0.16 = 160` raw damage.
- `004c5cfc..004c5d38` obtains direct damage through `004c8b10`; `004c6132`
  dispatches it through `004892c0`.
- `004c62b5..004c62f5` obtains and dispatches the independent radial request
  after direct contact.
- Installed flight metadata is speed 20, lifetime 15 seconds, collision radius
  0.051, blast radius 25, crater radius 7 and one impact effect of radius 9.

The existing port stores the accepted damage/definition in each paid shell and
uses straight, constant-speed flight. This patch serializes that implemented
owner; it does not claim the original game's save format or reconstruct late
source/descriptor mutation. Existing player difficulty, contact, Nano,
blast/terrain and terminal callback paths are unchanged.

## RFAP6 wire contract

The component header stays 16 bytes, each row stays 112 bytes and the world
format remains RFWC3. Version 6 is written only when an NPC Fusion flight exists.
Kinds 1–11 retain their meanings and layouts. Kind 6 remains exclusively player
Fusion; kind 7 remains exclusively the player input controller.

Kind 12 contains:

- `+0`: 12; `+4`: shared Fusion pool slot 0–3; `+8`: positive authored source UID
- `+12`: reserved zero
- `+16/+28`: position and velocity, three float32 values each
- `+40`: collision radius; `+44`: liquid-query flags, exactly 0x1004 or 4
- `+48`: remaining float64 lifetime; `+56`: accepted raw NPC damage
- `+60/+64/+68`: reserved zero, never player cooldown/reload ownership
- `+72`: nine float32 render-basis values; `+108`: reserved zero

All live NPC shells are checked against the independently admitted loaded
Fusion definition. Capture refuses a mismatched definition, invalid NPC role,
wrong radius/raw damage, impossible lifetime/query flags, nonfinite/out-of-range
position or invalid straight-flight basis/velocity. The reader applies the same
shape admission after reconstructing the definition. Actual accepted float bits
are preserved; basis checks allow only normalization roundoff.

Kinds 6 and 12 share one slot-uniqueness mask, so mixed rows cannot overwrite
each other. A separate player-flight mask owns kind 7 exclusivity. Consequently,
NPC-only flights neither claim nor suppress the player controller: an independent
pending player cooldown/reload adds kind 7, in either row order.

## Ownership, capture and publication

Capture proves a full source handle against the NPC registration wrapper,
entity view, damage owner and allocated body. It also proves the authored
persistence UID, level and nonretired key. It never infers source identity from
a slot, current selected weapon or the local-player role.

Preparation resolves the saved UID to the current full registered handle but
that alone cannot authorize publication. Before the existing final world
validation/assignment chain, every kind-12 row joins the complete candidate RFNC
stage: exactly one matching UID, registered source, nonretired saved state,
matching full handle and authored slot identity are required. Missing or retired
candidate sources fail even if a pristine constructor source exists. The
existing subsequent NPC validation seals the current registration/lifetime
before assignment.

Only ordinary composed export receives the explicit projectile-profile
capability. Standalone NPC export and legacy composed checkpoints retain their
live-NPC-Fusion refusal. No save-only gate is inserted into shared RFNC row
capture, projectile preparation or world restoration.

A stack-owned service-depth receipt covers NPC launch acceptance/presentation
and the shared Fusion flight tick. Saves refuse while it is active, including
the callback window after the last shot has retired but before all direct,
radial and terrain work completes. World reset does not erase an executing
stack's receipt. Existing valid restore is still allowed and outgoing-flight
world-epoch handling remains in place.

The existing assignment-only projectile publication copies the staged pool,
including the NPC role/source, after NPC state assignment. No new external
callback or fallible work is inserted into that publication. The existing
reload-specific storage-close barrier remains unchanged; this patch does not
claim general whole-world failure rollback or stronger transactionality.

## Resource, compatibility and footprint boundaries

Saved NPC Fusion rows request only shared Fusion metadata, shell mesh and impact
resources through a separate NPC resource mask. They do not demand a first-person
view or grant the player a weapon. Independently present player rows/inventory
retain their existing player resource demand.

Both full RFAP readers and the early continuous-charge preflight accept RFAP6.
Version 6 preserves RFAP5's reserved-zero fractional-debt rule and all existing
Flame/charge equality joins. RFAP1–5 remain readable with their previous
semantics and refusal policies; no old bytes are repurposed. Existing exact-
version diagnostic harnesses are unchanged and were not run.

The four-slot pool and live flight structure do not grow. Four transient source
UIDs add 16 bytes to the already sizeof-budgeted projectile stage; one service
counter adds 4 static bytes. The maximum component row bound remains 154 and
112-byte row/prefix/storage limits do not change. A previously refused NPC-only
save uses the existing 16-byte component header plus 112 bytes per shell and
RFWC3's existing prefix. Actual compiler stack/layout and stock64 runtime remain
for the sole parent batch.

Deferred: already-retired historical sources; independently unsupported NPC
reload, movement, scripts or attacks; original launch/flight fidelity; tester
coverage of mixed roles, source death/switching and actual saved continuation.

Suggested milestone: "NPC shoulder-cannon paid flights are source-written for
ordinary RFAP6 save/load with authored-source/candidate joins and mixed-player
pool continuity; compilation and saved continuation remain unverified
(docs/NPC-FUSION-FLIGHT-SAVE.md)."
