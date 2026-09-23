#include "rf/npc_checkpoint.h"
#include <math.h>
#include <string.h>
_Static_assert(sizeof(float)==4,"RFNC requires binary32");
static uint32_t word(const unsigned char *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put(unsigned char *p,uint32_t v)
{uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static int32_t signed_word(const unsigned char *p)
{uint32_t u=word(p);int32_t v;memcpy(&v,&u,4);return v;}
static float real(const unsigned char *p)
{uint32_t u=word(p);float v;memcpy(&v,&u,4);return v;}
static void put_real(unsigned char *p,float v)
{uint32_t u;memcpy(&u,&v,4);put(p,u);}
static uint32_t hash(const unsigned char *p,uint32_t bytes)
{uint32_t i,h=2166136261u;for(i=0;i<bytes;i++){h^=(i>=12&&i<16)?0:p[i];h*=16777619u;}return h;}
static int catalog_valid(const rf_npc_checkpoint_catalog *c)
{
    uint32_t i;if(!c||!c->count||c->count>64)return RF_RANGE;
    for(i=0;i<64;i++){
        const rf_weapon_acquire_definition *d=c->weapons+i;
        if(c->supported[i]>1||(i>=c->count&&c->supported[i]))return RF_RANGE;
        if(c->supported[i]&&(d->ammo_type< -1||d->ammo_type>=32||d->capacity<0||d->magazine<0||
           (d->ammo_type<0&&d->magazine)))return RF_RANGE;
    }
    return RF_OK;
}
static int selected(int32_t id,const rf_npc_checkpoint_record *r,const rf_npc_checkpoint_catalog *c)
{return id==-1||(id>=0&&(uint32_t)id<c->count&&c->supported[id]&&r->inventory.owned[id]);}
static int animation_valid(const rf_npc_checkpoint_record *r)
{
    const rf_motion_playback_state *p=&r->playback;const rf_motion_slot_state *a=&p->completion.active;
    uint32_t i,j;rf_motion_playback_state zero={0};rf_motion_controller zero_controller={0};
    const rf_motion_controller *controller=&r->controller;
    if(r->animation_present>1)return RF_FORMAT;
    if(!r->animation_present)return r->script_animation.active||r->script_animation.loop||r->script_animation.freeze||
        r->script_animation.motion||memcmp(p,&zero,sizeof(zero))||memcmp(controller,&zero_controller,sizeof(zero_controller))?RF_FORMAT:RF_OK;
    if(r->retired||r->script_animation.active>1||r->script_animation.loop>1||r->script_animation.freeze>1||
       (r->script_animation.active?r->script_animation.motion<0:(r->script_animation.loop||r->script_animation.freeze||r->script_animation.motion))||a->count>16||!a->count||p->completion.frozen>1||p->completion.primary_flag>255||
       p->generation>65535||p->event_mask>3||!isfinite(p->phase)||p->phase<0||p->phase>1||
       a->freeze_slot< -1||a->primary_slot< -1||a->dominant_slot< -1||
       a->freeze_slot>=(int32_t)a->count||a->primary_slot>=(int32_t)a->count||a->dominant_slot>=(int32_t)a->count)return RF_FORMAT;
    if(controller->current<0||controller->current>=23||controller->next< -1||controller->next>=23||
       controller->override_enabled>1||controller->override_state< -1||controller->override_state>=23||
       (controller->override_enabled&&controller->override_state<0)||!isfinite(controller->duration)||controller->duration<0||
       !isfinite(controller->elapsed)||controller->elapsed<0||
       (controller->duration>0&&(controller->next<0||controller->elapsed>controller->duration)))return RF_FORMAT;
    for(i=0;i<2;i++)for(j=0;j<3;j++)if(!isfinite(p->completion.primary_vectors[i][j]))return RF_FORMAT;
    for(i=0;i<a->count;i++){
        if(a->slots[i].motion<0||!isfinite(a->slots[i].weight)||a->slots[i].weight<0)return RF_FORMAT;
        for(j=0;j<i;j++)if(a->slots[j].motion==a->slots[i].motion)return RF_FORMAT;
    }
    /* Inactive slots are implementation scratch, intentionally not serialized. */
    return RF_OK;
}
static uint32_t animation_bytes(const rf_npc_checkpoint_record *r)
{return r->animation_present?RF_NPC_CHECKPOINT_ANIMATION_BASE+12*r->playback.completion.active.count:0;}
static void read_animation(const unsigned char *p,rf_npc_checkpoint_record *r)
{
    rf_motion_playback_state *b=&r->playback;rf_motion_slot_state *a=&b->completion.active;uint32_t i,j;
    r->animation_present=1;r->script_animation.active=word(p);r->script_animation.loop=word(p+4);
    r->script_animation.freeze=word(p+8);r->script_animation.motion=signed_word(p+12);
    a->count=word(p+16);a->freeze_slot=signed_word(p+20);a->primary_slot=signed_word(p+24);a->dominant_slot=signed_word(p+28);
    b->completion.frozen=word(p+32);b->completion.primary_flag=word(p+36);
    b->completion.primary_words[0]=word(p+40);b->completion.primary_words[1]=word(p+44);
    for(i=0;i<2;i++)for(j=0;j<3;j++)b->completion.primary_vectors[i][j]=real(p+48+12*i+4*j);
    b->phase=real(p+72);b->generation=word(p+76);b->event_mask=word(p+80);
    r->controller.current=signed_word(p+84);r->controller.next=signed_word(p+88);
    r->controller.duration=real(p+92);r->controller.elapsed=real(p+96);
    r->controller.override_state=signed_word(p+100);r->controller.override_enabled=word(p+104);
    for(i=0;i<a->count;i++){a->slots[i].motion=signed_word(p+108+12*i);a->slots[i].tick=signed_word(p+112+12*i);a->slots[i].weight=real(p+116+12*i);}
}
static void write_animation(unsigned char *p,const rf_npc_checkpoint_record *r)
{
    const rf_motion_playback_state *b=&r->playback;const rf_motion_slot_state *a=&b->completion.active;uint32_t i,j;
    put(p,r->script_animation.active);put(p+4,r->script_animation.loop);put(p+8,r->script_animation.freeze);put(p+12,(uint32_t)r->script_animation.motion);
    put(p+16,a->count);put(p+20,(uint32_t)a->freeze_slot);put(p+24,(uint32_t)a->primary_slot);put(p+28,(uint32_t)a->dominant_slot);
    put(p+32,b->completion.frozen);put(p+36,b->completion.primary_flag);put(p+40,b->completion.primary_words[0]);put(p+44,b->completion.primary_words[1]);
    for(i=0;i<2;i++)for(j=0;j<3;j++)put_real(p+48+12*i+4*j,b->completion.primary_vectors[i][j]);
    put_real(p+72,b->phase);put(p+76,b->generation);put(p+80,b->event_mask);
    put(p+84,(uint32_t)r->controller.current);put(p+88,(uint32_t)r->controller.next);
    put_real(p+92,r->controller.duration);put_real(p+96,r->controller.elapsed);
    put(p+100,(uint32_t)r->controller.override_state);put(p+104,r->controller.override_enabled);
    for(i=0;i<a->count;i++){put(p+108+12*i,(uint32_t)a->slots[i].motion);put(p+112+12*i,(uint32_t)a->slots[i].tick);put_real(p+116+12*i,a->slots[i].weight);}
}
static int valid(const rf_npc_checkpoint_record *r,const rf_npc_checkpoint_catalog *c)
{
    uint32_t i;uint8_t used[32]={0};
    if(r->uid==UINT32_MAX||r->class_id==UINT32_MAX||r->retired>1||(r->flags&~0x4004u)||
       !isfinite(r->health)||(!r->retired&&r->health<=0)||!isfinite(r->armor)||r->armor<0||
       !isfinite(r->yaw)||
       (r->ai_mode!=-1&&r->ai_mode!=0&&r->ai_mode!=1&&r->ai_mode!=2&&r->ai_mode!=11))return RF_FORMAT;
    for(i=0;i<3;i++)if(!isfinite(r->eye_angles[i])||!isfinite(r->position[i])||!isfinite(r->drop.position[i]))return RF_FORMAT;
    for(i=0;i<64;i++){
        const rf_weapon_acquire_definition *d=c->weapons+i;
        if(r->inventory.owned[i]>1||r->inventory.loaded[i]<0||
           (!c->supported[i]&&(r->inventory.owned[i]||r->inventory.loaded[i]))||
           (c->supported[i]&&r->inventory.loaded[i]>d->magazine))return RF_FORMAT;
        if(c->supported[i]&&d->ammo_type>=0)used[d->ammo_type]=1;
    }
    for(i=0;i<32;i++)if(r->inventory.reserve[i]<0||(!used[i]&&r->inventory.reserve[i]))return RF_FORMAT;
    if(!selected(r->primary,r,c)||!selected(r->secondary,r,c))return RF_FORMAT;
    if(r->drop.state>2||r->drop.quantity<0)return RF_FORMAT;
    if(r->drop.state){
        if(!r->retired||r->drop.weapon<0||(uint32_t)r->drop.weapon>=c->count||!c->supported[r->drop.weapon])return RF_FORMAT;
    }else if(r->drop.weapon||r->drop.quantity||r->drop.position[0]||r->drop.position[1]||r->drop.position[2])return RF_FORMAT;
    return animation_valid(r);
}
static void read_row(const unsigned char *p,uint32_t version,rf_npc_checkpoint_record *r)
{
    uint32_t i;memset(r,0,sizeof(*r));r->uid=word(p);r->class_id=word(p+4);r->retired=word(p+8);
    r->flags=word(p+12);r->affiliation=word(p+16);r->health=real(p+20);r->armor=real(p+24);
    for(i=0;i<3;i++)r->position[i]=real(p+28+i*4);r->yaw=real(p+40);
    r->primary=signed_word(p+44);r->secondary=signed_word(p+48);
    r->drop.state=word(p+52);r->drop.weapon=signed_word(p+56);r->drop.quantity=signed_word(p+60);
    for(i=0;i<3;i++)r->drop.position[i]=real(p+64+i*4);
    memcpy(r->inventory.owned,p+76,64);
    for(i=0;i<32;i++)r->inventory.reserve[i]=signed_word(p+140+i*4);
    for(i=0;i<64;i++)r->inventory.loaded[i]=signed_word(p+268+i*4);
    r->ai_mode=signed_word(p+524);
    if(version>=2)for(i=0;i<3;i++)r->eye_angles[i]=real(p+528+i*4);
    if(version>=3&&word(p+540))read_animation(p+544,r);
}
static void write_row(unsigned char *p,const rf_npc_checkpoint_record *r)
{
    uint32_t i;put(p,r->uid);put(p+4,r->class_id);put(p+8,r->retired);put(p+12,r->flags);put(p+16,r->affiliation);
    put_real(p+20,r->health);put_real(p+24,r->armor);for(i=0;i<3;i++)put_real(p+28+i*4,r->position[i]);
    put_real(p+40,r->yaw);put(p+44,(uint32_t)r->primary);put(p+48,(uint32_t)r->secondary);
    put(p+52,r->drop.state);put(p+56,(uint32_t)r->drop.weapon);put(p+60,(uint32_t)r->drop.quantity);
    for(i=0;i<3;i++)put_real(p+64+i*4,r->drop.position[i]);memcpy(p+76,r->inventory.owned,64);
    for(i=0;i<32;i++)put(p+140+i*4,(uint32_t)r->inventory.reserve[i]);
    for(i=0;i<64;i++)put(p+268+i*4,(uint32_t)r->inventory.loaded[i]);put(p+524,(uint32_t)r->ai_mode);
    for(i=0;i<3;i++)put_real(p+528+i*4,r->eye_angles[i]);
    put(p+540,animation_bytes(r));if(r->animation_present)write_animation(p+544,r);
}
int rf_npc_checkpoint_encode(const unsigned char identity[32],const rf_npc_checkpoint_catalog *c,
    const rf_npc_checkpoint_record *rows,uint32_t count,void *output,uint32_t capacity,uint32_t *written)
{
    unsigned char *p=output;uint32_t i,bytes,at;int status;
    if(!identity||!output||!written||(count&&!rows)||count>RF_NPC_CHECKPOINT_MAX_COUNT)return RF_RANGE;
    status=catalog_valid(c);if(status)return status;
    bytes=64+count*RF_NPC_CHECKPOINT_ROW;if(bytes>capacity)return RF_RANGE;
    for(i=0;i<count;i++){status=valid(rows+i,c);if(status)return status;if(i&&rows[i-1].uid>=rows[i].uid)return RF_FORMAT;bytes+=animation_bytes(rows+i);}
    if(bytes>capacity)return RF_RANGE;
    memset(p,0,bytes);memcpy(p,"RFNC",4);put(p+4,3);put(p+8,bytes);put(p+16,count);memcpy(p+24,identity,32);put(p+56,c->hash);
    for(i=0,at=64;i<count;i++){write_row(p+at,rows+i);at+=RF_NPC_CHECKPOINT_ROW+animation_bytes(rows+i);}
    put(p+12,hash(p,bytes));*written=bytes;return RF_OK;
}
static int row_span(const unsigned char *p,uint32_t available,uint32_t version,uint32_t *span)
{
    uint32_t base=version==1?RF_NPC_CHECKPOINT_ROW_V1:version==2?RF_NPC_CHECKPOINT_ROW_V2:RF_NPC_CHECKPOINT_ROW,n=0;
    if(available<base)return RF_FORMAT;
    if(version==3){n=word(p+540);if(n){
        uint32_t count;if(n<RF_NPC_CHECKPOINT_ANIMATION_BASE||n>RF_NPC_CHECKPOINT_ANIMATION_BASE+192||n>available-base)return RF_FORMAT;
        count=word(p+544+16);if(count>16||n!=RF_NPC_CHECKPOINT_ANIMATION_BASE+12*count)return RF_FORMAT;
    }}
    *span=base+n;return RF_OK;
}
static int decode(const void *data,uint32_t bytes,const unsigned char identity[32],const rf_npc_checkpoint_catalog *c,
    rf_npc_checkpoint_record *rows,uint32_t capacity,uint32_t *out_count,uint32_t publish)
{
    const unsigned char *p=data;rf_npc_checkpoint_record r;uint32_t i,count,version,span,at,previous=0;int status;
    if(!data||!identity||!out_count)return RF_RANGE;
    status=catalog_valid(c);if(status)return status;
    if(bytes<64||bytes>64+RF_NPC_CHECKPOINT_MAX_COUNT*RF_NPC_CHECKPOINT_ROW_MAX||memcmp(p,"RFNC",4)||word(p+4)<1||word(p+4)>3||
       word(p+8)!=bytes||word(p+20)||word(p+60)||word(p+56)!=c->hash||memcmp(p+24,identity,32)||word(p+12)!=hash(p,bytes))return RF_FORMAT;
    version=word(p+4);count=word(p+16);if(count>RF_NPC_CHECKPOINT_MAX_COUNT)return RF_FORMAT;
    if(publish&&(count>capacity||(count&&!rows)))return RF_RANGE;
    for(i=0,at=64;i<count;i++){
        status=row_span(p+at,bytes-at,version,&span);if(status)return status;
        read_row(p+at,version,&r);status=valid(&r,c);if(status)return status;
        if(i&&previous>=r.uid)return RF_FORMAT;previous=r.uid;at+=span;
    }
    if(at!=bytes)return RF_FORMAT;
    if(publish)for(i=0,at=64;i<count;i++){(void)row_span(p+at,bytes-at,version,&span);read_row(p+at,version,rows+i);at+=span;}
    *out_count=count;return RF_OK;
}
int rf_npc_checkpoint_decode(const void *data,uint32_t bytes,const unsigned char identity[32],const rf_npc_checkpoint_catalog *c,
    rf_npc_checkpoint_record *rows,uint32_t capacity,uint32_t *count)
{return decode(data,bytes,identity,c,rows,capacity,count,1);}
int rf_npc_checkpoint_preflight(const void *data,uint32_t bytes,const unsigned char identity[32],const rf_npc_checkpoint_catalog *c,uint32_t *count)
{return decode(data,bytes,identity,c,NULL,0,count,0);}
