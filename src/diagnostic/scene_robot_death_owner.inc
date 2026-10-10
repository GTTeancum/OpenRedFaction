#ifndef RF_SCENE_ROBOT_DEATH_OWNER_INCLUDED
#define RF_SCENE_ROBOT_DEATH_OWNER_INCLUDED
/* Include after the allocated NPC owner and count. Exact installed classes
 * only: do not infer this finalizer from a weapon, movement or robot flag. */
/* Keep exact class admission, table validation and every profile bound on
 * one bank. Spike's omitted Vclip Damage is the original zero default, not
 * drone explode's 100: RF.exe 4c14e9..4c1505 initializes the damage field. */
static const struct {
    const char *name,*clip_name,*death_sound;float radius,clip_damage;
} scene_robot_death_authored[]={
    {"Tankbot","drone explode","Big Bot Death",10.f,100.f},
    {"Drone","drone explode","Big Bot Death",4.f,100.f},
    {"Spike","yellboom_metal","Spike Death",.9f,0.f}
};
#define SCENE_ROBOT_DEATH_PROFILE_COUNT (sizeof(scene_robot_death_authored)/sizeof(scene_robot_death_authored[0]))
static scene_robot_death_profile scene_robot_death_profiles[SCENE_ROBOT_DEATH_PROFILE_COUNT];
static uint64_t scene_robot_death_epoch,scene_robot_death_sequence;
static uint32_t scene_robot_death_draining,scene_robot_death_fault;
uint32_t rf_scene_robot_death[16]; /* queued,consumed,retired,already retired,radial requests,visual starts,visual skips,death sound requests,audio errors,duplicates,pending,UID,handle,last status,drains,bytes */
static int32_t scene_robot_death_profile_at(uint32_t slot)
{
    uint32_t cls,i;const char *name;
    if(!campaign_seeds.items||!campaign_seeds.records.items||!campaign_seeds.classes||
       slot>=campaign_seeds.records.count)return -1;
    cls=campaign_seeds.items[slot].class_index;if(cls>=campaign_seeds.class_count)return -1;
    name=campaign_seeds.records.items[slot].record.class_name;
    for(i=0;i<SCENE_ROBOT_DEATH_PROFILE_COUNT;i++)if(scene_robot_death_profiles[i].ready&&scene_robot_death_profiles[i].class_index==cls&&
        !strcmp(name,scene_robot_death_authored[i].name))return (int32_t)i;
    return -1;
}
static campaign_npc_body *scene_robot_death_identity(uint32_t slot,uint32_t handle)
{
    campaign_npc_body *o;
    if(!campaign_npc_bodies||slot>=campaign_npc_body_count||!handle||handle==UINT32_MAX)return NULL;
    o=campaign_npc_bodies+slot;
    if(o->registration.view!=&o->view||o->registration.handle!=handle||
       (uint32_t)o->view.handle!=handle||o->damage.effects.handle!=handle||
       rf_object_registry_lookup(&campaign_registry,handle)!=&o->registration||
       rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&o->view)return NULL;
    return o;
}
static void scene_robot_death_reset(void)
{
    uint32_t i;++scene_robot_death_epoch;scene_robot_death_sequence=0;scene_robot_death_fault=0;
    /* Never reopen an on-stack drain after a callback replaces the timeline. */
    if(campaign_npc_bodies)for(i=0;i<campaign_npc_body_count;i++)
        memset(&campaign_npc_bodies[i].robot_death,0,sizeof(campaign_npc_bodies[i].robot_death));
    rf_scene_robot_death[10]=0;
}
/* SAVE/EXPORT ONLY, never load preflight or shared row capture. Consumed
 * receipts need no wire extension: successful removal is the terminal state.
 * A partial gameplay failure remains visible and cannot masquerade as saved. */
static uint32_t scene_robot_death_save_pending(void)
{
    uint32_t i;if(scene_robot_death_draining||scene_robot_death_fault)return 1;
    if(campaign_npc_body_count&&!campaign_npc_bodies)return 1;
    for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].robot_death.state==1)return 1;
    return 0;
}
/* Called only after successful death motion publication, which establishes
 * action -1 for these clipless classes. Do not remove or blast while a caller
 * still holds an actor pointer (Slay and impact paths do). */
static int scene_robot_death_queue(uint32_t slot,uint32_t *handled)
{
    scene_robot_death_receipt next={0};const scene_robot_death_profile *p;
    campaign_npc_body *o;uint32_t i,j;int32_t profile;
    if(!handled)return RF_RANGE;*handled=0;profile=scene_robot_death_profile_at(slot);
    if(profile<0)return RF_OK;
    if(!campaign_npc_bodies||slot>=campaign_npc_body_count)return RF_RANGE;
    o=scene_robot_death_identity(slot,campaign_npc_bodies[slot].registration.handle);
    if(!o||!isfinite(o->damage.effects.health)||o->damage.effects.health>0||!(o->view.flags_810&1u))return RF_FORMAT;
    if(o->death.action_824!=-1||campaign_seeds.classes[campaign_seeds.items[slot].class_index].corpse.model[0])return RF_OK;
    if(!campaign_model_owners||slot>=campaign_model_owner_count||campaign_model_owners[slot].owned||
       !o->body.allocated_bytes||!o->persistence_registered||o->persistence_slot>=rf_scene_defeated_actors.count||
       o->persistence_slot>=RF_CAMPAIGN_ACTOR_SLOTS)return RF_FORMAT;
    *handled=1;
    if(o->robot_death.state){
        if(o->robot_death.handle!=o->registration.handle)return RF_FORMAT;
        ++rf_scene_robot_death[9];return RF_OK;
    }
    if(scene_robot_death_sequence==UINT64_MAX)return RF_RANGE;
    p=scene_robot_death_profiles+profile;next.state=1;next.handle=o->registration.handle;
    next.uid=(uint32_t)campaign_seeds.records.items[slot].record.uid;
    next.class_index=campaign_seeds.items[slot].class_index;next.persistence=o->persistence_slot;
    next.profile=(uint32_t)profile;next.health=o->damage.effects.health;
    next.damage=p->damage;next.radius=p->radius;next.clip=p->clip;next.foley_group=p->foley_group;
    for(i=0;i<3;i++){
        double offset=0;
        if(!isfinite(o->published[i]))return RF_FORMAT;
        for(j=0;j<3;j++){
            float basis=campaign_model_owners[slot].basis[3*j+i];
            if(!isfinite(basis))return RF_FORMAT;offset+=(double)basis*p->offset[j];
        }
        next.position[i]=(float)((double)o->published[i]+offset);
        if(!isfinite(next.position[i]))return RF_FORMAT;
    }
    next.sequence=++scene_robot_death_sequence;o->robot_death=next;
    ++rf_scene_robot_death[0];++rf_scene_robot_death[10];
    rf_scene_robot_death[11]=next.uid;rf_scene_robot_death[12]=next.handle;
    rf_scene_robot_death[15]=sizeof(scene_robot_death_profiles)+campaign_npc_body_count*sizeof(next);
    return RF_OK;
}
#endif
