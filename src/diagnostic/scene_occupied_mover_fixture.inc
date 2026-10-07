#ifndef RF_SCENE_OCCUPIED_MOVER_FIXTURE_INC
#define RF_SCENE_OCCUPIED_MOVER_FIXTURE_INC
/* Bounded Xbox fixture, opt-in D:\campaign-occupied-movers.bin:
 * u32 magic RFOM, phase1(source)/2(fresh load), frames180/80, reserved0.
 * Include after scene_group_control_fixture.inc. Call at the end of the
 * campaign_spawn NPC step, after campaign_glare_rooms_pass(frame), so all
 * poses belong to the same completed world step. The sole stimulus is one
 * ordinary current-center trigger contact/Use at source45. Real player Use
 * inputs come from RFI6; no ownership, controller, child or rider writes here.
 * Source rider seeding uses the existing explicit roof fixture at40.
 *
 * Summary: version,phase,rows,start calls,status,last frame,enabled,capacity.
 * Main row128: frame,status,phase,start; keyUID/handle/kind/current/next/flags
 * at4; phase/speed/distance10; pending13/committed16/velocity19; Fighter UID22,
 * handle23/attached24/flags25/health26/armor27/XYZ28/basis31; Jeep UID40,
 * profile41/handle42/active43/driver44/occupant45/player46/player-host47/
 * linked48/registered49/boards50/exits51/role52/setup53; trigger count54/
 * flags55/handle56/registered57; NPC UID58/handle59/support60/mode61/health62/
 * flags63/XYZ64/velocity67/support-velocity70/registered73; clutter publication
 * masks74; common step counters77..84; trigger XYZ85/basis88; player XYZ97;
 * Jeep health100/armor101/XYZ102/basis105; session host114/driver115/player
 * flags116/alive117/Fighter registry118; NPC support lifecycle120..127.
 * Child rows are unmodified rf_scene_group_control_child_read64 outputs.
 */
uint32_t rf_scene_occupied_mover_fixture[8];
uint32_t rf_scene_occupied_mover_rows[9][128];
uint32_t rf_scene_occupied_mover_children[9][4][64];
static uint32_t scene_occupied_mover_config[4],scene_occupied_mover_checked;

static int scene_occupied_mover_fixture_tick(scene_stream *stream,uint32_t frame,int32_t now)
{
    static const uint32_t uids[4]={4527,4528,4529,4530};
    static const uint32_t source_frames[9]={40,45,89,90,119,120,149,150,178};
    /* World load occurs after frame0 callbacks; first sample is one tick later. */
    static const uint32_t load_frames[9]={1,2,29,30,31,59,60,61,78};
    rf_runtime_trigger *trigger=NULL;scene_passive_vehicle *host=NULL;
    rf_group_runtime_entry *controller=NULL;campaign_npc_body *npc=NULL;
    scene_driller_runtime *vehicle;uint32_t phase,slot,i,parent=UINT32_MAX,*row;
    int status=RF_OK;
    if(!scene_occupied_mover_checked){
        FILE *file;scene_occupied_mover_checked=1;
#ifdef RF_IMAGE_XBOX_NATIVE
        file=fopen("D:\\campaign-occupied-movers.bin","rb");
#else
        file=NULL;
#endif
        if(!file)return RF_OK;
        i=fread(scene_occupied_mover_config,1,sizeof(scene_occupied_mover_config),file);
        status=i!=sizeof(scene_occupied_mover_config)||fgetc(file)!=EOF?RF_FORMAT:RF_OK;
        fclose(file);
        if(!status&&(scene_occupied_mover_config[0]!=UINT32_C(0x4d4f4652)||
            scene_occupied_mover_config[1]<1||scene_occupied_mover_config[1]>2||
            scene_occupied_mover_config[2]!=(scene_occupied_mover_config[1]==1?180u:80u)||
            scene_occupied_mover_config[3]))status=RF_FORMAT;
        rf_scene_occupied_mover_fixture[0]=1;
        rf_scene_occupied_mover_fixture[1]=scene_occupied_mover_config[1];
        rf_scene_occupied_mover_fixture[6]=1;rf_scene_occupied_mover_fixture[7]=9;
        if(status)goto failed;
    }
    if(!rf_scene_occupied_mover_fixture[6])return RF_OK;
    phase=scene_occupied_mover_config[1];slot=rf_scene_occupied_mover_fixture[2];
    if(slot>=9||frame!=(phase==1?source_frames[slot]:load_frames[slot]))return RF_OK;
    row=rf_scene_occupied_mover_rows[slot];row[0]=frame;row[2]=phase;
    if(!stream||!(vehicle=stream->driller_runtime)){status=RF_NOT_FOUND;goto row_failed;}
    for(i=0;i<campaign_triggers.count;i++)if(campaign_triggers.items[i].authored->record.uid==4530)trigger=campaign_triggers.items+i;
    for(i=0;i<campaign_passive_vehicle_count;i++)if(campaign_passive_vehicles[i].uid==4717)host=campaign_passive_vehicles+i;
    for(i=0;i<campaign_group_runtime.count;i++)if(campaign_group_runtime.items[i].source&&
        campaign_group_runtime.items[i].source->record.key_count&&
        campaign_group_runtime.items[i].source->keys[0].uid==4543)controller=campaign_group_runtime.items+i;
    for(i=0;i<campaign_npc_body_count;i++)if(i<campaign_seeds.records.count&&
        campaign_seeds.records.items[i].record.uid==4716)npc=campaign_npc_bodies+i;
    if(!trigger||!host||!controller||!npc||campaign_triggers.count!=1||
        campaign_group_control_child_count!=4||campaign_authored_vehicle_uid!=7629){status=RF_NOT_FOUND;goto row_failed;}
    for(i=0;i<campaign_group_registration.count;i++)if(campaign_group_registration.controllers[i].runtime==controller)
        parent=campaign_group_registration.controllers[i].handle;
    if(parent==UINT32_MAX){status=RF_NOT_FOUND;goto row_failed;}
    if(phase==1&&frame==45){
        float positions[3][3];for(i=0;i<3;i++)memcpy(positions[i],trigger->volume.center,12);
        ++rf_scene_occupied_mover_fixture[3];row[3]=1;
        status=campaign_actor_trigger_contacts(&campaign_player_view,positions,now,frame,&stream->particles,1,0);
        if(status)goto row_failed;
    }
    row[4]=4543;row[5]=parent;row[6]=controller->kind;
    row[7]=(uint32_t)controller->translation.motion.current_key;
    row[8]=(uint32_t)controller->translation.motion.next_key;row[9]=controller->translation.motion.flags;
    memcpy(row+10,&controller->translation.motion.phase,4);memcpy(row+11,&controller->translation.speed,4);
    memcpy(row+12,&controller->translation.distance,4);memcpy(row+13,controller->translation.pending,12);
    memcpy(row+16,controller->translation.position,12);memcpy(row+19,controller->translation.velocity,12);
    row[22]=host->uid;row[23]=host->handle;row[24]=host->attached;row[25]=host->damage.object_flags;
    memcpy(row+26,&host->damage.state.effects.health,4);memcpy(row+27,&host->damage.state.effects.armor,4);
    memcpy(row+28,host->pose.position,12);memcpy(row+31,host->pose.input_matrix,36);
    row[40]=(uint32_t)campaign_authored_vehicle_uid;row[41]=rf_scene_vehicle_enabled;
    row[42]=vehicle->entry.registration.handle;row[43]=vehicle->entry.session.active;
    row[44]=vehicle->entry.host.driver;row[45]=(uint32_t)vehicle->entry.occupant;
    row[46]=vehicle->entry.player.handle;row[47]=vehicle->entry.player.host;
    row[48]=(uint32_t)campaign_player_view.linked_handle;
    row[49]=rf_object_registry_lookup(&campaign_registry,row[42])==&vehicle->entry.registration;
    row[50]=rf_scene_vehicle_state[1];row[51]=rf_scene_vehicle_state[2];row[52]=vehicle->jeep_seat.role;
    row[53]=rf_scene_setup_result[0];row[54]=trigger->state.count;row[55]=trigger->state.flags;
    row[56]=trigger->handle;row[57]=rf_object_registry_lookup(&campaign_registry,trigger->handle)==trigger;
    row[58]=4716;row[59]=npc->registration.handle;row[60]=npc->support.handle;
    row[61]=npc->movement_slot<16?(uint32_t)campaign_modes[npc->movement_slot].index:UINT32_MAX;
    memcpy(row+62,&npc->damage.effects.health,4);row[63]=npc->body.state.flags;
    memcpy(row+64,npc->body.state.position,12);memcpy(row+67,npc->body.state.velocity,12);
    memcpy(row+70,npc->support_velocity,12);
    row[73]=rf_object_registry_lookup(&campaign_registry,row[59])==&npc->registration;
    memcpy(row+77,rf_scene_world_controller_steps,32);
    memcpy(row+85,trigger->volume.center,12);memcpy(row+88,trigger->volume.matrix,36);
    memcpy(row+97,scene_actor_body.state.position,12);
    memcpy(row+100,&vehicle->damage.damage.state.effects.health,4);
    memcpy(row+101,&vehicle->damage.damage.state.effects.armor,4);
    memcpy(row+102,vehicle->physics.state.position,12);memcpy(row+105,vehicle->physics.state.orientation,36);
    row[114]=vehicle->entry.session.host;row[115]=vehicle->entry.session.driver;
    row[116]=vehicle->entry.player.control_flags;row[117]=vehicle->entry.host.alive;
    row[118]=rf_object_registry_lookup(&campaign_registry,host->handle)==host;
    memcpy(row+120,rf_scene_npc_support_lifecycle,32);
    for(i=0;i<4;i++){
        uint32_t *child=rf_scene_occupied_mover_children[slot][i];
        int read=rf_scene_group_control_child_read(uids[i],child);if(!status&&read)status=read;
        if(i<3&&!read){
            const rf_clutter_base_owner *owner=campaign_clutter_bodies[child[3]];
            row[74+i]=(!memcmp(owner->state.position,owner->query_position,12)?1u:0u)|
                (!memcmp(owner->state.position,owner->body.state.position,12)?2u:0u)|
                (!memcmp(owner->matrix,owner->body.state.orientation,36)?4u:0u)|
                (!memcmp(owner->matrix,owner->body.state.next_orientation,36)?8u:0u)|
                (campaign_group_control_clutter_pose_valid(child[3])?16u:0u);
        }
    }
row_failed:
    row[1]=(uint32_t)status;++rf_scene_occupied_mover_fixture[2];rf_scene_occupied_mover_fixture[5]=frame;
failed:
    if(status)rf_scene_occupied_mover_fixture[4]=(uint32_t)status;
    return status;
}
#endif
