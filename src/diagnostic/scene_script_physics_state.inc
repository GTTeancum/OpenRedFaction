#ifndef RF_SCENE_SCRIPT_PHYSICS_STATE_INCLUDED
#define RF_SCENE_SCRIPT_PHYSICS_STATE_INCLUDED
/* Event62 controls ordinary body sleep/wake. Animation, actor lifetime and
 * authored parent transforms remain separate. Include after vehicle adapter. */
uint32_t rf_scene_physics_state[8]; /* calls,disabled,enabled,missing,UID,handle,bodyflags,status */
uint32_t rf_scene_physics_npcs[32]; /* two rows: UID,handle,bodyflags,objflags,XYZ,VXYZ,health,marker,frame,angularXYZ */
static uint32_t scene_script_physics_npcs[2],scene_script_physics_count;
static void scene_script_physics_reset(void)
{
    memset(rf_scene_physics_state,0,sizeof(rf_scene_physics_state));memset(rf_scene_physics_npcs,0,sizeof(rf_scene_physics_npcs));
    memset(scene_script_physics_npcs,0,sizeof(scene_script_physics_npcs));scene_script_physics_count=0;
}
static void scene_script_physics_sample(uint32_t frame)
{
    for(uint32_t n=0;n<scene_script_physics_count;n++)for(uint32_t i=0;i<campaign_npc_body_count;i++){
        campaign_npc_body *o=campaign_npc_bodies+i;uint32_t *p=rf_scene_physics_npcs+n*16;
        if(!o->registration.view||o->registration.handle!=scene_script_physics_npcs[n]||
           rf_object_registry_lookup(&campaign_registry,o->registration.handle)!=&o->registration)continue;
        p[0]=(uint32_t)campaign_seeds.records.items[i].record.uid;p[1]=o->registration.handle;
        p[2]=o->body.state.flags;p[3]=o->object_flags;memcpy(p+4,o->body.state.position,12);memcpy(p+7,o->body.state.velocity,12);
        memcpy(p+10,&o->damage.effects.health,4);p[11]=o->scripted_physics;p[12]=frame;memcpy(p+13,o->body.state.vector_c8,12);break;
    }
}
static int campaign_set_physics_enabled(void *context,uint32_t handle,uint32_t enabled)
{
    scene_stream *s=context;void *registered;uint32_t handled=0;int status;
    if(!s||enabled>1)return RF_RANGE;
    ++rf_scene_physics_state[0];rf_scene_physics_state[4]=UINT32_MAX;rf_scene_physics_state[5]=handle;
    rf_scene_physics_state[6]=rf_scene_physics_state[7]=0;
    status=scene_vehicle_physics_state(s,handle,enabled,&handled);
    if(handled||(status&&status!=RF_NOT_FOUND))goto done;
    registered=rf_object_registry_lookup(&campaign_registry,handle);
    if(!registered){status=RF_NOT_FOUND;goto done;}
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        campaign_npc_body *o=campaign_npc_bodies+i;
        if(registered!=&o->registration||!o->registration.view||o->registration.handle!=handle)continue;
        if(!o->body.allocated_bytes){status=RF_NOT_FOUND;goto done;}
        rf_scene_physics_state[4]=(uint32_t)campaign_seeds.records.items[i].record.uid;
        status=rf_physics_set_enabled(&o->body.state,&o->object_flags,enabled);
        if(!status){
            if(enabled)o->scripted_physics=0;else if(!(o->object_flags&0x08000000u))o->scripted_physics=1;
            o->view.flags_7c=o->object_flags;rf_scene_physics_state[6]=o->body.state.flags;
            uint32_t n;for(n=0;n<scene_script_physics_count;n++)if(scene_script_physics_npcs[n]==handle)break;
            if(n==scene_script_physics_count&&n<2)scene_script_physics_npcs[scene_script_physics_count++]=handle;
        }
        goto done;
    }
    for(uint32_t i=0;campaign_clutter_bodies&&i<campaign_clutter_records.count;i++){
        if(!campaign_clutter_bodies[i]||registered!=&campaign_clutter_bodies[i]->state||campaign_clutter_bodies[i]->state.handle!=handle)continue;
        if(!campaign_clutter_bodies[i]->body.allocated_bytes){status=RF_NOT_FOUND;goto done;}
        rf_scene_physics_state[4]=campaign_clutter_bodies[i]->uid;
        status=rf_physics_set_enabled(&campaign_clutter_bodies[i]->body.state,&campaign_clutter_bodies[i]->state.flags,enabled);
        rf_scene_physics_state[6]=campaign_clutter_bodies[i]->body.state.flags;goto done;
    }
    status=RF_NOT_FOUND;
done:
    if(status==RF_NOT_FOUND)++rf_scene_physics_state[3];else if(!status)++rf_scene_physics_state[enabled?2:1];
    rf_scene_physics_state[7]=(uint32_t)status;return status;
}
/* Until codecs preserve this event state, reject affected ordinary saves. */
static int scene_script_physics_save_allowed(const scene_stream *s)
{
    if(s->driller_runtime && (s->driller_runtime->scripted_physics || s->driller_runtime->scripted_exit_pending))return 0;
    for(uint32_t i=0;i<campaign_passive_vehicle_count;i++)if(campaign_passive_vehicles[i].scripted_physics || (campaign_passive_vehicles[i].damage.state.effects.flags_814&0x800u))return 0;
    for(uint32_t i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].scripted_physics)return 0;
    return 1;
}
#endif
