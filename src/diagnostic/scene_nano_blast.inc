#ifndef RF_SCENE_NANO_BLAST_INCLUDED
#define RF_SCENE_NANO_BLAST_INCLUDED
/* 48905d..48910b: the radial Nano branch consumes before generic damage.
 * Call only after the existing CF5 cover and positive body-position falloff.
 * Original overlap uses object+3c (published), not physics+e4 (body center).
 * Original signed outer-shell falloff is deliberately not imported: the
 * shared positive admission and nonnegative armor/save contract stay intact.
 * No direct-contact rail override, damage factor, armor split or retaliation. */
static int scene_nano_blast_contact(uint32_t slot,uint32_t handle,const float origin[3],
    float radius,float amount,uint32_t *consumed)
{
    campaign_npc_body *owner;uint32_t cls,k;float overlap_radius,next_armor,delta[3];
    double squared=0;int status;
    if(!origin || !consumed || !isfinite(radius) || radius<=.1f ||
       !isfinite(amount) || amount<=0)return RF_RANGE;
    *consumed=0;owner=scene_ai_melee_npc_identity(slot,handle);
    if(!owner || owner->view.type!=0)return RF_OK;
    if(!campaign_seeds.items || !campaign_seeds.classes || slot>=campaign_seeds.records.count)return RF_FORMAT;
    cls=campaign_seeds.items[slot].class_index;if(cls>=campaign_seeds.class_count)return RF_FORMAT;
    if(!isfinite(owner->damage.effects.armor))return RF_FORMAT;
    if(!rf_entity_armor_immunity(owner->damage.effects.armor,campaign_seeds.classes[cls].physics.flags,
        owner->damage.effects.flags_814))return RF_OK;
    *consumed=1;
    overlap_radius=(float)((double)radius+1.0);if(!isfinite(overlap_radius))return RF_RANGE;
    for(k=0;k<3;k++) {
        double d;
        if(!isfinite(origin[k]) || !isfinite(owner->published[k]))return RF_FORMAT;
        d=(double)origin[k]-owner->published[k];if(d>FLT_MAX || d< -FLT_MAX)return RF_RANGE;
        delta[k]=(float)d;squared+=(double)delta[k]*delta[k];
    }
    /* 506fd0's strict overlap; a rejected shell still cannot spill to health. */
    if(!(squared<(double)overlap_radius*overlap_radius))return RF_OK;
    if(owner->object_flags&4u)return RF_OK;
    next_armor=fmaxf(0,owner->damage.effects.armor-amount);
    if(next_armor==0) {
        status=scene_capek_shield_break(owner);if(status)return status;
    }
    owner->damage.effects.armor=next_armor;
    return RF_OK;
}
#endif
