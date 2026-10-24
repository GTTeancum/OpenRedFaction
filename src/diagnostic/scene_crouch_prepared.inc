#ifndef RF_SCENE_CROUCH_PREPARED_INCLUDED
#define RF_SCENE_CROUCH_PREPARED_INCLUDED
/* Native RAM-log crouch close diagnostics. Words: [0] __LINE__ of the last
 * refusing return in scene_crouch_prepared, [1] __LINE__ of the last refusing
 * return in scene_crouch_composed (scene_crouch_composed.inc), [2] NPCs loaded
 * standing because their saved crouch state did not compose, [3] last such
 * UID, [4] status of the original refusal for that NPC. */
uint32_t rf_scene_crouch_reject[5];
#define SCENE_CROUCH_PREPARED_REFUSE(code) do{rf_scene_crouch_reject[0]=__LINE__;return (code);}while(0)
/* Pure final-close revalidation. No animation, collision callback, resource
 * demand or gameplay repair is permitted here. Admission is still separate. */
static int scene_crouch_prepared(const scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_checkpoint_restore_entry *e)
{
    scene_crouch_receipt expected;scene_crouch_shape_candidate shape;
    const campaign_npc_body *o;uint32_t intent,physical,stamp;
    uint32_t frame=combat_frame==UINT32_MAX?0:combat_frame;int status;
    if(!stage||!e||!campaign_npc_bodies||e->slot>=campaign_npc_body_count||
       e->crouch_shape_present>1||e->crouch_retire_pending>1)SCENE_CROUCH_PREPARED_REFUSE(RF_RANGE);
    status=scene_crouch_receipt_prepare(&e->saved.crouch,e->saved.crouch_profile,
        stage->now,frame,&expected,&intent,&physical);if(status)SCENE_CROUCH_PREPARED_REFUSE(status);
    status=scene_crouch_stamp_prepare(&e->saved.crouch,(double)frame*scene_step_seconds,
        scene_step_seconds,&stamp);if(status)SCENE_CROUCH_PREPARED_REFUSE(status);
    if(memcmp(&expected,&e->crouch_prepared,sizeof(expected))||intent!=e->crouch_intent||
       stamp!=e->crouch_stamp)SCENE_CROUCH_PREPARED_REFUSE(RF_FORMAT);
    if(e->saved.dead_pose&&physical!=!!(e->saved.death_flags_810&0x400u))SCENE_CROUCH_PREPARED_REFUSE(RF_FORMAT);
    o=campaign_npc_bodies+e->slot;
    if(e->crouch_retire_pending){
        if(!e->saved.retired||e->saved.crouch.flags||!e->crouch_shape_present||
           !(e->owner_before.crouch.flags&SCENE_CROUCH_PRESENT))SCENE_CROUCH_PREPARED_REFUSE(RF_FORMAT);
        physical=!!(e->owner_before.view.flags_810&0x400u);
    }
    if(!e->crouch_shape_present){
        if(e->saved.retired){if(e->saved.crouch.flags)SCENE_CROUCH_PREPARED_REFUSE(RF_FORMAT);return RF_OK;}
        if(e->saved.crouch.flags||o->crouch.flags||(o->view.flags_810&0x400u))SCENE_CROUCH_PREPARED_REFUSE(RF_FORMAT);
        return RF_OK;
    }
    if(!e->registered||(e->saved.retired&&!e->crouch_retire_pending)||!campaign_npc_stances||!campaign_seeds.items||
       !campaign_seeds.classes||e->saved.class_id>=campaign_seeds.class_count||
       campaign_seeds.items[e->slot].class_index!=e->saved.class_id||
       o->registration.handle!=e->handle||scene_npc_reload_identity(e->slot,o)||
       !scene_npc_reload_standalone(o))SCENE_CROUCH_PREPARED_REFUSE(RF_FORMAT);
    status=scene_tactical_source(e->slot);if(status)SCENE_CROUCH_PREPARED_REFUSE(status);
    status=scene_crouch_shape_seal_check(&o->body.spheres,campaign_npc_stances+e->saved.class_id,
        &e->crouch_shape.prepared,&e->crouch_shape);if(status)SCENE_CROUCH_PREPARED_REFUSE(status);
    status=scene_crouch_shape_prepare_private(&o->body.spheres,campaign_npc_stances+e->saved.class_id,
        e->published,e->owner_before.view.flags_810,physical,&shape);if(status)SCENE_CROUCH_PREPARED_REFUSE(status);
    if(memcmp(&shape,&e->crouch_shape.prepared,sizeof(shape)))SCENE_CROUCH_PREPARED_REFUSE(RF_FORMAT);
    return RF_OK;
}
#endif
