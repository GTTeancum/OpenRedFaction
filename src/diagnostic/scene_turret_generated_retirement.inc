#ifndef RF_SCENE_TURRET_GENERATED_RETIREMENT_INCLUDED
#define RF_SCENE_TURRET_GENERATED_RETIREMENT_INCLUDED
/* Script removal is not death. Include after generated ownership/activation,
 * combat, scene_burning and turret death-presentation owners. Keep the bounded
 * head wrapper registered until ordinary level teardown: combat sidecars and
 * other saved/live references borrow its stable address. Same tombstone policy
 * as campaign_remove_object's clutter branch. No damage or death dispatch. */
enum {SCENE_TURRET_GENERATED_BASE_REMOVED=1,SCENE_TURRET_GENERATED_HEAD_REMOVED=2};
typedef struct scene_turret_generated_retirement_state {
    uint32_t base,head,reason;
} scene_turret_generated_retirement_state;
static scene_turret_generated_retirement_state scene_turret_generated_retirements[SCENE_TURRET_OWNER_LIMIT];
uint32_t rf_scene_turret_retirement[6]; /* base removals,head removals,idempotent calls,last base UID,last reason,error */
uint32_t rf_scene_turret_retirement_probe[8]; /* UID,base registered,head registered,head flags,link,health bits,dead,shots */

static void scene_turret_generated_retirement_probe(uint32_t index,uint32_t reason)
{
    const scene_turret_generated_binding *b=scene_turret_generated+index;
    const scene_turret_owner *head=scene_turret_lookup(b->head_handle);
    const campaign_npc_body *base=campaign_npc_bodies+b->base_slot;
    uint32_t *p=rf_scene_turret_retirement_probe;(void)reason;
    p[0]=b->key.base_uid;p[1]=base->registration.view!=NULL;p[2]=head->registration.view!=NULL;
    p[3]=head->view.flags_7c;p[4]=(uint32_t)head->view.linked_handle;
    memcpy(p+5,&head->damage.effects.health,4);p[6]=head->dead;p[7]=rf_scene_turret_shots[0];
}

/* Checkpoint code appears earlier in scene.c. These narrow APIs keep runtime
 * sidecar storage private while sharing the admitted generation identities. */
static int scene_turret_generated_retirement_read(uint32_t index,uint32_t *reason)
{
    const scene_turret_generated_binding *b;const scene_turret_generated_retirement_state *r;
    if(!reason||index>=scene_turret_generated_count)return RF_RANGE;
    b=scene_turret_generated+index;r=scene_turret_generated_retirements+index;
    if(r->reason>SCENE_TURRET_GENERATED_HEAD_REMOVED ||
       (r->reason&&(r->base!=b->base_handle||r->head!=b->head_handle)))return RF_FORMAT;
    *reason=r->reason;return RF_OK;
}
/* Assignment-only: caller admitted row/index before NPC publication. A removed
 * base's registration can now be absent; the stable binding/array slots remain.
 * No unregister, damage, presentation or fallible callbacks during commit. */
static void scene_turret_generated_retirement_assign(uint32_t index,uint32_t reason)
{
    const scene_turret_generated_binding *b=scene_turret_generated+index;
    scene_turret_generated_retirement_state *r=scene_turret_generated_retirements+index;
    const scene_turret_owner *head=scene_turret_lookup(b->head_handle);
    uint32_t slot=(uint32_t)(head-scene_turrets);
    memset(r,0,sizeof(*r));
    if(reason){
        campaign_npc_body *base=campaign_npc_bodies+b->base_slot;
        r->base=b->base_handle;r->head=b->head_handle;r->reason=reason;
        /* RFNC owns action/vitals; explicitly clear the ready mirror that its
         * generic actor codec does not represent. Never revive an orphan. */
        base->view.flags_7d0&=~1u;base->firing.flags_7d0=base->view.flags_7d0;
        base->view.flags_810&=~0x10u;
        base->damage.effects.flags_810=base->firing.flags_810=base->view.flags_810;
    }
    memset(scene_turret_death_pending_rows+slot,0,sizeof(scene_turret_death_pending_rows[slot]));
    memset(scene_turret_generated_activation+index,0,sizeof(scene_turret_generated_activation[index]));
    scene_turret_generated_retirement_probe(index,reason);
}

/* Before new generated factory startup, or after generated_close at teardown.
 * Never erase a removed-pair marker while those level bindings remain live. */
static int scene_turret_generated_retirement_reset(void)
{
    if(scene_turret_generated_count)return RF_RANGE;
    memset(scene_turret_generated_retirements,0,sizeof(scene_turret_generated_retirements));
    memset(rf_scene_turret_retirement,0,sizeof(rf_scene_turret_retirement));
    memset(rf_scene_turret_retirement_probe,0,sizeof(rf_scene_turret_retirement_probe));return RF_OK;
}
static uint32_t scene_turret_generated_retired(uint32_t head)
{
    for(uint32_t i=0;i<scene_turret_generated_count;i++) {
        const scene_turret_generated_binding *b=scene_turret_generated+i;
        const scene_turret_generated_retirement_state *r=scene_turret_generated_retirements+i;
        if(b->head_handle==head)return r->reason&&r->head==head&&r->base==b->base_handle;
    }
    return 0;
}
/* Call at the beginning of campaign_remove_object, before either endpoint can
 * unregister. handled1 means a generated HEAD is fully retired. handled0 means
 * either unrelated, or a BASE whose existing NPC removal must now continue.
 * All fallible admission precedes mutation; ordinary base unregister follows
 * this call in the caller, with no tick/reentrant callback between them. */
static int scene_turret_generated_remove(uint32_t actor,int32_t now,uint32_t *handled)
{
    uint32_t i,slot,reason;scene_turret_generated_binding *b;
    scene_turret_generated_retirement_state *r;scene_turret_owner *head;campaign_npc_body *base;
    rf_entity_ai_transition_state next={0};int status;
    if(!handled||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;*handled=0;
    for(i=0;i<scene_turret_generated_count;i++)
        if(scene_turret_generated[i].base_handle==actor||scene_turret_generated[i].head_handle==actor)break;
    if(i==scene_turret_generated_count)return RF_OK;
    b=scene_turret_generated+i;r=scene_turret_generated_retirements+i;
    reason=actor==b->base_handle?SCENE_TURRET_GENERATED_BASE_REMOVED:SCENE_TURRET_GENERATED_HEAD_REMOVED;
    head=scene_turret_lookup(b->head_handle);base=scene_turret_generated_base(b);
    if(!head||(reason==SCENE_TURRET_GENERATED_BASE_REMOVED&&!base)){status=RF_NOT_FOUND;goto fail;}
    if(r->reason&&(r->head!=b->head_handle||r->base!=b->base_handle)){status=RF_FORMAT;goto fail;}
    slot=(uint32_t)(head-scene_turrets);
    if(slot>=scene_turret_count||slot>=SCENE_TURRET_OWNER_LIMIT||head->view.occupant_count||head->view.weapon_owner||
       head->damage.effects.burn||(head->view.linked_handle!=-1&&head->view.linked_handle!=(int32_t)b->base_handle)) {
        status=RF_NOT_FOUND;goto fail;
    }
    if(scene_turret_combat_ready&&scene_turret_combat_states[slot].handle!=b->head_handle){status=RF_FORMAT;goto fail;}
    if(base&&base->damage.effects.health>0&&!(base->view.flags_810&1u)) {
        next=base->ai_mode;status=rf_entity_ai_set_action(&next,1,UINT32_MAX,UINT32_MAX,(float)now,0,0);
        if(status)goto fail;
    }
    if(r->reason)++rf_scene_turret_retirement[2];
    else ++rf_scene_turret_retirement[reason==SCENE_TURRET_GENERATED_BASE_REMOVED?0:1];
    /* A later base removal supersedes an earlier head-only retirement. */
    r->base=b->base_handle;r->head=b->head_handle;
    if(!r->reason||reason==SCENE_TURRET_GENERATED_BASE_REMOVED)r->reason=reason;
    head->view.flags_7c|=2u|0x4000u;head->view.linked_handle=-1;
    head->target=UINT32_MAX;head->fire_due=0;
    if(scene_turret_combat_ready)scene_turret_combat_release(head,scene_turret_combat_states+slot);
    memset(scene_turret_death_pending_rows+slot,0,sizeof(scene_turret_death_pending_rows[slot]));
    memset(scene_turret_generated_activation+i,0,sizeof(scene_turret_generated_activation[i]));
    if(base) {
        /* Removal must not manufacture a coupled death or mutate surviving
         * base health. Retain its visible body, but stop orphaned control. */
        base->view.flags_7d0&=~1u;base->firing.flags_7d0=base->view.flags_7d0;
        base->view.flags_810&=~0x10u;base->damage.effects.flags_810=base->firing.flags_810=base->view.flags_810;
        base->view.weapons[0]=base->view.weapons[1]=-1;
        if(base->damage.effects.health>0&&!(base->view.flags_810&1u)) {
            base->ai_mode=next;base->view.action_520=next.action_280;
            campaign_pursuit_stop(base);base->script_move.active=base->script_move.stop=0;
            base->navigation.retained.count=0;base->script_look.active=base->script_shoot.active=0;
            base->combat_scripted=base->combat_target=base->combat_alert=0;
            base->combat_due=base->combat_navigation_due=base->combat_burst_remaining=0;
            while(base->script_once.count)campaign_script_single_pop(base,0);
        }
        if(reason==SCENE_TURRET_GENERATED_BASE_REMOVED)scene_burning_extinguish(b->base_handle);
    }
    rf_scene_turret_retirement[3]=b->key.base_uid;rf_scene_turret_retirement[4]=r->reason;
    scene_turret_generated_retirement_probe(i,r->reason);
    *handled=reason==SCENE_TURRET_GENERATED_HEAD_REMOVED;return RF_OK;
fail:rf_scene_turret_retirement[5]=(uint32_t)status;return status;
}
#endif
