# Combat drops and authored pickups

NPC death drops now carry remaining loaded+reserve ammunition capped at the existing one-magazine policy. Supported held weapons include sniper and rail. Zero ammunition skips the drop because the current persistent-drop format requires positive quantity. Authored no-drop flags and physical tumbling remain deferred.

The focused scene_ai_drop_supply fixture executes actual campaign_weapon_drop_emit with a persistent actor and finite collision floor. It verifies exhausted weapons emit nothing,2loaded+1reserve persist exactly3rounds, placement above the floor, unchanged NPC inventory and no repeated emission. It then executes campaign_weapon_drops_tick: player acquires the sniper with3rounds, the persistent pickup retires, and repeat collection leaves inventory unchanged.

Authored pickup mapping grows from11 to20classes, preserving existing indices. Added classes are Remote Charge, Remote Charges, Sniper Rifle, rocket launcher, grenades, rail gun, .50cal_ammo, rocket_launcher_ammo and railgun_bolts. Collection and model validation use the same weapon-slot mapping. Table-backed checks verify the installed static models, weapon associations and weapon-versus-ammo effect. Assets still load only for classes present in the level. Full weapon selection remains developer-room-first; ordinary campaign selection still has its existing four-slot restriction.

PC executable and Xbox build pass. Focused drop/collection and table-backed mapping checks pass. Live rendered authored-pickup collection is not yet verified by this batch.

## Deferred AI mode seam
Set_AI_Mode34 now has an event callback with authored0..5 mapped to actions1,2,4,5,11,-1, including delayed dispatch and unchanged propagation. Its focused runtime check calls the existing core AI state setter. No scene callback is bound: current live scheduling consumes different state, so this does NOT establish working authored AI behavior. Mode13 cleanup and vehicle occupants also remain open. The unused scene adapter is not counted as implementation completion.

The separate scene_pickup_grant check now executes actual campaign_pickups_tick using installed item/supply tables: Remote Charges quantity3 grants shared reserve3; sniper weapon6+ammo6 produces loaded6/reserve6; rail weapon8+bolts8 observes its reserve cap. Five persistent retirement records survive campaign_pickups_restore, and repeated collection gives no duplicate ammo. The fixture uses an empty collision world; it does not prove rendered pickup appearance.
