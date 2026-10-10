# Armed-player weapon pickup auto-selection

Status: integrated and independently source-reviewed, uncompiled/runtime-unverified
on 2026-10-10. Compilation is scheduled for the 04:00 stock-64-MiB Xbox batch.
No extra build, syntax check, test, fixture, emulator, PC work, route or capture
was performed for this slice.

## Proven policy and missing consumer

The supplied read-only RF.exe 1.20 NA (SHA-256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`)
contains a separate armed-player tail after the already integrated unarmed
selection branch. Before this slice, successful placed/extra/corpse weapon
collections only called the unarmed consumer, so an armed player never reached
this policy.

- `45a793` initializes the call-local prior-owned byte to one. `403250`
  reports ownership; only the new-acquisition branch writes zero to the local
  at `45a7ac`, then grants at `45a7b3`. The already-owned refill path does not
  clear it. New ownership and accepted refill both reach `45a9be`, but the
  later armed branch distinguishes them.
- `45aa27..45aa3f` resolves the player and requires the actual local player.
  `45aa41..45aa45` requires an existing current primary.
- `45aa47..45aa4e` requires player `+f40` to equal one. `45aa50..45aa56`
  rejects a nonzero prior-owned byte. Thus the port must require the actual
  successful `grant.acquired == 1`; `grant.rounds` alone must not select.
- `45aa58..45aa6d` calls `4c9a70`, which reads weapon descriptor `+268 &
  0x100`, and rejects that candidate only if player `+f41` is nonzero.
- `45aa6f..45aa86` looks up the acquired weapon and the actual inventory
  current primary separately. `4a23c0..4a23e0` scans the 32 authored preference
  entries in order, returning the first index or signed `-1` when absent.
  `45aa89..45aa8b` requires acquired rank strictly less than current rank.
  It does not sort a candidate list, choose the best owned weapon, normalize
  paired modes or replace missing ranks with 32.
- `45aa8d..45aa98` requests the accepted weapon through `4a4a50` with
  `(defer=0, force=0)`. No general positive-ammunition test occurs in the
  pickup tail. Ordinary newly acquired weapons with zero granted rounds can
  still qualify. Retained MP Special is not changed to obtain ammunition.
- Ammo-only dispatch at `45a479 -> 45a500` does not enter the weapon tail.
  The gives-weapon guard is mandatory even if an ammo class names an owned
  weapon. Existing corpse/scripted-NPC drops enter this same real item path
  (`42aed0 -> 459a90`, `42b226 -> 459100`,
  `4597b0 -> 45a3d0 -> 45a420 -> 45a6d0`).

Factory policy is independently established by the controls-subobject stores
at `4afed2` and `4afed8`: player `+f40=1`, `+f41=0`. See
WEAPON-EMPTY-SELECTION.md for the complete pointer/default/profile chain.
This adapter explicitly uses that pair; it does not claim to load an existing
user's profile or implement a settings owner.

### Signed missing-rank behavior and identity

The authored map is reused without compaction, extra entries or fallback IDs.
A real unranked acquired primary has rank `-1`, just as in the original
helper; an unranked current primary also has rank `-1`. The signed comparison
is retained rather than adding a both-ranked policy.

The current completed MP Special ID, Undercover ID and Remote Detonator ID
have no authored ranks. A ranked new candidate therefore does not outrank any
of those current identities. In particular, `campaign_slot_weapon(9)` and
`campaign_selected_weapon()` intentionally alias the port's Detonator to the
Charge inventory ID. The pickup request instead captures and compares
`campaign_detonator_id` when slot 9 is active. MP Special uses the actual
Special ID already returned by `campaign_selected_weapon()`. Neither borrows
its ranked base's position.

Candidate slot admission still accepts only actual owned selectable primary
identities. Special and Detonator do not become independent pickup primaries;
Remote ownership remains the Charge ID, and MP ownership remains its base.
Undercover is not inserted into the map and no new pickup class is invented.

## Bounded selection adapter

`src/diagnostic/scene_armed_pickup_selection.inc` introduces no persistent
state, allocation, inventory change, option owner or save-layout change.
Its only operation record is 12 call-local bytes:

```c
typedef struct scene_armed_pickup_request {
    uint32_t source, slot;
    int32_t weapon;
} scene_armed_pickup_request;
```

The caller captures the full player handle, selected slot and actual outgoing
weapon immediately before the real grant. After successful grant, retirement
and feedback, the consumer requires all three still match. It also verifies
that the handle resolves to the actual player object/entity in both registries,
agrees with the damage owner and retained body, and has no retired/hidden or
nonfinite/dead health state. Mounted control, active player forms, cutscenes,
death/holster blocks and pending MP/Undercover transitions are conservatively
left alone. No global event, diagnostic counter, inventory delta, restored
state or idle ammunition condition is treated as a pickup.

The actual newly acquired weapon ID comes from the successful grant branch.
Placed extra pickups use their definition's catalog name, never an item-class
index or guessed view slot. Corpse/drop collection retains the exact grant ID
in a local before feedback. Both paths continue to require the actual
`gives_weapon == 1` and `grant.acquired == 1` values.

`scene_unarmed_pickup_slot` supplies the existing ownership/resource admission
for current base ownership and the candidate: loaded first-person views,
Grenade/Rocket/Flame/Fusion/shield resources, both Remote views, and both MP
views/custom owners with completed retained mode. It does not load missing
resources or require general positive ammunition. Selection failures return
zero without rolling back an accepted grant or preventing retirement, notices
or audio.

### Two existing downstream special cases

The direct selection boundary preserves these bounded `4a4a50` branches:

- `4a4c34..4a4c90`: a Grenade candidate with nonpositive mapped reserve is
  declined. The accepted pickup remains owned/retired normally. No global
  positive-ammunition filter is added for ordinary candidates.
- `4a4abb..4a4b34`: a Charge request becomes Detonator if Charge mapped reserve
  is nonpositive or a qualifying own charge remains. The adapter reuses the
  already source-reviewed `scene_remote_selection_has_charge()` predicate:
  retained active charge, full matching owner handle and weapon type, positive
  fuse, and no retired object flag 2. No diagnostic live count, attachment
  requirement or replacement pool owner is introduced. Both loaded Remote
  views and the actual Detonator identity must be admitted before slot 9 can
  be selected. Malformed negative reserve is conservatively declined.

The ordinary original draw/blend/queued selection dispatcher is still outside
this first-pass adapter. Integrated hooks cover world pickups/drops and the
separately verified default Give_Item join; full-strip correction is described
below. Active forms and saved preference/UI ownership remain separate.
None of these limitations prevents the new source-backed normal
first-acquisition branch from using the existing selection boundary.

### Input, audio and timing boundary

On selection, any outgoing delayed Riot primary is cancelled, then
`campaign_select_primary` closes selection-bound loop voices and publishes the
new definition. `scene_player_auto_select_inputs` clears old burst/delay,
reload and alternate work, preserves cooldown, and latches held fire,
alternate and reload until release. The view cache starts at the existing
idle selection boundary and ammo/counters are republished.

Actual pickup consumers run before manual cycling, MP/Undercover controls,
scope input, special controllers and conventional fire in the same combat
frame. Therefore the immediate input mask prevents firing/toggling/reloading
the new weapon in the pickup frame; the retained release mask extends that
protection until physical release. Manual cycling keeps its existing behavior.
An ongoing conventional reload may be cancelled by a qualifying new pickup;
the adapter does not add a reload-denial policy. Independent launched
projectiles, Remote charge lifetime and the shared world tick are untouched.

## Integration and evidence limits

The request type and forward declarations precede all grant consumers. The
implementation is included after the existing rank/input owner. Each real
world, corpse or scripted grant captures its own pre-grant identity and calls
unarmed selection first, then armed selection only when the first declined.
Accepted refills still cannot switch an armed player. No build-list or save
ABI changes are required. Compilation and any naturally available action
observation belong to the scheduled hourly Xbox batch; no runtime verification
is claimed here.

## Full strip and authored Give_Item integration

Original type56 ON reaches42cd20;42cd29..34 clears entity+810 bit0x800
and42cd53 publishes primary -1 after removing inventory. The port now clears
both player holster mirrors and publishes explicit unarmed while retaining
bounded slot storage. Type56 OFF and standalone item removal are unchanged.

Default Give_Item4bb769 reaches45a3d0; its weapon branch45a420 calls45a6d0,
including negative-primary selection45a9db..45a9fe and the armed selection tail.
The actual accepted scripted grant now captures pre-grant identity and invokes
unarmed then armed selection after its notice and before final ammo publication.
Ammo-only, custom suit/vital and notice-only callbacks retain their existing paths.
The existing scene_actor_collision_owner lifetime spans queued startup grants;
no synthetic event or runtime grant is introduced. Frame-zero input-latch reset and per-frame raw-control latch sampling
now precede queued grant application so a newly selected weapon retains its
held-action release latch. No save layout, inventory amount or original input changes.

All of this remains source-reviewed only until the scheduled 04:00 build.
Startup/resource admission cannot establish pickup, strip or Give_Item action coverage.

Independent final source review confirmed input order: animation begin_frame
polls before prepare_view/combat, while scene_frame authored-event dispatch
follows combat. Scripted selection after events therefore retains its latch
into the next fresh input poll. The moved pre-inventory tick is guarded by
combat_frame!=frame, preserving the existing duplicate-frame boundary.
