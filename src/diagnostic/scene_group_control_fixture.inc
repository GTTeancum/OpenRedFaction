#ifndef RF_SCENE_GROUP_CONTROL_FIXTURE_INC
#define RF_SCENE_GROUP_CONTROL_FIXTURE_INC
/* Explicit process-contained L20S2 contact fixture. Include after the ordinary
 * campaign_actor_trigger_contacts/campaign_link_effect callbacks, passive
 * owners and scene_group_control_children.inc. Call immediately before the
 * ordinary campaign_trigger_contacts(frame). No player relocation, host input,
 * direct owner-pose writes, checkpoint mutation or synthetic success state.
 *
 * D:\campaign-attached-controls.bin: u32 magic RFCT, phase 1(source)/2(load),
 * final frame count 150/40, reserved0. Absent file means no work. Source calls
 * ordinary contact/Use, then retained Invert17967 to disable trigger4530.
 * Fresh load first observes and tests the saved disabled trigger; the normal
 * linked-trigger callback then enables it before further contact/Use.
 *
 * Summary: version,phase,rows,contacts,disable calls,enable calls,error,last
 * frame,trigger handle,Fighter handle,three clutter handles,key UID,loaded,cap.
 * Each128-word row: frame,operation,use,status,UID,handle,count before/after,
 * flags before/after,object flags before/after,activations before/after,ready
 * before/after,parent handle/flags before/after/kind,phase before/after,
 * distance before/after,pending XYZ,committed XYZ,submitted XYZ,trigger center
 * XYZ/basis,base center XYZ/basis,post center XYZ/basis,Fighter handle/flags/
 * health/position XYZ/basis; clutter health at84,handles87; actor handle90,
 * controller current/next at91/92, velocity93, trigger size96/shape99,
 * cooldown100,deadline101,limit102,link kinds103/105 and values104/106,
 * registered trigger/Fighter107/108,three clutter publication masks109..111,
 * disable-event handle112,setup calls113. Remaining words reserved0.
 * Operations:0 observe,1 initial Use,2 old-center Use,3 current no-Use,
 * 4 current Use,5 Invert disable,6 disabled current Use,7 linked enable.
 * Child snapshots are the existing read-only64-word rows in UID order.
 */
uint32_t rf_scene_group_control_fixture[16];
uint32_t rf_scene_group_control_fixture_rows[8][128];
uint32_t rf_scene_group_control_fixture_children[8][4][64];
static uint32_t scene_group_control_fixture_config[4],scene_group_control_fixture_checked;

static int scene_group_control_fixture_tick(uint32_t frame,int32_t now,rf_level_particles *particles)
{
    static const uint32_t uids[4]={4527,4528,4529,4530};
    static const uint32_t source_frames[8]={0,5,110,111,112,140,141,148};
    static const uint32_t source_ops[8]={0,1,2,3,4,5,6,0};
    /* Ordinary startup applies the world save after frame-zero callbacks.
     * Frame1 therefore observes restoration plus one normal controller tick. */
    static const uint32_t load_frames[8]={1,2,3,4,5,6,20,38};
    static const uint32_t load_ops[8]={0,6,7,2,3,4,0,0};
    rf_runtime_trigger *trigger=NULL;rf_runtime_event *disable=NULL;
    scene_passive_vehicle *host=NULL;rf_group_runtime_entry *controller=NULL;
    uint32_t i,slot,op,*row,parent=UINT32_MAX,phase;float positions[3][3];int status=RF_OK;
    if(!scene_group_control_fixture_checked){
        FILE *file;scene_group_control_fixture_checked=1;
#ifdef RF_IMAGE_XBOX_NATIVE
        file=fopen("D:\\campaign-attached-controls.bin","rb");
#else
        file=NULL;
#endif
        if(!file)return RF_OK;
        i=fread(scene_group_control_fixture_config,1,sizeof(scene_group_control_fixture_config),file);
        status=i!=sizeof(scene_group_control_fixture_config)||fgetc(file)!=EOF?RF_FORMAT:RF_OK;
        fclose(file);
        if(!status && (scene_group_control_fixture_config[0]!=UINT32_C(0x54434652) ||
            scene_group_control_fixture_config[1]<1 || scene_group_control_fixture_config[1]>2 ||
            scene_group_control_fixture_config[2]!=(scene_group_control_fixture_config[1]==1?150u:40u) ||
            scene_group_control_fixture_config[3]))status=RF_FORMAT;
        rf_scene_group_control_fixture[0]=1;rf_scene_group_control_fixture[1]=scene_group_control_fixture_config[1];
        rf_scene_group_control_fixture[14]=1;rf_scene_group_control_fixture[15]=8;
        if(status)goto failed;
    }
    if(!rf_scene_group_control_fixture[14])return RF_OK;
    phase=scene_group_control_fixture_config[1];slot=rf_scene_group_control_fixture[2];
    if(slot>=8 || frame!=(phase==1?source_frames[slot]:load_frames[slot]))return RF_OK;
    op=phase==1?source_ops[slot]:load_ops[slot];row=rf_scene_group_control_fixture_rows[slot];
    row[0]=frame;row[1]=op;row[2]=op==1||op==2||op==4||op==6;
    for(i=0;i<campaign_triggers.count;i++)if(campaign_triggers.items[i].authored->record.uid==4530)trigger=campaign_triggers.items+i;
    for(i=0;i<campaign_passive_vehicle_count;i++)if(campaign_passive_vehicles[i].uid==4717)host=campaign_passive_vehicles+i;
    for(i=0;i<campaign_events.count;i++)if(campaign_events.items[i].authored->record.uid==17967)disable=campaign_events.items+i;
    for(i=0;i<campaign_group_runtime.count;i++)if(campaign_group_runtime.items[i].source &&
        campaign_group_runtime.items[i].source->record.key_count &&
        campaign_group_runtime.items[i].source->keys[0].uid==4543)controller=campaign_group_runtime.items+i;
    if(!trigger||!host||!disable||!controller||campaign_triggers.count!=1||campaign_group_control_child_count!=4){status=RF_NOT_FOUND;goto row_failed;}
    for(i=0;i<campaign_group_registration.count;i++)if(campaign_group_registration.controllers[i].runtime==controller)
        parent=campaign_group_registration.controllers[i].handle;
    if(parent==UINT32_MAX||trigger->authored->record.link_count!=2){status=RF_FORMAT;goto row_failed;}
    row[4]=4530;row[5]=trigger->handle;row[6]=trigger->state.count;row[8]=trigger->state.flags;
    row[10]=trigger->activation.object_flags;row[12]=rf_scene_live_activation[0];row[14]=rf_scene_trigger_contacts[1];
    row[16]=parent;row[17]=controller->translation.motion.flags;row[19]=controller->kind;
    memcpy(row+20,&controller->translation.motion.phase,4);memcpy(row+22,&controller->translation.distance,4);
    memcpy(row+24,controller->translation.pending,12);memcpy(row+27,controller->translation.position,12);
    memcpy(row+33,trigger->volume.center,12);memcpy(row+36,trigger->volume.matrix,36);
    {uint32_t child[64];status=rf_scene_group_control_child_read(4530,child);if(status)goto row_failed;
     memcpy(row+45,child+8,12);memcpy(row+48,child+11,36);}
    memcpy(positions[0],op==2?(const float *)(row+45):trigger->volume.center,12);
    memcpy(positions[1],positions[0],12);memcpy(positions[2],positions[0],12);memcpy(row+30,positions[0],12);
    if(op==1||op==2||op==3||op==4||op==6){
        ++rf_scene_group_control_fixture[3];
        status=campaign_actor_trigger_contacts(&campaign_player_view,positions,now,frame,particles,row[2],0);
    }else if(op==5){
        rf_startup_events_report report={0};++rf_scene_group_control_fixture[4];
        status=rf_runtime_event_fire(&campaign_triggers,disable->handle,trigger->handle,
            (uint32_t)campaign_player_view.handle,now,&scene_gravity,particles,&campaign_forces,&report);
    }else if(op==7){
        campaign_activation_context context={now,frame,particles};++rf_scene_group_control_fixture[5];
        status=campaign_link_effect(&context,5,trigger->handle,UINT32_MAX,(uint32_t)campaign_player_view.handle);
    }
    row[7]=trigger->state.count;row[9]=trigger->state.flags;row[11]=trigger->activation.object_flags;
    row[13]=rf_scene_live_activation[0];row[15]=rf_scene_trigger_contacts[1];row[18]=controller->translation.motion.flags;
    memcpy(row+21,&controller->translation.motion.phase,4);memcpy(row+23,&controller->translation.distance,4);
    memcpy(row+57,trigger->volume.center,12);memcpy(row+60,trigger->volume.matrix,36);
    row[69]=host->handle;row[70]=host->damage.object_flags;memcpy(row+71,&host->damage.state.effects.health,4);
    memcpy(row+72,host->pose.position,12);memcpy(row+75,host->pose.input_matrix,36);
    row[90]=(uint32_t)campaign_player_view.handle;row[91]=(uint32_t)controller->translation.motion.current_key;
    row[92]=(uint32_t)controller->translation.motion.next_key;memcpy(row+93,controller->translation.velocity,12);
    memcpy(row+96,trigger->volume.size,12);row[99]=trigger->volume.shape;row[100]=(uint32_t)trigger->state.cooldown_ms;
    row[101]=(uint32_t)trigger->state.deadline;row[102]=(uint32_t)trigger->activation.limit;
    for(i=0;i<2;i++){row[103+i*2]=trigger->links[i].kind;row[104+i*2]=trigger->links[i].value;}
    row[107]=rf_object_registry_lookup(&campaign_registry,trigger->handle)==trigger;
    row[108]=rf_object_registry_lookup(&campaign_registry,host->handle)==host;row[112]=disable->handle;
    row[113]=rf_scene_setup_result[0];
    for(i=0;i<4;i++){
        uint32_t *child=rf_scene_group_control_fixture_children[slot][i];int read=rf_scene_group_control_child_read(uids[i],child);
        if(!status&&read)status=read;
        if(i<3 && !read){
            const rf_clutter_base_owner *owner=campaign_clutter_bodies[child[3]];
            row[84+i]=child[7];row[87+i]=child[2];
            row[109+i]=(!memcmp(owner->state.position,owner->query_position,12)?1u:0u)|
                (!memcmp(owner->state.position,owner->body.state.position,12)?2u:0u)|
                (!memcmp(owner->matrix,owner->body.state.orientation,36)?4u:0u)|
                (!memcmp(owner->matrix,owner->body.state.next_orientation,36)?8u:0u)|
                (campaign_group_control_clutter_pose_valid(child[3])?16u:0u);
            rf_scene_group_control_fixture[10+i]=child[2];
        }
    }
    rf_scene_group_control_fixture[8]=trigger->handle;rf_scene_group_control_fixture[9]=host->handle;
    rf_scene_group_control_fixture[13]=4543;
row_failed:
    row[3]=(uint32_t)status;++rf_scene_group_control_fixture[2];rf_scene_group_control_fixture[7]=frame;
failed:
    if(status)rf_scene_group_control_fixture[6]=(uint32_t)status;
    return status;
}
#endif
