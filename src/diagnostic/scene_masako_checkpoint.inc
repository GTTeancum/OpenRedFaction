#ifndef RF_SCENE_MASAKO_CHECKPOINT_INCLUDED
#define RF_SCENE_MASAKO_CHECKPOINT_INCLUDED
/* RFMB1 wraps RFCL/... inside optional RFSW. Header16, one row16:
 * parent authored UID, generated role key, phase, completed effect bits.
 * RFNC/RFCH own child pose, combat, inventory, shield, vitals and history;
 * RFVA/RFSV own the fighter. No callback or allocation during publication. */
typedef struct scene_masako_checkpoint_stage {uint32_t present,phase,effects,admitted,legacy;} scene_masako_checkpoint_stage;
static int scene_masako_checkpoint_values(uint32_t phase,uint32_t effects)
{
    return phase>SCENE_MASAKO_FINISHED||effects>3||
        (phase<SCENE_MASAKO_DYING&&effects)||
        (phase==SCENE_MASAKO_FINISHED&&effects!=3)?RF_FORMAT:RF_OK;
}
static int scene_masako_checkpoint_capture(unsigned char *p,uint32_t capacity,uint32_t nested,uint32_t *written)
{
    int status;if(!p||!written||nested>capacity)return RF_RANGE;
    *written=nested;if(!scene_masako.enabled)return RF_OK;
    status=scene_masako_live_valid();if(status)return status;
    if((uint64_t)nested+32>capacity)return RF_RANGE;
    memmove(p+16,p,nested);memcpy(p,"RFMB",4);
    scene_history_put(p+4,1);scene_history_put(p+8,nested);scene_history_put(p+12,1);
    p+=16+nested;scene_history_put(p,scene_masako.parent_uid);scene_history_put(p+4,SCENE_MASAKO_GENERATED_UID);
    scene_history_put(p+8,scene_masako.phase);scene_history_put(p+12,scene_masako.end_effects);
    *written=nested+32;return RF_OK;
}
static int scene_masako_checkpoint_prepare(const unsigned char *p,uint32_t bytes,
    scene_masako_checkpoint_stage *out,const unsigned char **inner,uint32_t *inner_bytes)
{
    scene_masako_checkpoint_stage value={0};uint32_t nested;int status;
    if(!out||!inner||!inner_bytes||(!p&&bytes))return RF_RANGE;
    *inner=p;*inner_bytes=bytes;
    if(bytes<4||memcmp(p,"RFMB",4)){
        value.legacy=scene_masako.enabled;*out=value;return RF_OK;
    }
    if(!scene_masako.enabled||bytes<32||scene_history_word(p+4)!=1||scene_history_word(p+12)!=1)return RF_FORMAT;
    nested=scene_history_word(p+8);if((uint64_t)nested+32!=bytes)return RF_FORMAT;
    const unsigned char *row=p+16+nested;
    if(scene_history_word(row)!=scene_masako.parent_uid||scene_history_word(row+4)!=SCENE_MASAKO_GENERATED_UID)return RF_FORMAT;
    value.present=1;value.phase=scene_history_word(row+8);value.effects=scene_history_word(row+12);
    status=scene_masako_checkpoint_values(value.phase,value.effects);if(status)return status;
    *out=value;*inner=p+16;*inner_bytes=nested;return RF_OK;
}
/* Pre-feature saves have no generated actor/history key. Add only a dormant
 * constructor-history key to this PRIVATE candidate; full join below requires
 * a living parent. A legacy dead fighter has ambiguous phase and rejects. */
static int scene_masako_checkpoint_legacy_history(const scene_masako_checkpoint_stage *stage,
    scene_campaign_history_checkpoint_stage *history)
{
    uint32_t slot;if(!stage||!history)return RF_RANGE;
    if(!stage->legacy)return RF_OK;
    return rf_campaign_actor_register(&history->actors,campaign_current_level,SCENE_MASAKO_GENERATED_UID,&slot);
}
static int scene_masako_checkpoint_admit(scene_masako_checkpoint_stage *stage,
    const scene_npc_checkpoint_restore_stage *npcs,const scene_passive_vehicle_checkpoint_stage *passive)
{
    const rf_npc_checkpoint_record *actor=NULL;const scene_passive_vehicle_saved *parent=NULL;
    if(!stage)return RF_RANGE;stage->admitted=0;
    if(!scene_masako.enabled)return stage->present||stage->legacy?RF_FORMAT:RF_OK;
    if(!npcs||!passive||!scene_masako.ready)return RF_FORMAT;
    for(uint32_t i=0;i<npcs->count;i++)if(npcs->entries[i].saved.uid==SCENE_MASAKO_GENERATED_UID){
        if(actor)return RF_FORMAT;actor=&npcs->entries[i].saved;
        if(npcs->entries[i].slot!=scene_masako.actor_slot||actor->class_id!=scene_masako.class_index)return RF_FORMAT;
    }
    for(uint32_t i=0;i<passive->count;i++)if(passive->rows[i].uid==scene_masako.parent_uid){
        if(parent)return RF_FORMAT;parent=passive->rows+i;
    }
    if(!parent||!parent->loaded||!parent->has_damage)return RF_FORMAT;
    if(stage->legacy){
        if(actor||parent->destroyed||parent->health<=0)return RF_NOT_FOUND;
        stage->phase=SCENE_MASAKO_DORMANT;stage->effects=0;stage->admitted=1;return RF_OK;
    }
    if(!stage->present||!actor)return RF_FORMAT;
    if(stage->phase==SCENE_MASAKO_DORMANT){
        if(parent->destroyed||parent->health<=0||actor->retired||actor->dead_pose||actor->health<=0||!(actor->flags&0x4000u))return RF_FORMAT;
    }else{
        if(!parent->destroyed||parent->health>0||!(parent->object_flags&0x4000u)||parent->attached)return RF_FORMAT;
        if(stage->phase==SCENE_MASAKO_ACTIVE){
            if(actor->retired||actor->dead_pose||actor->health<=0||(actor->flags&0x4000u))return RF_FORMAT;
        }else if(actor->health>0||(!actor->retired&&!actor->dead_pose))return RF_FORMAT;
        if(stage->phase==SCENE_MASAKO_DYING&&actor->retired)return RF_FORMAT;
    }
    stage->admitted=1;return RF_OK;
}
static void scene_masako_checkpoint_assign(const scene_masako_checkpoint_stage *stage)
{
    if(!scene_masako.enabled||!stage||!stage->admitted)return;
    scene_masako.phase=stage->phase;scene_masako.end_effects=stage->effects;
    rf_scene_masako_phase[4]=scene_masako.phase;
}
#endif
