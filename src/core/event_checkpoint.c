#include "rf/event_checkpoint.h"
#include <string.h>
static uint32_t word(const unsigned char *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put(unsigned char *p,uint32_t v)
{uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static int32_t signed_word(const unsigned char *p)
{uint32_t v=word(p);return v<=INT32_MAX?(int32_t)v:-1-(int32_t)(UINT32_MAX-v);}
static uint32_t hash(const unsigned char *p)
{uint32_t i,h=2166136261u;for(i=0;i<192;i++){h^=i>=12&&i<16?0:p[i];h*=16777619u;}return h;}
int rf_event_checkpoint_type_supported(uint32_t type)
{
    switch(type){
    case 0:case 1:case 2:case 3:case 5:case 6:case 7:case 8:case 10:case 17:case 11:case 12:case 13:case 14:case 15:case 18:
    case 16:case 19:case 20:case 22:case 24:case 28:case 30:case 32:case 34:
    case 35:case 36:case 37:case 38:case 39:case 44:case 46:case 48:case 50:
    case 41:case 42:case 43:case 49:case 55:case 59:case 60:case 61:case 62:case 63:case 51:case 52:case 56:case 64:case 65:case 67:case 69:case 71:case 73:case 74:case 75:case 81:case 83:case 84:case 87:case 88:return 1;
    default:return 0;
    }
}
uint32_t rf_event_checkpoint_external_requirements(uint32_t type)
{
    switch(type){
    case 0:return RF_EVENT_CHECKPOINT_EXTERNAL_AUDIO;
    case 41:case 42:return RF_EVENT_CHECKPOINT_EXTERNAL_AUDIO;
    case 7:return RF_EVENT_CHECKPOINT_EXTERNAL_NPC|RF_EVENT_CHECKPOINT_EXTERNAL_UNIMPLEMENTED;
    case 10:return RF_EVENT_CHECKPOINT_EXTERNAL_WORLD|RF_EVENT_CHECKPOINT_EXTERNAL_DAMAGE|RF_EVENT_CHECKPOINT_EXTERNAL_VISUAL|RF_EVENT_CHECKPOINT_EXTERNAL_UNIMPLEMENTED;
    case 61:return RF_EVENT_CHECKPOINT_EXTERNAL_VISUAL;
    case 1:case 13:case 14:case 17:return RF_EVENT_CHECKPOINT_EXTERNAL_DAMAGE;
    case 5:case 6:case 11:case 12:case 24:case 28:case 30:case 34:case 38:case 67:return RF_EVENT_CHECKPOINT_EXTERNAL_NPC;
    case 15:return RF_EVENT_CHECKPOINT_EXTERNAL_AUDIO|RF_EVENT_CHECKPOINT_EXTERNAL_VISUAL;
    case 19:case 56:case 64:case 65:case 81:return RF_EVENT_CHECKPOINT_EXTERNAL_INVENTORY;
    case 22:return RF_EVENT_CHECKPOINT_EXTERNAL_LEVEL;
    case 35:case 36:case 37:return RF_EVENT_CHECKPOINT_EXTERNAL_GOALS;
    case 73:case 74:case 75:case 84:return RF_EVENT_CHECKPOINT_EXTERNAL_WORLD;
    case 8:case 18:case 43:case 49:case 59:case 60:case 62:case 71:return RF_EVENT_CHECKPOINT_EXTERNAL_UNIMPLEMENTED;
    case 46:return RF_EVENT_CHECKPOINT_EXTERNAL_AUDIO|RF_EVENT_CHECKPOINT_EXTERNAL_NPC;
    case 3:case 48:return 0;
    default:return RF_EVENT_CHECKPOINT_EXTERNAL_WORLD;
    }
}
static int owner_valid(const rf_runtime_event *e,int32_t now)
{
    if(!e||!e->authored||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    if(!rf_event_checkpoint_type_supported(e->state.type))return RF_NOT_FOUND;
    if(e->object_kind!=6||e->authored->record.uid==UINT32_MAX||e->retired>1)return RF_FORMAT;
    if(e->retired&&(!(e->state.flags&1)||e->death_fired!=1||e->state.deadline>=0))return RF_FORMAT;
    if(e->state.type==50&&(e->unhide.on>1||e->unhide.off>1||(e->retired&&(e->unhide.on||e->unhide.off))))return RF_FORMAT;
    if(e->state.type==32&&!e->switch_state)return RF_FORMAT;
    return RF_OK;
}
static int remaining(int32_t deadline,int32_t now,int32_t *out)
{
    int status;if(deadline<0){*out=-1;return RF_OK;}
    status=rf_timer_remaining(deadline,now,out);if(!status&&*out<0)*out=0;return status;
}
static int encode_ref(uint32_t handle,const rf_event_checkpoint_refs *refs,uint32_t *kind,uint32_t *value)
{
    int status;*value=0;if(!handle){*kind=0;return RF_OK;}if(handle==UINT32_MAX){*kind=1;return RF_OK;}
    if(!refs||!refs->uid_from_handle)return RF_NOT_FOUND;
    status=refs->uid_from_handle(refs->context,handle,value);if(status)return status;
    if(*value==UINT32_MAX)return RF_FORMAT;*kind=2;return RF_OK;
}
static int decode_ref(uint32_t kind,uint32_t value,const rf_event_checkpoint_refs *refs,uint32_t *handle)
{
    int status;
    if(kind<2){if(value)return RF_FORMAT;*handle=kind?UINT32_MAX:0;return RF_OK;}
    if(kind!=2||value==UINT32_MAX)return RF_FORMAT;
    if(!refs||!refs->handle_from_uid)return RF_NOT_FOUND;
    status=refs->handle_from_uid(refs->context,value,handle);if(status)return status;
    return !*handle||*handle==UINT32_MAX?RF_FORMAT:RF_OK;
}
int rf_event_checkpoint_encode_mapped(const unsigned char identity[32],const rf_runtime_event *e,int32_t now,
    const rf_event_checkpoint_refs *refs,void *output,uint32_t capacity)
{
    unsigned char *p=output;uint32_t source_kind,source_uid,actor_kind,actor_uid;
    int32_t cycle_remaining=-1,unhide_remaining=-1,common_remaining=-1;int status;
    if(!identity||!output||capacity<192)return RF_RANGE;
    status=owner_valid(e,now);if(status)return status;
    status=remaining(e->state.deadline,now,&common_remaining);if(status)return status;
    if(e->death_fired>1||e->death_time>(uint32_t)RF_TIMER_PERIOD)return RF_FORMAT;
    if(e->state.type==20){
        if(e->cycle.period_ms<0||e->cycle.period_ms>RF_TIMER_PERIOD||e->cycle.enabled>1||e->cycle.unlimited>255)return RF_FORMAT;
        status=remaining(e->cycle.deadline,now,&cycle_remaining);if(status)return status;
    }
    if((e->state.type==87||e->state.type==88)&&e->threshold.fired>1)return RF_FORMAT;
    if(e->state.type==84&&(e->countdown_armed>1||e->countdown_fired>1))return RF_FORMAT;
    if(e->state.type==32&&e->switch_state->unlimited>255)return RF_FORMAT;
    if(e->state.type==50){status=remaining(e->unhide.deadline,now,&unhide_remaining);if(status)return status;}
    status=encode_ref(e->state.source,refs,&source_kind,&source_uid);if(status)return status;
    status=encode_ref(e->state.actor,refs,&actor_kind,&actor_uid);if(status)return status;
    memset(p,0,192);memcpy(p,"RFEC",4);put(p+4,3);put(p+8,192);put(p+16,e->authored->record.uid);put(p+20,e->state.type);memcpy(p+24,identity,32);
    put(p+64,e->state.flags);put(p+68,e->state.mode);put(p+72,e->death_fired);put(p+76,e->death_time);
    if(e->state.type==20){put(p+80,(uint32_t)cycle_remaining);put(p+84,(uint32_t)e->cycle.period_ms);put(p+88,(uint32_t)e->cycle.limit);
        put(p+92,e->cycle.count);put(p+96,e->cycle.enabled);put(p+100,e->cycle.unlimited);}
    if(e->state.type==87||e->state.type==88){put(p+104,(uint32_t)e->threshold.threshold);put(p+108,e->threshold.fired);}
    put(p+112,e->retired);put(p+116,source_kind);put(p+120,source_uid);put(p+124,actor_kind);put(p+128,actor_uid);
    if(e->state.type==32){const rf_switch_state *s=e->switch_state;
        put(p+136,s->disabled);put(p+140,(uint32_t)s->limit);put(p+144,s->unlimited);put(p+148,s->activations);put(p+152,(uint32_t)s->mode);}
    if(e->state.type==50){put(p+156,(uint32_t)unhide_remaining);put(p+164,e->unhide.on);put(p+168,e->unhide.off);}
    if(e->state.type==84){put(p+172,e->countdown_armed);put(p+176,e->countdown_fired);}
    put(p+160,(uint32_t)common_remaining);
    put(p+12,hash(p));return RF_OK;
}
typedef struct event_checkpoint_resolved {uint32_t source,actor;int32_t cycle_deadline,unhide_deadline,common_deadline;} event_checkpoint_resolved;
static int validate(const void *data,uint32_t bytes,const unsigned char identity[32],
    const rf_runtime_event *e,int32_t now,const rf_event_checkpoint_refs *refs,event_checkpoint_resolved *resolved)
{
    const unsigned char *p=data;uint32_t type,i,version;int32_t left;int status;
    if(!data||!identity)return RF_RANGE;
    status=owner_valid(e,now);if(status)return status;
    if(bytes!=192||memcmp(p,"RFEC",4)||(word(p+4)!=2&&word(p+4)!=3)||word(p+8)!=192||word(p+12)!=hash(p)||
       memcmp(p+24,identity,32)||word(p+16)!=e->authored->record.uid||word(p+20)!=e->state.type||word(p+56)||word(p+60)||word(p+132))return RF_FORMAT;
    version=word(p+4);type=word(p+20);
    for(i=version==2?160:type==84?180:172;i<192;i+=4)if(word(p+i))return RF_FORMAT;
    if(word(p+72)>1||word(p+76)>(uint32_t)RF_TIMER_PERIOD||word(p+112)>1||
       (word(p+112)&&(!(word(p+64)&1)||word(p+72)!=1)))return RF_FORMAT;
    resolved->cycle_deadline=resolved->unhide_deadline=resolved->common_deadline=-1;
    if(type==84&&(version<3||word(p+172)>1||word(p+176)>1))return RF_FORMAT;
    if(version==3){
        left=signed_word(p+160);if(left<-1||left>RF_TIMER_PERIOD||(word(p+112)&&left>=0))return RF_FORMAT;
        if(left>=0){status=rf_timer_set(&resolved->common_deadline,now,left);if(status)return status;}
        if(type==50){if(word(p+164)>1||word(p+168)>1||(word(p+112)&&(word(p+164)||word(p+168))))return RF_FORMAT;}
        else if(word(p+164)||word(p+168))return RF_FORMAT;
    }
    if(type==20){
        left=signed_word(p+80);
        if(left<-1||left>RF_TIMER_PERIOD||signed_word(p+84)!=e->cycle.period_ms||signed_word(p+84)<0||
           signed_word(p+84)>RF_TIMER_PERIOD||signed_word(p+88)!=e->cycle.limit||word(p+96)>1||word(p+100)>255||word(p+100)!=e->cycle.unlimited)return RF_FORMAT;
        if(left>=0){status=rf_timer_set(&resolved->cycle_deadline,now,left);if(status)return status;}
    } else for(i=80;i<104;i+=4)if(word(p+i))return RF_FORMAT;
    if(type==87||type==88){if(signed_word(p+104)!=e->threshold.threshold||word(p+108)>1)return RF_FORMAT;}
    else if(word(p+104)||word(p+108))return RF_FORMAT;
    if(type==32){if(signed_word(p+140)!=e->switch_state->limit||word(p+144)!=e->switch_state->unlimited||
        word(p+144)>255||signed_word(p+152)!=e->switch_state->mode)return RF_FORMAT;}
    else for(i=136;i<156;i+=4)if(word(p+i))return RF_FORMAT;
    if(type==50){left=signed_word(p+156);if(left<-1||left>RF_TIMER_PERIOD)return RF_FORMAT;
        if(left>=0){status=rf_timer_set(&resolved->unhide_deadline,now,left);if(status)return status;}}
    else if(word(p+156))return RF_FORMAT;
    status=decode_ref(word(p+116),word(p+120),refs,&resolved->source);if(status)return status;
    return decode_ref(word(p+124),word(p+128),refs,&resolved->actor);
}
int rf_event_checkpoint_preflight_mapped(const void *data,uint32_t bytes,const unsigned char identity[32],
    const rf_runtime_event *e,int32_t now,const rf_event_checkpoint_refs *refs)
{event_checkpoint_resolved resolved;return validate(data,bytes,identity,e,now,refs,&resolved);}
int rf_event_checkpoint_restore_mapped(const void *data,uint32_t bytes,const unsigned char identity[32],
    rf_runtime_event *e,int32_t now,const rf_event_checkpoint_refs *refs)
{
    const unsigned char *p=data;event_checkpoint_resolved resolved;int status=validate(data,bytes,identity,e,now,refs,&resolved);
    if(status)return status;
    e->state.flags=word(p+64);e->state.mode=word(p+68);e->state.source=resolved.source;e->state.actor=resolved.actor;
    e->state.deadline=resolved.common_deadline;
    e->death_fired=word(p+72);e->death_time=word(p+76);e->retired=word(p+112);
    if(e->state.type==20){e->cycle.deadline=resolved.cycle_deadline;e->cycle.count=word(p+92);e->cycle.enabled=word(p+96);}
    if(e->state.type==87||e->state.type==88)e->threshold.fired=word(p+108);
    if(e->state.type==84){e->countdown_armed=word(p+172);e->countdown_fired=word(p+176);}
    if(e->state.type==32){e->switch_state->disabled=word(p+136);e->switch_state->activations=word(p+148);}
    if(e->state.type==50){e->unhide.deadline=resolved.unhide_deadline;e->unhide.on=(uint8_t)word(p+164);e->unhide.off=(uint8_t)word(p+168);}
    return RF_OK;
}
int rf_event_checkpoint_encode(const unsigned char identity[32],const rf_runtime_event *e,int32_t now,void *output,uint32_t capacity)
{return rf_event_checkpoint_encode_mapped(identity,e,now,NULL,output,capacity);}
int rf_event_checkpoint_preflight(const void *data,uint32_t bytes,const unsigned char identity[32],const rf_runtime_event *e,int32_t now)
{return rf_event_checkpoint_preflight_mapped(data,bytes,identity,e,now,NULL);}
int rf_event_checkpoint_restore(const void *data,uint32_t bytes,const unsigned char identity[32],rf_runtime_event *e,int32_t now)
{return rf_event_checkpoint_restore_mapped(data,bytes,identity,e,now,NULL);}
