#ifndef RF_SCENE_TURRET_GENERATED_ATTACK_CHECKPOINT_INCLUDED
#define RF_SCENE_TURRET_GENERATED_ATTACK_CHECKPOINT_INCLUDED
/* RFNC11 combat.active3 is Attack against a generated AutoHead. combat.event
 * stores its authored BASE UID, never a synthetic head UID/registry handle.
 * active2 remains the authored-target namespace. Include after NPC restore
 * stage + generated RFTU definitions; prototype scalar APIs before capture.
 * All helper calls are read-only; composed world admission precedes publish. */
static int scene_turret_generated_attack_key(uint32_t handle,uint32_t *base_uid)
{
    scene_turret_generated_key key;scene_turret_generated_binding *b;
    const scene_turret_owner *head;const campaign_npc_body *base;int status;
    if(!base_uid)return RF_RANGE;
    status=scene_turret_generated_identity(handle,&key);if(status)return status;
    b=scene_turret_generated_find(handle);head=scene_turret_lookup(handle);
    base=b?scene_turret_generated_base(b):NULL;
    if(!head||!base||key.role!=SCENE_TURRET_GENERATED_ROLE_AUTO_HEAD||!key.base_uid||key.base_uid==UINT32_MAX||
       b->death_dispatched||scene_turret_generated_retired(handle)||head->dead||
       !isfinite(head->damage.effects.health)||head->damage.effects.health<=0||
       (head->view.flags_7c&(2u|0x4000u))||(head->view.flags_810&1u)||
       !isfinite(base->damage.effects.health)||base->damage.effects.health<=0||(base->view.flags_810&1u))return RF_FORMAT;
    *base_uid=key.base_uid;return RF_OK;
}

/* Only RF_NOT_FOUND allows caller fallback to existing authored UID resolver;
 * malformed/dead generated targets must never turn into attacks on their base. */
static int scene_turret_generated_attack_handle(uint32_t base_uid,uint32_t *handle)
{
    scene_turret_generated_key key={base_uid,SCENE_TURRET_GENERATED_ROLE_AUTO_HEAD};
    uint32_t resolved,uid;int status;
    if(!handle||!base_uid||base_uid==UINT32_MAX)return RF_RANGE;
    status=scene_turret_generated_resolve(key,&resolved);if(status)return status;
    status=scene_turret_generated_attack_key(resolved,&uid);if(status)return status;
    if(uid!=base_uid)return RF_FORMAT;*handle=resolved;return RF_OK;
}

static int scene_turret_generated_attack_admit(const scene_npc_checkpoint_restore_stage *npcs,
    const scene_turret_generated_checkpoint_stage *generated)
{
    if(!npcs)return RF_RANGE;
    for(uint32_t i=0;i<npcs->count;i++) {
        const scene_npc_checkpoint_restore_entry *e=npcs->entries+i;
        const scene_turret_generated_saved *target=NULL;uint32_t handle;int status;
        if(e->saved.combat.active!=3)continue;
        if(!generated)return RF_FORMAT;
        status=scene_turret_generated_attack_handle(e->saved.combat.event,&handle);if(status)return status;
        if(handle!=e->attack_target_handle||handle==e->handle)return RF_FORMAT;
        for(uint32_t j=0;j<generated->count;j++) {
            const scene_turret_generated_saved *r=generated->rows+j;
            if(r->key.base_uid==e->saved.combat.event&&r->key.role==SCENE_TURRET_GENERATED_ROLE_AUTO_HEAD) {
                if(target)return RF_FORMAT;target=r;
            }
        }
        if(!target||target->value.handle!=handle||target->removal_reason||target->death_dispatched||
           target->base.retired||!isfinite(target->base.health)||target->base.health<=0||
           (target->base.flags_810&1u)||target->value.dead||
           !isfinite(target->value.health)||target->value.health<=0||
           (target->value.flags_810&1u)||(target->value.flags&(2u|0x4000u)))return RF_FORMAT;
        /* RFTU companion admission separately requires exact agreement with
         * the saved RFNC base. Fresh authored liveness is never sufficient. */
    }
    return RF_OK;
}

/* Assignment telemetry only; call immediately after ordinary NPC combat state
 * publication, before simulation. No lookup failure can mutate restored state. */
uint32_t rf_scene_turret_generated_attack_restore[12];
static void scene_turret_generated_attack_record(const campaign_npc_body *o,
    const rf_npc_checkpoint_record *row,uint32_t frame)
{
    uint32_t *p=rf_scene_turret_generated_attack_restore;
    if(row->combat.active!=3)return;
    ++p[0];p[1]=row->uid;p[2]=row->combat.event;p[3]=o->combat_target;
    p[4]=o->combat_scripted;p[5]=o->combat_alert;p[6]=o->combat_due;
    p[7]=o->combat_reload_due;p[8]=o->combat_burst_remaining;p[9]=frame;
    p[10]=row->combat.due_remaining;p[11]=campaign_enemy_spread_random.value;
}
#endif
