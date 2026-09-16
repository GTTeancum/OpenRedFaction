# GeoMod liquid contact remaining-time movement (2026-09-15)

Eight complete original4a01b0 calls execute the actual48a400 weapondispatcher and4c4b50/4c4e30 liquidcallback. Only49cd30 boundingbox refresh,4c16e0 effectallocation, and40a490 roomaccessor are supplied. Three additional49fb77..49fbe6 predictionintervals execute original vector arithmetic. No original-game launch or screenshot work. Evidence: tools/verify_projectile_liquid_remaining.py, geomod-liquid-remaining-time.json, geomod-liquid-next-prediction.json.

## Time and position are distinct

At4a01b8..4a01c4, consumedtime = originalremaining(object+1b0) * originalhitfraction(object+1cc), stored before modifyinghitfraction. Forfraction<1,4a01df..4a0248 computes pathlength from predictedendpoint+f0 minuscurrent+e4, then advancescurrent by max(0,originalfraction -0.05/pathlength) timespath. This is a0.05worldunit setback, withclampatcurrentposition. It is not radius-dependent inthiscode.

Afterrotation andbboxwork,4a034a calls48a400. Objectkind2 dispatches4c4b50. Result0 zerosremaining. Result2 calls49d330 collisionresponse then subtractsconsumedtime. Result1 skipsresponse and subtractsconsumedtime directly. It also marksphysicsflag10000000. Theliquidcallback returns1, clearsquery1000 andliquidstate, leavesordinarylife unchanged, anddoesnot callterrainorGeoMod.

Eight cases coverpathlength1/10,fractions0,.001,.25,.9 withremaining0.1. Atlength10,fraction.25,currentbecomes2.45 andremaining.075. Tinyfraction canadvancezero while stillconsuminga positive fraction oftime. Fraction0 retainsalltime buttheliquidquerybitclears, preventingthatparticularliquidface frombeingreselected underthenextsamequery.

## Original repeats collision with remaining time

Outer487770 repeatedlypredicts positions (ordinaryobject49f930 at487826), callsworldcollision49bb70 (4878f4 singleplayer), thenadvances/respondswith4a01b0 (48793c). It removesobjectswhose remaining<=0 fromtheactiveworklist. Afterpasscounter>3 it can cullsmallremainingtime using5893d4; atcounter>=10 it forcesremoval. These are staticloopconditions, not a provedfaithfuluniversaliterationbudgetforaport.

49fb77..49fbe6 predictsnewendpoint = current + velocity*remaining - 0.5*acceleration*remaining^2, usingglobalpreparedacceleration7c7048. Threeexecutedzeroaccelerationcases withcurrent2.45,velocity100 produce12.45,9.95,3.45 forremaining.1,.075,.01. Thuswatercontactdoesnot endtheframe andmustnotjumpstraighttotheoldendpoint. The renewedworldsweep canfindawallbehindwater. The0.05setback doesnotrefundtime: finalfreeflightaftertheexamplewaterentrywouldend9.95,not10.

## Query restoration limit

Directobject+1ac write audit findsprojectilecreation4c7c54 sets1000 andcontact4c4c94/4c4cb9 clearsit; genericcollisionresponses49d35b/49d81b also clearit. No projectile-specific nextiteration ornextframe OR1000 restoration was established. Actor initialization andotherobject/save-restoration writes mustnot be mislabeledprojectileresets. Consequently preserveclearedliquidprocessingstate acrossresweeps; do notautomaticallyre-enable iteachframe absentfurtherproof. This is a boundednegativefinding, notproof noaliasingwriterexists anywhere.

## Safe integration direction

A dedicatedliquid-awareflightstep canconsumehitfractiontime, advancewithprovedsetback, emitliquidevent, clearliquidprocessingstate andresweepremainingtime. Keepgenericstraight-flight API unchanged if callersdependonone terminalcontactperstep. Supportmultipleeventsorcallbackdeliveryinone ticksoawallbehindwaterstillproducesitsimpact. Bounditerationandfailwithoutpartialpublication iftheportcannotcomplete, instead ofdroppingtimeorsilentlyteleporting. Generalrotation/acceleration/objectcontacts areoutsidecurrentstraight-flight scope. Do not claim exactretailwholephysics fromthis numericalsubset.

## Exact endpoint and shared helper review

A ninth whole4a01b0 case sets fraction exactly1. Original4a0397 callsactual49d280, commitsfullpredictedendpoint, setsremaining0 andskips48a400 entirely: is_liquid andquery1000 remainunchanged. Thus afraction1 liquidcontact doesnotemitliquideffects inthisphysicsstep. Do notcalltheliquidpolicy onthisbranch orreusegenericflight'sterminalimpact-tie rule asanoriginalclaim.

Existingrf_physics_contact_advance alreadyimplements the0.05setback andfraction<1 domain, makingitareusecandidate. Oneprecisiondifference needsattention ifexactparitymatters: originalstoresconsumed dt*fraction tofloat beforelater subtraction; thecurrenthelper computesdouble dt-dt*fraction withonefinalfloatconversion. Tests here establishnumericalagreementwithtolerances, notbitidentityforthathelper. Otherhelperflagsupport comesfrompreviousphysicsRE andwasnotrevalidatedbythisliquidprobe.

## Reconstructed numerical comparison

The weapon route stores consumed time as float before subtraction, unlike the previously recovered49ffd2 path. `rf_physics_weapon_contact_advance` preserves that distinction while reusing bounded position advancement. Eight complete original weapon/liquid cases match position and remaining time bit-for-bit on PC and linked NXDK code under Unicorn. Three next-position samples and fraction1 bypass also pass against original instructions. Existing `verify_physics_advance.py` still passes256 PC and256 NXDK comparisons. Xbox compilation passes; no live scene water behavior or splash rendering is claimed.
