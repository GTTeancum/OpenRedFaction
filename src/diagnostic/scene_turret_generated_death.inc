#ifndef RF_SCENE_TURRET_GENERATED_DEATH_INCLUDED
#define RF_SCENE_TURRET_GENERATED_DEATH_INCLUDED
/* Include after combat_death_start and combat damage feedback callbacks, and
 * after scene_turret_generated.inc. Early death call sites need the join
 * prototype. No owner allocation, registry retirement or restore-time calls.
 * Original418f80: base->head life0; head->base damage1000/source-1/kind-1. */
typedef struct scene_turret_generated_death_context {
    uint32_t clock_bits;
    combat_feedback feedback;
    rf_damage_effect_backend effects;
} scene_turret_generated_death_context;
/* Shot owner is included later; counters are read only, never synthesized. */
extern uint32_t rf_scene_turret_shots[8];
extern uint32_t rf_scene_turret_death_effects[8];
uint32_t rf_scene_turret_generated_death_probe[10];
static void scene_turret_generated_death_record(const scene_turret_generated_binding *binding,
    const campaign_npc_body *base,const scene_turret_owner *head,uint32_t reason)
{
    rf_scene_turret_generated_death_probe[0]=binding->key.base_uid;
    rf_scene_turret_generated_death_probe[1]=binding->key.role;
    rf_scene_turret_generated_death_probe[2]=reason;
    memcpy(rf_scene_turret_generated_death_probe+3,&base->damage.effects.health,4);
    memcpy(rf_scene_turret_generated_death_probe+4,&head->damage.effects.health,4);
    rf_scene_turret_generated_death_probe[5]=head->dead;
    rf_scene_turret_generated_death_probe[6]=(uint32_t)head->view.linked_handle;
    rf_scene_turret_generated_death_probe[7]=rf_scene_turret_shots[0];
    rf_scene_turret_generated_death_probe[8]=rf_scene_turret_death_effects[0];
    rf_scene_turret_generated_death_probe[9]=0;
}

static int scene_turret_generated_death_dispatch(void *context,uint32_t base_handle,
    uint32_t head_handle,uint32_t reason)
{
    scene_turret_generated_death_context *c=context;
    scene_turret_generated_binding *binding=scene_turret_generated_find(head_handle);
    scene_turret_owner *head=scene_turret_lookup(head_handle);campaign_npc_body *base;
    uint32_t entered=0;float amount=0;int status;
    if(!c||!binding||!head||binding->base_handle!=base_handle||
       binding->death_dispatched!=reason)return RF_FORMAT;
    base=scene_turret_generated_base(binding);if(!base)return RF_NOT_FOUND;
    if(reason==SCENE_TURRET_GENERATED_BASE_DIED) {
        /* Original sets life directly, so immunity/armor must not keep an
         * orphan head alive. A zero request then runs the ordinary turret
         * terminal transition/model/effect code, without invented damage. */
        rf_damage_request request={0,UINT32_MAX,-1,0,UINT32_MAX,0};
        if(head->dead){scene_turret_generated_death_record(binding,base,head,reason);return RF_OK;}
        head->damage.effects.health=0;
        status=scene_turret_damage_receive(head_handle,&request,1,c->clock_bits,&c->effects,&amount);
        if(!status)status=c->feedback.status;
        if(status)return status;
        if(!head->dead||head->damage.effects.health>0)return RF_FORMAT;
        scene_turret_generated_death_record(binding,base,head,reason);return RF_OK;
    }
    if(reason!=SCENE_TURRET_GENERATED_HEAD_DIED)return RF_RANGE;
    /* Preserve the original request, including normal immunity handling.
     * Do not increase1000 to class life or force an invulnerable base dead. */
    if(base->view.flags_810&1u){scene_turret_generated_death_record(binding,base,head,reason);return RF_OK;}
    if(base->damage.effects.health>0) {
        rf_damage_request request={1000,UINT32_MAX,-1,0,UINT32_MAX,0};
        status=rf_scene_npc_damage(base_handle,&request,1,c->clock_bits,&c->effects,&amount);
        if(!status)status=c->feedback.status;if(status)return status;
    }
    if(base->damage.effects.health<=0) {
        status=rf_scene_npc_death_entry(base_handle,&entered);if(status)return status;
        if(entered){base->script_move.active=0;status=combat_death_start(binding->base_slot);if(status)return status;}
    }
    scene_turret_generated_death_record(binding,base,head,reason);return RF_OK;
}

/* Call only for a real terminal transition, before unregistering either side.
 * The generated helper marks/detaches before callback, so recursively reaching
 * the opposite owner's death hook is an immediate no-op. No retry on failure.
 * Non-generated actors are intentionally transparent to ordinary death paths. */
static int scene_turret_generated_death_join(uint32_t actor,uint32_t frame)
{
    scene_turret_generated_death_context c={0};scene_turret_generated_key key;
    float seconds;uint32_t i,resolved;int status;
    if(frame==UINT32_MAX)return RF_RANGE;
    for(i=0;i<scene_turret_generated_count;i++)if(scene_turret_generated[i].head_handle==actor||
        scene_turret_generated[i].base_handle==actor)break;
    if(i==scene_turret_generated_count)return RF_OK;
    if(scene_turret_generated_retired(scene_turret_generated[i].head_handle))return RF_OK;
    status=scene_turret_generated_identity(scene_turret_generated[i].head_handle,&key);if(status)return status;
    status=scene_turret_generated_resolve(key,&resolved);if(status)return status;
    if(resolved!=scene_turret_generated[i].head_handle)return RF_FORMAT;
    seconds=(float)frame/60.f;memcpy(&c.clock_bits,&seconds,4);
    c.feedback.now=(int32_t)((uint64_t)frame*1000/60%RF_TIMER_PERIOD);
    c.effects=(rf_damage_effect_backend){combat_predicate,combat_uid,combat_source,combat_burn,
        combat_random,combat_notify,combat_playing,combat_play,&c.feedback};
    return scene_turret_generated_death(actor,scene_turret_generated_death_dispatch,&c);
}
#endif
