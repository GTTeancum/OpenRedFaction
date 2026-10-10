#ifndef RF_SCENE_SNIPER_PENETRATION_INC
#define RF_SCENE_SNIPER_PENETRATION_INC
/* Shot-local4c9b20 object policy shared by sniper and authored-enabled
 * ordinary handheld and mounted Vauss scene adapters.
 * Initial descriptor27c -> projectile2f0 is4c7c2e..4c7c40. Power and visited
 * handles die with this synchronous shot; no persistent owner or save field.
 * World-metal exit/ricochet traversal is deliberately NOT approximated here. */
typedef struct scene_sniper_penetration {
    float initial,power,cursor,length,direction[3],start[3],delta[3];
    uint32_t enabled,count,handles[RF_WEAPON_PRECISION_HITS];
} scene_sniper_penetration;

static int scene_sniper_penetration_begin(uint32_t enabled,float power,
    const float position[3],const float delta[3],scene_sniper_penetration *out)
{
    scene_sniper_penetration value={0};double length2=0,length;uint32_t i;
    if(!out || !position || !delta || enabled>1 ||
       (enabled && (!isfinite(power) || power<=0)))return RF_RANGE;
    for(i=0;i<3;i++) {
        if(!isfinite(position[i]) || !isfinite(delta[i]))return RF_RANGE;
        length2+=(double)delta[i]*delta[i];
    }
    length=sqrt(length2);if(!isfinite(length) || length<=0)return RF_RANGE;
    value.length=(float)length;if(!isfinite(value.length) || value.length<=0)return RF_RANGE;
    for(i=0;i<3;i++)value.direction[i]=(float)((double)delta[i]/length);
    memcpy(value.start,position,12);memcpy(value.delta,delta,12);
    value.enabled=enabled;value.initial=value.power=enabled?power:0;
    *out=value;return RF_OK;
}
static uint32_t scene_sniper_penetration_visited(const scene_sniper_penetration *shot,uint32_t handle)
{
    uint32_t i;for(i=0;i<shot->count;i++)if(shot->handles[i]==handle)return 1;return 0;
}
/*4c9b48..4c9b73 scales this contact's fresh BASE damage, before spending its
 * target radius. It is not repeated multiplication of the prior hit's damage.
 * Preserve the explicit final binary32 store; finite guards are port safety. */
static int scene_sniper_penetration_damage(const scene_sniper_penetration *shot,float base,float *damage)
{
    float value;
    if(!shot || !damage || !isfinite(base) || base<0)return RF_RANGE;
    if(!shot->enabled){*damage=base;return RF_OK;}
    if(!isfinite(shot->initial) || shot->initial<=0 || !isfinite(shot->power) ||
       shot->power<=0 || shot->power>shot->initial)return RF_RANGE;
    value=(float)((((double)shot->power/shot->initial)+1.0)*(double)base*0.5);
    if(!isfinite(value) || value<0)return RF_RANGE;
    *damage=value;return RF_OK;
}
/*4c9b75..4c9bde: target object180 is body.bounds.radius, NOT a guessed
 * material resistance or a sphere's surface-to-surface chord. The original
 * stores power-.2f*radius, continues strictly above0, and advances from the
 * contact by normalized incoming direction*radius. It does not spend power
 * for clear-air travel. A full-handle once-only bound is explicit port policy.
 * Caller snapshots extent/point before damage can retire the target. */
static int scene_sniper_penetration_body(scene_sniper_penetration *shot,
    uint32_t handle,float extent,float fraction,const float point[3],const float end[3],
    uint32_t *more)
{
    scene_sniper_penetration value;uint32_t i;double cursor;
    if(!shot || !point || !end || !more || !isfinite(extent) || extent<0 ||
       !isfinite(fraction) || fraction<shot->cursor || fraction>1 ||
       shot->count>=RF_WEAPON_PRECISION_HITS)return RF_RANGE;
    if(!shot->enabled){*more=0;return RF_OK;}
    value=*shot;value.handles[value.count++]=handle;
    value.power=(float)((double)value.power-(double)extent*(double)0.2f);
    if(!isfinite(value.power))return RF_RANGE;
    if(value.power<=0 || value.count==RF_WEAPON_PRECISION_HITS) {
        *shot=value;*more=0;return RF_OK;
    }
    cursor=(double)fraction+(double)extent/value.length;
    if(cursor>=1){value.cursor=1;*shot=value;*more=0;return RF_OK;}
    value.cursor=(float)cursor;
    if(value.cursor>=1){*shot=value;*more=0;return RF_OK;}
    for(i=0;i<3;i++) {
        if(!isfinite(point[i]) || !isfinite(end[i]))return RF_RANGE;
        value.start[i]=(float)((double)point[i]+(double)value.direction[i]*extent);
        value.delta[i]=end[i]-value.start[i];
        if(!isfinite(value.start[i]) || !isfinite(value.delta[i]))return RF_RANGE;
    }
    /* Tiny/zero extents need no fabricated epsilon: visited handles guarantee
     * forward progress even when the rounded cursor cannot increase. */
    *shot=value;*more=1;return RF_OK;
}
#endif
