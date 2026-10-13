#ifndef RF_SCENE_PURSUIT_DELAY_SAMPLE_GUARD_INCLUDED
#define RF_SCENE_PURSUIT_DELAY_SAMPLE_GUARD_INCLUDED
/* Include after the Blind target helpers, before its sampler. Borrowed target
 * points are copied before the first query; every query closes epoch/identity
 * and sample-owner state before publication or another borrowed dereference. */
typedef struct scene_pursuit_delay_sample_guard {
    uint64_t epoch;
    campaign_npc_body *owners;
    uint32_t count,slot,handle,target,mode,blind_present,blind_target,blind_seen;
    scene_pursuit_delay_receipt delay;
    float position[3],eye[3];
} scene_pursuit_delay_sample_guard;
_Static_assert(sizeof(void*)!=4||sizeof(scene_pursuit_delay_sample_guard)<=96,"Pursuit sample guard exceeds reviewed Xbox32 bound");
static void scene_pursuit_delay_sample_capture(uint32_t slot,scene_pursuit_delay_sample_guard *g)
{
    const campaign_npc_body *o=campaign_npc_bodies+slot;
    g->epoch=scene_robot_death_epoch;g->owners=campaign_npc_bodies;g->count=campaign_npc_body_count;
    g->slot=slot;g->handle=o->registration.handle;g->target=scene_blind_target_handle(o);g->mode=o->combat_scripted;
    g->blind_present=o->blind.present;g->blind_target=o->blind.target;g->blind_seen=o->blind.last_seen;
    g->delay=o->pursuit_delay;memcpy(g->position,o->published,12);memcpy(g->eye,o->eye_position,12);
}
static int scene_pursuit_delay_sample_valid(const scene_pursuit_delay_sample_guard *g)
{
    const campaign_npc_body *o;
    if(g->epoch!=scene_robot_death_epoch||g->owners!=campaign_npc_bodies||
       g->count!=campaign_npc_body_count||!campaign_npc_bodies||g->slot>=campaign_npc_body_count)return RF_FORMAT;
    o=campaign_npc_bodies+g->slot;
    if(o->registration.handle!=g->handle||scene_npc_reload_identity(g->slot,o)||
       scene_blind_target_handle(o)!=g->target||o->combat_scripted!=g->mode||
       !scene_blind_target_live(g->target)||o->blind.present!=g->blind_present||
       o->blind.target!=g->blind_target||o->blind.last_seen!=g->blind_seen||
       memcmp(&o->pursuit_delay,&g->delay,sizeof(g->delay))||
       memcmp(o->published,g->position,12)||memcmp(o->eye_position,g->eye,12))return RF_FORMAT;
    return RF_OK;
}
#endif
