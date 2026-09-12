#include "rf/weapon.h"
#include "rf/timer.h"
#include <string.h>
#include <math.h>
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
