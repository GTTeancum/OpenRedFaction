#ifndef RF_SCENE_RADIAL_DEATH_HINT_INCLUDED
#define RF_SCENE_RADIAL_DEATH_HINT_INCLUDED
/* 4891bf..48921f: ordinary positive radial damage sets living skeletal
 * actor824 from (physics position - epicenter) dot physics forward.
 * This bounded adapter publishes only an accepted lethal pulse: living RFNC
 * has no owner for the original persistent nonlethal hint. No impulse.
 * Include after scene_nano_blast.inc and existing full-handle services. */
typedef struct scene_radial_death_hint {
    uint64_t epoch;uint32_t slot,handle,class_index;int32_t action;
} scene_radial_death_hint;
/* Model-present/class-kind2 is original40a1e0. The additional live pose join
 * excludes an already-transferred corpse without acquiring/evaluating a pose. */
static campaign_npc_body *scene_radial_death_hint_owner(uint32_t slot,uint32_t handle,uint32_t *cls)
{
    campaign_npc_body *o=scene_ai_melee_npc_identity(slot,handle);
    const campaign_model_owner *model;
    if(!o || o->view.type!=0 || !campaign_seeds.items || !campaign_seeds.classes ||
       slot>=campaign_seeds.records.count || !campaign_model_owners ||
       slot>=campaign_model_owner_count || !campaign_poses.items || slot>=campaign_poses.count)return NULL;
    *cls=campaign_seeds.items[slot].class_index;
    if(*cls>=campaign_seeds.class_count || campaign_seeds.classes[*cls].model_kind!=2)return NULL;
    model=campaign_model_owners+slot;
    if(!model->registration.loaded || model->owned || model->pose!=campaign_poses.items+slot ||
       model->registration.active!=&model->pose->playback.completion.active)return NULL;
    return o;
}
/* Called only after unchanged positive CF5/body-center falloff and a
 * non-consuming Nano result, before generic damage/presentation callbacks.
 * Snapshot the result, never reinterpret it using later body/model geometry. */
static void scene_radial_death_hint_capture(uint32_t slot,uint32_t handle,const float origin[3],
    uint64_t epoch,scene_radial_death_hint *out)
{
    scene_radial_death_hint value={0};campaign_npc_body *o;uint32_t cls,k;
    float delta[3];double projection;
    value.action=-1;*out=value;
    if(!origin || epoch!=scene_robot_death_epoch)return;
    o=scene_radial_death_hint_owner(slot,handle,&cls);
    if(!o || !isfinite(o->damage.effects.health) || o->damage.effects.health<=0 ||
       (o->view.flags_810&1u) || (o->object_flags&(2u|0x4000u)))return;
    for(k=0;k<3;k++) {
        double d;
        if(!isfinite(origin[k]) || !isfinite(o->body.state.position[k]) ||
           !isfinite(o->body.state.orientation[6+k]))return;
        d=(double)o->body.state.position[k]-origin[k];
        if(d>FLT_MAX || d< -FLT_MAX)return;delta[k]=(float)d;
    }
    /* 409fa0 stores the deltas. 40a0b0 sums Z, Y, X without a float
     * projection store before the sign test. Double is bounded port policy. */
    projection=((double)delta[2]*o->body.state.orientation[8]+
        (double)delta[1]*o->body.state.orientation[7])+
        (double)delta[0]*o->body.state.orientation[6];
    if(!isfinite(projection))return;
    value.epoch=epoch;value.slot=slot;value.handle=handle;
    value.class_index=cls;value.action=projection>0?6:7;*out=value;
}
/* This existing epoch advances on NPC teardown and successful standalone/
 * world checkpoint publication. It prevents an old stack receipt from
 * crossing a replaced timeline even when the blast caller has no pool epoch.
 * Capture uses the epoch retained at shared-blast entry, before other victims.
 * Caller retains its independent transient-flight guard. No callback occurs
 * between this final full-handle lookup/write and the existing death entry. */
static void scene_radial_death_hint_publish(const scene_radial_death_hint *hint,float applied)
{
    campaign_npc_body *o;uint32_t cls;
    if(!hint || (hint->action!=6 && hint->action!=7) || hint->epoch!=scene_robot_death_epoch ||
       !isfinite(applied) || applied<=0)return;
    o=scene_radial_death_hint_owner(hint->slot,hint->handle,&cls);
    if(!o || cls!=hint->class_index || !isfinite(o->damage.effects.health) ||
       o->damage.effects.health>0 || (o->view.flags_810&1u) ||
       (o->object_flags&(2u|0x4000u)))return;
    /* Original radial overwrite, even if an earlier hint exists. The current
     * death path retains its crouch/controller, clearance and fallback rules. */
    o->death.action_824=hint->action;
}
#endif
