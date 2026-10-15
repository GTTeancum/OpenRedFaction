#ifndef RF_SCENE_PURSUIT_NOTICE_MIRRORS_INCLUDED
#define RF_SCENE_PURSUIT_NOTICE_MIRRORS_INCLUDED
/* UNCALLED private proposals only. Original entity+7d0 is AI-relative+530.
 * Port suppression already mirrors this source word across AI/view/firing;
 * campaign_weapon_reset_publish/reload copy whole view/firing words.
 * Therefore notice bit 20000 must agree in all three before capture or use.
 * These helpers neither dereference actors nor repair divergent live state.
 * Caller must qualify lifetime/identity and seal all inputs across callbacks.
 * Publish all three prepared words together at a callback-free boundary.
 * Never reuse this full-word proposal after unrelated flag changes. */
#define SCENE_PURSUIT_NOTICE_BIT 0x00020000u
typedef struct scene_pursuit_notice_mirrors {
    uint32_t ai_flags,view_flags,firing_flags;
} scene_pursuit_notice_mirrors;
static int scene_pursuit_notice_mirrors_valid(
    const scene_pursuit_notice_mirrors *words)
{
    if(!words)return RF_RANGE;
    if(((words->ai_flags^words->view_flags)|
        (words->ai_flags^words->firing_flags))&SCENE_PURSUIT_NOTICE_BIT)
        return RF_FORMAT;
    return RF_OK;
}
/* Save proposal is one canonical boolean; never serialize unrelated flags.
 * Outer codec/version/legacy-absence policy remains deliberately unwired. */
static int scene_pursuit_notice_capture(
    const scene_pursuit_notice_mirrors *words,uint32_t *notice)
{
    int status;
    if(!notice)return RF_RANGE;
    status=scene_pursuit_notice_mirrors_valid(words);
    if(status!=RF_OK)return status;
    *notice=!!(words->ai_flags&SCENE_PURSUIT_NOTICE_BIT);return RF_OK;
}
/* Used for peer-set, reached-cone clear or an admitted restore candidate.
 * Each mirror retains its own unrelated bits; no whole-word unification.
 * On failure result is untouched. A private in-place proposal is allowed. */
static int scene_pursuit_notice_prepare(
    const scene_pursuit_notice_mirrors *before,uint32_t notice,
    scene_pursuit_notice_mirrors *result)
{
    scene_pursuit_notice_mirrors out;uint32_t bit;int status;
    if(!result)return RF_RANGE;
    if(notice>1)return RF_FORMAT;
    status=scene_pursuit_notice_mirrors_valid(before);
    if(status!=RF_OK)return status;
    bit=notice?SCENE_PURSUIT_NOTICE_BIT:0;
    out.ai_flags=(before->ai_flags&~SCENE_PURSUIT_NOTICE_BIT)|bit;
    out.view_flags=(before->view_flags&~SCENE_PURSUIT_NOTICE_BIT)|bit;
    out.firing_flags=(before->firing_flags&~SCENE_PURSUIT_NOTICE_BIT)|bit;
    *result=out;return RF_OK;
}
/* Reached-cone composition: preserve the single-candidate bypass decision
 * with the all-mirror clear. Caller commits clear BEFORE later visibility.
 * Earlier range/schedule gates must not call this helper. No implicit LOS. */
typedef struct scene_pursuit_notice_consume_plan {
    scene_pursuit_notice_mirrors words;
    uint32_t bypass_facing;
} scene_pursuit_notice_consume_plan;
static inline int scene_pursuit_notice_consume(
    const scene_pursuit_notice_mirrors *before,
    scene_pursuit_notice_consume_plan *result)
{
    scene_pursuit_notice_consume_plan out;int status;
    if(!result)return RF_RANGE;
    status=scene_pursuit_notice_capture(before,&out.bypass_facing);
    if(status)return status;
    status=scene_pursuit_notice_prepare(before,0,&out.words);
    if(status)return status;
    *result=out;return RF_OK;
}
#endif
