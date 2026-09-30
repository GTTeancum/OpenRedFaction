#ifndef RF_SCENE_TURRET_ATTACK_CHECKPOINT_INCLUDED
#define RF_SCENE_TURRET_ATTACK_CHECKPOINT_INCLUDED
/* NPC scripted Attack links use stable turret UIDs, never registry slots.
 * Include after scene_turret_checkpoint.inc and NPC restore-stage definitions.
 * Forward-declare the two scalar helpers before the NPC capture/restore
 * includes, then use them as the final target-resolution fallback. Ordinary
 * world load must call admit after NPC/RFTU staging and before publication.
 * No target is published by this helper and no transient global is required. */
static int scene_turret_attack_checkpoint_uid(uint32_t handle,uint32_t *uid)
{
    const scene_turret_owner *o;
    if(!uid||!handle||handle==UINT32_MAX)return RF_RANGE;
    o=scene_turret_lookup(handle);
    if(!o)return RF_NOT_FOUND;
    if(!o->uid||o->uid==UINT32_MAX||o->dead||!isfinite(o->damage.effects.health)||
       o->damage.effects.health<=0||(o->view.flags_810&1u)||
       (o->damage.effects.flags_810&1u))return RF_FORMAT;
    *uid=o->uid;return RF_OK;
}

static int scene_turret_attack_checkpoint_handle(uint32_t uid,uint32_t *handle)
{
    uint32_t i,resolved;int status;
    if(!handle||!uid||uid==UINT32_MAX)return RF_RANGE;
    for(i=0;i<scene_turret_count;i++)if(scene_turrets[i].uid==uid) {
        const scene_turret_owner *o=scene_turrets+i;
        /* Match owner identity first, then validate generation and liveness.
         * A dead/missing turret must not fall through to another owner type. */
        status=scene_turret_attack_checkpoint_uid(o->registration.handle,&resolved);
        if(status)return RF_FORMAT;
        if(resolved!=uid)return RF_FORMAT;
        *handle=o->registration.handle;return RF_OK;
    }
    return RF_FORMAT; /* Same unknown-target rejection as NPC restore. */
}

static int scene_turret_attack_checkpoint_admit(
    const scene_npc_checkpoint_restore_stage *npcs,
    const scene_turret_checkpoint_stage *turrets)
{
    uint32_t i,j,handle;int status;
    if(!npcs)return RF_RANGE;
    if(turrets&&turrets->count!=scene_turret_count)return RF_FORMAT;
    for(i=0;i<npcs->count;i++) {
        const scene_npc_checkpoint_restore_entry *e=npcs->entries+i;
        uint32_t uid=e->saved.combat.event;
        if(e->saved.combat.active!=2)continue;
        for(j=0;j<scene_turret_count;j++)if(scene_turrets[j].uid==uid) {
            status=scene_turret_attack_checkpoint_handle(uid,&handle);
            if(status)return status;
            if(handle!=e->attack_target_handle)return RF_FORMAT;
            if(turrets) {
                const scene_turret_saved *row=turrets->rows+j;
                /* Fresh authored health cannot authorize an Attack against a
                 * turret destroyed in the saved world. RFTU prepare already
                 * establishes row order; recheck the linked identity here. */
                if(row->uid!=uid||row->handle!=handle||row->dead||
                   !isfinite(row->health)||row->health<=0||
                   (row->flags_810&1u))return RF_FORMAT;
            }
            /* Legacy saves without RFTU retain authored turret state; the
             * registered/alive test above applies unchanged in that case. */
            break;
        }
    }
    return RF_OK;
}
#endif
