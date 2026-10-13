#ifndef RF_SCENE_CROUCH_FINISH_POLICY_INCLUDED
#define RF_SCENE_CROUCH_FINISH_POLICY_INCLUDED
/* Only the ordinary humanoid4068f4..406978 branch after relocation finishes.
 * The caller first owns ordinary tactical/recovery setup and special weapon
 * branches. These are private prospective fields, not live gameplay storage.
 * Independent attention4f8 service remains a separate required consumer. */
typedef struct scene_crouch_finish_policy {
    uint32_t stance;
    int32_t switch_due,attention_due,tactical_due;
    rf_random_state random;
} scene_crouch_finish_policy;
static int scene_crouch_finish_prepare(const scene_crouch_finish_policy *before,
    int32_t now,uint32_t difficulty,uint32_t humanoid,
    scene_crouch_finish_policy *out,uint32_t *changed)
{
    scene_crouch_finish_policy next;int expired,status;
    if(!before||!out||!changed||before->stance>1||difficulty>3||humanoid>1||
       now<0||now>RF_TIMER_PERIOD||before->switch_due< -1||before->switch_due>RF_TIMER_PERIOD||
       before->attention_due< -1||before->attention_due>RF_TIMER_PERIOD||
       before->tactical_due<0||before->tactical_due>RF_TIMER_PERIOD)return RF_RANGE;
    next=*before;
    if(!humanoid){*out=next;*changed=0;return RF_OK;}
    status=rf_timer_expired(next.switch_due,now,&expired);if(status)return status;
    if(!expired||(!next.stance&&difficulty>1)){*out=next;*changed=0;return RF_OK;}
    if(!next.stance){
        status=rf_timer_set(&next.switch_due,now,1500);if(status)return status;
        status=rf_timer_set(&next.attention_due,now,1500);if(status)return status;
        status=rf_timer_set(&next.tactical_due,now,1500);if(status)return status;
        next.stance=1;
    }else{
        status=rf_timer_set_random(&next.switch_due,now,3000,4000,&next.random);if(status)return status;
        next.stance=0;
    }
    *out=next;*changed=1;return RF_OK;
}
#endif
