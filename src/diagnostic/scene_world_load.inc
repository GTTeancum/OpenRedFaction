#ifndef RF_SCENE_WORLD_LOAD_INCLUDED
#define RF_SCENE_WORLD_LOAD_INCLUDED
#include "scene_pursuit_return_late_selection.inc"
#include "scene_pursuit_return_resource_binding.inc"
#include "scene_npc_speech_late_publication.inc"
#include "scene_pursuit_return_allocator.inc"
#include "rf/prepare_stdio.h"
_Static_assert(_Alignof(scene_world_environment_stage)<=8 &&
    _Alignof(scene_environment_entry)<=8 &&
    _Alignof(scene_environment_nav_entry)<=8 &&
    _Alignof(scene_monitor_cameras)<=8 &&
    _Alignof(scene_world_mission_stage)<=8 &&
    _Alignof(scene_world_mission_trigger)<=8 &&
    _Alignof(scene_world_mission_pickup)<=8 &&
    _Alignof(scene_campaign_history_checkpoint_stage)<=8 &&
    _Alignof(scene_campaign_history_rebind_stage)<=8 &&
    _Alignof(scene_history_npc_rebind)<=8 &&
    _Alignof(scene_world_event_stage)<=8 &&
    _Alignof(scene_npc_reload_publication_receipt)<=8 &&
    _Alignof(scene_npc_reload_publication_row)<=8 &&
    _Alignof(scene_npc_reload_publication_resource)<=8 &&
    _Alignof(scene_npc_reload_publication_child)<=8 &&
    _Alignof(scene_npc_reload_publication_link)<=8,
    "Retained restore arena alignment contract changed");
#ifdef RF_IMAGE_XBOX_NATIVE
#include "scene_world_stage_arena.inc"
/* Retained requests use distinct stable slot addresses as tokens, because
 * child constructors publish from local candidates. They stay reserved through
 * terminal transaction cleanup, including any aborted child preparation.
 * Transients use their pinned local pointer variable until matching release. */
static int scene_world_stage_memory(void *context,const void *owner,size_t bytes,uint32_t acquire)
{
    scene_return_memory_ledger *ledger=context;uint32_t i;int status;
    if(acquire>1)return RF_RANGE;
    status=scene_return_memory_validate(ledger);if(status)return status;
    if(acquire){
        for(i=3;i<ledger->slot_count;i++)if(!ledger->slots[i].owner)break;
        if(i==ledger->slot_count)return RF_RANGE;
        return scene_return_memory_acquire(ledger,i,owner?owner:ledger->slots+i,bytes,
            owner?SCENE_RETURN_LEDGER_TRANSIENT:SCENE_RETURN_LEDGER_RETAINED);
    }
    if(!owner||bytes)return RF_RANGE;
    for(i=3;i<ledger->slot_count;i++)if(ledger->slots[i].owner==owner)break;
    if(i==ledger->slot_count||ledger->slots[i].kind!=SCENE_RETURN_LEDGER_TRANSIENT)return RF_FORMAT;
    return scene_return_memory_release(ledger,i,owner);
}
/* Each fopen request stays cumulatively charged through terminal cleanup,
 * including missing-file attempts. Closing never refunds admission. */
static int scene_world_stdio_open_admit(void *context,const char *filename)
{
    size_t bytes;int status=rf_prepare_stdio_request(filename,&bytes);
    if(status)return status;return scene_world_stage_memory(context,NULL,bytes,1);
}
/* Slot2 is only the actual resource-request plan, never resident motion data. */
static int scene_world_npc_plan_account(void *context,const void *owner,size_t request_bytes,uint32_t acquire)
{
    scene_return_memory_ledger *ledger=context;int status;
    if(acquire>1)return RF_RANGE;
    if(acquire)return scene_return_memory_acquire(ledger,2,owner,request_bytes,SCENE_RETURN_LEDGER_TRANSIENT);
    status=scene_return_memory_validate(ledger);if(status)return status;
    if(ledger->slot_count<=2||ledger->slots[2].request_bytes!=request_bytes)return RF_FORMAT;
    return scene_return_memory_release(ledger,2,owner);
}
#endif
/* Ordinary loader: pristine world or the admitted L1S2 authored terrain cut,
 * standing/seated player and settled NPCs.
 * Called once after frame-zero initialization, before the next input tick.
 * All gameplay owners validate before assignment; cache demand may remain on
 * an aborted load and is owned by normal scene cleanup. */
uint32_t rf_scene_world_load_reject[3]; /* phase FNV1a, status, staged bytes */
static int scene_world_load(scene_stream *stream,const rf_level *level,const char *tables_path)
{
    extern rf_entity_room_state rf_scene_actor_room_state;
#ifndef RF_IMAGE_XBOX_NATIVE
    const char *path=scene_live_load_active?"redfaction-save":getenv("RF_REPLAY_WORLD_SNAPSHOT_IN");
#else
    rf_xbox_checkpoint_storage storage={0};uint32_t enabled=0,fixture=0;
    const char *path=RF_XBOX_WORLD_CHECKPOINT_BASE;
#endif
    scene_world_snapshot_identity identity;rf_checkpoint_file_selection selection={0};rf_world_checkpoint envelope;
    unsigned char *buffer=NULL;uint32_t bytes=0,i,rows_count=0,rows_bytes=0,added=0,flags=0,selected=0,cost=sizeof(uint32_t);int status=RF_OK,found=0;
    const uint32_t budget=2u*1024u*1024u;const char *phase="identity";
    if(!scene_live_load_active && scene_world_startup_snapshot_consumed)return RF_OK;
    if(scene_vehicle_switch_has_parked())return RF_NOT_FOUND;
    rf_npc_checkpoint_catalog catalog={0};rf_npc_checkpoint_record *rows=NULL;rf_npc_checkpoint_rng inspected_rng={0};uint32_t inspected_tactical_cursor=0;
    scene_world_restore_stage *world=NULL;scene_world_event_stage *events=NULL;scene_world_mission_stage *mission=NULL;
    scene_authored_checkpoint_stage *terrain_stage=NULL;
    scene_authored_collection_stage *collection_stage=NULL;
    scene_world_environment_stage *environment=NULL;scene_campaign_history_checkpoint_stage *history=NULL;
    scene_campaign_history_rebind_stage *bindings=NULL;scene_world_player_stage player;
    scene_ai_projectile_checkpoint_stage *projectiles=NULL;
    scene_continuous_charge_checkpoint_stage charge={0};
    uint32_t projectile_audio_resume=0,world_published=0,flight_locked=0;
    scene_npc_airborne_restore_receipt airborne_receipt={0};
    uint32_t reload_transaction=0,storage_close_attempted=0,blind_potential=0,crouch_transaction=0;
    scene_npc_reload_player_receipt reload_player={0};
    scene_npc_reload_player_receipt return_on_foot={0};
    scene_npc_reload_publication_receipt *reload_receipt=NULL;
    scene_crouch_late_receipt *crouch_late=NULL;
    uint32_t late_resource_state=0,speech_potential=0;
    scene_npc_speech_late speech_late={0};
    scene_return_barriers *return_late=NULL;
    scene_return_resource_binding return_binding={0};
    scene_return_close_gate return_gate={0};
    const uint32_t return_stack_reservation=131072u;
    /* Complete native return lane adopted after allocation and compiled-path assessment. Reserve the authenticated128KiB stack once; do not add it to the legacy mixed cost estimate as though that estimate were heap-only. Normal platform assumptions and measured residual are documented in docs/NPC-PURSUIT-RETURN.md. */
    uint32_t return_contract_ready=1,combined_late=0,return_potential=0;
    uint32_t return_stage_bytes=0,return_existing_bytes=0;
    scene_return_transaction_allocator return_allocator={0};
    const rf_prepare_storage *world_stage_storage=NULL;
#ifdef RF_IMAGE_XBOX_NATIVE
    /* Cumulative native accounting: slots0/1 name file buffer and initial NPC rows; slot2 names resource planning. Subsequent admissions charge real temporary arena backing and separate persistent children, demand payloads, file requests and return-driver allocations. Each backing request includes128B overhead and64KiB rounding; no free or failed allocation refunds its admission. The unchanged2MiB cap bounds admission slots. The ledger itself does not prove compiled stack fit; that assessment is separate. */
    scene_return_memory_ledger early_memory={0};
    scene_return_memory_slot early_memory_slots[(2u*1024u*1024u)/65536u]={0};
    const rf_prepare_memory world_memory={scene_world_stage_memory,&early_memory};
    scene_world_stage_arena stage_arena={NULL,&world_memory};
    const rf_prepare_storage stage_storage={scene_world_stage_arena_allocate,scene_world_stage_arena_release,&stage_arena};
    scene_return_allocator_memory return_memory={{scene_world_stage_memory,&early_memory}};
    const rf_checkpoint_file_admission file_memory={scene_world_stdio_open_admit,&early_memory};
#endif
    scene_passive_vehicle_checkpoint_stage *passive=NULL;
    scene_vehicle_allegiance_stage *allegiance=NULL;
    scene_vehicle_attack_stage *vehicle_attack=NULL;scene_vehicle_ai_stage *vehicle_ai=NULL;
    scene_vehicle_physics_checkpoint_stage *vehicle_physics=NULL;
    scene_secondary_vehicle_checkpoint_stage *secondary=NULL;
    scene_vehicle_switch_checkpoint_stage *vehicle_switch=NULL;
    scene_turret_checkpoint_stage *turrets=NULL;
    scene_turret_generated_checkpoint_stage *generated=NULL;
    scene_npc_seat_checkpoint_stage *seats=NULL;
    scene_jeep_gunner_checkpoint_stage *gunner=NULL;
    scene_corpse_lifetime_stage *corpses=NULL;
    scene_masako_checkpoint_stage masako={0};
    scene_turret_player_checkpoint_stage *turret_player=NULL;
    scene_vehicle_checkpoint_record vehicle={0};scene_campaign_vehicle_route vehicle_route={0};
    uint32_t vehicle_present=0,vehicle_base=0,vehicle_bytes=0,authored_terrain=0,authored_cuts=0;
    const unsigned char *vehicle_data=NULL;
    scene_checkpoint_world_context context={0};rf_geometry_materials mapping={0};rf_geometry_body_surfaces surfaces;
    float modified_min[3]={0},modified_max[3]={0};
    const rf_world_checkpoint_slice *v;
#ifdef RF_IMAGE_XBOX_NATIVE
    world_stage_storage=&stage_storage;
    phase="early_stdio_idle";
    status=scene_world_flag_checked_idle_status();if(status)goto done;
    status=rf_vpp_checked_idle_status();if(status)goto done;
    status=rf_checkpoint_file_idle_status();if(status)goto done;
    status=rf_xbox_checkpoint_storage_idle_status();if(status)goto done;
    phase="early_stdio_memory";
    status=scene_return_memory_initialize(&early_memory,early_memory_slots,
        (uint32_t)(sizeof(early_memory_slots)/sizeof(*early_memory_slots)),
        budget,return_stack_reservation);if(status)goto done;
    if(!scene_live_load_active){
        status=scene_world_storage_flag_accounted("D:\\world-fixture-load.flag",&fixture,&file_memory);if(status)return status;
        if(!fixture){status=scene_world_storage_flag_accounted("D:\\world-hdd-load.flag",&enabled,&file_memory);if(status||!enabled)return status;}
    }
#endif
    if(!path||!*path)return RF_OK;
    status=scene_waypoint_boundary();if(status)return status;
    rf_scene_world_checkpoint_state[8]=1;
    if(scene_turret_player_active()||scene_player_jeep_gunner_active(stream)){status=RF_RANGE;goto done;}
    if(!stream||!level||!tables_path||!stream->collision||
       (stream->terrain&&!scene_world_l1s2_authored_terrain(stream))||
       (!stream->terrain&&(stream->terrain_authored||stream->terrain_publication))||
       stream->terrain_history_count||stream->terrain_publication_serial){status=RF_RANGE;goto done;}
    /* This direct in-session loader retains its conservative outgoing
     * guided-timeline refusal. QuickLoad constructs a fresh scene; incoming
     * RFAP10 flights/guidance publish together only after final success. */
    if(scene_player_rocket_lock_save_pending(stream)){phase="player_rocket_lock";status=RF_NOT_FOUND;goto done;}
    if(scene_ai_melee_checkpoint_busy()){phase="delayed_melee_callback";status=RF_NOT_FOUND;goto done;}
    if(scene_turret_heap_checkpoint_busy()||scene_turret_heap_pending()){phase="turret_heap_outgoing";status=RF_NOT_FOUND;goto done;}
    phase="identity";
#ifdef RF_IMAGE_XBOX_NATIVE
    status=scene_world_checkpoint_source_identity_accounted(level,tables_path,rf_scene_weapon_supply[3],identity.bytes,&file_memory);
#else
    status=scene_world_checkpoint_source_identity(level,tables_path,rf_scene_weapon_supply[3],identity.bytes);
#endif
    if(status)goto done;
    if(strlen(campaign_current_level)>=64){status=RF_FORMAT;goto done;}
    memset(identity.level,0,64);strcpy(identity.level,campaign_current_level);
#ifdef RF_IMAGE_XBOX_NATIVE
    phase="early_file_memory";
    status=scene_return_memory_acquire(&early_memory,0,&buffer,
        RF_CHECKPOINT_FILE_MAX,SCENE_RETURN_LEDGER_RETAINED);if(status)goto done;
#endif
    buffer=malloc(RF_CHECKPOINT_FILE_MAX);if(!buffer){status=RF_IO;goto done;}
    phase="file";
#ifndef RF_IMAGE_XBOX_NATIVE
    status=rf_checkpoint_file_load(path,buffer,RF_CHECKPOINT_FILE_MAX,&bytes,scene_world_snapshot_validate,&identity,&selection);
#else
    (void)path;
    if(fixture)status=rf_checkpoint_file_load_accounted(SCENE_WORLD_FIXTURE_BASE,buffer,RF_CHECKPOINT_FILE_MAX,&bytes,
        scene_world_snapshot_validate,&identity,&selection,&file_memory);
    else {status=scene_live_load_active&&scene_live_load_auto_source?
        rf_xbox_checkpoint_storage_open_world_auto(&storage,0):rf_xbox_checkpoint_storage_open_world(&storage,0);
        if(!status)status=rf_xbox_checkpoint_storage_load_accounted(&storage,buffer,RF_CHECKPOINT_FILE_MAX,&bytes,scene_world_snapshot_validate,&identity,&file_memory);}
#endif
    if(status)goto done;
    status=rf_world_checkpoint_decode(buffer,bytes,RF_WORLD_CHECKPOINT_MASK(RF_WORLD_ENVIRONMENT),&envelope);if(status)goto done;
    phase="continuous_charge_legacy";v=envelope.sections+RF_WORLD_PROJECTILES-1;
    status=scene_continuous_charge_checkpoint_preflight(v->data,v->bytes);if(status)goto done;
    if(envelope.sections[RF_WORLD_SWITCHES-1].bytes){status=RF_NOT_FOUND;goto done;}
    phase="vehicle_switch";v=envelope.sections+RF_WORLD_VEHICLE-1;
#ifdef RF_IMAGE_XBOX_NATIVE
    status=scene_vehicle_switch_checkpoint_prepare_accounted(stream,v->data,v->bytes,budget-cost,&vehicle_switch,&vehicle_data,&vehicle_bytes,&world_memory);
#else
    status=scene_vehicle_switch_checkpoint_prepare(stream,v->data,v->bytes,budget-cost,&vehicle_switch,&vehicle_data,&vehicle_bytes);
#endif
    if(status)goto done;
    cost+=scene_vehicle_switch_checkpoint_bytes(vehicle_switch);
    phase="masako_phase";
    status=scene_masako_checkpoint_prepare(vehicle_data,vehicle_bytes,&masako,&vehicle_data,&vehicle_bytes);if(status)goto done;
    phase="corpse_lifetime";
#ifdef RF_IMAGE_XBOX_NATIVE
    status=scene_corpse_lifetime_checkpoint_prepare_accounted(vehicle_data,vehicle_bytes,budget-cost,&corpses,&vehicle_data,&vehicle_bytes,&world_memory);
#else
    status=scene_corpse_lifetime_checkpoint_prepare(vehicle_data,vehicle_bytes,budget-cost,&corpses,&vehicle_data,&vehicle_bytes);
#endif
    if(status)goto done;
    if(corpses)cost+=corpses->allocated_bytes;
    if(cost>budget){status=RF_RANGE;goto done;}
    phase="turret_player";
#ifdef RF_IMAGE_XBOX_NATIVE
    status=scene_turret_player_checkpoint_prepare_accounted(vehicle_data,vehicle_bytes,budget-cost,&turret_player,&vehicle_data,&vehicle_bytes,&world_memory);
#else
    status=scene_turret_player_checkpoint_prepare(vehicle_data,vehicle_bytes,budget-cost,&turret_player,&vehicle_data,&vehicle_bytes);
#endif
    if(status)goto done;
    if(turret_player)cost+=turret_player->allocated_bytes;
    if(cost>budget){status=RF_RANGE;goto done;}
    phase="npc_resources";catalog.hash=rf_scene_weapon_supply[3];catalog.count=campaign_weapon_supply.names.count;
    if(!catalog.count||catalog.count>64){status=RF_RANGE;goto done;}
    memcpy(catalog.weapons,campaign_weapon_supply.definitions,sizeof(catalog.weapons));
    for(i=0;i<catalog.count;i++){const rf_weapon_acquire_definition *d=catalog.weapons+i;
        catalog.supported[i]=(uint8_t)(d->ammo_type>=-1&&d->ammo_type<32&&d->capacity>=0&&d->magazine>=0&&(d->ammo_type!=-1||d->magazine==0));}
    v=envelope.sections+RF_WORLD_NPC-1;status=rf_npc_checkpoint_preflight(v->data,v->bytes,identity.bytes,&catalog,&rows_count);if(status)goto done;
    if((uint64_t)rows_count*sizeof(*rows)>budget-cost){status=RF_RANGE;goto done;}
    rows_bytes=rows_count*(uint32_t)sizeof(*rows);
    if(rows_count){
#ifdef RF_IMAGE_XBOX_NATIVE
        phase="early_npc_rows_memory";
        status=scene_return_memory_acquire(&early_memory,1,&rows,
            rows_bytes,SCENE_RETURN_LEDGER_TRANSIENT);if(status)goto done;
#endif
        rows=malloc(rows_bytes);if(!rows){status=RF_IO;goto done;}
    }
    /* Resource inspection validates but does not restore the singleton. */
    status=rf_npc_checkpoint_decode_with_speech_motion(v->data,v->bytes,identity.bytes,&catalog,rows,rows_count,&rows_count,&inspected_rng,&inspected_tactical_cursor);if(status)goto done;
    phase="speech_pending";
    status=scene_npc_speech_load_potential(rows,rows_count,&speech_potential);if(status)goto done;
    /* Validate outgoing and incoming return potential before resource demand. The following eligibility, memory, ownership and final-close checks govern the complete late transaction; return owners cannot use the legacy early-close lane. */
    {
     phase="return_late_pending";
     status=scene_pursuit_return_load_potential(rows,rows_count,&return_potential);
     if(status)goto done;
    }
    /* Select outgoing/incoming reload OR active guard before residency can
     * reclaim a zero-reference mapped payload. Incoming absence still closes
     * the outgoing guard only after this complete transaction succeeds. Invalid claims never fall through to legacy load. */
    phase="reload_selector";
    status=scene_npc_reload_live_present(combat_frame==UINT32_MAX?0:combat_frame,&reload_transaction);if(status)goto done;
    {uint32_t outgoing,incoming;
     status=scene_blind_live_present(combat_frame==UINT32_MAX?0:combat_frame,&outgoing);if(status)goto done;
     status=scene_blind_incoming_present(rows,rows_count,&incoming);if(status)goto done;
     blind_potential=incoming;reload_transaction|=outgoing;
     for(i=0;i<rows_count;i++)reload_transaction|=rows[i].blind_present;}
    {uint32_t outgoing,incoming;
     status=scene_crouch_live_present(&outgoing);if(status)goto done;
     status=scene_crouch_incoming_present(rows,rows_count,&incoming);if(status)goto done;
     crouch_transaction=outgoing|incoming;}
    {uint32_t tactical_present;
     status=scene_tactical_live_present(&tactical_present);if(status)goto done;
     if(!return_potential)reload_transaction|=tactical_present;
     else for(i=0;i<campaign_npc_body_count;i++){
         const campaign_npc_body *o=campaign_npc_bodies+i;
         if(!o->tactical.flags)continue;
         /* Only this return owner's stationary tactical state participates
          * in the late owner union. Unrelated owners retain early close. */
         if(!o->pursuit_return.phase||
            !(o->tactical.flags&SCENE_TACTICAL_PRESENT)||
            o->tactical.phase!=SCENE_TACTICAL_STATIONARY){
             reload_transaction=1;continue;
         }
         status=scene_pursuit_return_tactical_admit(
             &o->pursuit_return,&o->tactical);if(status)goto done;
     }
     for(i=0;i<rows_count;i++)if(rows[i].tactical.present)
         reload_transaction|=!return_potential||
             !rows[i].pursuit_return_phase||
             rows[i].tactical.phase!=SCENE_TACTICAL_STATIONARY;}
    {uint32_t guard_present;
     status=scene_guard_retreat_live_present(combat_frame==UINT32_MAX?0:combat_frame,&guard_present);if(status)goto done;
     reload_transaction|=guard_present;}
    {uint32_t alarm_present;status=scene_alarm_runner_live_present(&alarm_present);if(status)goto done;reload_transaction|=alarm_present;}
    {uint32_t noise_present;status=scene_noise_live_present(&noise_present);if(status)goto done;reload_transaction|=noise_present;}
    {uint32_t dwell_present;status=scene_patrol_dwell_live_present(&dwell_present);if(status)goto done;reload_transaction|=dwell_present;}
    {uint32_t tags_present;status=scene_waypoint_event_selection(&envelope,&tags_present);if(status)goto done;
     reload_transaction|=tags_present;}
    /* Both immutable defaults select before demand, even when the outgoing
     * current route is interrupted or the incoming row deliberately omits it. */
    {uint32_t outgoing,incoming;
     status=scene_patrol_one_way_live_present(&outgoing);if(status)goto done;
     status=scene_patrol_one_way_incoming_present(rows,rows_count,&incoming);if(status)goto done;
     reload_transaction|=outgoing|incoming;}
    for(i=0;i<rows_count;i++){
        if(rows[i].route.patrol&1){
            uint32_t slot,tags_present;
            for(slot=0;slot<campaign_npc_body_count;slot++)if((uint32_t)campaign_seeds.records.items[slot].record.uid==rows[i].uid)break;
            if(slot==campaign_npc_body_count){status=RF_FORMAT;goto done;}
            status=scene_waypoint_actor_events(slot,&tags_present);if(status)goto done;reload_transaction|=tags_present;
        }
        if(rows[i].reload_present>1||(!rows[i].reload_present&&(rows[i].reload_weapon||rows[i].reload_remaining))){status=RF_FORMAT;goto done;}
        reload_transaction|=(rows[i].ai_mode==6)|rows[i].reload_present|rows[i].guard_retreat.present|rows[i].alarm.present|!!(rows[i].route.flags&64);
    }
    /* Initial return qualification. Empty input alone cannot prove pristine outgoing geometry. Return owners must never fall through to an early-close transaction. */
    if(return_potential){
        phase="return_late_eligibility";
        if(reload_transaction||blind_potential||
           envelope.sections[RF_WORLD_VEHICLE-1].bytes||
           stream->driller_runtime||stream->driller||scene_vehicle_switch_has_parked()||
           envelope.sections[RF_WORLD_DESTRUCTION-1].bytes||
           stream->terrain||stream->terrain_authored||stream->terrain_publication||
           stream->terrain_history_count||stream->terrain_publication_serial||
           (stream->campaign_geomod&&stream->campaign_geomod->active)||
           (stream->campaign_wall&&stream->campaign_wall->active)){
            status=RF_NOT_FOUND;goto done;
        }
        /* Initial return lane excludes the complete encoded vehicle section,
         * including companion wrappers, and any outgoing primary host. The
         * later vehicle_present flag is not decoded yet. Authenticate on-foot
         * ownership even when no return edge targets the player. */
        status=scene_npc_reload_player_admit(stream,&return_on_foot);if(status)goto done;
    }
    /* Potential-only participants reserve the protected lane before demand,
     * but RFNS/player ownership must resolve before they require on-foot. */
    if(reload_transaction||blind_potential||crouch_transaction||return_potential||speech_potential){
        uint32_t extra=(uint32_t)(sizeof(reload_player)+sizeof(reload_receipt)+sizeof(crouch_late)+4*sizeof(uint32_t)+sizeof(speech_late)+sizeof(speech_potential));
        if(cost>budget||extra>budget-cost||rows_bytes>budget-cost-extra){status=RF_RANGE;goto done;}
        cost+=extra;phase="reload_current_on_foot";
        if(reload_transaction){status=scene_npc_reload_player_admit(stream,&reload_player);if(status)goto done;}
    }
    phase="npc_resources";
    status=scene_corpse_lifetime_checkpoint_admit(corpses,rows,rows_count);if(status)goto done;
#ifdef RF_IMAGE_XBOX_NATIVE
    status=scene_npc_checkpoint_resources_require_memory(rows,rows_count,budget-cost-rows_bytes,
        &added,scene_world_npc_plan_account,&early_memory,&world_memory);
#else
    status=scene_npc_checkpoint_resources_require(rows,rows_count,budget-cost-rows_bytes,&added);
#endif
    if(status)goto done;
    free(rows);rows=NULL;
#ifdef RF_IMAGE_XBOX_NATIVE
    /* Plan ownership was released before returning; row ownership ends now. Both remain cumulatively charged, including overlap with the retained file buffer. Missing motion payloads are separately admitted at their actual malloc; aggregate demand totals never replace these per-request charges. */
    if(early_memory_slots[1].owner){
        phase="early_npc_rows_release";
        status=scene_return_memory_release(&early_memory,1,&rows);if(status)goto done;
    }
#endif
    v=envelope.sections+RF_WORLD_DESTRUCTION-1;
    if(v->bytes>=4 && !memcmp(v->data,"RFDS",4)){
        rf_geomod_terrain_view edited;
        if(!scene_world_l1s2_authored_terrain(stream)){status=RF_FORMAT;goto done;}
        phase="authored_terrain";
        if(stream->terrain_source_count>1) {
            status=scene_authored_collection_stage_prepare(stream,v->data,v->bytes,&collection_stage);
            if(status)goto done;
            authored_cuts=collection_stage->shared.cuts;
            status=scene_authored_collection_stage_commit(collection_stage,NULL);if(status)goto done;
            scene_authored_collection_stage_discard(&collection_stage);
        } else {
            status=scene_authored_checkpoint_stage_prepare(stream,v->data,v->bytes,NULL,&terrain_stage);
            if(status)goto done;
            authored_cuts=terrain_stage->cuts;
            status=scene_authored_checkpoint_stage_commit(terrain_stage);if(status)goto done;
            scene_authored_checkpoint_stage_discard(&terrain_stage);
        }
        authored_terrain=1;
        status=scene_terrain_publication_view(stream,&edited);if(status)goto done;
        for(i=0;i<edited.mesh.vertex_count;i++)for(uint32_t axis=0;axis<3;axis++){
            float coordinate=edited.mesh.vertices[i].position[axis];
            if(!i){modified_min[axis]=modified_max[axis]=coordinate;}
            else {if(coordinate<modified_min[axis])modified_min[axis]=coordinate;
                  if(coordinate>modified_max[axis])modified_max[axis]=coordinate;}
        }
        if(authored_cuts && !edited.mesh.vertex_count){status=RF_FORMAT;goto done;}
    }else if(v->bytes){phase="destruction";
        if(stream->terrain){status=RF_FORMAT;goto done;}
        status=scene_campaign_geomod_checkpoint_restore(stream,v->data,v->bytes);if(status)goto done;
        {const rf_geomod_mesh_view *edited[3]={
            stream->campaign_geomod && stream->campaign_geomod->active?&stream->campaign_geomod->mesh:NULL,
            stream->campaign_wall && stream->campaign_wall->active?&stream->campaign_wall->rooms[0].mesh:NULL,
            stream->campaign_wall && stream->campaign_wall->active?&stream->campaign_wall->rooms[1].mesh:NULL};
         uint32_t side,found_vertex=0;
         for(side=0;side<3;side++)if(edited[side])for(uint32_t vertex=0;vertex<edited[side]->vertex_count;vertex++)for(i=0;i<3;i++){
             float coordinate=edited[side]->vertices[vertex].position[i];
             if(!found_vertex){modified_min[i]=modified_max[i]=coordinate;}
             else {if(coordinate<modified_min[i])modified_min[i]=coordinate;
                   if(coordinate>modified_max[i])modified_max[i]=coordinate;}
             if(i==2)found_vertex=1;
         }
         if(!found_vertex){status=RF_FORMAT;goto done;}
        }
    }
    mapping.offsets=actor_follow_world->offsets;mapping.slots=actor_follow_world->slots;mapping.count=actor_follow_world->geometry_count;mapping.textures.count=actor_follow_world->material_count;
    surfaces.geometries=campaign_surface_sources;surfaces.count=mapping.count;surfaces.mapping=&mapping;surfaces.palette=campaign_surface_palette;
    context.world=stream->collision;context.movers=&campaign_movers;context.metadata=campaign_body_surface;context.metadata_context=&surfaces;
    context.dt=1.0f/60;context.authored_static_unchanged=!v->bytes||(authored_terrain&&!authored_cuts);context.fresh_boot=1;
    if(v->bytes&&(!authored_terrain||authored_cuts)){context.modified_min=modified_min;context.modified_max=modified_max;}
    if(authored_terrain&&authored_cuts)context.modified_rooms[context.modified_room_count++]=stream->terrain_authored->asset_view.room;
    if(stream->campaign_geomod&&stream->campaign_geomod->active)context.modified_rooms[context.modified_room_count++]=28;
    if(stream->campaign_wall&&stream->campaign_wall->active){
        context.modified_rooms[context.modified_room_count++]=stream->campaign_wall->overlay[0].room;
        context.modified_rooms[context.modified_room_count++]=stream->campaign_wall->overlay[1].room;
    }
    if(cost>budget){status=RF_RANGE;goto done;}
#ifdef RF_IMAGE_XBOX_NATIVE
    status=scene_turret_retirement_checkpoint_prepare_accounted(vehicle_data,vehicle_bytes,0,budget-cost,&generated,&vehicle_data,&vehicle_bytes,&world_memory);
#else
    status=scene_turret_retirement_checkpoint_prepare(vehicle_data,vehicle_bytes,0,budget-cost,&generated,&vehicle_data,&vehicle_bytes);
#endif
    if(status)goto done;
    if(generated)cost+=generated->allocated_bytes;
    if(cost>budget){status=RF_RANGE;goto done;}
#ifdef RF_IMAGE_XBOX_NATIVE
    status=scene_turret_checkpoint_prepare_accounted(vehicle_data,vehicle_bytes,0,budget-cost,&turrets,&vehicle_data,&vehicle_bytes,&world_memory);
#else
    status=scene_turret_checkpoint_prepare(vehicle_data,vehicle_bytes,0,budget-cost,&turrets,&vehicle_data,&vehicle_bytes);
#endif
    if(status)goto done;
    if(turrets)cost+=turrets->allocated_bytes;
#ifdef RF_IMAGE_XBOX_NATIVE
    status=scene_npc_seat_checkpoint_prepare_accounted(vehicle_data,vehicle_bytes,budget-cost,&seats,&vehicle_data,&vehicle_bytes,&world_memory);
#else
    status=scene_npc_seat_checkpoint_prepare(vehicle_data,vehicle_bytes,budget-cost,&seats,&vehicle_data,&vehicle_bytes);
#endif
    if(status)goto done;
    cost+=scene_npc_seat_checkpoint_bytes(seats);
    if(cost>budget){status=RF_RANGE;goto done;}
    phase="vehicle_ai";
#ifdef RF_IMAGE_XBOX_NATIVE
    status=scene_vehicle_ai_prepare_accounted(stream,vehicle_data,vehicle_bytes,budget-cost,&vehicle_ai,&vehicle_data,&vehicle_bytes,&world_memory);
#else
    status=scene_vehicle_ai_prepare(stream,vehicle_data,vehicle_bytes,budget-cost,&vehicle_ai,&vehicle_data,&vehicle_bytes);
#endif
    if(status)goto done;
    cost+=scene_vehicle_ai_bytes(vehicle_ai);if(cost>budget){status=RF_RANGE;goto done;}
    phase="vehicle_attack";
#ifdef RF_IMAGE_XBOX_NATIVE
    status=scene_vehicle_attack_prepare_accounted(stream,vehicle_data,vehicle_bytes,budget-cost,
        &vehicle_attack,&vehicle_data,&vehicle_bytes,&world_memory);
#else
    status=scene_vehicle_attack_prepare(stream,vehicle_data,vehicle_bytes,budget-cost,
        &vehicle_attack,&vehicle_data,&vehicle_bytes);
#endif
    if(status)goto done;
    cost+=scene_vehicle_attack_bytes(vehicle_attack);
    if(cost>budget){status=RF_RANGE;goto done;}
    phase="vehicle_allegiance";

#ifdef RF_IMAGE_XBOX_NATIVE
    status=scene_vehicle_allegiance_prepare_accounted(vehicle_data,vehicle_bytes,budget-cost,
        &allegiance,&vehicle_data,&vehicle_bytes,&world_memory);
#else
    status=scene_vehicle_allegiance_prepare(vehicle_data,vehicle_bytes,budget-cost,
        &allegiance,&vehicle_data,&vehicle_bytes);
#endif
    if(status)goto done;
    cost+=scene_vehicle_allegiance_bytes(allegiance);
    if(cost>budget){status=RF_RANGE;goto done;}
    phase="vehicle_physics";

#ifdef RF_IMAGE_XBOX_NATIVE
    status=scene_vehicle_physics_checkpoint_prepare_accounted(stream,vehicle_data,vehicle_bytes,budget-cost,&vehicle_physics,&vehicle_data,&vehicle_bytes,&world_memory);
#else
    status=scene_vehicle_physics_checkpoint_prepare(stream,vehicle_data,vehicle_bytes,budget-cost,&vehicle_physics,&vehicle_data,&vehicle_bytes);
#endif
    if(status)goto done;
    cost+=scene_vehicle_physics_checkpoint_bytes(vehicle_physics);
    if(cost>budget){status=RF_RANGE;goto done;}
    phase="secondary_vehicle";
#ifdef RF_IMAGE_XBOX_NATIVE
    status=scene_secondary_vehicle_checkpoint_prepare_accounted(stream,vehicle_data,vehicle_bytes,budget-cost,
        &secondary,&vehicle_data,&vehicle_bytes,&world_memory);
#else
    status=scene_secondary_vehicle_checkpoint_prepare(stream,vehicle_data,vehicle_bytes,budget-cost,
        &secondary,&vehicle_data,&vehicle_bytes);
#endif
    if(status)goto done;
    cost+=scene_secondary_vehicle_checkpoint_bytes(secondary);
    status=scene_vehicle_attack_admit(vehicle_attack,secondary);if(status)goto done;
    status=scene_vehicle_ai_admit(vehicle_ai,secondary,vehicle_attack);if(status)goto done;
    /* Old passive-only saves cannot silently become a different active owner. */
    if(campaign_vehicle_selection_fallback && (!vehicle_bytes ||
       (vehicle_bytes>=SCENE_PASSIVE_VEHICLE_HEADER && !memcmp(vehicle_data,"RFVA",4) && !scene_history_word(vehicle_data+8)))){
        phase="vehicle_selection_legacy";status=RF_NOT_FOUND;goto done;
    }
    if(vehicle_bytes>=4&&!memcmp(vehicle_data,"RFVA",4)){
        phase="passive_vehicle";

#ifdef RF_IMAGE_XBOX_NATIVE
    status=scene_passive_vehicle_checkpoint_prepare_accounted(vehicle_data,vehicle_bytes,budget-cost,&passive,&world_memory);
#else
    status=scene_passive_vehicle_checkpoint_prepare(vehicle_data,vehicle_bytes,budget-cost,&passive);
#endif

        if(status)goto done;cost+=passive->allocated_bytes;
        vehicle_data+=SCENE_PASSIVE_VEHICLE_HEADER;vehicle_bytes=passive->host_bytes;
    }else if(campaign_passive_vehicle_count){status=RF_FORMAT;goto done;}
    vehicle_present=vehicle_bytes!=0;
    if(vehicle_present){
        vehicle_base=(rf_scene_vehicle_enabled==3||rf_scene_vehicle_enabled==5)?160u:128u;
        if(vehicle_bytes<vehicle_base){status=RF_FORMAT;goto done;}
        phase="vehicle_read";
#ifdef RF_IMAGE_XBOX_NATIVE
        status=scene_vehicle_checkpoint_read_accounted(stream,vehicle_data,vehicle_base,&vehicle,&world_memory);
#else
        status=scene_vehicle_checkpoint_read(stream,vehicle_data,vehicle_base,&vehicle);
#endif
        if(status)goto done;
        phase="vehicle_route";status=scene_world_vehicle_route_read(vehicle_data,vehicle_bytes,vehicle_base,&vehicle_route);if(status)goto done;
        if(vehicle_route.active&&!vehicle.value.vehicle.alive){status=RF_FORMAT;goto done;}
    }
    status=scene_vehicle_physics_checkpoint_admit(stream,vehicle_physics,vehicle_present?&vehicle:NULL,passive);if(status)goto done;
    status=scene_secondary_vehicle_checkpoint_admit(stream,secondary,passive,vehicle_physics);if(status)goto done;
    status=scene_vehicle_switch_checkpoint_admit(stream,vehicle_switch,vehicle_present?&vehicle:NULL,passive,vehicle_physics,secondary);if(status)goto done;
    status=scene_npc_seat_checkpoint_hosts(seats,turrets,vehicle_present?&vehicle:NULL,
        scene_vehicle_physics_checkpoint_active_hidden(vehicle_physics),vehicle_switch,passive);if(status)goto done;
    phase="world";
#ifdef RF_IMAGE_XBOX_NATIVE
    status=scene_world_restore_prepare_with_passive_storage_accounted(buffer,bytes,identity.bytes,0,&catalog,0,&context,
        budget-cost,&world,stream,passive,seats,secondary,&world_memory,world_stage_storage);
#else
    status=scene_world_restore_prepare_with_passive(buffer,bytes,identity.bytes,0,&catalog,0,&context,
        budget-cost,&world,stream,passive,seats,secondary);
#endif
    if(status)goto done;
    /* Legacy budget only: individual requests already entered the ledger. */
    cost+=world->allocated_bytes;
    phase="environment";v=envelope.sections+RF_WORLD_ENVIRONMENT-1;
#ifdef RF_IMAGE_XBOX_NATIVE
    status=scene_world_environment_prepare_storage_accounted(identity.bytes,16,v->data,v->bytes,budget-cost,&environment,&world_memory,world_stage_storage);
#else
    status=scene_world_environment_prepare(identity.bytes,16,v->data,v->bytes,budget-cost,&environment);
#endif
    if(status)goto done;cost+=environment->bytes;
    phase="player";
#ifdef RF_IMAGE_XBOX_NATIVE
    status=scene_world_player_prepare_accounted(stream,world,environment->support_uid,
        environment->support_point_present?environment->support_point_velocity:NULL,
        vehicle_present?&vehicle:NULL,passive,&player,&world_memory);
#else
    status=scene_world_player_prepare(stream,world,environment->support_uid,
        environment->support_point_present?environment->support_point_velocity:NULL,
        vehicle_present?&vehicle:NULL,passive,&player);
#endif
    if(status)goto done;
    if(vehicle_present){phase="vehicle";
#ifdef RF_IMAGE_XBOX_NATIVE
        status=scene_world_vehicle_prepare_accounted(stream,world,&player,&vehicle,seats,
            scene_vehicle_physics_checkpoint_active_hidden(vehicle_physics),&world_memory);
#else
        status=scene_world_vehicle_prepare(stream,world,&player,&vehicle,seats,
            scene_vehicle_physics_checkpoint_active_hidden(vehicle_physics));
#endif
        if(status)goto done;}
    if(player.player.player.weapon==UINT32_MAX)found=1;
    else for(i=0;i<scene_weapon_slots();i++)if(scene_weapon_available(i)&&player.player.player.weapon==(uint32_t)campaign_slot_weapon(i)){
        selected=i;found=1;break;}
    if(!found||(player.player.player.weapon!=UINT32_MAX&&!stream->player_weapon[selected])){status=RF_NOT_FOUND;goto done;}
    phase="form";
    if(environment->form_next.active &&
       (campaign_extra_ids[3]<0||campaign_extra_ids[3]>=64||!scene_weapon_available(16)||
        !stream->player_weapon[16]||!player.player.player.inventory.owned[campaign_extra_ids[3]])){
        status=RF_NOT_FOUND;goto done;
    }
    phase="mission";
#ifdef RF_IMAGE_XBOX_NATIVE
    status=scene_world_mission_prepare_with_movers_storage_accounted(stream,&envelope,0,budget-cost,world->movers.entries,world->movers.controller_count,world->world.world,&mission,&world_memory,world_stage_storage);
#else
    status=scene_world_mission_prepare_with_movers(stream,&envelope,0,budget-cost,world->movers.entries,world->movers.controller_count,world->world.world,&mission);
#endif
    if(status)goto done;cost+=mission->bytes;
    phase="history";v=envelope.sections+RF_WORLD_CAMPAIGN_HISTORY-1;
#ifdef RF_IMAGE_XBOX_NATIVE
    status=scene_campaign_history_checkpoint_prepare_storage_accounted(identity.bytes,v->data,v->bytes,budget-cost,&history,&world_memory,world_stage_storage);
#else
    status=scene_campaign_history_checkpoint_prepare(identity.bytes,v->data,v->bytes,budget-cost,&history);
#endif
    if(status)goto done;cost+=(uint32_t)sizeof(*history);
    status=scene_masako_checkpoint_legacy_history(&masako,history);if(status)goto done;
#ifdef RF_IMAGE_XBOX_NATIVE
    status=scene_campaign_history_checkpoint_rebind_prepare_storage_accounted(history,budget-cost,&bindings,&world_memory,world_stage_storage);
#else
    status=scene_campaign_history_checkpoint_rebind_prepare(history,budget-cost,&bindings);
#endif
    if(status)goto done;cost+=bindings->bytes;
    phase="events";
#ifdef RF_IMAGE_XBOX_NATIVE
    status=scene_world_event_prepare_storage_accounted(stream,&envelope,0,world,budget-cost,&events,&flags,&world_memory,world_stage_storage);
#else
    status=scene_world_event_prepare(stream,&envelope,0,world,budget-cost,&events,&flags);
#endif
    if(status)goto done;cost+=events->bytes;
    phase="alarm_event_join";status=scene_alarm_runner_event_join(world->npcs,events);if(status)goto done;
    phase="waypoint_event_join";status=scene_waypoint_event_join(world->npcs,events);if(status)goto done;
    if(flags&(SCENE_WORLD_EVENT_PUBLICATION_BLOCKERS&~(SCENE_WORLD_EVENT_MISSING_GRAVITY|SCENE_WORLD_EVENT_MISSING_FORCES|SCENE_WORLD_EVENT_NEEDS_PLAYER_MISSION))){status=RF_NOT_FOUND;goto done;}
    phase="projectiles";v=envelope.sections+RF_WORLD_PROJECTILES-1;
    if(v->bytes){
#ifdef RF_IMAGE_XBOX_NATIVE
        status=scene_ai_projectile_checkpoint_prepare_accounted(stream,v->data,v->bytes,budget-cost,&projectiles,&world_memory);
#else
        status=scene_ai_projectile_checkpoint_prepare(stream,v->data,v->bytes,budget-cost,&projectiles);
#endif
        if(status)goto done;cost+=scene_ai_projectile_checkpoint_stage_bytes();
        if(scene_ai_projectile_checkpoint_trigger_pending(projectiles)&&
           player.player.player.weapon!=(uint32_t)campaign_rocket_id){status=RF_FORMAT;goto done;}}
    status=scene_ai_projectile_checkpoint_npc_fusion_admit(projectiles,world->npcs);if(status)goto done;
    status=scene_ai_projectile_checkpoint_direct_admit(projectiles,world->npcs);if(status)goto done;
    status=scene_ai_projectile_checkpoint_delayed_admit(projectiles,world->npcs);if(status)goto done;
    status=scene_ai_projectile_checkpoint_secondary_admit(projectiles,world->npcs);if(status)goto done;
    status=scene_robot_death_checkpoint_admit(projectiles,world->npcs);if(status)goto done;
    status=scene_ai_melee_checkpoint_admit(projectiles,world->npcs,seats,generated,&player,
        turret_player!=NULL||(vehicle_present&&vehicle.value.vehicle.player_occupied),
        environment->spread_next,turret_player);if(status)goto done;
    status=scene_turret_heap_checkpoint_admit(projectiles,world->npcs,turrets,&player);if(status)goto done;
    status=scene_ai_projectile_checkpoint_fusion_admit(projectiles,&player.player.player);if(status)goto done;
    status=scene_ai_projectile_checkpoint_flame_admit(projectiles,&player.player.player);if(status)goto done;
    status=scene_player_rocket_checkpoint_admit(projectiles,world->npcs,passive,turrets,&player,
        environment->form_next.active||environment->cutscene_next.active||turret_player!=NULL||
        (vehicle_present&&vehicle.value.vehicle.player_occupied));if(status)goto done;
    status=scene_ai_projectile_checkpoint_holster_admit(projectiles,!!(player.modes.flags&RF_WEAPON_MODES_PLAYER_HOLSTERED));if(status)goto done;
    phase="continuous_charge";
    if(cost>budget||sizeof(charge)>budget-cost){status=RF_RANGE;goto done;}
    status=scene_continuous_charge_checkpoint_prepare(stream,v->data,v->bytes,&player.player.player,&charge);if(status)goto done;
    cost+=(uint32_t)sizeof(charge);
    if((player.modes.flags&RF_WEAPON_MODES_PLAYER_HOLSTERED) &&
       (charge.next.episode_weapon>=0 || charge.next.current==2 || charge.next.pending==2)){status=RF_FORMAT;goto done;}
    status=scene_ai_projectile_checkpoint_burning_admit(projectiles,world->npcs);if(status)goto done;
    phase="remote";v=envelope.sections+RF_WORLD_REMOTE-1;
    status=scene_remote_checkpoint_world_preflight(stream,v->data,v->bytes,world);if(status)goto done;
    status=scene_remote_checkpoint_holster_admit(!!(player.modes.flags&RF_WEAPON_MODES_PLAYER_HOLSTERED));if(status)goto done;
    phase="validate";
    status=scene_masako_checkpoint_admit(&masako,world->npcs,passive);if(status)goto done;
    status=scene_medic_flight_player_checkpoint_admit(world->npcs,&player);if(status)goto done;
    {int outside=scene_npc_reload_incoming_vehicle(vehicle_present?&vehicle:NULL,seats,0,turret_player!=NULL);
     if(outside&&outside!=RF_NOT_FOUND){status=outside;goto done;}
     status=scene_blind_composed_prepare(world->npcs,seats,&player,outside==RF_NOT_FOUND);if(status)goto done;
     status=scene_tactical_composed_prepare(world->npcs,seats,&player,outside==RF_NOT_FOUND);if(status)goto done;
     status=scene_crouch_composed_prepare(world->npcs,seats,&player);if(status)goto done;
     if(blind_potential&&!reload_transaction){
         for(i=0;i<world->npcs->count;i++)reload_transaction|=!!world->npcs->entries[i].blind_target;
         if(reload_transaction){status=scene_npc_reload_player_admit(stream,&reload_player);if(status)goto done;}
     }}
    status=scene_npc_checkpoint_restore_validate(world->npcs);if(status)goto done;
    status=scene_mover_checkpoint_validate(&campaign_group_runtime,&campaign_group_registration,campaign_controller_views,&campaign_movers,&world->movers);if(status)goto done;
    status=scene_passive_vehicle_checkpoint_validate(passive);if(status)goto done;
    status=scene_turret_checkpoint_validate(turrets);if(status)goto done;
    status=scene_turret_attack_checkpoint_admit(world->npcs,turrets);if(status)goto done;
    status=scene_npc_seat_checkpoint_admit(seats,world->npcs);if(status)goto done;
    phase="secondary_clearance";
    status=scene_secondary_vehicle_restore_admit(stream,world,&player,vehicle_present?&vehicle:NULL,passive,
        secondary,scene_vehicle_physics_checkpoint_active_hidden(vehicle_physics));if(status)goto done;
    phase="passive_seat";
    status=scene_passive_jeep_seat_restore_admit(stream,world,&player,vehicle_present?&vehicle:NULL,
        seats,vehicle_switch,passive,scene_vehicle_physics_checkpoint_active_hidden(vehicle_physics));if(status)goto done;
    status=scene_passive_jeep_seat_restore_audit(seats,vehicle_switch,passive);if(status)goto done;
    phase="turret_player";status=scene_turret_player_checkpoint_admit(turret_player,turrets,&player,0);if(status)goto done;
    phase="gunner";if(cost>budget){status=RF_RANGE;goto done;}
#ifdef RF_IMAGE_XBOX_NATIVE
    status=scene_jeep_gunner_checkpoint_prepare_accounted(stream,seats,vehicle_present?&vehicle:NULL,&player,budget-cost,&gunner,&world_memory);
#else
    status=scene_jeep_gunner_checkpoint_prepare(stream,seats,vehicle_present?&vehicle:NULL,&player,budget-cost,&gunner);
#endif
    if(status)goto done;cost+=scene_jeep_gunner_checkpoint_bytes(gunner);
    status=scene_turret_generated_world_admit(generated,world->npcs,&player,
        vehicle_present?&vehicle:NULL,(uint32_t)campaign_authored_vehicle_uid,passive,turrets);if(status)goto done;
    status=scene_turret_generated_attack_admit(world->npcs,generated);if(status)goto done;
    status=scene_clutter_checkpoint_targets_valid(world->clutter);if(status)goto done;
    status=campaign_group_control_children_validate(world->controls);if(status)goto done;
    status=scene_event_checkpoint_validate(&events->events);if(status)goto done;
    status=scene_world_mission_validate(mission);if(status)goto done;
    status=scene_campaign_history_checkpoint_rebind_validate(bindings);if(status)goto done;
    status=scene_campaign_history_checkpoint_capek_admit(history,bindings,world->npcs);if(status)goto done;
    status=scene_world_environment_validate(environment);if(status)goto done;
    status=scene_continuous_charge_checkpoint_validate(&charge);if(status)goto done;
    status=scene_npc_airborne_restore_begin(world->npcs,&airborne_receipt);if(status)goto done;
    if(reload_transaction||crouch_transaction||return_potential||speech_potential){
        if(reload_transaction){
            phase="reload_two_sided_on_foot";
            status=scene_npc_reload_player_seal(stream,&reload_player);if(status)goto done;
            status=scene_npc_reload_incoming_vehicle(vehicle_present?&vehicle:NULL,seats,gunner!=NULL,turret_player!=NULL);if(status)goto done;
        }
        phase="reload_publication_begin";
        if(cost>budget){status=RF_RANGE;goto done;}
        status=scene_blind_composed(world->npcs,seats,&player,0);if(status)goto done;
        status=scene_tactical_composed(world->npcs,seats,&player,0);if(status)goto done;
        status=scene_crouch_composed(world->npcs,seats,&player);if(status)goto done;
#ifdef RF_IMAGE_XBOX_NATIVE
        status=scene_npc_reload_publication_begin_storage_accounted(world->npcs,budget-cost,&reload_receipt,&world_memory,world_stage_storage);
#else
        status=scene_npc_reload_publication_begin(world->npcs,budget-cost,&reload_receipt);
#endif
        if(status)goto done;
        cost+=scene_npc_reload_publication_bytes(reload_receipt);
        if(speech_potential){
            phase="speech_late_begin";
            status=scene_npc_speech_late_begin(&speech_late,world->npcs,
                reload_receipt,speech_potential);if(status)goto done;
        }
        /* Tactical composed preparation authenticates return rows. Select only after potential Blind ownership resolves, while preserving reload early-close exclusions. */
        combined_late=return_contract_ready&&return_potential&&!reload_transaction;
        if(crouch_transaction&&(!reload_transaction||speech_potential)&&!combined_late){
            phase="crouch_late_begin";
#ifdef RF_IMAGE_XBOX_NATIVE
            status=scene_crouch_late_begin_accounted(world->npcs,reload_receipt,budget-cost,&crouch_late,&world_memory);
#else
            status=scene_crouch_late_begin(world->npcs,reload_receipt,budget-cost,&crouch_late);
#endif
            if(status)goto done;
            cost+=crouch_late->bytes;
        }
    }
    if(return_potential){phase="return_on_foot_precommit";
        if(stream->driller_runtime||stream->driller||scene_vehicle_switch_has_parked()){
            status=RF_NOT_FOUND;goto done;}
        status=scene_npc_reload_player_seal(stream,&return_on_foot);if(status)goto done;}
    scene_medic_flight_restore_scope(1);flight_locked=1;
    scene_continuous_charge_restore_scope(1);charge.locked=1;
    /* Parked publication has no callbacks or mutation on rejection. Admit it
     * before the assignment-only world transaction below. */
    if(vehicle_present && !vehicle.value.vehicle.player_occupied){phase="vehicle_publish";stream->vehicle_record=vehicle;
        status=scene_vehicle_checkpoint_publish(stream);if(status)goto done;}
    /* Publish validated world owners before restoring the occupied vehicle:
     * driver possession needs the saved player body and health in place. */
    scene_alarm_siren_stop_voices();
    scene_npc_loop_audio_close();scene_turret_audio_close();scene_vehicle_loop_audio_close();
    scene_grenade_empty_input_reset();scene_machine_pistol_audio_close();scene_hmg_audio_close();scene_rifle_audio_close();scene_weapon_loop_audio_close();
    scene_npc_seat_checkpoint_detach(seats);
    scene_jeep_gunner_checkpoint_host_assign(stream,gunner);
    scene_mover_checkpoint_assign(&campaign_group_runtime,&campaign_movers,&world->movers);
    scene_passive_vehicle_checkpoint_assign(passive);
    scene_clutter_checkpoint_assign(world->clutter);
    if(airborne_receipt.active)phase="airborne_assignment_identity";
    status=scene_npc_airborne_restore_identity(world->npcs,&airborne_receipt,1);if(status)goto done;
    if(reload_transaction||crouch_transaction||return_potential||speech_potential){
        phase="reload_publication_seal";
        status=scene_event_checkpoint_validate(&events->events);if(status)goto done;
        status=scene_alarm_runner_event_join(world->npcs,events);if(status)goto done;
        status=scene_waypoint_event_join(world->npcs,events);if(status)goto done;
        if(reload_transaction){
            status=scene_npc_reload_player_seal(stream,&reload_player);if(status)goto done;
            status=scene_npc_reload_incoming_vehicle(vehicle_present?&vehicle:NULL,seats,gunner!=NULL,turret_player!=NULL);if(status)goto done;
        }
        status=scene_blind_composed(world->npcs,seats,&player,0);if(status)goto done;
        status=scene_tactical_composed(world->npcs,seats,&player,0);if(status)goto done;
        status=scene_crouch_composed(world->npcs,seats,&player);if(status)goto done;
        status=scene_npc_reload_publication_seal(world->npcs,reload_receipt);if(status)goto done;
        /* Actual native unmount can fail while leaving mounted set. Record
         * the attempt before calling; neither success nor failure retries it.
         * Fixture/portable paths cross the same logical publication barrier. */
        if(reload_transaction&&!speech_potential){
            phase="reload_storage_close";storage_close_attempted=1;
#ifdef RF_IMAGE_XBOX_NATIVE
            status=scene_world_storage_close(&storage);if(status)goto done;
#endif
        }
    }
    if(combined_late){
        phase="return_late_prepare";
        /* Snapshot cumulative outside heap BEFORE wrapper, driver and payload allocation. Those three requests join the same ledger through return_memory; pure planning adds their charges to this immutable snapshot. Reserve the full stack separately once. Accounting alone does not establish compiled stack fit. */
#ifdef RF_IMAGE_XBOX_NATIVE
        if(early_memory.stack_bytes!=return_stack_reservation||
           early_memory.budget!=budget){status=RF_RANGE;goto done;}
        return_existing_bytes=early_memory.admitted_bytes;
#endif
        if(reload_transaction||blind_potential||storage_close_attempted||!return_contract_ready||
           !return_existing_bytes||!reload_receipt||crouch_late||
           world->npcs->pursuit_return_joined!=1){status=RF_NOT_FOUND;goto done;}
        /* Publication begin authenticated each candidate child's allocation
         * size and candidate matrix spans; the immediately preceding seal
         * revalidated those identities. Recover that same main-block extent,
         * not aggregate stage->bytes. This is span metadata, not another heap
         * charge or allocation ledger. No demand or callback intervenes. */
        {uint64_t child_bytes=0,stage_storage;
         for(i=0;i<world->npcs->count;i++){
             const scene_npc_checkpoint_restore_entry *e=world->npcs->entries+i;
             if(e->medic_owner_snapshot&&e->medic_child.child)
                 child_bytes+=e->medic_child.allocated_bytes;
         }
         if(child_bytes>world->npcs->bytes){status=RF_FORMAT;goto done;}
         stage_storage=world->npcs->bytes-child_bytes;
         if(stage_storage>UINT32_MAX||stage_storage<sizeof(*world->npcs)+
            (uint64_t)world->npcs->count*sizeof(*world->npcs->entries)){
             status=RF_FORMAT;goto done;}
         return_stage_bytes=(uint32_t)stage_storage;}
#ifdef RF_IMAGE_XBOX_NATIVE
        status=scene_pursuit_return_allocator_bind_accounted(&return_allocator,&return_memory);
#else
        status=scene_pursuit_return_allocator_bind(&return_allocator);
#endif
        if(status)goto done;
        return_gate.generation=1;return_gate.open=1; /* Fresh pinned local gate. */
        status=scene_return_barriers_create(stream,&player,world->npcs,reload_receipt,
            return_stage_bytes,scene_npc_reload_publication_bytes(reload_receipt),
            gunner!=NULL,turret_player!=NULL,return_existing_bytes,
            return_stack_reservation,budget,&return_allocator,&return_gate,
            scene_return_resource_binding_rebase,scene_return_resource_binding_seal,
            &return_binding,&return_late);if(status)goto done;
        status=scene_return_resource_binding_initialize(&return_binding,sizeof(return_binding),
            return_late,reload_receipt,return_stage_bytes,
            scene_npc_reload_publication_bytes(reload_receipt),
            &late_resource_state,sizeof(late_resource_state));if(status)goto done;
        if(return_late->driver->total_bytes>cost)cost=return_late->driver->total_bytes;
        phase="return_late_arm";
        status=scene_return_barriers_arm(return_late);if(status)goto done;
    }
    scene_npc_checkpoint_restore_assign(world->npcs);airborne_receipt.assigned=airborne_receipt.active;
    /* The early-close lane never performs this fallible step. Combined return and independent crouch loads retain native storage until all occupied-vehicle and other world publication succeeds. */
    if(return_late){phase="return_assigned_projection";
        status=scene_return_barriers_capture(return_late);if(status)goto done;}
    else if(crouch_late){phase="crouch_assigned_projection";
        status=scene_crouch_late_capture(world->npcs,crouch_late);if(status)goto done;}
    /* Capture every participating late target adapter before this barrier. Never admit a late adapter after an early storage close. */
    if(speech_potential){
        phase="speech_assigned_projection";
        if(storage_close_attempted){status=RF_FORMAT;goto done;}
        status=scene_npc_speech_late_capture(&speech_late);if(status)goto done;
    }
    if(return_late){phase="return_resource_rebase";
        status=scene_return_barriers_rebase(return_late);if(status)goto done;}
    else if(crouch_late||speech_potential){phase="late_resource_rebase";
        if(storage_close_attempted){status=RF_FORMAT;goto done;}
        status=scene_npc_late_resources(world->npcs,reload_receipt,&late_resource_state,1);
        if(status)goto done;}
    scene_corpse_lifetime_checkpoint_assign(corpses);
    scene_turret_checkpoint_assign(turrets);
    scene_turret_retirement_checkpoint_assign_deferred(generated);
    scene_turret_generated_activation_reset();
    scene_npc_seat_checkpoint_assign(seats);
    scene_turret_death_effects_reset_pending();
    scene_event_checkpoint_assign(&events->events);scene_world_mission_assign_owners(mission);
    campaign_group_control_children_assign(world->controls);
    if(vehicle_present){scene_world_vehicle_route_assign(&vehicle_route);campaign_vehicle_mode_publish(stream);}
    /* History keys and the new suppression lane remain coupled until final
     * storage success. Intervening assignments use live/candidate owners,
     * not campaign history or its persistence-slot rebinding. */
    scene_world_environment_assign(environment);
    scene_remote_checkpoint_publish();
    scene_actor_body.state=player.body;rf_scene_actor_room_state=player.room;
    memcpy(actor_look.state.body_angles,player.player.body_angles,12);memcpy(actor_look.state.eye_angles,player.player.eye_angles,12);
    memset(actor_look.state.command,0,12);memset(actor_look.state.angular_velocity,0,12);actor_look.state.pending_pitch=actor_look.state.pending_yaw=0;
    memcpy(campaign_position,player.player.position,12);memcpy(campaign_orientation,player.eye,36);
    campaign_player_inventory=player.player.player.inventory;campaign_player_damage.state.effects.health=player.player.player.health;
    campaign_player_damage.state.effects.armor=player.player.player.armor;campaign_select_primary(selected);
    campaign_explicit_unarmed=player.player.player.weapon==UINT32_MAX;
    scene_remote_checkpoint_world_selection();
    scene_weapon_modes_checkpoint_assign(stream,&player.modes);
    scene_ai_projectile_checkpoint_assign(stream,projectiles);
    campaign_player_form=campaign_player_form_carry=environment->form_next;
    memset(rf_scene_player_form,0,sizeof(rf_scene_player_form));
    if(campaign_player_form.active){
        campaign_player_damage.state.effects.class_armor=0;
        campaign_player_view.flags_810&=~0x40u;
        campaign_player_view.flags_7d0|=0x10000u;
        campaign_player_damage.state.effects.flags_810=campaign_player_view.flags_810;
        rf_scene_player_form[0]=1;rf_scene_player_form[1]=campaign_player_form.variant;
        rf_scene_player_form[2]=campaign_player_form.compromised;
        rf_scene_player_form[5]=(uint32_t)campaign_extra_ids[3];
        memcpy(rf_scene_player_form+6,&campaign_player_damage.state.effects.armor,4);
    }
    campaign_ammo_publish();
    if(gunner)scene_jeep_gunner_checkpoint_player_assign(stream,gunner);
    else if(vehicle_present && vehicle.value.vehicle.player_occupied){
        phase="vehicle_publish";stream->vehicle_record=vehicle;stream->player_checkpoint_value=player.player;
        status=scene_vehicle_checkpoint_publish(stream);if(status)goto done;
    }
    scene_vehicle_physics_checkpoint_assign(stream,vehicle_physics);
    scene_vehicle_switch_checkpoint_assign(stream,vehicle_switch);
    scene_secondary_vehicle_checkpoint_assign(stream,secondary);
    scene_vehicle_allegiance_assign(allegiance);
    scene_vehicle_attack_assign(vehicle_attack);scene_vehicle_ai_assign(vehicle_ai);
    scene_vehicle_headlamp_assign(stream);
    scene_vehicle_loadout_refresh(stream);
    scene_masako_checkpoint_assign(&masako);
    campaign_support_handle=player.support.handle;memcpy(campaign_support_velocity,player.support_velocity,12);
    campaign_player_support_epoch=0; /* No pre-load chassis interval may authorize a new capture. */
    rf_scene_actor_landing[1]=1;rf_scene_actor_landing[2]=0;
    scene_actor_body.state.flags&=~(0x200000u|0x400000u);
    if(campaign_support_handle)scene_actor_body.state.flags|=0x400000u;
    if(campaign_support_handle)printf("WORLD_PLAYER_SUPPORT_RESTORED uid%u handle%u velocity%.9g,%.9g,%.9g\n",
        environment->support_uid,campaign_support_handle,(double)campaign_support_velocity[0],
        (double)campaign_support_velocity[1],(double)campaign_support_velocity[2]);
    scene_turret_player_checkpoint_assign(turret_player);
    scene_player_impact_relocated();
    campaign_switch_ambient_restore();
    if(return_late){phase="return_on_foot_final";
        if(stream->driller_runtime||stream->driller||scene_vehicle_switch_has_parked()){
            status=RF_NOT_FOUND;goto done;}
        status=scene_npc_reload_player_seal(stream,&return_on_foot);if(status)goto done;}
    if(speech_potential){
        phase="speech_late_seal";
        if(storage_close_attempted){status=RF_FORMAT;goto done;}
        status=scene_npc_speech_late_seal(&speech_late);if(status)goto done;
    }
    if(return_late){phase="return_late_seal";
        status=scene_return_barriers_seal(return_late);if(status)goto done;}
    else if(crouch_late){phase="crouch_late_seal";
        status=scene_crouch_late_seal(world->npcs,crouch_late);if(status)goto done;}
    /* Seal every participating late target adapter before the same final-close barrier. */
    if(!return_late&&(crouch_late||speech_potential)){phase="late_resource_seal";
        status=scene_npc_late_resources(world->npcs,reload_receipt,&late_resource_state,0);
        if(status)goto done;}
    printf("WORLD_SNAPSHOT_LOADED bytes%u staged%u npc%u events%u added_motion%u health%.9g armor%.9g weapon%u\n",bytes,cost,world->npcs->count,events->events.count,added,
        (double)player.player.player.health,(double)player.player.player.armor,player.player.player.weapon);
    scene_world_startup_snapshot_consumed=1;
    projectile_audio_resume=projectiles!=NULL;world_published=1;
 done:
    if(speech_potential&&!status&&world_published){
        phase="speech_storage_close";
        if(storage_close_attempted)status=RF_FORMAT;
        else status=scene_npc_speech_late_close_begin(&speech_late,late_resource_state);
    }
    if(return_late&&!status&&world_published){
        phase="return_storage_close";
        if(storage_close_attempted)status=RF_FORMAT;
        else status=scene_return_barriers_close_begin(return_late);
    }
    /* Includes create/context/arm/capture/seal failures and absent driver.
     * No late validation or resource callback may follow this boundary. */
    if(speech_potential&&(status||!world_published))speech_late.state=SCENE_SPEECH_LATE_FAILED;
    return_gate.open=0;
    if(status){const unsigned char *name=(const unsigned char *)phase;uint32_t hash=2166136261u;
        while(*name)hash=(hash^*name++)*16777619u;
        rf_scene_world_load_reject[0]=hash;rf_scene_world_load_reject[1]=(uint32_t)status;
        rf_scene_world_load_reject[2]=cost;}
    rf_scene_world_checkpoint_state[0]=(uint32_t)status;rf_scene_world_checkpoint_state[1]=status?0:bytes;
    rf_scene_world_checkpoint_state[2]=status?0:cost;
#ifdef RF_IMAGE_XBOX_NATIVE
    if(!storage_close_attempted){int closed;
        storage_close_attempted=1;
        closed=scene_world_storage_close(&storage);if(!status)status=closed;
        rf_scene_world_checkpoint_state[0]=(uint32_t)status;}
#endif
    if((reload_transaction||return_late||speech_potential)&&status){rf_scene_world_checkpoint_state[0]=(uint32_t)status;
        rf_scene_world_checkpoint_state[1]=rf_scene_world_checkpoint_state[2]=0;}
    if(status&&airborne_receipt.assigned&&world)
        (void)scene_npc_airborne_restore_abort(world->npcs,&airborne_receipt);
    /* Transient gasp/scope/melee belongs to the replaced timeline only after the load
     * succeeds. Preparation, rejection and storage failure preserve it. */
    if(!status && world_published){scene_campaign_history_checkpoint_assign_rebound(history,bindings);scene_airlock_restore_assign(&mission->airlock);scene_npc_checkpoint_restore_finish(world->npcs);scene_player_outside_audio_forget();scene_scope_owner_reset(!!player_input.alt_fire);scene_player_rocket_lock_reset(stream,!!player_input.alt_fire);scene_player_riot_reset();scene_ai_drone_smash_reset();scene_ai_drone_secondary_reset();scene_ai_tankbot_secondary_reset();scene_ai_drone_missile_reset();scene_ai_laser_reset();scene_ai_tribeam_reset();scene_ai_cane_reset();scene_ai_spit_reset();scene_ai_sonar_reset();}
    if(!status&&world_published)scene_npc_use_dialogue_stop();
    if(!status&&world_published){scene_ai_projectile_checkpoint_flame_finish(projectiles);
        scene_ai_projectile_checkpoint_direct_finish(projectiles);
        scene_ai_projectile_checkpoint_delayed_finish(projectiles);
        scene_ai_projectile_checkpoint_secondary_finish(projectiles);
        scene_ai_melee_checkpoint_finish(projectiles);
        scene_turret_heap_checkpoint_finish(projectiles);
        scene_robot_death_checkpoint_finish(projectiles);
        scene_player_rocket_checkpoint_finish(stream,projectiles);}
    scene_continuous_charge_checkpoint_finish(&charge,!status&&world_published);
    if(flight_locked){
        /* Ordinary pain/burn assignment may write this shared RNG before close.
         * Preserve the new old-flight alias on failure, without changing the
         * established fresh-world failure policy for those other controllers. */
        if(status&&world)scene_npc_checkpoint_flight_abort(world->npcs);
        scene_medic_flight_restore_scope(0);
    }
    /* Voice reconstruction follows ALL fallible publication and storage
     * close work. Failed/absent staged loads never start a new flight loop. */
    if(!status && projectile_audio_resume)scene_projectile_flight_audio_resume(stream);
    if(status)printf("WORLD_SNAPSHOT_LOAD_REJECT phase%s status%d\n",phase,status);
    /* Cleanup only: no stage/resource callbacks. Active operations have all
     * unwound; dispose the sole driver owner before borrowed stages/receipt. */
    (void)scene_return_barriers_dispose(&return_late);
    scene_authored_checkpoint_stage_discard(&terrain_stage);
    scene_authored_collection_stage_discard(&collection_stage);
    scene_remote_checkpoint_discard();scene_world_event_close_storage(&events,world_stage_storage);
    /* Sky is independent of the earlier environment assignment: failed native
     * storage close must preserve its pre-load timeline. No fallible work follows. */
    if(!status&&world_published)scene_world_environment_finish(environment);
    scene_world_environment_close_storage(&environment,world_stage_storage);
    scene_campaign_history_checkpoint_rebind_close_storage(&bindings,world_stage_storage);scene_campaign_history_checkpoint_close_storage(&history,world_stage_storage);
    scene_ai_projectile_checkpoint_discard(&projectiles);
    scene_jeep_gunner_checkpoint_close(&gunner);
    scene_corpse_lifetime_checkpoint_close(&corpses);
    scene_turret_player_checkpoint_close(&turret_player);
    scene_vehicle_allegiance_close(&allegiance);
    scene_vehicle_attack_stage_close(&vehicle_attack);scene_vehicle_ai_stage_close(&vehicle_ai);
    scene_secondary_vehicle_checkpoint_close(&secondary);
    scene_vehicle_physics_checkpoint_close(&vehicle_physics);
    scene_vehicle_switch_checkpoint_close(&vehicle_switch);
    scene_passive_vehicle_checkpoint_close(&passive);scene_turret_checkpoint_close(&turrets);scene_turret_generated_checkpoint_close(&generated);scene_npc_seat_checkpoint_close(&seats);
    scene_crouch_late_close(&crouch_late);
    scene_npc_reload_publication_close_storage(&reload_receipt,world_stage_storage);
    /* Terminal frees end the cumulative ledger scope. Keep reservations charged through cleanup; do not add fallible accounting callbacks after storage close. Failed malloc attempts remain charged. */
    scene_world_mission_close_storage(&mission,world_stage_storage);scene_world_restore_discard_storage(&world,world_stage_storage);free(rows);free(buffer);
#ifdef RF_IMAGE_XBOX_NATIVE
    scene_world_stage_arena_dispose(&stage_arena);
#endif
    /* No saved history exists in this codec. This final return boundary is
     * after publication, native storage close, rollback decisions and staged
     * owner release. A failed/absent load cannot discard the prior ring. */
    if(!status&&world_published){scene_message_history_reset();scene_weapon_impact_visual_reset(stream,1);}
    if(!status&&world_published)scene_script_explode_effects_reset(stream);
    if(!status&&world_published){scene_play_vclip_reset();++rf_scene_play_vclip[21];}
    if(!status&&world_published)scene_nano_visual_reset();
    if(!status&&world_published)scene_cane_warmup_reset();
    if(!status&&world_published)scene_nano_hit_reset();
    return status;
}
#endif
