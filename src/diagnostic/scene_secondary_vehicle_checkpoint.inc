#ifndef RF_SCENE_SECONDARY_VEHICLE_CHECKPOINT_INCLUDED
#define RF_SCENE_SECONDARY_VEHICLE_CHECKPOINT_INCLUDED
/* RFSV1 wraps RFVA2 beneath optional RFPV. One owner-local secondary APC is
 * supported. RFWC already pins the exact authored source. Wire fields are
 * explicit little-endian words, never runtime pointers, handles or structs.
 * Physics resources/inertia come from the real matching owner. Retained graph
 * nodes use the existing RFNC convention: 1..N, MAX=start, MAX-1=goal.
 * Endpoint positions are authoritative; the next graph request regenerates
 * endpoint search scratch exactly as for the existing NPC checkpoint route.
 * Include after secondary runtime and RFPV implementation. No event replay. */
enum {SCENE_SECONDARY_VEHICLE_HEADER=16,SCENE_SECONDARY_VEHICLE_ROW=256};
struct scene_secondary_vehicle_checkpoint_stage {
    scene_secondary_vehicle_runtime *runtime;
    scene_passive_vehicle_saved passive;
    float velocity[3]; /* Accepted displacement/dt projection, not rigid velocity. */
    uint32_t bytes,admitted;
};
/* captures,prepares,admissions,assignments,legacy clears,UID,retired,status */
uint32_t rf_scene_secondary_vehicle_checkpoint[8];
/* valid,UID,handle,slot,profile,retired,event,active; exact LE row words[64]. */
uint32_t rf_scene_secondary_vehicle_restore[80];
/* runs,checks,passed mask,failed mask,status,wire bytes,legacy pass,unchanged */
uint32_t rf_scene_secondary_vehicle_checkpoint_audit[16];
static uint32_t scene_secondary_vehicle_checkpoint_bytes(const scene_secondary_vehicle_checkpoint_stage *s)
{return s?s->bytes:0;}
static void scene_secondary_vehicle_checkpoint_close(scene_secondary_vehicle_checkpoint_stage **s)
{if(s&&*s){free((*s)->runtime);free(*s);*s=NULL;}}
static int scene_secondary_vehicle_checkpoint_basis(const float *m)
{
    for(uint32_t i=0;i<9;i++)if(!isfinite(m[i]))return RF_FORMAT;
    for(uint32_t i=0;i<3;i++)for(uint32_t j=i;j<3;j++){
        double d=0;for(uint32_t k=0;k<3;k++)d+=(double)m[i*3+k]*m[j*3+k];
        if(fabs(d-(i==j?1.:0.))>.015)return RF_FORMAT;
    }
    double d=(double)m[0]*(m[4]*m[8]-m[5]*m[7])-
        (double)m[1]*(m[3]*m[8]-m[5]*m[6])+(double)m[2]*(m[3]*m[7]-m[4]*m[6]);
    return fabs(d-1.)<=.025?RF_OK:RF_FORMAT;
}
static int scene_secondary_vehicle_checkpoint_floats(const float *v,uint32_t n)
{for(uint32_t i=0;i<n;i++)if(!isfinite(v[i])||fabsf(v[i])>1e9f)return RF_FORMAT;return RF_OK;}
static void scene_secondary_vehicle_checkpoint_put_floats(unsigned char *p,const float *v,uint32_t n)
{for(uint32_t i=0;i<n;i++)scene_history_put_float(p+i*4,v[i]);}
static void scene_secondary_vehicle_checkpoint_get_floats(const unsigned char *p,float *v,uint32_t n)
{for(uint32_t i=0;i<n;i++)v[i]=scene_history_float(p+i*4);}
static int scene_secondary_vehicle_checkpoint_ordinal(const scene_secondary_vehicle_runtime *r,
    const rf_entity_navigation_candidate *node,uint32_t *ordinal)
{
    if(!r||!node||!ordinal)return RF_RANGE;
    if(node==&r->navigation.start){*ordinal=UINT32_MAX;return RF_OK;}
    if(node==&r->navigation.goal){*ordinal=UINT32_MAX-1;return RF_OK;}
    if(!campaign_navigation.references)return RF_NOT_FOUND;
    for(uint32_t i=0;i<campaign_navigation.count;i++)if(campaign_navigation.references[i].candidate==node){
        *ordinal=i+1;return RF_OK;
    }
    return RF_NOT_FOUND;
}
static int scene_secondary_vehicle_checkpoint_owner(const scene_secondary_vehicle_runtime *r)
{
    uint32_t matches=0,seeds=0;
    if(!r||!r->uid||r->uid==UINT32_MAX||r->profile!=2||r->slot>=campaign_passive_vehicle_count||!campaign_passive_vehicles||
       r->handle==campaign_authored_vehicle_handle)return RF_NOT_FOUND;
    const scene_passive_vehicle *p=campaign_passive_vehicles+r->slot;
    if(p->uid!=r->uid||p->handle!=r->handle||p->resource_kind!=4||p->attached||p->group_owned||
       p->seat_driver!=UINT32_MAX||p->seat_occupants[0]!=-1||p->seat_occupants[1]!=-1||
       p->damage.state.effects.handle!=r->handle||
       rf_object_registry_lookup(&campaign_registry,r->handle)!=p)return RF_NOT_FOUND;
    for(uint32_t i=0;i<campaign_passive_vehicle_count;i++)matches+=campaign_passive_vehicles[i].uid==r->uid;
    if(!campaign_seeds.records.items||!campaign_seeds.items||!campaign_seeds.classes||
       (campaign_group_runtime.count&&!campaign_controller_views)||scene_npc_seat_checkpoint_host(r->handle))return RF_NOT_FOUND;
    for(uint32_t i=0;i<campaign_seeds.records.count;i++)if((uint32_t)campaign_seeds.records.items[i].record.uid==r->uid){
        uint32_t cls=campaign_seeds.items[i].class_index;
        if(cls>=campaign_seeds.class_count||strcmp(campaign_seeds.records.items[i].record.class_name,"APC"))return RF_FORMAT;
        ++seeds;
    }
    if(matches!=1||seeds!=1)return RF_FORMAT;
    for(uint32_t i=0;i<campaign_group_runtime.count;i++){
        const rf_group_controller_view *v=campaign_controller_views+i;
        if(v->general_count&&!v->general_handles)return RF_FORMAT;
        for(uint32_t j=0;j<v->general_count;j++)if(v->general_handles[j]==r->handle)return RF_NOT_FOUND;
    }
    return RF_OK;
}
static int scene_secondary_vehicle_checkpoint_event(const scene_secondary_vehicle_runtime *r)
{
    uint32_t found=0;
    if(!r->route.event)return RF_OK;
    if(r->route.event==UINT32_MAX)return RF_FORMAT;
    for(uint32_t i=0;i<campaign_events.count;i++){
        const rf_runtime_event *e=campaign_events.items+i;uint32_t linked=0;
        if(!e->authored||e->authored->record.uid!=r->route.event)continue;
        const rf_level_event *a=&e->authored->record;
        if(++found!=1||strcmp(a->type,"Goto")||(a->link_count&&!e->links)||
           memcmp(a->position,r->route.target,12))return RF_FORMAT;
        for(uint32_t j=0;j<a->link_count;j++)if((e->links[j].kind==1||e->links[j].kind==2)&&
            e->links[j].value==r->handle)++linked;
        if(linked!=1)return RF_FORMAT;
    }
    return found==1?RF_OK:RF_NOT_FOUND;
}
static int scene_secondary_vehicle_checkpoint_valid(const scene_secondary_vehicle_runtime *r,
    const scene_passive_vehicle_saved *p)
{
    const rf_vehicle_rigid_state *b=&r->physics.state;uint32_t ordinal;
    if(r->profile!=2||!r->uid||r->uid==UINT32_MAX||r->retired>1||r->scripted_physics>1||b->skip_forces>1||
       r->route.active>1||r->route.retry>60||r->navigation.retained.count>4||r->route.index>4||
       p->attached||p->uid!=r->uid||!p->loaded||!p->has_damage||
       p->destroyed>1||!isfinite(p->health)||fabsf(p->health)>1e7f||!isfinite(p->armor)||
       p->armor<0||p->armor>1e7f||(p->destroyed&&p->health>0)||
       memcmp(p->position,b->position,12)||memcmp(p->basis,b->orientation,36))return RF_FORMAT;
    if(r->scripted_physics&&!(r->physics.body_flags&0x80000000u)&&
       (r->physics.body_flags&0x18000000u)!=0x18000000u)return RF_FORMAT;
    if(scene_secondary_vehicle_checkpoint_basis(b->orientation)||
       scene_secondary_vehicle_checkpoint_floats(b->position,3)||
       scene_secondary_vehicle_checkpoint_floats(b->velocity,3)||
       scene_secondary_vehicle_checkpoint_floats(b->momentum,3)||
       scene_secondary_vehicle_checkpoint_floats(b->force,3)||
       scene_secondary_vehicle_checkpoint_floats(b->torque,3)||
       scene_secondary_vehicle_checkpoint_floats(r->route.target,3)||
       scene_secondary_vehicle_checkpoint_floats(r->navigation.start.position,3)||
       scene_secondary_vehicle_checkpoint_floats(r->navigation.goal.position,3)||
       !isfinite(r->navigation.cost)||r->navigation.cost<0||r->navigation.cost>1e9f)return RF_FORMAT;
    if(!r->route.event){
        if(r->route.active||r->route.index||r->route.retry||r->navigation.retained.count||r->navigation.cost)return RF_FORMAT;
        for(uint32_t i=0;i<3;i++)if(r->route.target[i]||r->navigation.start.position[i]||r->navigation.goal.position[i])return RF_FORMAT;
    }else{
        if(!r->route.index||(r->navigation.retained.count&&
           (r->navigation.retained.count<2||r->route.index>r->navigation.retained.count)))return RF_FORMAT;
        if(r->navigation.retained.count&&memcmp(r->route.target,r->navigation.goal.position,12))return RF_FORMAT;
    }
    if(r->retired){
        if(r->route.active||(p->object_flags&(2u|0x4000u))!=(2u|0x4000u))return RF_FORMAT;
        for(uint32_t i=0;i<3;i++)if(b->velocity[i]||b->momentum[i]||b->force[i]||b->torque[i])return RF_FORMAT;
    }else if((p->object_flags&2u)&&(!p->destroyed||p->health>0))return RF_FORMAT;
    for(uint32_t i=0;i<r->navigation.retained.count;i++){
        if(scene_secondary_vehicle_checkpoint_ordinal(r,r->navigation.retained.nodes[i],&ordinal)||
           scene_secondary_vehicle_checkpoint_floats(r->navigation.retained.nodes[i]->position,3))return RF_FORMAT;
    }
    return scene_secondary_vehicle_checkpoint_event(r);
}
static void scene_secondary_vehicle_checkpoint_passive(const scene_secondary_vehicle_runtime *r,
    scene_passive_vehicle_saved *p)
{
    const scene_passive_vehicle *owner=campaign_passive_vehicles+r->slot;memset(p,0,sizeof(*p));
    p->uid=owner->uid;p->attached=owner->attached;p->loaded=p->has_damage=1;
    memcpy(p->position,owner->pose.position,12);memcpy(p->basis,owner->pose.input_matrix,36);
    p->health=owner->damage.state.effects.health;p->armor=owner->damage.state.effects.armor;
    p->object_flags=owner->damage.object_flags;p->flags_814=owner->damage.state.effects.flags_814;
    p->flags_810=owner->damage.state.effects.flags_810;p->destroyed=owner->damage.destroyed;
}
static void scene_secondary_vehicle_checkpoint_encode(unsigned char *p,const scene_secondary_vehicle_runtime *r,
    const scene_passive_vehicle_saved *v,const float velocity[3])
{
    const rf_vehicle_rigid_state *b=&r->physics.state;uint32_t ordinal=0;memset(p,0,SCENE_SECONDARY_VEHICLE_ROW);
    scene_history_put(p,r->uid);scene_history_put(p+4,r->profile);scene_history_put(p+8,r->retired);
    scene_history_put(p+12,r->scripted_physics);scene_history_put(p+16,r->physics.body_flags);scene_history_put(p+20,b->skip_forces);
    scene_history_put(p+24,r->route.active);scene_history_put(p+28,r->route.event);scene_history_put(p+32,r->route.index);
    scene_history_put(p+36,r->route.retry);scene_history_put(p+40,r->navigation.retained.count);
    scene_secondary_vehicle_checkpoint_put_floats(p+48,b->position,3);scene_secondary_vehicle_checkpoint_put_floats(p+60,b->orientation,9);
    scene_secondary_vehicle_checkpoint_put_floats(p+96,b->velocity,3);scene_secondary_vehicle_checkpoint_put_floats(p+108,b->momentum,3);
    scene_secondary_vehicle_checkpoint_put_floats(p+120,b->force,3);scene_secondary_vehicle_checkpoint_put_floats(p+132,b->torque,3);
    scene_secondary_vehicle_checkpoint_put_floats(p+144,r->route.target,3);
    scene_secondary_vehicle_checkpoint_put_floats(p+156,r->navigation.start.position,3);
    scene_secondary_vehicle_checkpoint_put_floats(p+168,r->navigation.goal.position,3);
    for(uint32_t i=0;i<r->navigation.retained.count;i++){
        scene_secondary_vehicle_checkpoint_ordinal(r,r->navigation.retained.nodes[i],&ordinal);scene_history_put(p+180+i*4,ordinal);
    }
    scene_history_put_float(p+196,r->navigation.cost);scene_history_put_float(p+200,v->health);scene_history_put_float(p+204,v->armor);
    scene_history_put(p+208,v->object_flags);scene_history_put(p+212,v->flags_814);scene_history_put(p+216,v->flags_810);
    scene_history_put(p+220,v->destroyed);
    scene_secondary_vehicle_checkpoint_put_floats(p+224,velocity,3);
}
static int scene_secondary_vehicle_checkpoint_capture(const scene_stream *s,unsigned char *p,uint32_t capacity,
    uint32_t inner_bytes,uint32_t *written)
{
    const scene_secondary_vehicle_runtime *r=campaign_secondary_vehicle;scene_passive_vehicle_saved saved;int status;
    uint64_t required=SCENE_SECONDARY_VEHICLE_HEADER+(uint64_t)inner_bytes+SCENE_SECONDARY_VEHICLE_ROW;
    if(!s||!p||!written||inner_bytes>capacity)return RF_RANGE;*written=0;
    if(!r){*written=inner_bytes;return RF_OK;}
    if(inner_bytes<SCENE_PASSIVE_VEHICLE_HEADER||memcmp(p,"RFVA",4)||scene_history_word(p+4)!=2)return RF_FORMAT;
    if(required>capacity||required>RF_CHECKPOINT_FILE_MAX)return RF_RANGE;
    status=scene_secondary_vehicle_checkpoint_owner(r);if(status)return status;
    if(scene_secondary_vehicle_has_rider(r->handle))return RF_NOT_FOUND;
    scene_secondary_vehicle_checkpoint_passive(r,&saved);
    status=scene_secondary_vehicle_checkpoint_valid(r,&saved);if(status)return status;
    if(scene_secondary_vehicle_checkpoint_floats(campaign_passive_vehicles[r->slot].velocity,3)||
       campaign_passive_vehicles[r->slot].scripted_physics!=r->scripted_physics||
       campaign_passive_vehicles[r->slot].physics_body_flags!=r->physics.body_flags)return RF_FORMAT;
    if(r->retired)for(uint32_t i=0;i<3;i++)if(campaign_passive_vehicles[r->slot].velocity[i])return RF_FORMAT;
    memmove(p+SCENE_SECONDARY_VEHICLE_HEADER,p,inner_bytes);memcpy(p,"RFSV",4);
    scene_history_put(p+4,1);scene_history_put(p+8,inner_bytes);scene_history_put(p+12,1);
    scene_secondary_vehicle_checkpoint_encode(p+SCENE_SECONDARY_VEHICLE_HEADER+inner_bytes,r,&saved,
        campaign_passive_vehicles[r->slot].velocity);
    ++rf_scene_secondary_vehicle_checkpoint[0];rf_scene_secondary_vehicle_checkpoint[5]=r->uid;
    rf_scene_secondary_vehicle_checkpoint[6]=r->retired;*written=(uint32_t)required;return RF_OK;
}
static int scene_secondary_vehicle_checkpoint_prepare(scene_stream *s,const unsigned char *p,uint32_t bytes,uint32_t budget,
    scene_secondary_vehicle_checkpoint_stage **out,const unsigned char **inner,uint32_t *inner_bytes)
{
    scene_secondary_vehicle_checkpoint_stage *stage=NULL;scene_secondary_vehicle_runtime *r;
    uint32_t nested,slot,matches=0;const unsigned char *row;int status;uint64_t required;
    if(!s||!out||*out||!inner||!inner_bytes||(!p&&bytes))return RF_RANGE;*inner=p;*inner_bytes=bytes;
    if(bytes<4||memcmp(p,"RFSV",4))return RF_OK;
    if(bytes<SCENE_SECONDARY_VEHICLE_HEADER||scene_history_word(p+4)!=1||scene_history_word(p+12)!=1)return RF_FORMAT;
    nested=scene_history_word(p+8);required=SCENE_SECONDARY_VEHICLE_HEADER+(uint64_t)nested+SCENE_SECONDARY_VEHICLE_ROW;
    if(required!=bytes||required>RF_CHECKPOINT_FILE_MAX||nested<SCENE_PASSIVE_VEHICLE_HEADER||
       memcmp(p+SCENE_SECONDARY_VEHICLE_HEADER,"RFVA",4)||scene_history_word(p+SCENE_SECONDARY_VEHICLE_HEADER+4)!=2)return RF_FORMAT;
    if((uint64_t)sizeof(*stage)+sizeof(*r)>budget)return RF_RANGE;
    row=p+SCENE_SECONDARY_VEHICLE_HEADER+nested;
    if(!scene_history_word(row)||scene_history_word(row)==UINT32_MAX||scene_history_word(row+4)!=2||scene_history_word(row+44))return RF_FORMAT;
    for(uint32_t i=236;i<SCENE_SECONDARY_VEHICLE_ROW;i++)if(row[i])return RF_FORMAT;
    slot=0;for(uint32_t i=0;i<campaign_passive_vehicle_count;i++)if(campaign_passive_vehicles[i].uid==scene_history_word(row)){slot=i;++matches;}
    if(matches!=1)return RF_NOT_FOUND;
    stage=calloc(1,sizeof(*stage));if(!stage)return RF_IO;
    stage->runtime=calloc(1,sizeof(*r));if(!stage->runtime){scene_secondary_vehicle_checkpoint_close(&stage);return RF_IO;}
    stage->bytes=sizeof(*stage)+sizeof(*r);r=stage->runtime;
    status=scene_secondary_vehicle_prepare(s,slot,r);if(status)goto fail;
    status=scene_secondary_vehicle_checkpoint_owner(r);if(status)goto fail;
    r->retired=scene_history_word(row+8);r->scripted_physics=scene_history_word(row+12);
    r->physics.body_flags=scene_history_word(row+16);r->physics.state.skip_forces=scene_history_word(row+20);
    r->route.active=scene_history_word(row+24);r->route.event=scene_history_word(row+28);
    r->route.index=scene_history_word(row+32);r->route.retry=scene_history_word(row+36);
    r->navigation.retained.count=scene_history_word(row+40);
    if(r->navigation.retained.count>4){status=RF_FORMAT;goto fail;}
    scene_secondary_vehicle_checkpoint_get_floats(row+48,r->physics.state.position,3);
    scene_secondary_vehicle_checkpoint_get_floats(row+60,r->physics.state.orientation,9);
    scene_secondary_vehicle_checkpoint_get_floats(row+96,r->physics.state.velocity,3);
    scene_secondary_vehicle_checkpoint_get_floats(row+108,r->physics.state.momentum,3);
    scene_secondary_vehicle_checkpoint_get_floats(row+120,r->physics.state.force,3);
    scene_secondary_vehicle_checkpoint_get_floats(row+132,r->physics.state.torque,3);
    scene_secondary_vehicle_checkpoint_get_floats(row+144,r->route.target,3);
    scene_secondary_vehicle_checkpoint_get_floats(row+156,r->navigation.start.position,3);
    scene_secondary_vehicle_checkpoint_get_floats(row+168,r->navigation.goal.position,3);
    memcpy(r->navigation.start.query_point,r->navigation.start.position,12);
    memcpy(r->navigation.goal.query_point,r->navigation.goal.position,12);
    for(uint32_t i=0;i<4;i++){
        uint32_t ordinal=scene_history_word(row+180+i*4);
        if(i>=r->navigation.retained.count){if(ordinal){status=RF_FORMAT;goto fail;}continue;}
        if(ordinal==UINT32_MAX)r->navigation.retained.nodes[i]=&r->navigation.start;
        else if(ordinal==UINT32_MAX-1)r->navigation.retained.nodes[i]=&r->navigation.goal;
        else{
            if(!ordinal||ordinal>campaign_navigation.count||!campaign_navigation.references||
               !campaign_navigation.references[ordinal-1].candidate){status=RF_FORMAT;goto fail;}
            r->navigation.retained.nodes[i]=campaign_navigation.references[ordinal-1].candidate;
        }
    }
    r->navigation.cost=scene_history_float(row+196);
    scene_passive_vehicle_saved *v=&stage->passive;v->uid=r->uid;v->loaded=v->has_damage=1;
    memcpy(v->position,r->physics.state.position,12);memcpy(v->basis,r->physics.state.orientation,36);
    v->health=scene_history_float(row+200);v->armor=scene_history_float(row+204);v->object_flags=scene_history_word(row+208);
    v->flags_814=scene_history_word(row+212);v->flags_810=scene_history_word(row+216);v->destroyed=scene_history_word(row+220);
    scene_secondary_vehicle_checkpoint_get_floats(row+224,stage->velocity,3);
    status=scene_secondary_vehicle_checkpoint_floats(stage->velocity,3);if(status)goto fail;
    if(r->retired)for(uint32_t i=0;i<3;i++)if(stage->velocity[i]){status=RF_FORMAT;goto fail;}
    status=scene_secondary_vehicle_checkpoint_valid(r,v);if(status)goto fail;
    status=scene_secondary_vehicle_bind(s,r);if(status)goto fail;
    *out=stage;*inner=p+SCENE_SECONDARY_VEHICLE_HEADER;*inner_bytes=nested;
    ++rf_scene_secondary_vehicle_checkpoint[1];return RF_OK;
fail:
    scene_secondary_vehicle_checkpoint_close(&stage);return status;
}
static int scene_secondary_vehicle_checkpoint_admit(const scene_stream *s,scene_secondary_vehicle_checkpoint_stage *stage,
    const scene_passive_vehicle_checkpoint_stage *passive,const scene_vehicle_physics_checkpoint_stage *physics)
{
    if(!stage)return RF_OK;stage->admitted=0;
    if(!s||!stage->runtime||!passive||passive->version!=2||!passive->rows)return RF_FORMAT;
    const scene_secondary_vehicle_runtime *r=stage->runtime;const scene_passive_vehicle_saved *p,*v=&stage->passive;
    int status=scene_secondary_vehicle_checkpoint_owner(r);if(status)return status;
    if(r->slot>=passive->count)return RF_FORMAT;p=passive->rows+r->slot;
    if(p->uid!=v->uid||!p->loaded||!p->has_damage||p->attached||
       memcmp(p->position,v->position,12)||memcmp(p->basis,v->basis,36)||
       memcmp(&p->health,&v->health,4)||memcmp(&p->armor,&v->armor,4)||
       p->object_flags!=v->object_flags||p->flags_814!=v->flags_814||p->flags_810!=v->flags_810||
       p->destroyed!=v->destroyed)return RF_FORMAT;
    status=scene_secondary_vehicle_checkpoint_valid(r,p);if(status)return status;
    if(physics){
        uint32_t found=0;if(!physics->admitted||!physics->rows)return RF_FORMAT;
        for(uint32_t i=0;i<physics->count;i++)if(physics->rows[i].uid==r->uid){
            const scene_vehicle_physics_saved *f=physics->rows+i;++found;
            if(f->role!=1||f->scripted!=r->scripted_physics||f->body_flags!=r->physics.body_flags||
               f->pending||f->skip_forces||memcmp(f->velocity,stage->velocity,12)||
               f->object_bits!=(v->object_flags&scene_vehicle_physics_checkpoint_object_mask(physics->version))||
               f->entity_bits!=(v->flags_814&0x800u))return RF_FORMAT;
        }
        if(found!=1)return RF_FORMAT;
    }else if(r->scripted_physics||(v->flags_814&0x800u))return RF_FORMAT;
    stage->admitted=1;++rf_scene_secondary_vehicle_checkpoint[2];return RF_OK;
}
/* Pure pointer/state publication after ALL candidate-world clearance and
 * RFVA/RFPV publication. The candidate already owns correctly rebound pointers.
 * Legacy absence clears prior session state without executing authored Goto. */
static void scene_secondary_vehicle_checkpoint_assign(scene_stream *s,scene_secondary_vehicle_checkpoint_stage *stage)
{
    uint32_t *p=rf_scene_secondary_vehicle_restore;unsigned char row[SCENE_SECONDARY_VEHICLE_ROW];
    if(stage&&!stage->admitted)return;memset(p,0,sizeof(rf_scene_secondary_vehicle_restore));
    scene_secondary_vehicle_close();campaign_secondary_vehicle=stage?stage->runtime:NULL;
    if(!stage){++rf_scene_secondary_vehicle_checkpoint[4];return;}stage->runtime=NULL;
    scene_secondary_vehicle_runtime *r=campaign_secondary_vehicle;scene_passive_vehicle *owner=campaign_passive_vehicles+r->slot;
    scene_vehicle_owner_collision_bind(s,1);
    owner->scripted_physics=r->scripted_physics;owner->physics_body_flags=r->physics.body_flags;
    memcpy(owner->velocity,stage->velocity,12);memcpy(owner->pose.velocity,stage->velocity,12);owner->motion_valid=0;
    memcpy(owner->previous_position,r->physics.state.position,12);memcpy(owner->previous_basis,r->physics.state.orientation,36);
    p[0]=1;p[1]=r->uid;p[2]=r->handle;p[3]=r->slot;p[4]=r->profile;p[5]=r->retired;p[6]=r->route.event;p[7]=r->route.active;
    scene_secondary_vehicle_checkpoint_encode(row,r,&stage->passive,stage->velocity);
    for(uint32_t i=0;i<SCENE_SECONDARY_VEHICLE_ROW/4;i++)p[8+i]=scene_history_word(row+i*4);
    ++rf_scene_secondary_vehicle_checkpoint[3];rf_scene_secondary_vehicle_checkpoint[5]=r->uid;
    rf_scene_secondary_vehicle_checkpoint[6]=r->retired;
}
/* Bounded native audit: encode current real owners and reject malformed
 * candidates without assigning any stage, firing events or moving a body.
 * A one-row RFPV projection exercises this wrapper's cross-component gate;
 * the ordinary load independently admits the complete RFPV collection. */
static int scene_secondary_vehicle_checkpoint_audit(scene_stream *s)
{
    scene_secondary_vehicle_runtime *live=campaign_secondary_vehicle,live_before;
    scene_passive_vehicle owner_before;scene_secondary_vehicle_checkpoint_stage *stage=NULL;
    scene_passive_vehicle_checkpoint_stage *passive=NULL;
    scene_vehicle_physics_saved projection={0};scene_vehicle_physics_checkpoint_stage physics={0};
    unsigned char *wire=NULL,*scratch=NULL,encoded[SCENE_SECONDARY_VEHICLE_ROW];
    const unsigned char *inner=NULL;uint32_t nested=0,bytes=0,inner_bytes=0,*a=rf_scene_secondary_vehicle_checkpoint_audit;
    uint64_t required;int status=RF_OK;uint32_t row_at,runs=a[0]+1;
    memset(a,0,sizeof(rf_scene_secondary_vehicle_checkpoint_audit));a[0]=runs;
    if(!s||!live||live->slot>=campaign_passive_vehicle_count)return RF_NOT_FOUND;
    live_before=*live;owner_before=campaign_passive_vehicles[live->slot];
    required=SCENE_SECONDARY_VEHICLE_HEADER+SCENE_SECONDARY_VEHICLE_ROW+
        SCENE_PASSIVE_VEHICLE_HEADER+(uint64_t)campaign_passive_vehicle_count*SCENE_PASSIVE_VEHICLE_ROW_V2;
    if(required>RF_CHECKPOINT_FILE_MAX||required>65536)return RF_RANGE;
    wire=malloc((size_t)required);scratch=malloc((size_t)required);if(!wire||!scratch){status=RF_IO;goto done;}
    status=scene_passive_vehicle_checkpoint_capture(wire,(uint32_t)required,0,&nested);if(status)goto done;
    status=scene_secondary_vehicle_checkpoint_capture(s,wire,(uint32_t)required,nested,&bytes);if(status)goto done;
    a[5]=bytes;row_at=SCENE_SECONDARY_VEHICLE_HEADER+nested;
    projection.uid=live->uid;projection.role=1;projection.scripted=live->scripted_physics;
    projection.body_flags=live->physics.body_flags;memcpy(projection.velocity,owner_before.velocity,12);
    projection.object_bits=owner_before.damage.object_flags&scene_vehicle_physics_checkpoint_object_mask(2);
    projection.entity_bits=owner_before.damage.state.effects.flags_814&0x800u;
    physics.count=1;physics.version=2;physics.admitted=1;physics.rows=&projection;
#define SCENE_SECONDARY_AUDIT_CHECK(bit,condition) do {++a[1];if(condition)a[2]|=1u<<(bit);else a[3]|=1u<<(bit);} while(0)
    status=scene_secondary_vehicle_checkpoint_prepare(s,wire,bytes,65536,&stage,&inner,&inner_bytes);
    if(!status)status=scene_passive_vehicle_checkpoint_prepare(inner,inner_bytes,65536,&passive);
    if(!status)status=scene_secondary_vehicle_checkpoint_admit(s,stage,passive,&physics);
    if(!status)scene_secondary_vehicle_checkpoint_encode(encoded,stage->runtime,&stage->passive,stage->velocity);
    SCENE_SECONDARY_AUDIT_CHECK(0,!status&&stage&&stage->admitted&&!memcmp(encoded,wire+row_at,sizeof(encoded)));
    scene_secondary_vehicle_checkpoint_close(&stage);scene_passive_vehicle_checkpoint_close(&passive);
    status=scene_secondary_vehicle_checkpoint_prepare(s,wire+SCENE_SECONDARY_VEHICLE_HEADER,nested,
        65536,&stage,&inner,&inner_bytes);
    a[6]=!status&&!stage&&inner==wire+SCENE_SECONDARY_VEHICLE_HEADER&&inner_bytes==nested;
    SCENE_SECONDARY_AUDIT_CHECK(1,a[6]);scene_secondary_vehicle_checkpoint_close(&stage);
    /* Version, profile, NaN, non-rigid basis, invalid retained ordinal,
     * non-authored event, lifecycle range and reserved data respectively. */
    for(uint32_t test=2;test<=9;test++){
        memcpy(scratch,wire,bytes);
        if(test==2)scene_history_put(scratch+4,2);
        if(test==3)scene_history_put(scratch+row_at+4,3);
        if(test==4)scene_history_put(scratch+row_at+96,0x7fc00000u);
        if(test==5)scene_history_put_float(scratch+row_at+60,17.f);
        if(test==6){scene_history_put(scratch+row_at+40,2);scene_history_put(scratch+row_at+180,0);}
        if(test==7)scene_history_put(scratch+row_at+28,UINT32_MAX);
        if(test==8)scene_history_put(scratch+row_at+8,2);
        if(test==9)scene_history_put(scratch+row_at+236,1);
        status=scene_secondary_vehicle_checkpoint_prepare(s,scratch,bytes,65536,&stage,&inner,&inner_bytes);
        SCENE_SECONDARY_AUDIT_CHECK(test,status&&!stage);scene_secondary_vehicle_checkpoint_close(&stage);
    }
    /* Both inputs remain individually plausible; only composition rejects
     * disagreements in pose, vitals and the accepted velocity projection. */
    for(uint32_t test=10;test<=12;test++){
        status=scene_secondary_vehicle_checkpoint_prepare(s,wire,bytes,65536,&stage,&inner,&inner_bytes);
        if(!status)status=scene_passive_vehicle_checkpoint_prepare(inner,inner_bytes,65536,&passive);
        if(!status){
            if(test==10)passive->rows[live->slot].position[0]+=1.f;
            if(test==11)passive->rows[live->slot].health+=1.f;
            if(test==12)projection.velocity[0]+=1.f;
            status=scene_secondary_vehicle_checkpoint_admit(s,stage,passive,&physics);
            if(test==12)memcpy(projection.velocity,owner_before.velocity,12);
        }
        SCENE_SECONDARY_AUDIT_CHECK(test,status&&stage&&!stage->admitted);
        scene_secondary_vehicle_checkpoint_close(&stage);scene_passive_vehicle_checkpoint_close(&passive);
    }
    status=scene_secondary_vehicle_checkpoint_prepare(s,wire,bytes,0,&stage,&inner,&inner_bytes);
    SCENE_SECONDARY_AUDIT_CHECK(13,status==RF_RANGE&&!stage);
    scene_secondary_vehicle_checkpoint_close(&stage);
    status=scene_secondary_vehicle_checkpoint_prepare(s,wire,bytes-1,65536,&stage,&inner,&inner_bytes);
    SCENE_SECONDARY_AUDIT_CHECK(14,status&&!stage);
    scene_secondary_vehicle_checkpoint_close(&stage);
    memcpy(scratch,wire,bytes);scene_history_put(scratch+row_at,UINT32_MAX);
    status=scene_secondary_vehicle_checkpoint_prepare(s,scratch,bytes,65536,&stage,&inner,&inner_bytes);
    SCENE_SECONDARY_AUDIT_CHECK(16,status&&!stage);
    scene_secondary_vehicle_checkpoint_close(&stage);
    a[7]=campaign_secondary_vehicle==live&&!memcmp(live,&live_before,sizeof(live_before))&&
        !memcmp(campaign_passive_vehicles+live->slot,&owner_before,sizeof(owner_before));
    SCENE_SECONDARY_AUDIT_CHECK(15,a[7]);status=a[3]?RF_FORMAT:RF_OK;
#undef SCENE_SECONDARY_AUDIT_CHECK
done:
    scene_secondary_vehicle_checkpoint_close(&stage);scene_passive_vehicle_checkpoint_close(&passive);
    free(scratch);free(wire);a[4]=(uint32_t)status;return status;
}
#endif
