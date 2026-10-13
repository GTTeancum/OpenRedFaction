#ifndef RF_SCENE_CROUCH_PREPARED_INCLUDED
#define RF_SCENE_CROUCH_PREPARED_INCLUDED
/* Pure final-close revalidation. No animation, collision callback, resource
 * demand or gameplay repair is permitted here. Admission is still separate. */
static int scene_crouch_prepared(const scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_checkpoint_restore_entry *e)
{
    scene_crouch_receipt expected;scene_crouch_shape_candidate shape;
    const campaign_npc_body *o;uint32_t intent,physical,stamp;
    uint32_t frame=combat_frame==UINT32_MAX?0:combat_frame;int status;
    if(!stage||!e||!campaign_npc_bodies||e->slot>=campaign_npc_body_count||
       e->crouch_shape_present>1||e->crouch_retire_pending>1)return RF_RANGE;
    status=scene_crouch_receipt_prepare(&e->saved.crouch,e->saved.crouch_profile,
        stage->now,frame,&expected,&intent,&physical);if(status)return status;
    status=scene_crouch_stamp_prepare(&e->saved.crouch,(double)frame*scene_step_seconds,
        scene_step_seconds,&stamp);if(status)return status;
    if(memcmp(&expected,&e->crouch_prepared,sizeof(expected))||intent!=e->crouch_intent||
       stamp!=e->crouch_stamp)return RF_FORMAT;
    if(e->saved.dead_pose&&physical!=!!(e->saved.death_flags_810&0x400u))return RF_FORMAT;
    o=campaign_npc_bodies+e->slot;
    if(e->crouch_retire_pending){
        if(!e->saved.retired||e->saved.crouch.flags||!e->crouch_shape_present||
           !(e->owner_before.crouch.flags&SCENE_CROUCH_PRESENT))return RF_FORMAT;
        physical=!!(e->owner_before.view.flags_810&0x400u);
    }
    if(!e->crouch_shape_present){
        if(e->saved.retired)return e->saved.crouch.flags?RF_FORMAT:RF_OK;
        if(e->saved.crouch.flags||o->crouch.flags||(o->view.flags_810&0x400u))return RF_FORMAT;
        return RF_OK;
    }
    if(!e->registered||(e->saved.retired&&!e->crouch_retire_pending)||!campaign_npc_stances||!campaign_seeds.items||
       !campaign_seeds.classes||e->saved.class_id>=campaign_seeds.class_count||
       campaign_seeds.items[e->slot].class_index!=e->saved.class_id||
       o->registration.handle!=e->handle||scene_npc_reload_identity(e->slot,o)||
       !scene_npc_reload_standalone(o))return RF_FORMAT;
    status=scene_tactical_source(e->slot);if(status)return status;
    status=scene_crouch_shape_seal_check(&o->body.spheres,campaign_npc_stances+e->saved.class_id,
        &e->crouch_shape.prepared,&e->crouch_shape);if(status)return status;
    status=scene_crouch_shape_prepare(&o->body.spheres,campaign_npc_stances+e->saved.class_id,
        e->published,e->owner_before.view.flags_810,physical,&shape);if(status)return status;
    if(memcmp(&shape,&e->crouch_shape.prepared,sizeof(shape)))return RF_FORMAT;
    return RF_OK;
}
#endif
