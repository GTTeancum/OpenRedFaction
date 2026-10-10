#ifndef RF_SCENE_NPC_PHYSICS_CHECKPOINT_INCLUDED
#define RF_SCENE_NPC_PHYSICS_CHECKPOINT_INCLUDED
/* RFNC14/17 continuation for an accepted Turn_Off_Physics62 NPC owner. Include
 * before NPC capture/restore. No event, wake, support or movement callback is
 * replayed; all fallible work and mutation below concern a private candidate. */
static uint32_t scene_npc_physics_checkpoint_suspended(const rf_npc_checkpoint_record *r)
{return r->physics.present&&r->physics.scripted&&!(r->physics.body_bits&0x80000000u);}
static int scene_npc_physics_checkpoint_capture(const campaign_npc_body *o,
    rf_npc_checkpoint_record *r)
{
    rf_npc_checkpoint_physics *p=&r->physics;
    const rf_physics_body_state *body=&o->body.state;
    if(o->scripted_physics>1||o->scripted_physics_seen>1)return RF_FORMAT;
    if(!o->scripted_physics&&!o->scripted_physics_seen)return RF_OK;
    /* RFNC17 retains the already-admitted terminal source body after death,
     * including seen-only post-wake owners. The separately created corpse
     * does not inherit these bits. Retired/unadmitted owners remain excluded. */
    if(r->retired||(!r->dead_pose&&r->health<=0))return o->scripted_physics?RF_RANGE:RF_OK;
    if(!o->registration.view||!o->body.allocated_bytes||o->view.linked_handle!=-1||
       o->attachment_75c!=-1||r->ai_mode==13||body->word_164||body->word_168||
       (body->state_124&0x1000u)||
       (o->scripted_physics&&(o->object_flags&0x08000000u)))return RF_RANGE;
    p->present=1;p->scripted=o->scripted_physics;
    p->body_bits=body->flags&RF_NPC_CHECKPOINT_PHYSICS_BODY_MASK;
    p->object_bits=o->object_flags&RF_NPC_CHECKPOINT_PHYSICS_OBJECT_MASK;
    memcpy(p->velocity,body->velocity,12);memcpy(p->angular,body->vector_c8,12);
    memcpy(p->momentum,body->mass_vector_d4,12);memcpy(p->force,body->vector_e0,12);
    memcpy(p->torque,body->vector_ec,12);
    return RF_OK; /* The enclosing RFNC codec validates flags/vectors atomically. */
}
/* Install after rf_physics_publish_position, which otherwise normalizes one
 * retained object bit. Low class/descriptor/sphere bits are never overwritten.
 * Placement still checks the whole body against the complete candidate world. */
static int scene_npc_physics_checkpoint_prepare(const campaign_npc_body *o,
    const rf_npc_checkpoint_record *r,rf_physics_body_state *body,uint32_t *flags)
{
    const rf_npc_checkpoint_physics *p=&r->physics;
    if(p->present){
        if(r->ai_mode==13||(p->scripted&&(o->object_flags&0x08000000u)))return RF_FORMAT;
        body->flags=(body->flags&~RF_NPC_CHECKPOINT_PHYSICS_BODY_MASK)|p->body_bits;
        *flags=(*flags&~RF_NPC_CHECKPOINT_PHYSICS_OBJECT_MASK)|p->object_bits;
        memcpy(body->velocity,p->velocity,12);memcpy(body->vector_c8,p->angular,12);
        memcpy(body->mass_vector_d4,p->momentum,12);memcpy(body->vector_e0,p->force,12);
        memcpy(body->vector_ec,p->torque,12);
    }else if(o->scripted_physics||o->scripted_physics_seen){
        /* Pre14 and explicit absent rows could not retain a script freeze.
         * Loading them in-session must not inherit a later freeze or force.
         * Restore the constructor's event-bit baseline without invoking OFF. */
        body->flags=(body->flags&~0x99000000u)|0x80000000u;
        *flags=(*flags&~RF_NPC_CHECKPOINT_PHYSICS_OBJECT_MASK)|0x04000000u;
        memset(body->velocity,0,12);memset(body->vector_c8,0,12);
        memset(body->mass_vector_d4,0,12);memset(body->vector_e0,0,12);memset(body->vector_ec,0,12);
    }
    return RF_OK;
}
/* affected assignments, suspended assignments, latest UID/handle/presence/
 * marker/body bits/object bits. No actor/event side effects are replayed. */
uint32_t rf_scene_npc_physics_restored[8];
static void scene_npc_physics_checkpoint_assign(campaign_npc_body *o,
    const rf_npc_checkpoint_record *r)
{
    o->scripted_physics_seen=r->physics.present;o->scripted_physics=r->physics.scripted;
    if(!r->physics.present)return;
    ++rf_scene_npc_physics_restored[0];
    rf_scene_npc_physics_restored[1]+=scene_npc_physics_checkpoint_suspended(r);
    rf_scene_npc_physics_restored[2]=r->uid;rf_scene_npc_physics_restored[3]=o->registration.handle;
    rf_scene_npc_physics_restored[4]=r->physics.present;rf_scene_npc_physics_restored[5]=r->physics.scripted;
    rf_scene_npc_physics_restored[6]=r->physics.body_bits;rf_scene_npc_physics_restored[7]=r->physics.object_bits;
}
#endif
