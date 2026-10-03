#ifndef RF_SCENE_VEHICLE_PHYSICS_STATE_INCLUDED
#define RF_SCENE_VEHICLE_PHYSICS_STATE_INCLUDED
/* Turn_Off_Physics62 adapter. Include after scene_driller_runtime.inc.
 * enabled0 is event ON; enabled1 is OFF. The rigid solver has no ordinary
 * actor active-bit gate, so its parent must honor the predicate below.
 * Required runtime fields: scripted_physics,scripted_exit_pending.
 * Required passive fields: scripted_physics,physics_body_flags.
 */
uint32_t rf_scene_vehicle_physics[16]; /* requests,freezes,wakes,immune,exit requests,exits,blocked,errors,UID,handle,enabled,bodyFlags,objectFlags,flags814,frozen,pending */
uint32_t rf_scene_vehicle_physics_apply[24]; /* latest request: valid,UID,handle,enabled,nowMs,bodyFlagsBefore/after,objectFlagsBefore/after,814Before/after,positionXYZ,velocityBeforeXYZ,velocityAfterXYZ,momentumAfterXYZ,attached */
static void scene_vehicle_physics_reset(void)
{memset(rf_scene_vehicle_physics,0,sizeof(rf_scene_vehicle_physics));memset(rf_scene_vehicle_physics_apply,0,sizeof(rf_scene_vehicle_physics_apply));}
static uint32_t scene_vehicle_physics_frozen(const scene_stream *s)
{
    const scene_driller_runtime *r=s?s->driller_runtime:NULL;
    return r&&r->scripted_physics&&!(r->physics.body_flags&0x80000000u);
}
static uint32_t scene_vehicle_physics_allows_control(const scene_stream *s)
{
    const scene_driller_runtime *r=s?s->driller_runtime:NULL;
    return !r||(!scene_vehicle_physics_frozen(s)&&!r->scripted_exit_pending);
}
/* Unsupported owners return NOT_FOUND/handled0, allowing the scene dispatcher
 * to try its NPC/prop implementations. Resolved vehicle errors are handled1.
 * No owner allocation, registration, health mutation or invented destruction. */
static int scene_vehicle_physics_state(scene_stream *s,uint32_t handle,uint32_t enabled,uint32_t *handled)
{
    scene_driller_runtime *r=s?s->driller_runtime:NULL;scene_passive_vehicle *passive=NULL;
    rf_physics_body_state body={0};uint32_t object_flags,old_flags,old_object,old_814,uid,immune;
    uint32_t *marker,*body_flags;scene_driller_damage *damage;float *velocity,*momentum=NULL,*position;
    int status;
    if(!handled||enabled>1)return RF_RANGE;*handled=0;if(!s)return RF_NOT_FOUND;
    if(r&&handle==r->entry.registration.handle){
        *handled=1;
        if(!scene_driller_damage_runtime_valid(&r->damage)||
           rf_object_registry_lookup(&campaign_registry,handle)!=&r->entry.registration)return RF_NOT_FOUND;
        marker=&r->scripted_physics;body_flags=&r->physics.body_flags;damage=&r->damage.damage;
        velocity=r->physics.state.velocity;momentum=r->physics.state.momentum;position=r->physics.state.position;
        uid=(uint32_t)campaign_authored_vehicle_uid;
    }else{
        for(uint32_t i=0;i<campaign_passive_vehicle_count;i++)if(campaign_passive_vehicles[i].handle==handle){passive=campaign_passive_vehicles+i;break;}
        if(!passive)return RF_NOT_FOUND;*handled=1;
        if(rf_object_registry_lookup(&campaign_registry,handle)!=passive)return RF_NOT_FOUND;
        marker=&passive->scripted_physics;body_flags=&passive->physics_body_flags;damage=&passive->damage;
        velocity=passive->velocity;position=passive->pose.position;uid=passive->uid;
    }
    ++rf_scene_vehicle_physics[0];rf_scene_vehicle_physics[8]=uid;rf_scene_vehicle_physics[9]=handle;
    rf_scene_vehicle_physics[10]=enabled;
    old_flags=*body_flags;old_object=object_flags=damage->object_flags;old_814=damage->state.effects.flags_814;
    immune=!enabled&&(object_flags&0x08000000u);
    body.flags=*body_flags;memcpy(body.velocity,velocity,12);
    if(momentum)memcpy(body.mass_vector_d4,momentum,12);
    /* Reuse exact417e00/40a420 flag and immunity policy. Temporary ordinary
     * body is only a projection: force/torque/skip_forces remain untouched. */
    status=rf_physics_set_enabled(&body,&object_flags,enabled);
    if(status){++rf_scene_vehicle_physics[7];return status;}
    uint32_t *p=rf_scene_vehicle_physics_apply;memset(p,0,sizeof(rf_scene_vehicle_physics_apply));
    p[0]=1;p[1]=uid;p[2]=handle;p[3]=enabled;p[4]=(uint32_t)campaign_blackout_now;
    p[5]=old_flags;p[7]=old_object;p[9]=old_814;memcpy(p+11,position,12);memcpy(p+14,velocity,12);
    *body_flags=body.flags;damage->object_flags=object_flags;
    memcpy(velocity,body.velocity,12);if(momentum)memcpy(momentum,body.mass_vector_d4,12);
    /* An immune ON must not turn an untouched ordinary rigid owner into an
     * inactive one merely because its constructor retained a rest flag. */
    if(enabled||!immune)*marker=1;
    if(!enabled&&(damage->class_flags&0x1000u)){
        damage->state.effects.flags_814|=0x800u;
        if(!passive&&r->entry.session.active){
            if(!r->scripted_exit_pending)++rf_scene_vehicle_physics[4];r->scripted_exit_pending=1;
        }
    }
    if(!passive){r->entry.view.flags_7c=object_flags;r->entry.view.flags_814=damage->state.effects.flags_814;}
    else passive->pose.flags|=object_flags&0x06000000u;
    ++rf_scene_vehicle_physics[enabled?2:1];rf_scene_vehicle_physics[3]+=!!immune;
    rf_scene_vehicle_physics[11]=p[6]=*body_flags;rf_scene_vehicle_physics[12]=p[8]=object_flags;
    rf_scene_vehicle_physics[13]=p[10]=damage->state.effects.flags_814;
    rf_scene_vehicle_physics[14]=*marker&&!(*body_flags&0x80000000u);
    rf_scene_vehicle_physics[15]=passive?0:r->scripted_exit_pending;
    memcpy(p+17,velocity,12);if(momentum)memcpy(p+20,momentum,12);p[23]=passive?passive->attached:r->entry.session.active;
    return RF_OK;
}
/* Called once each runtime tick before ordinary Use and before the rigid
 * step. Frozen chassis has no final-step movement to defer. A blocked living
 * exit retains ownership, no artificial host death and no forced relocation.
 * Existing projectile flights must continue in their normal weapon ticks. */
static int scene_vehicle_physics_exit_tick(scene_stream *s,uint32_t *changed)
{
    scene_driller_runtime *r=s?s->driller_runtime:NULL;scene_driller_entry_exit *e;
    scene_vehicle_possession_ops ops;uint32_t exited=0;int status;
    if(!changed)return RF_RANGE;*changed=0;
    if(!r||!r->scripted_exit_pending)return RF_OK;
    if(!scene_driller_damage_runtime_valid(&r->damage))return RF_NOT_FOUND;e=&r->entry;
    if(!e->session.active){r->scripted_exit_pending=0;rf_scene_vehicle_physics[15]=0;return RF_OK;}
    uint32_t reason=campaign_player_damage.state.effects.health>0?1u:2u;
    if(scene_jeep_npc_gunner_active(e))status=scene_jeep_npc_gunner_unpossess(e,reason,&exited);
    else{
        ops=(scene_vehicle_possession_ops){scene_driller_entry_allowed,scene_driller_entry_seat,scene_driller_exit_pose,e};
        status=scene_vehicle_unpossess(&e->session,&e->player,&e->host,reason,&ops,&exited);
        if(!status&&exited){e->player_view->linked_handle=-1;e->occupant=-1;}
    }
    if(status){++rf_scene_vehicle_physics[7];return status;}
    if(!exited){++rf_scene_vehicle_physics[6];return RF_OK;}
    r->scripted_exit_pending=0;rf_scene_vehicle_physics[15]=0;++rf_scene_vehicle_physics[5];
    *changed=1;return RF_OK; /* Parent performs normal player publication once. */
}
#endif
