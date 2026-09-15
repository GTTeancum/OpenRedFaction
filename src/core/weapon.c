#include "rf/weapon.h"
#include "rf/timer.h"
#include "rf/effect.h"
#include <string.h>
#include <math.h>
int rf_weapon_flight_launch(rf_weapon_flight *flight,const float position[3],const float direction[3],
    float speed,float lifetime,float radius)
{
    rf_weapon_flight value={0};double length=0;uint32_t i;
    if(!flight || flight->active || !position || !direction)return RF_RANGE;
    if(!isfinite(speed) || speed<=0 || !isfinite(lifetime) || lifetime<=0 || !isfinite(radius) || radius<0)return RF_FORMAT;
    for(i=0;i<3;i++){if(!isfinite(position[i]) || !isfinite(direction[i]))return RF_FORMAT;length+=(double)direction[i]*direction[i];}
    if(length<=1e-24)return RF_FORMAT;
    length=sqrt(length);
    for(i=0;i<3;i++){value.position[i]=position[i];value.velocity[i]=(float)(direction[i]/length*speed);}
    value.radius=radius;value.remaining=lifetime;value.active=1;*flight=value;return RF_OK;
}
int rf_weapon_flight_step(rf_weapon_flight *flight,float dt,rf_weapon_flight_sweep sweep,void *context,rf_weapon_flight_event *out)
{
    rf_weapon_flight value;rf_weapon_flight_event event={0};float delta[3];double elapsed;uint32_t i,matched=0;int status;
    if(!flight || !out || !sweep)return RF_RANGE;
    if(!isfinite(dt) || dt<0 || flight->active>1)return RF_FORMAT;
    if(!flight->active || dt==0){*out=event;return RF_OK;}
    if(!isfinite(flight->remaining) || flight->remaining<=0 || !isfinite(flight->radius) || flight->radius<0)return RF_FORMAT;
    value=*flight;elapsed=fmin((double)dt,value.remaining);
    for(i=0;i<3;i++) {
        if(!isfinite(value.position[i]) || !isfinite(value.velocity[i]))return RF_FORMAT;
        delta[i]=(float)(value.velocity[i]*elapsed);
        if(!isfinite(delta[i]) || !isfinite(value.position[i]+delta[i]))return RF_FORMAT;
    }
    status=sweep(context,value.position,delta,value.radius,&event.contact,&matched);if(status)return status;
    if(matched>1)return RF_FORMAT;
    if(matched) {
        double length=0;
        if(!isfinite(event.contact.hit.fraction) || event.contact.hit.fraction<0 || event.contact.hit.fraction>1)return RF_FORMAT;
        for(i=0;i<3;i++) {
            if(!isfinite(event.contact.hit.point[i]) || !isfinite(event.contact.hit.normal[i]))return RF_FORMAT;
            length+=(double)event.contact.hit.normal[i]*event.contact.hit.normal[i];
        }
        if(fabs(length-1)>1e-3)return RF_FORMAT;
        for(i=0;i<3;i++)value.position[i]+=delta[i]*event.contact.hit.fraction;
        value.remaining-=elapsed*event.contact.hit.fraction;value.active=0;event.kind=1;
    } else {
        memset(&event.contact,0,sizeof(event.contact));
        for(i=0;i<3;i++)value.position[i]+=delta[i];
        value.remaining-=elapsed;
        if(value.remaining<=0){value.remaining=0;value.active=0;event.kind=2;}
    }
    *flight=value;*out=event;return RF_OK;
}
int rf_weapon_charge_step(uint32_t *remainder,uint32_t capacity,uint32_t drain_ticks,
    uint32_t active,int32_t *loaded,uint32_t *consumed)
{
    uint32_t units,total;
    if(!remainder || !loaded || !consumed || !capacity || capacity>256 || !drain_ticks || drain_ticks>3600 ||
       *remainder>=drain_ticks || active>1 || *loaded<0 || (uint32_t)*loaded>capacity)return RF_RANGE;
    *consumed=0;if(!active || !*loaded)return RF_OK;
    total=*remainder+capacity;units=total/drain_ticks;*remainder=total%drain_ticks;
    if(units>(uint32_t)*loaded)units=(uint32_t)*loaded;
    *loaded-=(int32_t)units;*consumed=units;return RF_OK;
}
int rf_weapon_trigger_step(rf_weapon_trigger_state *state,const rf_weapon_trigger_rules *rules,
    uint32_t trigger,uint32_t blocked,uint32_t loaded,uint32_t *event)
{
    uint32_t pressed;
    if(!state || !rules || !event || !rules->fire_ticks || rules->fire_ticks>3600 ||
        !rules->burst_count || rules->burst_count>32 || rules->burst_ticks>3600 ||
        (rules->burst_count>1 && !rules->burst_ticks) || rules->semi_automatic>1 ||
        state->remaining>=rules->burst_count || state->cooldown>3600 || state->delay>3600)return RF_RANGE;
    pressed=trigger && (!rules->semi_automatic || !state->held);state->held=!!trigger;*event=0;
    if(state->cooldown)--state->cooldown;if(state->delay)--state->delay;
    if(blocked){state->remaining=state->delay=0;return RF_OK;}
    if(state->remaining) {
        if(state->delay)return RF_OK;
        if(!loaded){state->remaining=0;*event=2;return RF_OK;}
        --state->remaining;state->delay=state->remaining?rules->burst_ticks:0;*event=1;return RF_OK;
    }
    if(!pressed || state->cooldown)return RF_OK;
    if(!loaded){*event=2;return RF_OK;}
    state->cooldown=rules->fire_ticks;state->remaining=rules->burst_count-1;
    state->delay=state->remaining?rules->burst_ticks:0;*event=1;return RF_OK;
}

void rf_projectile_pool_init(rf_projectile_pool *pool)
{
    uint32_t i;if(!pool)return;
    for(i=0;i<RF_PROJECTILE_CAPACITY;++i)pool->records[i][0]=i+1<RF_PROJECTILE_CAPACITY?i+2:0;
    pool->head=1;pool->free_count=RF_PROJECTILE_CAPACITY;pool->live_count=pool->peak=0;
    pool->live_bits[0]=pool->live_bits[1]=0;
}
int rf_projectile_pool_acquire(rf_projectile_pool *pool,uint32_t *slot)
{
    uint32_t i,mask,next;if(!pool || !slot)return RF_RANGE;
    if(!pool->free_count)return RF_NOT_FOUND;
    if(!pool->head || pool->head>RF_PROJECTILE_CAPACITY || pool->live_count>=RF_PROJECTILE_CAPACITY)return RF_RANGE;
    i=pool->head-1;mask=1u<<(i%32);next=pool->records[i][0];
    if((pool->live_bits[i/32]&mask) || next>RF_PROJECTILE_CAPACITY)return RF_RANGE;
    pool->head=next;--pool->free_count;++pool->live_count;pool->live_bits[i/32]|=mask;
    if(pool->peak<pool->live_count)pool->peak=pool->live_count;*slot=i;return RF_OK;
}
int rf_projectile_pool_release(rf_projectile_pool *pool,uint32_t slot)
{
    uint32_t mask;if(!pool || slot>=RF_PROJECTILE_CAPACITY)return RF_RANGE;
    mask=1u<<(slot%32);if(!(pool->live_bits[slot/32]&mask))return RF_NOT_FOUND;
    if(!pool->live_count || pool->free_count>=RF_PROJECTILE_CAPACITY)return RF_RANGE;
    pool->records[slot][0]=pool->head;pool->head=slot+1;pool->live_bits[slot/32]&=~mask;
    ++pool->free_count;--pool->live_count;return RF_OK;
}

void rf_projectile_store_init(rf_projectile_store *store)
{
    if(!store)return;rf_projectile_pool_init(&store->pool);memset(store->owners,0,sizeof(store->owners));
}
int rf_projectile_store_close(rf_projectile_store *store,rf_object_registry *registry,rf_object_list *objects,
    uint32_t handle,const rf_projectile_owner_ops *ops,void *context)
{
    rf_projectile_owner *owner;uint32_t i;int status;
    if(!store || !registry || !objects || !ops || !ops->cleanup)return RF_RANGE;
    owner=rf_object_registry_lookup(registry,handle);if(!owner)return RF_NOT_FOUND;
    for(i=0;i<50;++i)if(owner==store->owners+i)break;
    if(i==50 || owner->slot!=i || owner->handle!=handle || owner->object_kind!=2 ||
       !(store->pool.live_bits[i/32]&(1u<<(i%32))) || !owner->object_link.next || !owner->object_link.previous)return RF_RANGE;
    rf_object_list_remove(objects,&owner->object_link);
    ops->cleanup(context,owner,store->pool.records[i]);
    status=rf_object_registry_remove(registry,handle);if(status)return status;
    status=rf_projectile_pool_release(&store->pool,i);if(status)return status;
    memset(owner,0,sizeof(*owner));return RF_OK;
}
int rf_projectile_store_open(rf_projectile_store *store,rf_object_registry *registry,rf_object_list *objects,
    uint32_t *uid_cursor,const rf_projectile_creation_descriptor *descriptor,const rf_projectile_owner_ops *ops,
    void *context,rf_projectile_owner **out)
{
    rf_projectile_owner *owner;uint32_t slot;int status,cleanup;
    if(!store || !registry || !objects || !uid_cursor || !descriptor || !ops || !ops->initialize || !ops->cleanup ||
       !out || *out || !objects->sentinel.next || !objects->sentinel.previous ||
       objects->sentinel.next->previous!=&objects->sentinel || objects->sentinel.previous->next!=&objects->sentinel)return RF_RANGE;
    if(!registry->count)return RF_NOT_FOUND;
    status=rf_projectile_pool_acquire(&store->pool,&slot);if(status)return status;
    owner=store->owners+slot;memset(owner,0,sizeof(*owner));owner->object_kind=2;owner->slot=slot;
    rf_object_list_append(objects,&owner->object_link);
    status=rf_object_registry_insert(registry,owner,&owner->handle);
    if(status){rf_object_list_remove(objects,&owner->object_link);(void)rf_projectile_pool_release(&store->pool,slot);memset(owner,0,sizeof(*owner));return status;}
    owner->uid=(*uid_cursor)--;owner->flags=0x06100000u; /*486da0 allocation flags100000 +6000000. */
    status=ops->initialize(context,owner,store->pool.records[slot],descriptor);
    if(status) {
        cleanup=rf_projectile_store_close(store,registry,objects,owner->handle,ops,context);return cleanup?cleanup:status;
    }
    owner->flags|=0x400000u;*out=owner;return RF_OK;
}

int rf_projectile_descriptor_prepare(const rf_projectile_descriptor_input *in,rf_projectile_creation_descriptor *out)
{
    rf_projectile_creation_descriptor d={{0},0};float velocity[3],angular[3],speed,spin;uint32_t i;
    if(!in || !out)return RF_RANGE;
    if(in->weapon<0 || in->weapon>in->weapon_count)return RF_NOT_FOUND;
    if(!isfinite(in->speed) || !isfinite(in->speed_scale))return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(in->position[i]))return RF_RANGE;
    for(i=0;i<9;++i)if(!isfinite(in->basis[i]))return RF_RANGE;
    if(in->name_length>4){d.words[0]=in->name_token;d.words[1]=in->model_token;}
    d.boosted=!(in->multiplayer&255) && (!(in->flags_264&0x400) || !(in->player_controlled&255)) &&
        (in->flags_264&0x40000000u) && (in->powerup&255)==1;
    memcpy(d.words+5,&in->field_bc,4);memcpy(d.words+33,&in->field_ac,4);
    memcpy(d.words+15,in->position,12);memcpy(d.words+18,in->basis,36);
    speed=d.boosted?(float)((double)in->speed_scale*in->speed):in->speed;
    for(i=0;i<3;++i){velocity[i]=(float)((double)in->basis[6+i]*speed);if(!isfinite(velocity[i]))return RF_RANGE;}
    memcpy(d.words+27,velocity,12);d.words[37]=0x80000870u;
    if(in->weapon==in->special_weapon || (in->weapon<in->weapon_count && (in->flags_268&0x40))) {
        spin=in->weapon==in->special_weapon?10.0f:20.0f;
        for(i=0;i<3;++i)angular[i]=(float)(((double)0.0f*in->basis[6+i]+(double)0.0f*in->basis[3+i])+(double)spin*in->basis[i]);
        memcpy(d.words+30,angular,12);d.words[4]=2;
    }
    if(in->flags_264&0x1000)d.words[37]|=1;
    if(in->flags_264&0x80000000u)d.words[37]|=0x200;
    if(in->speed<50.0f)d.words[37]|=8;
    if(in->flags_268&0x10)d.words[37]&=~0x10u;
    *out=d;return RF_OK;
}

uint32_t rf_weapon_world_model_token(const rf_weapon_world_model models[64],int32_t weapon)
{
    if(!models || weapon<0 || weapon>=64 || !models[weapon].name_nonempty)return 0;
    return models[weapon].model;
}
int rf_weapon_world_tag(rf_weapon_world_model models[64],int32_t weapon,uint32_t kind,
    int (*lookup)(void *,uint32_t,const char *,int32_t *),void *context,int32_t *tag)
{
    uint32_t model;int32_t value,*cached;int status;
    if(!models || !tag || kind>1)return RF_RANGE;
    model=rf_weapon_world_model_token(models,weapon);if(!model){*tag=-1;return RF_OK;}
    cached=kind?&models[weapon].muzzle:&models[weapon].grip;
    if(*cached==-1) {
        if(!lookup)return RF_NOT_FOUND;
        status=lookup(context,model,kind?"muzzle_1":"grip_1",&value);if(status)return status;
        *cached=value;
    }
    *tag=*cached;return RF_OK;
}

int rf_weapon_recoil_basis(const float basis[9],float recoil,int32_t hand,float result[9])
{
    static const unsigned char order[9][3]={{0,2,1},{2,1,0},{1,0,2},{2,1,0},{2,1,0},{0,1,2},{2,1,0},{2,0,1},{1,0,2}};
    double angle,s,c,wz;float x,y,z,xx,yy,zz,xy,xz,yz,wx,wy,r[9],out[9];uint32_t i,row,col;
    if(!basis || !result || !isfinite(recoil) || hand<0 || hand>1)return RF_RANGE;
    for(i=0;i<9;++i)if(!isfinite(basis[i]))return RF_RANGE;
    if(recoil==0){memmove(result,basis,36);return RF_OK;}
    angle=(hand==0?-(double)recoil:(double)recoil)*0.5;s=sin(angle);c=cos(angle);
    x=(float)(s*basis[6]);y=(float)(s*basis[7]);z=(float)(s*basis[8]);
    xx=x*x;yy=y*y;zz=z*z;xy=x*y;xz=x*z;yz=y*z;wx=(float)(c*x);wy=(float)(c*y);wz=c*z;
    r[0]=(float)((1.0-2.0*yy)-2.0*zz);r[1]=(float)(2.0*(wz+xy));r[2]=(float)(2.0*xz-2.0*wy);
    r[3]=(float)(2.0*xy-2.0*wz);r[4]=(float)((1.0-2.0*xx)-2.0*zz);r[5]=(float)(2.0*((double)wx+yz));
    r[6]=(float)(2.0*((double)wy+xz));r[7]=(float)(2.0*yz-2.0*wx);r[8]=(float)((1.0-2.0*xx)-2.0*yy);
    for(row=0;row<3;++row)for(col=0;col<3;++col) {
        const unsigned char *k=order[row*3+col];double v=(double)basis[row*3+k[0]]*r[k[0]*3+col];
        for(i=1;i<3;++i)v+=(double)basis[row*3+k[i]]*r[k[i]*3+col];
        out[row*3+col]=(float)v;if(!isfinite(out[row*3+col]))return RF_RANGE;
    }
    memcpy(result,out,36);return RF_OK;
}

int rf_weapon_draw_state_prepare(uint32_t state[20],uint32_t special_view,uint32_t tint,const float basis[9])
{
    float copy[9];if(!state || !basis)return RF_RANGE;memcpy(copy,basis,sizeof(copy));
    state[0]=0x80;state[1]=UINT32_MAX;state[2]=255;state[4]=0;state[5]=0xbf800000u;
    state[7]=0;state[8]=state[9]=UINT32_MAX;state[10]=tint;
    if((special_view&255)==1){state[0]|=2;state[3]=0xff002000u;}
    memcpy(state+11,copy,sizeof(copy));return RF_OK;
}

int rf_weapon_world_visibility(rf_weapon_world_view *view,const rf_weapon_world_model models[64],uint32_t *model)
{
    if(!view || !models || !model)return RF_RANGE;
    view->flags_810&=~0x200u;*model=0;
    if((view->flags_810&1) || view->weapon==-1 || !(view->class_flags_724&8) ||
       (view->inventory_flags_7d0&0x200) || view->attachment_75c!=-1 ||
       (view->linked_kind!=4 && (view->flags_810&0x800)) || view->linked_kind==1)return RF_OK;
    if(view->weapon==view->special_weapon && view->player_present && !(view->player_1044&255))return RF_OK;
    *model=rf_weapon_world_model_token(models,view->weapon);
    if(view->player_present && (view->player_103c&255))*model=view->override_model;
    return RF_OK;
}

int rf_weapon_place_in_hand(const rf_weapon_hand_source *source,int32_t hand,
    rf_weapon_world_model models[64],const rf_weapon_hand_ops *ops,void *context,rf_weapon_hand_placement *result)
{
    uint32_t model,i;int32_t grip;int status;float basis[9]={1,0,0,0,1,0,0,0,1};
    float unused_basis[9]={1,0,0,0,1,0,0,0,1},point[3]={0};
    if(!source || !models || !result || hand<0 || source->hand_count>2)return RF_RANGE;
    if((uint32_t)hand>=source->hand_count)return RF_NOT_FOUND;
    model=rf_weapon_world_model_token(models,source->weapon);if(!model)return RF_NOT_FOUND;
    if(!ops || !ops->transform)return RF_NOT_FOUND;
    status=ops->transform(context,source->actor_model,source->hands[hand],source->basis,source->position,basis,result->hand);if(status)return status;
    memcpy(result->position,result->hand,12);memcpy(result->basis,basis,36);
    status=rf_weapon_world_tag(models,source->weapon,0,ops->tag,context,&grip);if(status)return status;
    if(grip!=-1) {
        status=ops->transform(context,model,grip,basis,result->hand,unused_basis,point);if(status)return status;
        for(i=0;i<3;++i) {
            float offset=(float)((double)point[i]-result->hand[i]);
            result->position[i]=(float)((double)result->position[i]-offset);
        }
    }
    return RF_OK;
}

static double weapon_normalize(float v[3])
{
    double length=sqrt(((double)v[0]*v[0]+(double)v[1]*v[1])+(double)v[2]*v[2]);uint32_t i;
    if(length>0)for(i=0;i<3;++i)v[i]=(float)((double)v[i]*(1.0/length));
    return length;
}
static void weapon_cross(const float a[3],const float b[3],float out[3])
{
    uint32_t i;for(i=0;i<3;++i)out[i]=(float)((double)a[(i+1)%3]*b[(i+2)%3]-(double)a[(i+2)%3]*b[(i+1)%3]);
}
int rf_weapon_target_aim(const rf_weapon_aim_source *source,const float muzzle[3],float basis[9])
{
    float direction[3],out[9],original[3];const float *target;double dot;uint32_t i;int status;
    if(!source || !muzzle || !basis)return RF_RANGE;
    if((source->local_related&255)!=1 && (source->animation_locked&255))return RF_OK;
    if((source->local_related&255)==1 || !source->target_present){memcpy(basis,source->eye_basis,36);return RF_OK;}
    target=source->target_actor?source->target_eye:source->target_position;
    for(i=0;i<3;++i) {
        if(!isfinite(target[i]) || !isfinite(muzzle[i]))return RF_RANGE;
        direction[i]=(float)((double)target[i]-muzzle[i]);if(!isfinite(direction[i]))return RF_RANGE;
    }
    for(i=0;i<9;++i)if(!isfinite(source->eye_basis[i]))return RF_RANGE;
    if(!(weapon_normalize(direction)>0))return RF_OK;
    dot=((double)direction[0]*source->eye_basis[6]+(double)direction[1]*source->eye_basis[7])+(double)direction[2]*source->eye_basis[8];
    if(!(dot>0.8))return RF_OK;
    memcpy(original,direction,12);memcpy(out+6,direction,12);weapon_normalize(out+6);
    memcpy(out+3,source->eye_basis+3,12);
    if(!(weapon_normalize(out+3)>0)) {
        status=rf_entity_navigation_basis(original,(float(*)[3])out);if(status)return status;
    } else {
        weapon_cross(out+3,out+6,out);
        if(!(weapon_normalize(out)>0)) {status=rf_entity_navigation_basis(original,(float(*)[3])out);if(status)return status;}
        else weapon_cross(out+6,out,out+3);
    }
    memcpy(basis,out,36);return RF_OK;
}

int rf_weapon_muzzle_pose(const rf_weapon_muzzle_source *source,rf_weapon_world_model models[64],
    const rf_weapon_hand_ops *ops,int (*aim)(void *,const float position[3],float basis[9]),
    void *context,float position[3],float basis[9])
{
    int32_t index,tag,hand;uint32_t model,i;int status;
    float hand_basis[9]={0},hand_point[3]={0},muzzle_basis[9]={0},muzzle_point[3]={0};
    if(!source || !models || !position || !basis || source->primary_count>2)return RF_RANGE;
    index=source->weapon<source->primary_limit?source->primary_index:source->secondary_index;
    status=rf_weapon_world_tag(models,source->weapon,1,ops?ops->tag:NULL,context,&tag);if(status)return status;
    model=rf_weapon_world_model_token(models,source->weapon);
    if(tag!=-1 && model && source->primary_count) {
        if(index<0 || index>=2)return RF_RANGE;
        hand=source->weapon<source->primary_limit?source->primary_tags[index]:source->secondary_tags[index];
        if(!ops || !ops->transform)return RF_NOT_FOUND;
        status=ops->transform(context,source->actor_model,hand,source->basis,source->position,hand_basis,hand_point);if(status)return status;
        status=ops->transform(context,model,tag,hand_basis,hand_point,muzzle_basis,muzzle_point);if(status)return status;
        memcpy(position,muzzle_point,12);memcpy(basis,hand_basis,36);
        if(!aim)return RF_NOT_FOUND;return aim(context,position,basis);
    }
    memcpy(basis,source->eye_basis,36);
    for(i=0;i<3;++i) {
        float offset=(float)((double)source->eye_basis[6+i]*0.300000011920928955078125);
        position[i]=(float)((double)source->eye[i]+offset);
    }
    return RF_OK;
}

int rf_weapon_world_draw_run(rf_weapon_world_draw *draw,const rf_weapon_world_model models[64],
    uint32_t scratch[20],const rf_weapon_world_draw_ops *ops,void *context)
{
    uint32_t model,hand=0;int status;rf_weapon_hand_placement pose;
    if(!draw || !scratch)return RF_RANGE;
    status=rf_weapon_world_visibility(&draw->view,models,&model);if(status || !model)return status;
    for(;;) {
        if(draw->hand_count>2)return RF_RANGE;if(hand>=draw->hand_count)break;
        if(!ops || !ops->place)return RF_NOT_FOUND;
        status=ops->place(context,(int32_t)hand,&pose);
        if(status==RF_NOT_FOUND){++hand;continue;}if(status)return status;
        status=rf_weapon_recoil_basis(pose.basis,draw->recoil,(int32_t)hand,pose.basis);if(status)return status;
        status=rf_weapon_draw_state_prepare(scratch,draw->special_view,draw->tint,pose.basis);if(status)return status;
        if(!ops->submit)return RF_NOT_FOUND;
        status=ops->submit(context,model,&pose,scratch);if(status)return status;++hand;
    }
    draw->view.flags_810|=0x200;return RF_OK;
}

int rf_weapon_update_presentation(rf_weapon_presentation_state *state,int32_t weapon,
    const rf_weapon_model_descriptor descriptors[64],const rf_weapon_model_cache cache[32],
    const rf_weapon_presentation_context *context,const rf_weapon_presentation_ops *ops,
    void *user,uint32_t *result)
{
    int32_t source; unsigned i; int status;
    if (!state || !context || !result || context->local_player>1) return RF_RANGE;
    if (!context->local_player) { *result=0; return RF_OK; }
    if (weapon==-1) {
        /* Original 4a73b0 is a single ret, even with a nonzero model. */
        *result=0; return RF_OK;
    }
    if (weapon<0 || weapon>=64) { *result=0; return RF_OK; }
    if (!descriptors || !cache) return RF_RANGE;
    if (!descriptors[weapon].name_nonempty) { *result=state->model; return RF_OK; }
    if ((weapon==context->paired_first && state->current==context->paired_second) ||
        (weapon==context->paired_second && state->current==context->paired_first)) {
        state->current=weapon; *result=state->model; return RF_OK;
    }
    if (weapon!=state->current && state->model) state->model=state->auxiliary=0;
    if (state->model) { *result=state->model; return RF_OK; }
    source=weapon==context->alternate_weapon ? context->base_weapon :
        weapon==context->paired_second ? context->paired_first : weapon;
    for (i=0;i<32;++i) if (cache[i].weapon==source) break;
    if (i<32) state->model=weapon==context->alternate_weapon ? cache[i].alternate : cache[i].normal;
    else {
        if (source<0 || source>=64) return RF_RANGE;
        state->model=descriptors[source].model;
    }
    if (!state->model) return RF_NOT_FOUND;
    state->pending=-1; rf_timer_clear(&state->deadline);
    if (descriptors[weapon].resource!=-1 && context->resource_backend==0x66) {
        if (!ops || !ops->resource) return RF_NOT_FOUND;
        status=ops->resource(user,descriptors[weapon].resource); if (status!=RF_OK) return status;
    }
    state->auxiliary=0; state->current=weapon;
    if ((context->mode&255)==1 && weapon==context->mode_weapon && context->mode_kind==1) {
        if (!ops || !ops->mode_finish) return RF_NOT_FOUND;
        status=ops->mode_finish(user); if (status!=RF_OK) return status;
    }
    *result=state->model; return RF_OK;
}
int rf_weapon_current(const rf_entity_registry *registry,int32_t entity_handle,
    uint32_t local_player,int (*update_presentation)(void *user,int32_t weapon),
    void *user,int32_t *weapon)
{
    const rf_entity_view *entity,*linked; int32_t current=-1; int status;
    if (!registry || !weapon || local_player>1) return RF_RANGE;
    entity=rf_entity_lookup(registry,entity_handle);
    if (entity) {
        linked=rf_entity_lookup(registry,entity->linked_handle);
        if (linked && (linked->class_type==1 || linked->class_type==4)) entity=linked;
        current=entity->weapons[0];
        if (current!=-1 && local_player) {
            if (!update_presentation) return RF_NOT_FOUND;
            status=update_presentation(user,current);
            if (status!=RF_OK) return status;
        }
    }
    *weapon=current;
    return RF_OK;
}
int rf_weapon_clear_followup(rf_weapon_selection_state *state)
{
    if (!state) return RF_RANGE;
    state->flag_f94=state->flag_f95=0;
    state->value_f98=0;
    return RF_OK;
}
int rf_weapon_queue_selection(rf_weapon_selection_state *state,int32_t weapon)
{
    if (!state) return RF_RANGE;
    state->pending_weapon=weapon;
    rf_timer_clear(&state->deadline);
    return RF_OK;
}
static int weapon_total(const rf_weapon_inventory *inventory,const rf_weapon_supply supply[64],int32_t weapon,int32_t *total)
{
    int32_t reserve; uint32_t bits; int status=rf_weapon_reserve(inventory,supply,weapon,&reserve);
    if (status!=RF_OK) return status;
    if (!inventory || weapon<0 || weapon>=64) return RF_RANGE;
    bits=(uint32_t)reserve+(uint32_t)inventory->loaded[weapon]; memcpy(total,&bits,4); return RF_OK;
}
int rf_weapon_finish_selection(rf_weapon_selection_state *state,
    const rf_weapon_selection_input *input,const uint8_t owned[64],
    const uint32_t flags_264[64],uint32_t weapon_count,
    const rf_weapon_selection_ops *ops,void *user)
{
    int32_t weapon,current; int status,available;
    if (!state || !input || !owned || !flags_264 || weapon_count>64) return RF_RANGE;
    weapon=input->requested;
    if (weapon>=0 && weapon<32 && (input->paired_mask&(UINT32_C(1)<<weapon)) &&
        weapon==input->paired_first) weapon=input->paired_second;
    if (state->pending_weapon==weapon) return RF_OK;
    current=weapon<input->category_split ? input->current_primary : input->current_secondary;
    if (current==weapon && !(input->force_flag&255)) {
        if (!(input->player_flags&16)) return RF_OK;
        if (!ops || !ops->already_selected) return RF_NOT_FOUND;
        return ops->already_selected(user,weapon);
    }
    available=weapon>=0 && weapon<64 && owned[weapon]!=0;
    if (!available && weapon>=0 && (uint32_t)weapon<weapon_count)
        available=(flags_264[weapon]&UINT32_C(0x40000))!=0;
    if (available) {
        rf_weapon_queue_selection(state,weapon);
        if (!(input->defer_flag&255)) {
            if (!ops || !ops->apply_queued) return RF_NOT_FOUND;
            status=ops->apply_queued(user);
            if (status!=RF_OK) return status;
        }
    }
    if (state->flag_f94) return rf_weapon_clear_followup(state);
    return RF_OK;
}
int rf_weapon_decide_empty(const rf_weapon_inventory *primary,const rf_weapon_inventory *linked,
    const rf_weapon_supply supply[64],const uint32_t flags_264[64],uint32_t weapon_count,
    const int32_t preference[32],const rf_weapon_empty_input *input,rf_weapon_empty_action *action)
{
    rf_weapon_empty_action next={RF_WEAPON_EMPTY_NONE,-1};
    const rf_weapon_inventory *owner; int32_t current,total,other=-1,replacement; int status,paired=0;
    if (!input || !action) return RF_RANGE;
    current=input->current;
    if (!primary || current<0) { *action=next; return RF_OK; }
    if (!supply || !flags_264 || !preference || current>=64 || weapon_count>64 ||
        input->passenger>1 || input->special_block>1 || input->linked_present>1) return RF_RANGE;
    if (!(uint8_t)input->automatic_enabled && current!=input->always_weapon) goto done;
    if (input->passenger && (uint8_t)input->request_flag) goto done;
    if (current==input->block_weapon && input->special_block) goto done;
    if (supply[current].capacity<=0 || current==input->excluded_weapon) goto done;
    owner=primary;
    if (input->linked_present && (input->linked_class==1 || input->linked_class==4)) {
        if (!linked) return RF_RANGE;
        owner=linked;
    }
    status=weapon_total(owner,supply,current,&total); if (status!=RF_OK) return status;
    if (total>0 || ((uint32_t)current<weapon_count && (flags_264[current] & 0x20u))) goto done;
    if (current==input->paired_first && !(uint8_t)input->override_mode) { paired=1; other=input->paired_second; }
    else if (current==input->paired_second) { paired=1; other=input->paired_first; }
    if (paired) {
        status=weapon_total(owner,supply,other,&total); if (status!=RF_OK) return status;
        if (total>0) { next.kind=RF_WEAPON_EMPTY_PAIR; goto done; }
    } else if ((input->linked_present && (input->linked_class==1 || input->linked_class==4)) || input->passenger) {
        next.kind=RF_WEAPON_EMPTY_MESSAGE; goto done;
    }
    status=rf_weapon_choose_available(primary,supply,preference,input->defer_flag,&replacement); if (status!=RF_OK) return status;
    if (replacement>=0) { next.kind=RF_WEAPON_EMPTY_SELECT; next.weapon=replacement; }
done:
    *action=next; return RF_OK;
}

int rf_weapon_consume_shot(rf_weapon_inventory *inventory,const rf_weapon_acquire_definition definitions[64],
    uint32_t weapon_count,int32_t weapon)
{
    int32_t *amount,value,ammo;uint32_t bits;
    if(weapon<0 || weapon>=64)return RF_OK;
    if(!inventory || !definitions || weapon_count>64)return RF_RANGE;
    if((uint32_t)weapon<weapon_count && definitions[weapon].magazine>0)amount=&inventory->loaded[weapon];
    else {
        ammo=definitions[weapon].ammo_type;if(ammo<0 || ammo>=32)return RF_RANGE;
        amount=&inventory->reserve[ammo];
    }
    bits=(uint32_t)*amount-1;memcpy(&value,&bits,4);*amount=value<0?0:value;return RF_OK;
}

int rf_weapon_reserve(const rf_weapon_inventory *inventory,const rf_weapon_supply supply[64],
    int32_t weapon,int32_t *amount)
{
    int32_t type;
    if (!amount) return RF_RANGE;
    if (!inventory || weapon<0) { *amount=0; return RF_OK; }
    if (!supply || weapon>=64) return RF_RANGE;
    type=supply[weapon].ammo_type;
    if (type<0) { *amount=0; return RF_OK; }
    if (type>=32) return RF_RANGE;
    *amount=inventory->reserve[type]; return RF_OK;
}
int rf_weapon_choose_available(const rf_weapon_inventory *inventory,const rf_weapon_supply supply[64],
    const int32_t preference[32],uint32_t defer_flag,int32_t *selected)
{
    int32_t fallback=-1,weapon,reserve,total; uint32_t bits; unsigned i; int status;
    if (!selected) return RF_RANGE;
    if (!inventory) { *selected=-1; return RF_OK; }
    if (!supply || !preference) return RF_RANGE;
    for (i=0;i<32;++i) {
        weapon=preference[i];
        if (weapon<0 || weapon>=64 || !inventory->owned[weapon]) continue;
        if (supply[weapon].capacity>0) {
            status=rf_weapon_reserve(inventory,supply,weapon,&reserve); if (status!=RF_OK) return status;
            bits=(uint32_t)reserve+(uint32_t)inventory->loaded[weapon];
            memcpy(&total,&bits,4);
            if (total<=0) continue;
        }
        if ((supply[weapon].flags_268 & 0x100u) && (uint8_t)defer_flag) {
            if (fallback==-1) fallback=weapon;
        } else { *selected=weapon; return RF_OK; }
    }
    *selected=fallback; return RF_OK;
}

int rf_weapon_reset(rf_weapon_reset_state *state,int32_t weapon,
    const rf_weapon_descriptor descriptors[64],const rf_weapon_reset_context *context,
    rf_motion_playback_state *playback,const rf_motion_playback_resource *resources,uint32_t resource_count,
    const rf_weapon_reset_ops *ops,void *user)
{
    int was_active,status; int32_t sound;
    if (!state || weapon<0 || weapon>=64) return RF_OK;
    if (!descriptors || !context || context->weapon_count>64) return RF_RANGE;
    was_active=state->active[weapon]!=0;
    if (was_active && (uint8_t)context->disabled!=1 && (descriptors[weapon].flags_264 & 6u)) {
        if (state->sound_81c!=-1) {
            if (!ops || !ops->stop_sound) return RF_NOT_FOUND;
            status=ops->stop_sound(user,state->sound_81c); if (status!=RF_OK) return status;
            state->sound_81c=-1;
        }
        if (descriptors[weapon].release_sound_class>-1) {
            if (!ops || !ops->release_sound) return RF_NOT_FOUND;
            status=ops->release_sound(user,descriptors[weapon].release_sound_class,&sound); if (status!=RF_OK) return status;
            state->sound_820=sound;
        }
        if (state->character_present && !(state->flags_810 & 1u)) {
            status=rf_motion_stop_nonlooping(playback,resources,resource_count); if (status!=RF_OK) return status;
        }
    }
    state->active[weapon]=0; state->flags_7d0 &= ~0x2000u;
    if ((uint32_t)weapon<context->weapon_count && (descriptors[weapon].flags_268 & 0x40u) && state->effect_13d4!=-1) {
        if (!ops || !ops->stop_effect) return RF_NOT_FOUND;
        status=ops->stop_effect(user,state->effect_13d4); if (status!=RF_OK) return status;
    }
    /* 41afbb..41b015 performs read-only player/weapon lookups (42a8e0,
     * 4c90f0,48aa30,48aa90) and discards their results. 48aa90 returns a
     * player pointer; it is not a release operation. Stable views permit
     * omitting this block without changing reset effects. */
    if (state->player_present) {
        if (!ops || !ops->player_reset) return RF_NOT_FOUND;
        status=ops->player_reset(user); if (status!=RF_OK) return status;
    }
    return RF_OK;
}


static void weapon_drop_cross(const float a[3],const float b[3],float out[3])
{
    out[0]=(float)((double)a[1]*b[2]-(double)a[2]*b[1]);
    out[1]=(float)((double)a[2]*b[0]-(double)a[0]*b[2]);
    out[2]=(float)((double)a[0]*b[1]-(double)a[1]*b[0]);
}
static int weapon_drop_basis(float basis[9],const float normal[3])
{
    float value[9];double dot,length;uint32_t i;
    for(i=0;i<9;++i)if(!isfinite(basis[i]))return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(normal[i]))return RF_RANGE;
    dot=((double)basis[0]*normal[0]+(double)basis[1]*normal[1])+(double)basis[2]*normal[2];
    if(dot>0)weapon_drop_cross(basis+6,normal,value+3);
    else weapon_drop_cross(normal,basis+6,value+3);
    length=sqrt(((double)value[3]*value[3]+(double)value[4]*value[4])+(double)value[5]*value[5]);
    if(!isfinite(length))return RF_RANGE;
    if(length<=0){value[3]=1;value[4]=value[5]=0;}
    else {length=1.0/length;for(i=3;i<6;++i)value[i]=(float)(value[i]*length);}
    if(dot>0)weapon_drop_cross(normal,value+3,value+6);
    else weapon_drop_cross(value+3,normal,value+6);
    weapon_drop_cross(value+3,value+6,value);
    for(i=0;i<9;++i)if(!isfinite(value[i]))return RF_RANGE;
    memcpy(basis,value,36);return RF_OK;
}
int rf_weapon_drop_sp(rf_weapon_drop_source *s,rf_weapon_inventory *inventory,const rf_weapon_drop_definition definitions[64],
    int32_t excluded,uint32_t parameter,rf_random_state *random,const rf_weapon_drop_backend *b,rf_entity_death_drop_item **result)
{
    rf_weapon_drop_request request={0};rf_entity_death_drop_hit hit={0};rf_entity_death_drop_item *item;
    int32_t weapon,ammo,quantity,reduction;uint32_t draw,i;uint64_t magnitude,product;
    float start[3],delta[3],end_y,size,offset;int status;
    if(!s || !inventory || !definitions || !random || !b || !result || !b->pose || !b->item ||
       !b->remote || !b->resolve_remote || !b->remove || !b->query || !b->create || !b->notify || !b->bounds)return RF_RANGE;
    *result=NULL;weapon=s->current;
    if(weapon==excluded || s->flags_1a8&0x400000u)return RF_OK;
    if(weapon>=0) {
        if(weapon>=64)return RF_RANGE;ammo=definitions[weapon].ammo_type;
        if(ammo<0 || ammo>=32)return RF_RANGE;
        if((int32_t)((uint32_t)inventory->reserve[ammo]+(uint32_t)inventory->loaded[weapon])<=0)return RF_OK;
    }
    request.pose.basis[0]=request.pose.basis[4]=request.pose.basis[8]=1;
    if(b->pose(b->context,&request.pose))return RF_OK;
    request.item=b->item(b->context,s->current);if(request.item==-1)return RF_OK;
    weapon=s->current;if(weapon<0 || weapon>=64)return RF_RANGE;
    quantity=definitions[weapon].quantity;status=rf_random_next(random,&draw);if(status)return status;
    /* Exact trunc(quantity*draw*.2f/32768), using .2f=13421773/2^26.
     * Split the up-to71-bit numerator before multiplying, avoiding64-bit overflow. */
    magnitude=quantity<0?(uint64_t)(-(int64_t)quantity):(uint64_t)quantity;
    product=magnitude*draw;
    magnitude=(((product>>24)*13421773u+(((product&0xffffffu)*13421773u)>>24))>>17);
    reduction=quantity<0?-(int32_t)magnitude:(int32_t)magnitude;
    quantity=(int32_t)((uint32_t)quantity-(uint32_t)reduction);request.quantity=quantity<4?4:quantity;
    if(b->remote(b->context,request.item)&255u)request.item=b->resolve_remote(b->context);
    parameter&=255u;
    memcpy(start,parameter==1?request.pose.position:s->position,12);
    if(!isfinite(s->extent_7c4))return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(start[i]))return RF_RANGE;
    end_y=(float)((double)start[1]-(double)s->extent_7c4*4.0);
    if(parameter==1)start[1]=(float)((double)start[1]+.5);
    else {b->remove(b->context,s->current);s->current=-1;}
    delta[0]=delta[2]=0;delta[1]=(float)((double)end_y-start[1]);
    if(!isfinite(start[1]) || !isfinite(delta[1]))return RF_RANGE;
    status=b->query(b->context,start,delta,&hit);if(status)return status;if(hit.count<=0)return RF_OK;
    status=weapon_drop_basis(request.pose.basis,hit.normal);if(status)return status;
    for(i=0;i<3;++i)if(!isfinite(hit.point[i]))return RF_RANGE;
    memcpy(request.pose.position,hit.point,12);request.owner=s->handle;
    item=b->create(b->context,&request);*result=item;
    if(parameter)b->notify(b->context,s->notification_owner,request.item,request.pose.position);
    if(!item)return RF_OK;item->flags_2bc|=8u;
    status=b->bounds(b->context,item->model,&size);if(status)return status;if(!isfinite(size))return RF_RANGE;
    for(i=0;i<3;++i) {
        offset=(float)((double)hit.normal[i]*size);
        item->base_position[i]=(float)((double)item->base_position[i]+offset);
    }
    memcpy(item->position,item->base_position,12);return RF_OK;
}

int rf_weapon_remove_owned(rf_weapon_inventory *inventory,int32_t weapon,const rf_weapon_remove_backend *b)
{
    uint32_t i=0;int32_t count;rf_weapon_inventory *resolved;
    if(weapon<0 || weapon>=64)return RF_OK;
    if(!inventory || !b || !b->count || !b->special_weapon || !b->inventory || !b->notify ||
       (b->capacity && !b->players))return RF_RANGE;
    inventory->owned[weapon]=0;
    while((count=*b->count)>0 && i<(uint32_t)count) {
        if((uint32_t)count>b->capacity)return RF_RANGE;
        resolved=b->inventory(b->context,b->players[i]);
        if(resolved==inventory && weapon==*b->special_weapon)b->notify(b->context,b->players[i]);
        ++i;
    }
    return RF_OK;
}

int rf_weapon_release_player_slots(uint32_t slots[25],void (*release)(void *,uint32_t),void *context)
{
    unsigned i;
    if(!slots || !release)return RF_RANGE;
    for(i=0;i<25;++i)if(slots[i]) {release(context,slots[i]);slots[i]=0;}
    return RF_OK;
}

static int32_t acquire_wrap_add(int32_t a,int32_t b)
{uint32_t bits=(uint32_t)a+(uint32_t)b;int32_t value;memcpy(&value,&bits,4);return value;}
static int32_t acquire_clamp(int32_t value,int32_t maximum)
{return value<0?0:(value>maximum?maximum:value);}
int rf_weapon_acquire(rf_weapon_inventory *inventory,const rf_weapon_acquire_definition *definition,
    int32_t weapon,int32_t quantity,int (*notify)(void *,rf_weapon_inventory *,uint32_t),void *context)
{
    int32_t ammo,magazine,excess=0;
    if(!inventory || weapon<0 || weapon>=64)return RF_RANGE;
    if(inventory->owned[weapon])return RF_OK;
    if(!definition || !notify)return RF_RANGE;
    ammo=definition->ammo_type;magazine=definition->magazine;
    if(magazine<1) {
        if(ammo>=0) {
            if(ammo>=32)return RF_RANGE;
            inventory->reserve[ammo]=acquire_clamp(acquire_wrap_add(inventory->reserve[ammo],quantity),definition->capacity);
        }
    } else if(quantity==-1)inventory->loaded[weapon]=magazine;
    else {
        if(ammo<0 || ammo>=32)return RF_RANGE;
        if(magazine<quantity)excess=quantity-magazine;
        inventory->loaded[weapon]=quantity<magazine?quantity:magazine;
        inventory->reserve[ammo]=acquire_clamp(acquire_wrap_add(inventory->reserve[ammo],excess),definition->capacity);
    }
    inventory->owned[weapon]=1;
    return notify(context,inventory,1);
}

int rf_weapon_pickup_grant_sp(rf_weapon_inventory *inventory,const rf_weapon_acquire_definition *d,int32_t weapon,int32_t quantity,uint32_t gives_weapon,rf_weapon_pickup_grant *result)
{
    rf_weapon_pickup_grant v={0};int32_t available,added;int status;
    if(!inventory || !d || !result || weapon<0 || weapon>=64 || quantity<0 || gives_weapon>1 || d->ammo_type<0 || d->ammo_type>=32 || d->capacity<0 || d->magazine<0)return RF_RANGE;
    available=inventory->reserve[d->ammo_type];
    if(available<0 || available>d->capacity || inventory->loaded[weapon]<0 || inventory->loaded[weapon]>d->magazine)return RF_RANGE;
    if(gives_weapon && !inventory->owned[weapon]) {
        if(inventory->loaded[weapon])return RF_RANGE;
        int64_t limit=(int64_t)d->magazine+d->capacity-available;
        if(quantity>limit)quantity=(int32_t)limit;
        status=rf_weapon_acquire_sp(inventory,d,weapon,quantity);if(status)return status;
        v.acquired=1;v.rounds=(uint32_t)inventory->loaded[weapon]+(uint32_t)(inventory->reserve[d->ammo_type]-available);
    } else {
        added=d->capacity-available;if(added>quantity)added=quantity;
        inventory->reserve[d->ammo_type]+=added;v.rounds=(uint32_t)added;
    }
    *result=v;return RF_OK;
}

int rf_weapon_reload_transfer(rf_weapon_inventory *inventory,const rf_weapon_acquire_definition *definition,int32_t weapon,uint32_t *transferred)
{
    int32_t missing,available,amount;
    if(!inventory || !definition || !transferred || weapon<0 || weapon>=64 || !inventory->owned[weapon] ||
       definition->ammo_type<0 || definition->ammo_type>=32 || definition->magazine<=0)return RF_RANGE;
    available=inventory->reserve[definition->ammo_type];
    if(available<0 || inventory->loaded[weapon]<0 || inventory->loaded[weapon]>definition->magazine)return RF_RANGE;
    missing=definition->magazine-inventory->loaded[weapon];amount=missing<available?missing:available;
    inventory->loaded[weapon]+=amount;inventory->reserve[definition->ammo_type]-=amount;
    *transferred=(uint32_t)amount;return RF_OK;
}

int rf_weapon_add_ammo(rf_weapon_inventory *inventory,rf_weapon_ammo_state *state,
    const rf_weapon_acquire_definition *definition,int32_t weapon,int32_t quantity,
    const rf_weapon_ammo_backend *backend)
{
    int32_t ammo;uint32_t reloading;int status;
    if(!inventory || !state || !definition || weapon<0 || weapon>=64)return RF_RANGE;
    ammo=definition->ammo_type;if(ammo<0)return RF_OK;
    if(ammo>=32 || !backend || !backend->is_reloading)return RF_RANGE;
    if(inventory->reserve[ammo]<0)inventory->reserve[ammo]=0;
    inventory->reserve[ammo]=acquire_wrap_add(inventory->reserve[ammo],quantity);
    status=backend->is_reloading(backend->context,&reloading);if(status)return status;
    if((reloading&255u) && state->current==weapon)
        state->pending=acquire_wrap_add(state->pending,quantity);
    if(inventory->reserve[ammo]>definition->capacity)inventory->reserve[ammo]=definition->capacity;
    if(weapon<state->weapon_count && definition->magazine>0 &&
        inventory->loaded[weapon]<1 && state->current==weapon) {
        if(!backend->reload)return RF_RANGE;
        return backend->reload(backend->context,0,0);
    }
    return RF_OK;
}

/* A96-bit unsigned numerator, rounded to the original x87 precision. */
static void pickup_round64(uint32_t *hi,uint64_t *lo)
{
    uint32_t h=*hi,drop=0;uint64_t mask,half,low,old;
    while(h){++drop;h>>=1;}
    if(!drop)return;
    mask=(UINT64_C(1)<<drop)-1;half=UINT64_C(1)<<(drop-1);low=*lo&mask;
    *lo&=~mask;
    if(low>half || (low==half && ((*lo>>drop)&1))) {
        old=*lo;*lo+=UINT64_C(1)<<drop;if(*lo<old)++*hi;
    }
}
static int32_t pickup_scaled_round(int32_t quantity,uint32_t factor,uint32_t shift,uint32_t hundredths)
{
    uint64_t magnitude=quantity<0?(uint64_t)(-(int64_t)quantity):(uint64_t)quantity;
    uint64_t product=magnitude*factor,lo=product,half,old,upper;
    uint32_t hi=0,bits;int32_t result;
    if(hundredths) {
        /* .01f is10737418 /2^30; first product is exact in x87. */
        upper=(product>>32)*UINT64_C(10737418);
        lo=(product&UINT64_C(0xffffffff))*UINT64_C(10737418);
        old=lo;lo+=upper<<32;hi=(uint32_t)(upper>>32)+(lo<old);shift+=30;
        pickup_round64(&hi,&lo);
    }
    half=UINT64_C(1)<<(shift-1);
    if(quantity<0) {
        if(!hi && lo<half)return 0;
        old=lo;lo-=half;if(old<half)--hi;
    } else {old=lo;lo+=half;if(lo<old)++hi;}
    pickup_round64(&hi,&lo);
    bits=(uint32_t)((lo>>shift)|((uint64_t)hi<<(64-shift)));
    if(quantity<0)bits=0u-bits;
    memcpy(&result,&bits,4);return result;
}
int rf_weapon_pickup_amount(int32_t quantity,int32_t reserve,int32_t capacity,
    uint32_t difficulty,uint32_t special,uint32_t scale_disabled,
    int32_t *granted,int32_t *displayed)
{
    static const uint32_t factors[4]={3,15099494,13421773,10066330};
    int32_t amount=quantity,room,message;uint32_t bits;
    if(!granted || !displayed || granted==displayed || difficulty>=4)return RF_RANGE;
    if(!scale_disabled) {
        amount=pickup_scaled_round(quantity,factors[difficulty],difficulty?24:1,special!=0);
        if(special){bits=(uint32_t)amount*100u;memcpy(&amount,&bits,4);}
    }
    bits=(uint32_t)capacity-(uint32_t)reserve;memcpy(&room,&bits,4);
    if(amount>room)amount=room;
    if(special && amount<100)amount=100;
    message=amount;
    if(special){message=pickup_scaled_round(amount,1,0,1);if(message<1)message=1;}
    *granted=amount;*displayed=message;return RF_OK;
}

static int weapon_sp_acquire_notice(void *context,rf_weapon_inventory *inventory,uint32_t reason)
{(void)context;(void)inventory;(void)reason;return RF_OK;}
int rf_weapon_acquire_sp(rf_weapon_inventory *inventory,const rf_weapon_acquire_definition *definition,
    int32_t weapon,int32_t quantity)
{return rf_weapon_acquire(inventory,definition,weapon,quantity,weapon_sp_acquire_notice,NULL);}

static int weapon_startup_fill(rf_weapon_inventory *inventory,
    const rf_weapon_acquire_definition definitions[64],int32_t weapon)
{
    int32_t ammo;if(weapon<0 || weapon>=64)return RF_RANGE;
    ammo=definitions[weapon].ammo_type;if(ammo<0)return RF_OK;if(ammo>=32)return RF_RANGE;
    inventory->reserve[ammo]=definitions[weapon].capacity;return RF_OK;
}
int rf_weapon_startup_grant_sp(rf_weapon_inventory *inventory,rf_weapon_startup_state *state,
    const int32_t defaults[3],const rf_weapon_acquire_definition definitions[64],
    int (*equip)(void *,int32_t),void *context)
{
    int32_t weapon;int status;
    if(!inventory || !state || !defaults || !definitions)return RF_RANGE;
    weapon=defaults[0];
    if(weapon!=-1) {
        if(weapon<0 || weapon>=64)return RF_RANGE;
        status=rf_weapon_acquire_sp(inventory,definitions+weapon,weapon,-1);if(status)return status;
        state->primary=defaults[0];if(!equip)return RF_RANGE;
        status=equip(context,state->primary);if(status)return status;
        status=weapon_startup_fill(inventory,definitions,defaults[0]);if(status)return status;
    }
    weapon=defaults[1];
    if(weapon!=-1) {
        if(weapon<0 || weapon>=64)return RF_RANGE;
        status=rf_weapon_acquire_sp(inventory,definitions+weapon,weapon,-1);if(status)return status;
        state->secondary=defaults[1];
        status=weapon_startup_fill(inventory,definitions,state->secondary);if(status)return status;
    }
    weapon=defaults[2];
    if(weapon!=-1) {
        if(weapon<0 || weapon>=64)return RF_RANGE;
        status=rf_weapon_acquire_sp(inventory,definitions+weapon,weapon,-1);if(status)return status;
    }
    return RF_OK;
}

int rf_weapon_spread_ray(const float ray[3],float degrees,rf_random_state *random,float result[3])
{
    float axis[3],value[3];double length=0;rf_random_state next;unsigned i;int status;
    if(!ray || !random || !result || !isfinite(degrees) || degrees<0 || degrees>90)return RF_RANGE;
    for(i=0;i<3;i++){if(!isfinite(ray[i]))return RF_RANGE;length+=(double)ray[i]*ray[i];}
    if(!(length>0) || length>1e12)return RF_RANGE;
    if(degrees==0){memcpy(result,ray,12);return RF_OK;}
    length=sqrt(length);for(i=0;i<3;i++)axis[i]=(float)(ray[i]/length);
    next=*random;status=rf_particle_cone_oriented(axis,(float)cos(degrees*0.017453292519943295),&next,value);
    if(status)return status;
    for(i=0;i<3;i++)value[i]=(float)(value[i]*length);
    memcpy(result,value,12);*random=next;return RF_OK;
}
