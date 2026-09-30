#ifndef RF_SCENE_TURRET_GENERATED_ACTIVATION_INCLUDED
#define RF_SCENE_TURRET_GENERATED_ACTIVATION_INCLUDED
/* Include after generated ownership, turret combat types and NPC motion APIs.
 * Original403076/408ea0/407375..40741a provide identity/ready/action semantics.
 * Acquisition uses the port's existing turret target/cover policy, not a claim
 * of original awareness scheduling. No weapons, targets or readiness on load. */
typedef struct scene_turret_generated_activation_state {
    uint32_t head,last_frame,pending;
} scene_turret_generated_activation_state;
static scene_turret_generated_activation_state scene_turret_generated_activation[SCENE_TURRET_OWNER_LIMIT];
uint32_t rf_scene_turret_activation[8]; /* checks,stimuli,ready,action starts,held,unarmed skips,last base UID,error */

/* Reset before factory startup and after checkpoint assignment. Restored
 * flags/playback remain authoritative; this only drops transient frame guards
 * and optional-action tracking. Call after all restored rows publish. */
static void scene_turret_generated_activation_reset(void)
{
    memset(scene_turret_generated_activation,0,sizeof(scene_turret_generated_activation));
    memset(rf_scene_turret_activation,0,sizeof(rf_scene_turret_activation));
}
static int scene_turret_generated_activation_initialize(uint32_t head)
{
    scene_turret_generated_binding *b=scene_turret_generated_find(head);
    campaign_npc_body *base=b?scene_turret_generated_base(b):NULL;
    scene_turret_generated_activation_state *s;
    if(!base)return RF_NOT_FOUND;
    s=scene_turret_generated_activation+(b-scene_turret_generated);
    s->head=head;s->last_frame=UINT32_MAX;s->pending=0;
    base->view.flags_7d0|=0x20000000u; /* Class identity; never sets ready bit0. */
    base->firing.flags_7d0=base->view.flags_7d0;
    base->view.weapons[0]=base->view.weapons[1]=-1;
    return RF_OK;
}
/* Call before ordinary NPC pending-shot/weapon/aim processing. The base's
 * actor remains damageable and animated; all firing belongs to its child. */
static uint32_t scene_turret_generated_activation_suppresses(uint32_t actor)
{
    for(uint32_t i=0;i<scene_turret_generated_count;i++) {
        const scene_turret_generated_binding *b=scene_turret_generated+i;
        if(b->base_handle==actor&&scene_turret_generated_base(b)) {
            ++rf_scene_turret_activation[5];return 1;
        }
    }
    return 0;
}
/* Add this to generated_ready's gate. Readiness is a retained latch, not
 * permission to bypass a later Set_AI_Mode(Catatonic), Hide or scripted pose.
 * Waiting2 can reacquire; motion-only11 is handled at stimulus acquisition. */
static uint32_t scene_turret_generated_activation_allows(uint32_t head)
{
    const scene_turret_generated_binding *b=scene_turret_generated_find(head);
    campaign_npc_body *base=b?scene_turret_generated_base(b):NULL;
    if(!base||scene_turret_generated_retired(head)||b->death_dispatched||base->damage.effects.health<=0||
       (base->view.flags_810&1u)||(base->object_flags&(2u|0x4000u))||
       (base->view.flags_7c&(2u|0x4000u))||(base->view.flags_7d0&0x100u)||
       base->ai_mode.action_280==1||base->view.action_520==1||base->script_animation.active)return 0;
    return campaign_enemy_mode_admits(base,1,0);
}
static int scene_turret_generated_activation_pose(uint32_t slot,
    campaign_npc_body **base,rf_entity_pose **pose,rf_entity_playback_model **model)
{
    uint32_t cls;int status;
    if(slot>=scene_turret_generated_count)return RF_RANGE;
    status=campaign_npc_motion_owner(scene_turret_generated[slot].base_handle,base,&cls,pose);if(status)return status;
    if(!campaign_playback_resources.models||(*pose)->skeleton>=campaign_playback_resources.model_count||
       (*base)->selection.mapping.skeleton!=(*pose)->skeleton)return RF_FORMAT;
    *model=campaign_playback_resources.models+(*pose)->skeleton;return RF_OK;
}
static int scene_turret_generated_activation_finish(campaign_npc_body *base,rf_entity_pose *pose)
{
    int status;
    /* Base has authored attack_stand. Missing state follows the existing
     * request-state fallback rather than manufacturing a motion. */
    if(!rf_motion_has_state(&pose->controller,1)) {
        status=rf_motion_request_state(&pose->controller,base->selection.mapping.states,1,.25f);if(status)return status;
    }
    if(!(base->view.flags_7d0&1u))++rf_scene_turret_activation[2];
    base->view.flags_7d0|=0x20000001u;base->firing.flags_7d0=base->view.flags_7d0;
    base->view.flags_810|=0x10u;
    base->damage.effects.flags_810=base->firing.flags_810=base->view.flags_810;
    base->view.weapons[0]=base->view.weapons[1]=-1;
    return RF_OK;
}
/* Called with the SAME backend as head combat, immediately before combat_tick.
 * Its hostility/disguise and world/clutter cover rules remain authoritative.
 * Base/head candidate exclusion belongs in that shared backend. */
static int scene_turret_generated_activation_tick(uint32_t frame,const scene_turret_combat_backend *be)
{
    uint32_t i,j,k;int status=RF_OK;
    if(!scene_turret_generated_count)return RF_OK;
    if(!be||!be->target||!be->blocked||frame==UINT32_MAX||!scene_turret_combat_ready)return RF_RANGE;
    if(!frame)return RF_OK; /* Ordinary checkpoint assignment follows frame0. */
    for(i=0;i<scene_turret_generated_count;i++) {
        const scene_turret_generated_binding *b=scene_turret_generated+i;
        scene_turret_generated_activation_state *s=scene_turret_generated_activation+i;
        scene_turret_owner *head=scene_turret_lookup(b->head_handle);
        campaign_npc_body *base=scene_turret_generated_base(b);rf_entity_pose *pose;rf_entity_playback_model *model;
        uint32_t stimulus=0,alive,affiliation;float eye[3],cone;int active;int32_t motion;
        if(!base||!head||head->dead||b->death_dispatched||scene_turret_generated_retired(b->head_handle))continue;
        if(s->head!=b->head_handle){status=scene_turret_generated_activation_initialize(b->head_handle);if(status)goto fail;}
        if(s->last_frame==frame)continue;s->last_frame=frame;
        /* Weapon-reset/pain code mirrors these words in either direction.
         * Keep the base identity and retained ready latch in both owners. */
        base->view.flags_7d0|=0x20000000u;base->firing.flags_7d0=base->view.flags_7d0;
        base->view.weapons[0]=base->view.weapons[1]=-1;
        if(!scene_turret_generated_activation_allows(b->head_handle)) {++rf_scene_turret_activation[4];continue;}
        if(head->view.flags_7c&(2u|0x4000u))continue;
        status=scene_turret_generated_activation_pose(i,&base,&pose,&model);if(status)goto fail;
        if(base->view.flags_7d0&1u) {
            s->pending=0;status=scene_turret_generated_activation_finish(base,pose);if(status)goto fail;continue;
        }
        motion=base->selection.mapping.actions[28];
        if(motion>=0) {
            status=rf_motion_action_active(&pose->playback,model->resources,model->count,
                base->selection.mapping.actions,28,&active);if(status)goto fail;
            /* An active restored action recovers pending ownership without
             * replay. Installed Auto Turret has no action28; see save notes. */
            if(active){s->pending=1;continue;}
            if(s->pending){s->pending=0;status=scene_turret_generated_activation_finish(base,pose);if(status)goto fail;continue;}
        }
        stimulus=base->combat_alert||base->combat_scripted;
        if(!stimulus) {
            if(frame%30!=b->base_slot%30)continue;
            status=scene_turret_target(b->head_handle,eye,&affiliation,&alive);if(status)goto fail;if(!alive)continue;
            if(head->class_index>=campaign_seeds.class_count){status=RF_FORMAT;goto fail;}
            memcpy(&cone,&campaign_seeds.classes[head->class_index].vitals.field_764,4);
            if(!isfinite(cone)||cone< -1||cone>1){status=RF_FORMAT;goto fail;}
            ++rf_scene_turret_activation[0];
            for(j=0;j<be->count;j++) {
                scene_turret_combat_target candidate={0};float delta[3],distance=0,dot=0;uint32_t blocked;
                status=be->target(be->context,b->head_handle,j,&candidate);if(status==RF_NOT_FOUND)continue;if(status)goto fail;
                if(!candidate.alive||!candidate.hostile||candidate.handle==UINT32_MAX||
                   candidate.handle==b->head_handle||candidate.handle==b->base_handle||
                   !campaign_enemy_mode_admits(base,candidate.moving,0))continue;
                for(k=0;k<3;k++) {
                    if(!isfinite(candidate.point[k])){status=RF_FORMAT;goto fail;}
                    delta[k]=candidate.point[k]-eye[k];distance+=delta[k]*delta[k];dot+=head->basis[6+k]*delta[k];
                }
                if(distance<.0001f||distance>scene_turret_primary.ai_attack_range*scene_turret_primary.ai_attack_range||
                   dot<cone*sqrtf(distance))continue;
                status=be->blocked(be->context,b->head_handle,eye,delta,&blocked);if(status)goto fail;
                if(!blocked){stimulus=1;break;}
            }
        }
        if(!stimulus)continue;++rf_scene_turret_activation[1];rf_scene_turret_activation[6]=b->key.base_uid;
        if(motion<0) { /* Original407375: absent ready action does not deadlock. */
            status=scene_turret_generated_activation_finish(base,pose);if(status)goto fail;continue;
        }
        {int32_t sounds[45],sound;
         status=campaign_npc_motion_require(pose->skeleton,(uint32_t)motion);if(status)goto fail;
         for(k=0;k<45;k++)sounds[k]=-1;
         status=rf_motion_start_action(&pose->playback,model->resources,model->count,
             base->selection.mapping.actions,sounds,28,1,0,0,&sound);if(status)goto fail;
         status=rf_motion_action_active(&pose->playback,model->resources,model->count,
             base->selection.mapping.actions,28,&active);if(status)goto fail;
         /* A malformed looping action must not be retried forever as if it
          * started. Its declared data requires correction, not forced ready. */
         if(!active){status=RF_FORMAT;goto fail;}
         s->pending=1;++rf_scene_turret_activation[3];
         if(base->selection.action_sounds[28]&&base->selection.action_sounds[28][0])
             combat_sound(base->selection.action_sounds[28],base->eye_position);}
    }
    return RF_OK;
fail:rf_scene_turret_activation[7]=(uint32_t)status;return status;
}
/* RFTU2 stores the ready latch, not this optional-action pending word. Installed
 * Auto Turret has action28 absent. Reject a modified class's in-flight ready
 * transition until its pending state has a codec; never silently restart it. */
static int scene_turret_generated_activation_checkpoint_admit(void)
{
    for(uint32_t i=0;i<scene_turret_generated_count;i++) {
        campaign_npc_body *base=scene_turret_generated_base(scene_turret_generated+i);
        rf_entity_pose *pose;rf_entity_playback_model *model;int active,status;
        if(!base||base->damage.effects.health<=0||(base->view.flags_7d0&1u))continue;
        if(base->selection.mapping.actions[28]<0)continue;
        if(scene_turret_generated_activation[i].pending)return RF_NOT_FOUND;
        status=scene_turret_generated_activation_pose(i,&base,&pose,&model);if(status)return status;
        status=rf_motion_action_active(&pose->playback,model->resources,model->count,
            base->selection.mapping.actions,28,&active);if(status)return status;
        if(active)return RF_NOT_FOUND;
    }
    return RF_OK;
}
#endif
