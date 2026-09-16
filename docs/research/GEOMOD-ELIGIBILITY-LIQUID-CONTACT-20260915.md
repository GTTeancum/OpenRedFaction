# GeoMod liquid contacts must not enter terrain excavation (2026-09-15)

Evidence: tools/verify_projectile_liquid_contact.py and geomod-liquid-contact.json. Six complete original4c4b50 contactdispatcher calls execute actual4c4e30 liquidhandler, vectorhelpers andstatewrites. Onlyeffectallocation4c16e0 androomaccessor40a490 are supplied; enteringterrain4c4ec0,entity4c59f0 orGeoMod467020 triggersatestfailure. No nativegame launches orshared edits.

## Original dispatch

4c4b63 readsobject+1ec (collisionis_liquid). Any nonzero branches4c4b73 to4c4e30 BEFORE testingothercontacttypes. Zero proceeds4c4bf7:object+1d4 zero selectsgeometrycontact4c4ec0, nonzero selectsentitycontact4c59f0. Thus aliqcontactmustnot simplypass throughterrainimpact becauseithas aroom/face.

4c4e30 creates a splash-likevclip atobject+1b4 withsize=max(2*objectradius+78,0.5), currentroom andparent-1. Effecthandlecomesfrom8568a8 whenweapontype+52c is1or2, otherwise8568b0. The probe suppliesfirsthandle42 andobservesonespawnsize0.5 forradius0.1. This reportdoesnotidentifytheinstalledassetnames orvalidatevisualappearance.

Onlyweapontypeflag0x10000 (retainedheadernamesTORPEDO) setsobjectlife+34=0. Ordinaryflags0 leaveslifeunchanged;4c4e30 returns1. The contactdispatcher marksweaponstatebits, clearscontactis_liquid and clearsgeometryquerybit0x1000, thenreturns1 inthese fixtures. Sixcases coverliquidvalue1/2 andweaponflags0,10000,1000000(STICKY). Everycase bypassesmaster467020 andterrainhandler;lifebecomes0onlyTORPEDO.

This establishes no excavation and noordinarylifeexpiry **withinthishandler**. It doesnotexecuteentirelaterphysics/movement, so "rocketcontinuesunchangedforever" wouldbeoverbroad. Querybitclear preventsrepeatedliquidsurfacecollision untilanotherownerrestoresit. Whether/whenthisoccursneedsseparatetracing.

## Current implementation comparison

src/diagnostic/scene.c scene_rocket_sweep currentlycallsrf_geometry_collision_world_sweep withquery0x460 andcopieshit/room/face/object, withouta liquidcontactdiscriminator. src/core/weapon.c rf_weapon_flight_step treatsanymatchedcontact asactive=0,event.kind=1. scene_rockets_tick thenstartsimpacteffect/audio/blast and,forroom0 pluseligibleterrain,runsGeoMod.

Currentquery0x460 doesnotinclude1000, sotheknownDEV flowdoesnotyetadmitordinaryliquidsurfaces. **Do notswitchthatquerytooriginalfreshprojectile1004 withoutalsoaddingliquidcontactclassificationanddispatch.** Otherwisewatercanerroneouslytriggerblast/excavationandendflight. The minimumfuturebehavior is aseparateliquidentryevent thatspawnsitsownauthoredeffect, clearsliquidprocessingstate, anddoesnotqueueGeoMod. Ordinaryremaininglife muststayunchanged bythatentryhandler;TORPEDOexpiry is aseparateweaponflagbehavior. Preserveothernonliquidcollisionfilters recoveredearlier.

No currentDEV regression orvisualparityclaim: room0fixture hasnotbeenproved toincludeawatersurface. This reportis anactionableguardforgeneralizingauthoredrooms, not a reason tochange dryroombehaviorblindly.

## Shared reconstruction

`rf_weapon_liquid_contact` now implements the bounded state/effect policy. Six original contact cases and selector/clamp/error tests pass in `weapon_explosive_fields`; PC playable and NXDK Xbox builds pass. Live scene movement and splash rendering remain unconnected, so this is not a claim of working water gameplay.
