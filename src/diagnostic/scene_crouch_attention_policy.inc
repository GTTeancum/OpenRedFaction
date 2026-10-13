#ifndef RF_SCENE_CROUCH_ATTENTION_POLICY_INCLUDED
#define RF_SCENE_CROUCH_ATTENTION_POLICY_INCLUDED
/*406146..4061fb, private policy only. The caller must resolve original
 * AI+29c/+29d gates, behavior+2b4 and target+2c0 before calling this helper.
 * player_dot is the original normalized actor-minus-player-eye vector dot
 * player forward; threshold589444 is binary32 0.95 (0x3f733333).
 * No visibility query, live receipt update or invented gate is done here. */
static int scene_crouch_attention_prepare(const scene_crouch_finish_policy *before,
    int32_t now,uint32_t difficulty,int32_t behavior,uint32_t target_is_player,
    uint32_t gate_29c,uint32_t gate_29d,float player_dot,
    scene_crouch_finish_policy *out,uint32_t *tactical_restarted)
{
    scene_crouch_finish_policy next;int expired,status;
    if(!before||!out||!tactical_restarted||difficulty>3||target_is_player>1||
       gate_29c>1||gate_29d>1||!isfinite(player_dot)||
       before->attention_due< -1||before->attention_due>RF_TIMER_PERIOD||
       now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    next=*before;
    status=rf_timer_expired(next.attention_due,now,&expired);if(status)return status;
    if(!expired){*out=next;*tactical_restarted=0;return RF_OK;}
    if(difficulty<=1){
        status=rf_timer_set_random(&next.attention_due,now,600,1200,&next.random);
        if(status)return status;
    }
    if(behavior!=11&&target_is_player&&(gate_29c||gate_29d)&&player_dot>.95f){
        status=rf_timer_set(&next.tactical_due,now,0);if(status)return status;
        *out=next;*tactical_restarted=1;return RF_OK;
    }
    *out=next;*tactical_restarted=0;return RF_OK;
}
#endif
