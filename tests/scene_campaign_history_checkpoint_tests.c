#include "../src/diagnostic/scene.c"
#include "../src/diagnostic/scene_campaign_history_checkpoint.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"campaign history line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static unsigned char wire[SCENE_CAMPAIGN_HISTORY_MAX_BYTES],saved[SCENE_CAMPAIGN_HISTORY_MAX_BYTES];
static scene_campaign_history_checkpoint_stage before;
static campaign_npc_body npc;
static rf_level_owned_entity seed;
int main(void)
{
    unsigned char identity[32]={9},wrong[32]={8};scene_campaign_history_checkpoint_stage *stage=NULL;
    rf_runtime_event events[3]={{0}};rf_level_owned_event authored[3]={{0}};rf_switch_state current_switch;
    uint32_t old_switch,old_event,old_actor,current_actor,bytes=777,row_offset;float position[3]={1,2,3};
    strcpy(campaign_current_level,"L1S2.rfl");rf_object_registry_init(&campaign_registry);
    CHECK(!rf_campaign_pickup_register(&campaign_switch_history,"L1S1.rfl",10,&old_switch));
    CHECK(!rf_event_switch_init(campaign_switch_saved+old_switch,1,8,0,0));
    campaign_switch_saved[old_switch].activations=3;campaign_switch_history.items[old_switch].retired=1;
    CHECK(!rf_campaign_pickup_register(&campaign_event_history,"L1S1.rfl",11,&old_event));
    campaign_event_saved_states[old_event]=(campaign_event_saved){87,0,0,1,-1};campaign_event_history.items[old_event].retired=1;
    CHECK(!rf_campaign_actor_register(&rf_scene_defeated_actors,"L1S1.rfl",100,&old_actor));
    rf_scene_defeated_actors.items[old_actor].retired=1;rf_scene_defeated_actors.vitals[old_actor].valid=1;
    rf_scene_defeated_actors.vitals[old_actor].health=10;rf_scene_defeated_actors.vitals[old_actor].armor=2;
    CHECK(!rf_campaign_actor_drop_emit(&rf_scene_defeated_actors,old_actor,3,0,position));rf_scene_defeated_actors.drops[old_actor].state=2;
    CHECK(!rf_campaign_actor_register(&rf_scene_defeated_actors,"L1S2.rfl",100,&current_actor));
    rf_scene_defeated_actors.vitals[current_actor].valid=1;rf_scene_defeated_actors.vitals[current_actor].health=100;
    npc.persistence_registered=1;npc.persistence_slot=current_actor;npc.registration.view=&npc.view;
    npc.damage.effects.health=37.5f;npc.damage.effects.armor=9;npc.damage.effects.affiliation=2;npc.object_flags=0x4004;
    CHECK(!rf_object_registry_insert(&campaign_registry,&npc.registration,&npc.registration.handle));
    campaign_npc_body_count=1;campaign_npc_bodies=&npc;seed.record.uid=100;campaign_seeds.records.count=1;campaign_seeds.records.items=&seed;
    CHECK(!rf_event_switch_init(&current_switch,0,10,0,0));current_switch.activations=6;
    authored[0].record.uid=20;authored[1].record.uid=21;authored[2].record.uid=22;
    events[0].authored=authored;events[0].state.type=32;events[0].switch_state=&current_switch;
    events[1].authored=authored+1;events[1].state.type=20;
    CHECK(!rf_event_cycle_init(&events[1].cycle,.5f,10,0,0));events[1].cycle.count=9;events[1].cycle.enabled=1;events[1].cycle.deadline=250;
    events[2].authored=authored+2;events[2].state.type=84;events[2].countdown_armed=1;
    campaign_events.items=events;campaign_events.count=3;
    rf_scene_campaign_countdown=(rf_campaign_countdown){42.25f,1,2};
    before.switches=campaign_switch_history;before.events=campaign_event_history;before.actors=rf_scene_defeated_actors;
    memcpy(before.switch_saved,campaign_switch_saved,sizeof(before.switch_saved));memcpy(before.event_saved,campaign_event_saved_states,sizeof(before.event_saved));
    CHECK(!scene_campaign_history_checkpoint_encode(identity,100,wire,sizeof(wire),&bytes));
    CHECK(!memcmp(&before.switches,&campaign_switch_history,sizeof(before.switches))&&!memcmp(&before.events,&campaign_event_history,sizeof(before.events)));
    CHECK(!memcmp(&before.actors,&rf_scene_defeated_actors,sizeof(before.actors)));
    CHECK(!scene_campaign_history_checkpoint_prepare(identity,wire,bytes,sizeof(*stage),&stage));
    CHECK(stage->switches.count==2&&stage->switch_saved[old_switch].activations==3&&stage->switch_saved[1].activations==6);
    CHECK(stage->events.count==3&&stage->event_saved[old_event].fired==1&&stage->event_saved[1].count==9&&stage->event_saved[1].cycle_remaining==150);
    CHECK(stage->event_saved[2].type==84&&stage->event_saved[2].enabled==1&&!stage->event_saved[2].fired);
    CHECK(stage->countdown.remaining==42.25f&&stage->countdown.expiry_pending==1&&stage->countdown.difficulty==2);
    CHECK(stage->actors.vitals[current_actor].health==37.5f&&stage->actors.vitals[current_actor].armor==9&&stage->actors.mission[current_actor].flags==0x4004);
    CHECK(stage->actors.items[old_actor].retired&&stage->actors.drops[old_actor].state==2&&stage->actors.drops[old_actor].quantity==0);
    scene_campaign_history_checkpoint_close(&stage);CHECK(!stage);memcpy(saved,wire,bytes);
    CHECK(scene_campaign_history_checkpoint_prepare(wrong,wire,bytes,sizeof(*stage),&stage)==RF_FORMAT&&!stage);
    wire[bytes-1]^=1;CHECK(scene_campaign_history_checkpoint_prepare(identity,wire,bytes,sizeof(*stage),&stage)==RF_FORMAT&&!stage);memcpy(wire,saved,bytes);
    row_offset=96+scene_history_word(wire+48)*64;memcpy(wire+row_offset+32,wire+row_offset,12);
    scene_history_put(wire+12,scene_history_hash(wire,bytes));
    CHECK(scene_campaign_history_checkpoint_prepare(identity,wire,bytes,sizeof(*stage),&stage)==RF_FORMAT&&!stage);memcpy(wire,saved,bytes);
    CHECK(scene_campaign_history_checkpoint_prepare(identity,wire,bytes,sizeof(*stage)-1,&stage)==RF_RANGE&&!stage);
    CHECK(!memcmp(&before.actors,&rf_scene_defeated_actors,sizeof(before.actors)));
    CHECK(!scene_campaign_history_checkpoint_prepare(identity,wire,bytes,sizeof(*stage),&stage));
    scene_campaign_history_checkpoint_publish(stage);scene_campaign_history_checkpoint_close(&stage);
    CHECK(campaign_switch_history.count==2&&rf_scene_defeated_actors.vitals[current_actor].health==37.5f);
    CHECK(rf_scene_campaign_countdown.remaining==42.25f&&rf_scene_campaign_countdown.expiry_pending==1);
    events[1].cycle.count=0;CHECK(!campaign_event_restore(events+1,1000,campaign_event_saved_states+1));
    CHECK(events[1].cycle.count==9&&events[1].cycle.deadline==1150);
    CHECK(!rf_campaign_actor_drop_emit(&rf_scene_defeated_actors,old_actor,3,20,position)&&rf_scene_defeated_actors.drops[old_actor].state==2&&rf_scene_defeated_actors.drops[old_actor].quantity==0);
    {
        scene_campaign_history_rebind_stage *bindings=NULL;unsigned char swap[56];
        CHECK(!scene_campaign_history_checkpoint_prepare(identity,wire,bytes,sizeof(*stage),&stage));
        /* Incoming all-level history can have another slot order. */
#define SWAP_HISTORY(field) do{memcpy(swap,&stage->actors.field[0],sizeof(stage->actors.field[0]));memcpy(&stage->actors.field[0],&stage->actors.field[1],sizeof(stage->actors.field[0]));memcpy(&stage->actors.field[1],swap,sizeof(stage->actors.field[0]));}while(0)
        SWAP_HISTORY(items);SWAP_HISTORY(vitals);SWAP_HISTORY(mission);SWAP_HISTORY(drops);
#undef SWAP_HISTORY
        CHECK(!scene_campaign_history_checkpoint_rebind_prepare(stage,65536,&bindings));
        CHECK(bindings->entries[0].old_slot==current_actor&&bindings->entries[0].new_slot==0);
        npc.persistence_slot=0;
        CHECK(scene_campaign_history_checkpoint_publish_rebound(stage,bindings)==RF_FORMAT&&rf_scene_defeated_actors.items[0].level==0);
        npc.persistence_slot=current_actor;
        scene_shield_history_count=1;
        CHECK(scene_campaign_history_checkpoint_publish_rebound(stage,bindings)==RF_NOT_FOUND&&rf_scene_defeated_actors.items[0].level==0);
        scene_shield_history_count=0;
        CHECK(!scene_campaign_history_checkpoint_publish_rebound(stage,bindings));
        CHECK(npc.persistence_slot==0&&rf_scene_defeated_actors.items[0].level==1&&rf_scene_defeated_actors.vitals[0].health==37.5f);
        CHECK(rf_scene_defeated_actors.drops[1].state==2&&rf_scene_defeated_actors.drops[1].quantity==0);
        scene_campaign_history_checkpoint_rebind_close(&bindings);CHECK(!bindings);
        authored[0].record.uid=999;
        CHECK(scene_campaign_history_checkpoint_rebind_prepare(stage,65536,&bindings)==RF_NOT_FOUND&&!bindings);
        authored[0].record.uid=20;
        scene_campaign_history_checkpoint_close(&stage);
    }
    {
        rf_campaign_player_state carry={0};scene_machine_pistol_mode_state mode={0};scene_undercover_mode undercover={0};
        uint32_t first_bytes,tail;scene_machine_pistol_mode_state restored={0};
        carry.health=73;carry.armor=19;carry.weapon=13;carry.catalog_hash=0x12345678;
        carry.inventory.owned[13]=carry.inventory.owned[16]=1;
        carry.inventory.loaded[13]=17;carry.inventory.loaded[17]=6;carry.inventory.loaded[16]=9;carry.inventory.reserve[4]=41;
        CHECK(!rf_campaign_player_copy(&campaign_player_import,&carry,carry.catalog_hash));campaign_import_pending=1;
        mode.special=1;undercover.attached=1;
        CHECK(!scene_machine_pistol_carry_capture(&campaign_machine_import_sidecar,&carry,&mode,13,17));
        CHECK(!scene_undercover_carry_capture(&campaign_undercover_import_sidecar,&carry,16,&undercover));
        campaign_machine_export_sidecar=campaign_machine_import_sidecar;campaign_undercover_export_sidecar=campaign_undercover_import_sidecar;
        /* Export advanced after the mode sidecars: preserve these snapshots,
         * so exact-match admission still rejects their application to it. */
        carry.health=61;carry.inventory.loaded[13]=12;
        CHECK(!rf_campaign_player_copy(&campaign_player_export,&carry,carry.catalog_hash));campaign_export_valid=1;
        CHECK(!scene_campaign_history_checkpoint_encode(identity,1000,wire,sizeof(wire),&bytes));
        CHECK(scene_history_word(wire+4)==3&&scene_history_word(wire+72)==63&&scene_history_word(wire+76)==2824);
        first_bytes=bytes;tail=bytes-2824;memcpy(saved,wire,bytes);
        CHECK(!scene_campaign_history_checkpoint_prepare(identity,wire,bytes,sizeof(*stage),&stage));
        CHECK(stage->player_import.health==73&&stage->player_export.health==61&&stage->player_export.inventory.loaded[13]==12);
        CHECK(stage->machine_export.special==1&&stage->machine_export.player.inventory.loaded[17]==6&&stage->undercover_import.attached==1);
        CHECK(!scene_machine_pistol_carry_restore(&stage->machine_import,&stage->player_import,13,17,0,&restored)&&restored.special==1);
        CHECK(!scene_machine_pistol_carry_matches(&stage->machine_export,&stage->player_export,13,17));
        CHECK(scene_undercover_carry_matches(&stage->undercover_import,&stage->player_import,16));
        campaign_import_pending=campaign_export_valid=0;
        memset(&campaign_player_import,0,sizeof(campaign_player_import));memset(&campaign_player_export,0,sizeof(campaign_player_export));
        scene_machine_pistol_carry_reset(&campaign_machine_import_sidecar);scene_machine_pistol_carry_reset(&campaign_machine_export_sidecar);
        scene_undercover_carry_reset(&campaign_undercover_import_sidecar);scene_undercover_carry_reset(&campaign_undercover_export_sidecar);
        scene_campaign_history_checkpoint_publish(stage);scene_campaign_history_checkpoint_close(&stage);
        CHECK(campaign_import_pending&&campaign_export_valid&&campaign_machine_import_sidecar.valid&&campaign_undercover_export_sidecar.attached);
        CHECK(!scene_campaign_history_checkpoint_encode(identity,1000,wire,sizeof(wire),&bytes));
        CHECK(bytes==first_bytes&&!memcmp(wire,saved,bytes));
        /* Valid checksum with malformed ammo or sidecar mode must fail before
         * publication, preserving all live carry and the caller output slot. */
        scene_history_put(wire+tail+192+13*4,UINT32_MAX);scene_history_put(wire+12,scene_history_hash(wire,bytes));
        CHECK(scene_campaign_history_checkpoint_prepare(identity,wire,bytes,sizeof(*stage),&stage)==RF_FORMAT&&!stage);
        CHECK(campaign_player_import.inventory.loaded[13]==17&&campaign_player_export.health==61);
        memcpy(wire,saved,bytes);scene_history_put(wire+tail+928+472,2);scene_history_put(wire+12,scene_history_hash(wire,bytes));
        CHECK(scene_campaign_history_checkpoint_prepare(identity,wire,bytes,sizeof(*stage),&stage)==RF_FORMAT&&!stage);
        CHECK(campaign_machine_import_sidecar.special==1);
        memcpy(wire,saved,bytes);
        campaign_machine_import_sidecar.special=2;first_bytes=bytes;
        CHECK(scene_campaign_history_checkpoint_encode(identity,1000,wire,sizeof(wire),&bytes)==RF_FORMAT&&bytes==first_bytes&&!memcmp(wire,saved,bytes));
        campaign_machine_import_sidecar.special=1;
        /* Version1 has no carry; absent records publish as empty, never retain
         * unrelated process-global carry from before loading. */
        scene_history_put(wire+4,2);scene_history_put(wire+80,0);scene_history_put(wire+84,0);scene_history_put(wire+88,0);
        scene_history_put(wire+12,scene_history_hash(wire,bytes));
        CHECK(!scene_campaign_history_checkpoint_prepare(identity,wire,bytes,sizeof(*stage),&stage));
        CHECK(!stage->countdown.remaining&&!stage->countdown.expiry_pending&&stage->countdown.difficulty==1);
        scene_campaign_history_checkpoint_close(&stage);
        scene_history_put(wire+4,1);scene_history_put(wire+8,tail);scene_history_put(wire+72,0);scene_history_put(wire+76,0);
        scene_history_put(wire+12,scene_history_hash(wire,tail));
        CHECK(!scene_campaign_history_checkpoint_prepare(identity,wire,tail,sizeof(*stage),&stage));
        CHECK(!stage->carry_mask);scene_campaign_history_checkpoint_publish(stage);scene_campaign_history_checkpoint_close(&stage);
        CHECK(!campaign_import_pending&&!campaign_export_valid&&!campaign_machine_import_sidecar.valid&&!campaign_undercover_export_sidecar.valid);
    }
    scene_shield_history_count=1;campaign_ai_saved_modes[0].flags=1;
    CHECK(scene_campaign_history_checkpoint_omissions()==12);
    puts("PASS campaign history snapshot, rebinding, carry/mode binary roundtrip and atomic validation");return 0;
}
