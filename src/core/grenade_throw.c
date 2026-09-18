#include "rf/grenade_throw.h"
static int valid(const rf_grenade_throw *s)
{
    return s && s->phase<=2 && s->held<=3 && s->alternate<=1 &&
        s->cooldown<=180 && s->ticks<=102;
}
int rf_grenade_throw_step(rf_grenade_throw *state,uint32_t trigger,uint32_t selected,
    int32_t reserve,rf_grenade_throw_event *out)
{
    rf_grenade_throw next;rf_grenade_throw_event event={0,0};uint32_t edges;
    if(!valid(state) || !out || trigger>3 || selected>1 || reserve<0)return RF_RANGE;
    next=*state;edges=trigger&~next.held;next.held=trigger;
    if(next.cooldown)--next.cooldown;
    if(next.phase==1){
        if(!selected){next.phase=0;next.ticks=0;}
        else{
            if(next.ticks)--next.ticks;
            if(!next.ticks){
                if(reserve>0){next.phase=2;event.kind=RF_GRENADE_THROW_RELEASE;event.alternate=next.alternate;}
                else next.phase=0;
            }
        }
    }else if(next.phase==0 && selected && edges && !next.cooldown && reserve>0){
        next.alternate=(edges&RF_GRENADE_THROW_ALTERNATE)!=0;
        next.phase=1;next.ticks=next.alternate?96:102;
        event.kind=RF_GRENADE_THROW_START;event.alternate=next.alternate;
    }
    *state=next;*out=event;return RF_OK;
}
int rf_grenade_throw_resolve(rf_grenade_throw *state,uint32_t spawned,int32_t *reserve)
{
    rf_grenade_throw next;
    if(!valid(state) || state->phase!=2 || !reserve || *reserve<0 || spawned>1 ||
        (spawned && !*reserve))return RF_RANGE;
    next=*state;next.phase=0;next.ticks=0;
    if(spawned){next.cooldown=180;--*reserve;}
    *state=next;return RF_OK;
}
