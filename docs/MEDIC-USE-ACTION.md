# Medic Use: bounded action and input ownership

Source-written implementation staged from clean `2122f65db243647baa745fb998d28606e42ff0ec`.
No build, syntax check, test, runtime session, campaign traversal, new fixture or
active-repository edit was performed. This slice requires the coordinated real
syringe owner and finite RFNC/RFCH codec; it is not independently installable.

## Original evidence

Read-only RF.exe SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
The source research is recorded in sibling `known-gameplay-next` files
`MEDIC-USE-SOURCE-CONTRACT.md`, `MEDIC-DISPATCH-SPEECH-ADDENDUM.md`,
`MEDIC-SUPERSESSION-AIM-ADDENDUM.md`, and their bounded disassembly. Addresses below identify that original executable.

- `43d0a9`, `43d4f0`, `4a1660`, `4897d0`: Use edge, one actor selection, then
  handler. A refusing medic consumes the press. `4a29f0` returns at `4a2a4f`
  after consumed `4a1660`, bypassing ordinary Use-trigger fallback `4a1970`.
- `4243c0`, `503230`, `4facb0`, `4fa7a0`, `4fa930`, `40a0b0`: consecutive
  interface tags, posed local point transformed by actor basis, strict
  approximate distance < class radius, normalized positive-facing dot, strict
  distance/dot ranking. Kinds1/4 alone require world LOS. Kind9 uses origin
  distance without facing. No medic friendliness or extra LOS gate.
- `4649c1`/`59c014`: authored AI byte maps through `{1,2,4,5,11,-1}`; actual
  L3S1 medic1 UID783 byte1 becomes default/current2. The special default2
  override is camera2, not this medic. Initial reserve200 is `402ea2`.
- `40a670`: waiting2, positive reserve, real held-slot prerequisite, active
  actor speech gate, current target class-health deficit; enter14, bind target,
  clear animation override and start43. `409210` permits accepted Use to retire
  the actual startup kneeling loop, not to reject it wholesale.
- `40a540`: full target resolution, model-origin radius sum +3.5 range,
  target-facing turn toward the actual resolved entity eye7d4 (`48a8d0`),
  delayed min(current deficit,current reserve), finite
  debit, exact player-class clamp and default transition. Armor is unchanged.
  Range abort explicitly stops nonloops; missing/full/completed cases do not.
- `4bb2e6`→`4299b0` and `429a50`: actor-route Message speech is the existing
  equivalent of the actor1400 voice. Poll only actual current actor-route
  voice activity; stale completions, radio, flat messages and subtitles do not
  block healing.

## Implemented owners

`scene_use_edge.inc` samples raw Use before modal/turret filtering. The source
player full identity, one selected handle and retained claim prevent a consumed
press from exiting/mounting/healing and then activating a Use trigger. Occupied,
modal and dead-player consumers take priority. A selected handler refusal does
not select a farther candidate. Medic6 and single-player AI-response9 claim
immediately; successful vehicle1/turret4 admission claims, while their refusals
retain trigger fallback. Kinds2/3 retain fallback. Existing automatic contacts
still run with Use0 when claimed. Since unclaimed port triggers retain their
existing held behavior, a claimed press remains claimed until raw release.

`scene_use_dispatch.inc` selects across the existing campaign skeletal NPC,
authored turret, selected vehicle and parked-vehicle owners in stable authored
record traversal. Actual medic01 `interface_1` is used; no body-center fallback
or invented second interface is added. Noncampaign developer vehicle/turret
selection retains its existing path, explicitly outside this campaign selector.

`scene_medic_action.inc` owns the pending source/target full handles and view
pointers, deadline, retained authored default, accepted motion ID and independent
residual-clip ownership. A dedicated current-frame/player-handle body-eye
receipt is published before aim/cinematic camera overrides and retains its
body-relative world offset through the subsequent physics step; it does not
borrow the listener or another weapon's eye cache. All fallible
resource/timer/playback work is staged
before startup kneel ownership is retired. Playback/resource references are
published together; neither pain nor death fields are used for healing.

The AI14 lane suppresses ordinary acquisition/fire/route/look steering while
keeping normal ground/support/contact/liquid services. Already-started reload
completion remains independent. Acceptance does not erase unrelated script or
combat records. A later current-AI change retires pending work without restoring
a stale default over its replacement. Source34 Set_AI_Mode changes current
mode only, leaving the authored default unchanged; no changed-default codec
lane is added. Source9 Shoot_Once and Goto ON invalidate the pending target
before firing/route admission, allowing the normal missing-target tick to
cancel without debit. Accepted Goto/Goto_Player/Shoot_At commands take over
through the port's existing command owners. Attack OFF and Shoot_At OFF
explicitly default; Goto/Goto_Player OFF remains the original no-op for this
transaction.
A pending ordinary unarmed medic's Attack ON targeting the same player is a
no-op success; a different target returns the existing nonfatal unsupported
result before mutation rather than retaining a stale recipient or fabricating
NPC healing. Look_At supersession is qualified to a resolved player/NPC UID;
unsupported zero-activator/stale-target overlaps skip only that linked actor,
and OFF defaults pending14. Existing general Look_At and armed Attack
admission differences remain bounded port behavior. Disarm, hiding and pain alone do not
invent extra cancellation. A new script that restarts the exact accepted motion
relinquishes only the medic presentation owner, not its pending AI14 transaction.

Transfer is one exact float min(current deficit,current reserve) at deadline,
with reserve_seen set only on a positive debit. No pickup multiplier, armor,
inventory grant, maximum100 fallback, fabricated damage or fresh syringe is
introduced. Exhaustion does not detach the real child. The prerequisite calls
`scene_medic_syringe_present`, supplied by the actual child owner.

## Explicit first-pass policies and limitations

1. Success persona speech selection is not implemented. Deadline uses the
   retained real action43 clip duration plus750ms. For the actual clip,
   start160/end24000 is about4.9666667s; timer conversion is rounded milliseconds,
   not a hardcoded five seconds. Optional Foley failure never decides health.
2. The no-resurrection rule is an explicit port lifecycle policy: a dead or
   replaced healer/player cannot complete an old transaction. Original426fc0
   itself only proves full-handle/type resolution, not a positive-health check.
3. Exact original factory-list creation ordering is not replicated. Strict ties
   preserve the port's stable authored-record traversal, including cross-owner
   vehicle/turret comparisons. Already-written vehicle handler origin-radius,
   clearance, seat, frozen-host and promotion gates remain post-selection
   admission constraints. Kinds5/10 have additional original monitor/type4
   gates whose handlers remain unimplemented; their existing trigger fallback
   is retained as a bounded deferral. Kind9 consumes without fabricating an
   AI-response handler. No missing handler is synthesized by this slice.
4. The existing single actor model update retains normal visibility culling.
   If only the specifically owned nonfreezing43 would otherwise be stranded by
   culling or another clip's whole-model freeze, an isolated normal-elapsed
   cursor branch advances43 and naturally retires it at its retained end tick.
   It preserves unrelated clip cursors, loop phase, freeze and event flags,
   repairs selected indices/refcount with the existing removal primitive, and
   never double-advances ordinarily playing43. A range stop only zeroes weights,
   so its residual ownership survives until actual slot removal; otherwise a
   culled zero-weight nonloop could become an unowned permanent save blocker. This explicit port presentation
   policy prevents a permanent post-heal save or section-exit veto.
5. Live action/owned residual43 remains export-only guarded. No transient wire
   codec or general nonloop43 admission is invented. A restoring-only predicate
   identifies the exact current owner for candidate preparation; it is not a
   saved-row allowance. The finite codec performs final-success-only publication
   and calls `scene_medic_load_committed` to discard old transient fields and
   reconcile the actual raw held input without changing newly loaded playback.
6. Frame0 never accepts or completes a heal. It may create the fresh scene and
   validate/publish a save. Failed loading does not reconcile the input edge.
   Section handoff waits for finite normal action/presentation progress; an
   accepted live QuickLoad or restart bypasses that wait and tears down the old
   timeline. Missing/transferred model ownership retires the clip guard.

## Shared integration contract

Actor fields added here:
- `scene_medic_action medic`
- `uint32_t medic_reserve_seen`
- `float medic_reserve`

Sibling syringe supplies `static int scene_medic_syringe_present(const campaign_npc_body *)`.
Sibling codec consumes `scene_medic_qualified`, `scene_medic_capture_transient`,
`scene_medic_load_committed`, and the independent finite reserve fields.
`scene_medic_save_guard(const char **reason)` is wired only into world snapshot
and NPC export; candidate load/ordinary history capture must not call it.

Parent owns source merge, any required compiler fixes, the one scheduled Xbox
validation batch and repository cleanup. Newly written behavior is unverified.

## Remaining bounded command scope

The already-selected L3S1 inventory has no Set_AI_Mode; actual UID783 appears
only under When_Hit2163 and startup Play_Animation2211. No synthetic overlap was
created or executed. General default4-without-waypoint→current2 behavior is
outside this actual authored-default2 slice. Fire_No_Animation79 target semantics
were not reconstructed and are unchanged. No broad original AI/default engine
or complete arbitrary event-target handoff is claimed.
