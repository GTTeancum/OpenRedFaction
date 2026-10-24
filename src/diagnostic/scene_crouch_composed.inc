#ifndef RF_SCENE_CROUCH_COMPOSED_INCLUDED
#define RF_SCENE_CROUCH_COMPOSED_INCLUDED
/* Refusing returns record __LINE__ in rf_scene_crouch_reject[1]
 * (declared in scene_crouch_prepared.inc). */
#define SCENE_CROUCH_COMPOSED_REFUSE(code) do{rf_scene_crouch_reject[1]=__LINE__;return (code);}while(0)
/* Incoming physical shape has no combat-target or living-player requirement.
 * Resolve RFNS attachment independently of the old destination link. */
static int scene_crouch_composed_entry(const scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_seat_checkpoint_stage *seats,const scene_npc_checkpoint_restore_entry *e)
{
    const campaign_npc_body *o;uint32_t seated;float basis[9];int status;
    status=scene_crouch_prepared(stage,e);if(status)SCENE_CROUCH_COMPOSED_REFUSE(status);
    if(!e->crouch_shape_present)return RF_OK;
    o=campaign_npc_bodies+e->slot;
    if(!e->registered||(e->saved.retired&&!e->crouch_retire_pending)||scene_npc_reload_identity(e->slot,o)||
       e->handle==campaign_player_object.handle||(o->object_flags&8u)||
       o->view.occupant_count||o->attachment_75c!=-1||
       (o->view.linked_handle!=-1&&!scene_npc_seat_checkpoint_actor(e->handle)))SCENE_CROUCH_COMPOSED_REFUSE(RF_FORMAT);
    status=scene_npc_seat_checkpoint_basis(seats,&e->saved,basis,&seated);if(status)SCENE_CROUCH_COMPOSED_REFUSE(status);
    if(seated)SCENE_CROUCH_COMPOSED_REFUSE(RF_FORMAT);
    return RF_OK;
}
static int scene_crouch_composed(const scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_seat_checkpoint_stage *seats,const scene_world_player_stage *player)
{
    if(!stage||!player||player->npcs!=stage)SCENE_CROUCH_COMPOSED_REFUSE(RF_RANGE);
    for(uint32_t i=0;i<stage->count;i++){
        int status=scene_crouch_composed_entry(stage,seats,stage->entries+i);if(status)return status;
    }
    return RF_OK;
}
/* Ordinary-load tolerance: a living row whose saved crouch state does not
 * compose (mid stance change, clock drift between staging and close, live
 * owner state that no longer matches) loads standing. The saved crouch wire
 * is cleared and the receipt/intent/stamp are re-derived from that empty wire,
 * so every later scene_crouch_prepared caller and the final publication see
 * one consistent standing state. A live crouched owner gets a prepared
 * standing shape (published at final close); a standing owner keeps its
 * current spheres. Candidate body bounds follow the selected spheres.
 * Retired/retiring rows are not repaired. Shape preparation failure keeps
 * the original refusal. */
static int scene_crouch_load_standing(scene_npc_checkpoint_restore_stage *stage,
    scene_npc_checkpoint_restore_entry *e)
{
    campaign_npc_body *o;const rf_physics_sphere *spheres;uint32_t count,physical,body_flags,object_flags;
    uint32_t frame=combat_frame==UINT32_MAX?0:combat_frame;float published[3];int status;
    if(!stage||!e||!campaign_npc_bodies||e->slot>=campaign_npc_body_count||
       e->saved.retired||e->crouch_retire_pending)return RF_FORMAT;
    o=campaign_npc_bodies+e->slot;
    memset(&e->saved.crouch,0,sizeof(e->saved.crouch));e->saved.death_flags_810&=~0x400u;
    status=scene_crouch_receipt_prepare(&e->saved.crouch,e->saved.crouch_profile,
        stage->now,frame,&e->crouch_prepared,&e->crouch_intent,&physical);if(status)return status;
    status=scene_crouch_stamp_prepare(&e->saved.crouch,(double)frame*scene_step_seconds,
        scene_step_seconds,&e->crouch_stamp);if(status)return status;
    if(!(o->crouch.flags)&&!(o->view.flags_810&0x400u))e->crouch_shape_present=0;
    else{
        scene_crouch_shape_candidate shape;
        if(!e->registered||!campaign_npc_stances||!campaign_seeds.items||
           e->saved.class_id>=campaign_seeds.class_count)return RF_FORMAT;
        status=scene_crouch_shape_prepare(&o->body.spheres,campaign_npc_stances+e->saved.class_id,
            e->published,e->owner_before.view.flags_810,0,&shape);if(status)return status;
        status=scene_crouch_shape_seal_prepare(&o->body.spheres,&shape,&e->crouch_shape);if(status)return status;
        e->crouch_shape_present=1;
    }
    status=scene_npc_checkpoint_restore_spheres(e,&spheres,&count);if(status)return status;
    if(count){
        body_flags=e->body.flags;object_flags=e->flags;
        status=rf_physics_spheres_bounds(spheres,count,e->body.position,&e->body.bounds);if(status)return status;
        status=rf_physics_publish_position(&e->body,published,&object_flags);e->body.flags=body_flags;if(status)return status;
    }
    ++rf_scene_crouch_reject[2];rf_scene_crouch_reject[3]=e->saved.uid;
    return RF_OK;
}
static int scene_crouch_composed_prepare(scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_seat_checkpoint_stage *seats,const scene_world_player_stage *player)
{
    if(!stage||!player||player->npcs!=stage)SCENE_CROUCH_COMPOSED_REFUSE(RF_RANGE);
    for(uint32_t i=0;i<stage->count;i++){
        scene_npc_checkpoint_restore_entry *e=stage->entries+i;
        int status=scene_crouch_composed_entry(stage,seats,e);
        if(status==RF_FORMAT&&!e->saved.retired&&!e->crouch_retire_pending){
            rf_scene_crouch_reject[4]=(uint32_t)status;
            if(scene_crouch_load_standing(stage,e))return status;
            status=scene_crouch_composed_entry(stage,seats,e);
        }
        if(status)return status;
    }
    stage->crouch_joined=1;return RF_OK;
}
#endif
