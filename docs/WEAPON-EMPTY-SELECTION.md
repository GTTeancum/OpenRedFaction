# Shared depleted-weapon automatic selection

Integrated and independently source-reviewed on 2026-10-10 after the failed
03:00 UTC build. The scheduled 04:00 Xbox batch is its first compilation
opportunity. Actual exhaustion, replacement presentation and held-input behavior
remain runtime-unverified. No fixtures, grants or save-layout changes were added.

The Fusion successful-final-shot extension is integrated and independently
source-reviewed against `a333c0f4` on 2026-10-10, pending the scheduled Xbox batch.
It shares this ranked consumer but does not admit Fusion DRY operations; see
FUSION-EMPTY-SELECTION.md for the input-only deselection and retained-flight limit.

The Flame primary fuel-debit extension is source-written against `ae64a383` on
2026-10-10, uncompiled/runtime-unverified pending the
scheduled Xbox batch. Only an actual successful continuous-fuel debit qualifies;
see FLAME-EMPTY-SELECTION.md for the original active-empty reset, same-frame
projection, input-only deselection and explicit alternate/dry exclusions.

## Finding: enabled by the original factory defaults

The supplied read-only RF.exe, version 1.20 NA (SHA-256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`),
establishes the previously
unresolved defaults through the **controls subobject**, not direct player-field
stores:

1. Player allocation at `4a3459` obtains `player + 0xe4` in EDX. `4a3471`
   pushes it, and `4a349a` calls `4afe50` with that pointer.
2. `4afe51` receives the pointer in EBX. `4afe6e` zeroes ECX; `4afe70` sets
   AL to one. Neither value changes before the relevant stores.
3. `4afed2` writes AL to `[EBX + 0xe5c]`, exactly player `+0xf40`: one.
4. `4afed8` writes CL to `[EBX + 0xe5d]`, exactly player `+0xf41`: zero.

Thus original factory initialization enables Autoswitch Weapons and disables
the explosive-defer bit. The latter corresponds to enabling the UI's
Autoswitch Explosives checkbox. This is a factory default, not a claim about
an existing user's saved profile.

`452cb0..452cce` toggles the Autoswitch Weapons checkbox and writes its state
to player `+f40` at `452ccf`. `452cf0..452d13` toggles Autoswitch Explosives
and stores the inverse in `+f41`. The profile decoder `4a8bbc..4a8bd0` restores
profile `+78` bits 12 and 13 into these bytes. It can override the factory
values. No saved profile/preferences owner exists in the port; this slice
explicitly uses the now-proven factory pair `automatic_enabled=1`,
`defer_flag=0`. A later actual settings owner must supply the actual values.

The previous grenade document's statement that this default could not be
established is superseded by the above subobject-relative evidence. Its
strict-rank behavior now agrees with this factory initialization. That does
not turn it into support for loading an arbitrary saved preference.

## Original consumer and current gap

- `4a6f41..4a6f57`: require nonzero `+f40`, except Grenade (`872118`).
- `4a6f8a..4a6fab`: require positive descriptor capacity and exclude current
  Remote Charge (`87210c`). Remote Charge Detonator (`85cce0`) has the separate
  live-projectile gate at `4a6f70..4a6f88`.
- `4a6fea..4a7010`: only nonpositive loaded plus mapped reserve is depleted;
  descriptor `+264 & 0x20` blocks automatic replacement.
- `4a7016..4a7074`: Machine Pistol (`85ccd8`) and its Special counterpart
  (`85cd00`) have an explicit counterpart-ammunition/action branch. They are
  not safely equivalent to ordinary ranked replacement.
- `4a70b8..4a70cf`: choose with `4a6e50`, then request selection via `4a4a50`.
- `4a6e84..4a6f01`: scan the 32 preference entries in order; require actual
  base-ID ownership and positive loaded plus mapped reserve for positive
  capacity. `+f41` defers `+268 & 0x100` candidates only when nonzero.
- `426c14` publishes firing presentation, and `426c32` calls `4a6f10` in the
  completed firing path. `4a564a` also reaches that consumer after the
  accepted empty-fire path's feedback/backoff. These are action consumers,
  not an every-idle-frame chooser.

Current shared `rf_weapon_decide_empty` already implements that decision.
The original live gap was that only `scene_grenade_empty_after_release`
called it, with general automatic selection disabled. `scene_weapon_auto_reload_ready`
solves a different case: an empty magazine with positive reserve. It correctly
does not consult Autoswitch. Therefore ordinary zero-total-ammo automatic
replacement is a concrete missing live consumer under original factory policy.

## Small adapter and bounded behavior

`scene_weapon_empty_selection.inc` introduces no persistent owner, allocation,
I/O, save field, option UI or ammo mutation. It reuses the existing independent
32-entry authored rank owner and the existing selection/input release boundary.
Its operation record is now 16 call-local bytes, including the paired consumer's
explicit operation kind. The declaration lives in scene.c before the Flame
and Fusion gameplay includes, with a static note prototype; the consumer remains later:

    typedef struct scene_weapon_empty_request {
        uint32_t source, slot;
        int32_t weapon;
        uint32_t operation; /* SHOT, DRY, or distinct primary FUEL_DEBIT. */
    } scene_weapon_empty_request;

The supported conventional outgoing slots are Pistol, Assault Rifle, Shotgun, Rocket
Launcher, Sniper Rifle, Rail Gun, Heavy Machine Gun, Scoped Assault Rifle and
independently selected Undercover handgun: `0,1,3,4,6,7,14,15,16`.
Fusion slot `12` additionally qualifies only on a successful SHOT operation.
Flamethrower slot `10` with its exact actual weapon ID qualifies only on the
distinct FUEL_DEBIT operation. That operation cannot admit another outgoing slot;
SHOT and DRY cannot admit Flame.

Machine Pistol remains excluded from this conventional consumer. Its separate
paired consumer now handles accepted-dry counterpart actions and both-empty
ranked fallback, preserving the original counterpart-action distinction (see
MACHINE-PISTOL-EMPTY-SELECTION.md). Riot Stick primary is
also excluded; an empty battery must not force a switch away from its usable
uncharged primary. Grenade, Remote controls and shield retain
their existing controller boundaries. Flame's actual positive primary fuel debit
records FUEL_DEBIT before ammo publication and contacts, independently of damage
cadence; alternate, reload discard, dry and idle never record it. Fusion's real launch/debit now records
SHOT before launch audio; idle emptiness and its unbacked-off dry event cannot
request replacement. No sentinel substitutes for a
special gate are relied on by the admitted outgoing subset.

The operation records the full current player handle, selected slot and actual
weapon ID at the real debit or accepted dry-fire boundary. The post-combat
consumer requires all three still match, and the full handle must still resolve
to the actual player object and entity view in both registries, agree with the
damage owner, retain the body, and have neither retired/hidden flags nor
nonfinite/dead health. It also requires on-foot state, no active form,
cutscene, holster/death/reload block and valid current ownership/resources.
No request is inferred from a diagnostic counter, delta in ammunition, idle
emptiness, manual selection or restore. A depleted weapon can still be selected
manually; its next accepted conventional empty firing attempt may request replacement.

Current loaded and mapped reserve must both be exactly zero. Positive reserve
continues through the existing reload owner. Malformed negative counts are
conservatively declined. The authored `+264` no-switch flag remains enforced
by the existing shared decision helper.

### Candidate ranks and resources

Candidate admission uses `scene_unarmed_pickup_slot`, not the grenade-specific
helper that excludes slot 5. Therefore an owned Grenade with real ammunition
can be a conventional replacement, provided its live resource is admitted.
The reused predicate also requires both Remote views and both MP mode views
and transition resources, plus each special weapon's corresponding admission.
Only authored ranks are considered; unavailable entries become `-1` without
compacting/reordering or adding fallback identities. The shared chooser still
checks the actual inventory for ownership and mapped ammunition.

The existing completed MP mode is preserved. As with the grenade adapter, an
MP candidate's ranked base must pass the shared chooser, and its retained
Special mode must independently have actual usable ammunition. Special-only
supply cannot borrow the base rank. Undercover may be the outgoing weapon,
but it has no authored rank and is not inserted as a fallback.

Selection occurs only after the outgoing combat operation finishes. All shot
hearing, launch/impact audio, shotgun pellets, precision rays and piercing
contacts therefore run with the outgoing weapon first. Successful replacement
uses `campaign_select_primary` and `scene_player_auto_select_inputs`, which
retire old loop voices, queued burst/delay and reload work, retain cooldown,
and require held fire/alternate/reload controls to be released. The existing
new-view idle boundary is published. After an actual replacement of Fusion,
only its input state is reset, matching the current deselection policy and
preserving RFAP selection/input consistency; this is not original cooldown
parity. Actual Flame replacement stops ignition/delay and its loop, clears reload
bookkeeping, pending throw and active stream at the completed combat position.
Fractional fuel, cadence, alternate held/cooldown, canisters and burns are retained;
no second controller tick or blanket reset runs. Launched rockets, Fusion shots
and thrown tanks retain independent flight/audio/damage lifetime. No replacement
can fire again within that tick.

This is the established first-pass immediate selection/view boundary, not the
full original queued draw/blend dispatcher. Source-exact presentation transitions,
remaining special-controller depletion, Fusion dry retry/backoff, Flame dry/alternate,
passenger messages and real saved option
loading remain outside this bounded slice. Paired current MP depletion is handled
by the separate consumer documented in MACHINE-PISTOL-EMPTY-SELECTION.md.
Original `4a4ac1..4a4b32` can remap a chosen Remote Charge to its detonator
when qualifying owned charges are live. As in the existing grenade adapter,
this bounded direct selection retains the authored charge-slot policy rather
than claiming to reproduce that full selection dispatcher. The Remote
controller's own charge/detonator follow-ups remain independent.

## Parent integration contract

1. Place the shared operation enum/request typedef and exact static note
   prototype before `scene_flame_gameplay.inc` (and therefore Fusion), without duplicate types.
   Include the adapter after `scene_weapon_auto_reload.inc`, before
   `campaign_combat_tick`. Its helpers depend on the earlier unarmed pickup
   admission and the existing grenade rank/input owner.
2. Add `scene_weapon_empty_request *empty_request` to `campaign_combat_tick`.
   At entry require non-null (return `RF_RANGE` otherwise) and set
   `empty_request->weapon=-1` and `operation=0`, before any early return or duplicate-frame guard.
3. At the existing successful `rf_weapon_consume_shot` branch, after its status
   is checked and before `campaign_ammo_publish`, call
   `scene_weapon_empty_note(empty_request,SCENE_WEAPON_EMPTY_SHOT)`. The note
   captures MP for its separate consumer and ignores Riot/unsupported controllers. It records any accepted shot;
   the consumer cheaply declines while any ammunition remains. Pass the same
   call-local record into Fusion gameplay; note SHOT only after event.shot
   proves real launch/debit, before optional launch audio. Do not note Fusion DRY.
   Pass the request through Flame input into its admitted primary gameplay tick;
   note FUEL_DEBIT only after successful `rf_weapon_charge_step` reports consumed>0,
   before ammo publication/damage. Require the actual Flame weapon ID. Inhibited
   ticks and existing direct helper checks can pass NULL to opt out of capture.
4. In the existing no-reserve empty-attempt branch, call the same note helper
   with `SCENE_WEAPON_EMPTY_DRY` only when `fire==2`, alongside the existing Launch Fail playback. Do not
   trigger it merely from a held alternate, zoom request or `fire==0`.
5. At the single outer combat callsite, create the frame-local record
   `scene_weapon_empty_request empty_request={0,UINT32_MAX,-1,0};` and pass its
   address to `campaign_combat_tick`. Do not retain it between frames.
6. Only after combat returns `RF_OK`, call
   `scene_weapon_empty_after_combat(stream,frame,position,&empty_request,&changed)` and
   propagate its status through the existing combat/profile error boundary.
   Place it before subsequent consumers/view advancement. A changed result
   does not return early from the outer world update; shared world/frame work
   still needs to finish. The combat function itself has already returned,
   so no special/conventional input is executed again. Pass the same actual
   staged-or-normal combat position for Flame's ordinary release Foley. Only
   after a real Flame replacement apply its narrow off-selection cleanup.
7. After this consumer succeeds without changing selection, invoke the separate
   MP consumer with the same operation record and actual combat position. Its
   paired action is admitted only for DRY; both-empty ranked fallback admits
   SHOT or DRY. See MACHINE-PISTOL-EMPTY-SELECTION.md for exact wiring.
8. Update the grenade defaults explanation and the top-level task milestone
   with the new source evidence and honest uncompiled/runtime-unverified state.

Capture occurs **inside** the actual operation, not before combat starts:
manual cycling and real pickup selection happen within that tick and can change
which weapon fires. Recheck the captured identity after all operation callbacks.
The adapter includes actual object/entity registry and damage-owner qualification;
never replace it with a before/after diagnostic counter comparison.

No new build-list entry, lifecycle hook, save-format change or gameplay fixture
is needed. The parent scheduled stock-64-MiB Xbox batch remains the first
compilation opportunity; natural exhaustion and held-input behavior remain
runtime-unverified until actually observed.
