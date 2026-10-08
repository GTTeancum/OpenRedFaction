#ifndef RF_SCENE_VEHICLE_PHYSICS_CHECKPOINT_INCLUDED
#define RF_SCENE_VEHICLE_PHYSICS_CHECKPOINT_INCLUDED
/* RFPV1..4 wrap RFVA/ordinary vehicle bytes, beneath RFNS. Version2 adds the
 * hidden object bit; version3 adds selected-host invulnerability in the same
 * row. Version4 uses the formerly zero final word for headlamp810 bit4000.
 * Older versions retain their exact masks, reserved zero and wire size.
 * Source identity and checksum belong to RFWC. No runtime handles or pointers
 * enter the wire.
 * Include after vehicle runtime and passive checkpoint definitions. */
enum {SCENE_VEHICLE_PHYSICS_HEADER=16,SCENE_VEHICLE_PHYSICS_ROW=72};
typedef struct scene_vehicle_physics_saved {
    uint32_t uid,role,scripted,body_flags,pending,object_bits,entity_bits,skip_forces;
    float velocity[3],force[3],torque[3];
    uint32_t headlamp_bits;
} scene_vehicle_physics_saved;
struct scene_vehicle_physics_checkpoint_stage {
    uint32_t count,bytes,admitted,version;
    scene_vehicle_physics_saved *rows;
};
/* captures,rows,assigns,active marker,bodyflags,pending,passive frozen,status */
uint32_t rf_scene_vehicle_physics_checkpoint[8];
/* Immutable final assignment: valid,activeUID,marker,bodyflags,pending,7c,814,
 * skipforces,velocity3,force3,torque3,passivecount,firstpassiveUID/marker/flags,
 * firstpassivevelocity3. */
uint32_t rf_scene_vehicle_physics_restore[24];
static uint32_t scene_vehicle_physics_checkpoint_bytes(const scene_vehicle_physics_checkpoint_stage *s)
{return s?s->bytes:0;}
static void scene_vehicle_physics_checkpoint_close(scene_vehicle_physics_checkpoint_stage **s)
{if(s&&*s){free((*s)->rows);free(*s);*s=NULL;}}
static uint32_t scene_vehicle_physics_checkpoint_object_mask(uint32_t version)
{return version>=3?0x06004004u:version==2?0x06004000u:0x06000000u;}
/* Use only after admission. Old active saves always restored a visible host,
 * including authored-hidden hosts; they contain no persisted visibility. */
static uint32_t scene_vehicle_physics_checkpoint_active_hidden(const scene_vehicle_physics_checkpoint_stage *stage)
{
    return stage&&stage->admitted&&stage->version>=2&&stage->count&&stage->rows&&
        !stage->rows[0].role&&!!(stage->rows[0].object_bits&0x4000u);
}
static int scene_vehicle_physics_saved_valid(const scene_vehicle_physics_saved *v,uint32_t version)
{
    if((version<1||version>4)||v->uid==UINT32_MAX||v->role>1||v->scripted>1||v->pending>1||v->skip_forces>1||
       (v->object_bits&~scene_vehicle_physics_checkpoint_object_mask(version))||(v->entity_bits&~0x800u))return RF_FORMAT;
    if(v->headlamp_bits&~(version==4?0x4000u:0u))return RF_FORMAT;
    if(v->role&&(!v->uid||v->pending||v->skip_forces))return RF_FORMAT;
    if(v->pending&&!(v->entity_bits&0x800u))return RF_FORMAT;
    if(v->scripted&&!(v->body_flags&0x80000000u)&&
       (v->body_flags&0x18000000u)!=0x18000000u)return RF_FORMAT;
    for(uint32_t j=0;j<3;j++){
        if(!isfinite(v->velocity[j])||!isfinite(v->force[j])||!isfinite(v->torque[j]))return RF_FORMAT;
        if(v->role&&(v->force[j]!=0||v->torque[j]!=0))return RF_FORMAT;
    }
    return RF_OK;
}
static uint32_t scene_vehicle_physics_checkpoint_count(const scene_stream *s)
{return campaign_passive_vehicle_count+(s&&s->driller_runtime?1u:0u);}
static int scene_vehicle_physics_checkpoint_live(const scene_stream *s,uint32_t index,uint32_t version,
    scene_vehicle_physics_saved *out)
{
    const scene_driller_runtime *r=s?s->driller_runtime:NULL;scene_vehicle_physics_saved v={0};
    const scene_driller_damage *damage;
    if(r&&!index){
        if(!scene_driller_damage_runtime_valid(&r->damage)||r->damage.entry!=&r->entry||
           r->entry.host.handle!=r->entry.registration.handle||
           (campaign_authored_vehicle_uid&&r->entry.host.handle!=campaign_authored_vehicle_handle)||
           rf_object_registry_lookup(&campaign_registry,r->entry.registration.handle)!=&r->entry.registration)return RF_NOT_FOUND;
        v.uid=(uint32_t)campaign_authored_vehicle_uid;v.scripted=r->scripted_physics;
        v.body_flags=r->physics.body_flags;v.pending=r->scripted_exit_pending;
        v.skip_forces=r->physics.state.skip_forces;damage=&r->damage.damage;
        memcpy(v.velocity,r->physics.state.velocity,12);memcpy(v.force,r->physics.state.force,12);
        memcpy(v.torque,r->physics.state.torque,12);
        if(v.pending&&!r->entry.session.active)return RF_NOT_FOUND;
    }else{
        const scene_passive_vehicle *p;
        if(r)--index;if(index>=campaign_passive_vehicle_count||!campaign_passive_vehicles)return RF_FORMAT;
        p=campaign_passive_vehicles+index;
        if(rf_object_registry_lookup(&campaign_registry,p->handle)!=p||p->damage.state.effects.handle!=p->handle)return RF_NOT_FOUND;
        v.uid=p->uid;v.role=1;v.scripted=p->scripted_physics;v.body_flags=p->physics_body_flags;
        damage=&p->damage;memcpy(v.velocity,p->velocity,12);
    }
    v.object_bits=damage->object_flags&scene_vehicle_physics_checkpoint_object_mask(version);
    v.entity_bits=damage->state.effects.flags_814&0x800u;
    v.headlamp_bits=version==4?damage->state.effects.flags_810&0x4000u:0;
    if(scene_vehicle_physics_saved_valid(&v,version))return RF_FORMAT;*out=v;return RF_OK;
}
static int scene_vehicle_physics_checkpoint_capture(const scene_stream *s,unsigned char *p,uint32_t capacity,
    uint32_t inner_bytes,uint32_t *written)
{
    uint32_t count=scene_vehicle_physics_checkpoint_count(s),affected=0,version=1;
    scene_vehicle_physics_saved v;int status;
    uint64_t required=SCENE_VEHICLE_PHYSICS_HEADER+(uint64_t)inner_bytes+(uint64_t)count*SCENE_VEHICLE_PHYSICS_ROW;
    if(!s||!p||!written||inner_bytes>capacity)return RF_RANGE;*written=0;
    if(!count){*written=inner_bytes;return RF_OK;}
    if(s->driller_runtime){
        const scene_driller_entry_exit *entry=&s->driller_runtime->entry;
        if(s->driller_runtime->damage.damage.object_flags&0x4000u){
            if(entry->session.active||entry->occupant!=-1||entry->gunner_occupant!=-1||
               entry->host.driver!=UINT32_MAX)return RF_NOT_FOUND;
            version=2;
        }
        /* A revealed authored-hidden owner must keep writing visibility even
         * after its current hidden bit clears. UID0 is the existing DEV host. */
        if(campaign_authored_vehicle_uid){
            uint32_t matches=0;
            for(uint32_t i=0;i<campaign_seeds.records.count;i++)
                if(campaign_seeds.records.items[i].record.uid==campaign_authored_vehicle_uid){
                    ++matches;if(campaign_seeds.items[i].spawn.creation_flags&2u)version=2;
                }
            if(matches!=1)return RF_NOT_FOUND;
        }
        /* RFVA2 already owns passive immunity. The selected RFVC family does
         * not: use v3 when protection is set, or when an authored protected
         * class needs an explicit OFF value. No mode/history marker needed. */
        if((s->driller_runtime->damage.damage.object_flags&4u)||
           !(s->driller_runtime->damage.damage.state.effects.class_health>=0))version=3;
        if((s->driller_runtime->damage.damage.state.effects.flags_810&0x4000u)||
           scene_vehicle_headlamp_initial(s->driller_runtime->entry.host.handle))version=4;
    }
    affected=version>=2;
    /* Preflight every row before shifting the caller's inner record. */
    for(uint32_t i=0;i<count;i++){status=scene_vehicle_physics_checkpoint_live(s,i,version,&v);if(status)return status;
        affected|=v.scripted|v.pending|v.entity_bits;}
    if(!affected){*written=inner_bytes;return RF_OK;}
    if(required>capacity||required>RF_CHECKPOINT_FILE_MAX)return RF_RANGE;
    memmove(p+SCENE_VEHICLE_PHYSICS_HEADER,p,inner_bytes);
    memcpy(p,"RFPV",4);scene_history_put(p+4,version);scene_history_put(p+8,inner_bytes);scene_history_put(p+12,count);
    for(uint32_t i=0;i<count;i++){
        unsigned char *row=p+SCENE_VEHICLE_PHYSICS_HEADER+inner_bytes+i*SCENE_VEHICLE_PHYSICS_ROW;
        status=scene_vehicle_physics_checkpoint_live(s,i,version,&v);if(status)return status;
        scene_history_put(row,v.uid);scene_history_put(row+4,v.role);scene_history_put(row+8,v.scripted);
        scene_history_put(row+12,v.body_flags);scene_history_put(row+16,v.pending);
        scene_history_put(row+20,v.object_bits);scene_history_put(row+24,v.entity_bits);scene_history_put(row+28,v.skip_forces);
        for(uint32_t j=0;j<3;j++){scene_history_put_float(row+32+j*4,v.velocity[j]);
            scene_history_put_float(row+44+j*4,v.force[j]);scene_history_put_float(row+56+j*4,v.torque[j]);}
        scene_history_put(row+68,v.headlamp_bits);
    }
    ++rf_scene_vehicle_physics_checkpoint[0];rf_scene_vehicle_physics_checkpoint[1]=count;
    *written=(uint32_t)required;return RF_OK;
}
static int scene_vehicle_physics_checkpoint_prepare(const scene_stream *s,const unsigned char *p,uint32_t bytes,uint32_t budget,
    scene_vehicle_physics_checkpoint_stage **out,const unsigned char **inner,uint32_t *inner_bytes)
{
    scene_vehicle_physics_checkpoint_stage *stage=NULL;uint32_t count,nested,version;uint64_t required,cost;
    if(!s||!out||*out||!inner||!inner_bytes||(!p&&bytes))return RF_RANGE;
    *inner=p;*inner_bytes=bytes;
    if(bytes<4||memcmp(p,"RFPV",4))return RF_OK; /* Exact legacy payload. */
    if(bytes<SCENE_VEHICLE_PHYSICS_HEADER)return RF_FORMAT;
    version=scene_history_word(p+4);if(version<1||version>4)return RF_FORMAT;
    nested=scene_history_word(p+8);count=scene_history_word(p+12);
    required=SCENE_VEHICLE_PHYSICS_HEADER+(uint64_t)nested+(uint64_t)count*SCENE_VEHICLE_PHYSICS_ROW;
    cost=sizeof(*stage)+(uint64_t)count*sizeof(*stage->rows);
    if(!count||count!=scene_vehicle_physics_checkpoint_count(s)||required!=bytes||required>RF_CHECKPOINT_FILE_MAX)return RF_FORMAT;
    if(cost>budget||cost>UINT32_MAX)return RF_RANGE;
    stage=calloc(1,sizeof(*stage));if(!stage)return RF_IO;
    stage->rows=calloc(count,sizeof(*stage->rows));if(!stage->rows){free(stage);return RF_IO;}
    stage->count=count;stage->bytes=(uint32_t)cost;stage->version=version;
    for(uint32_t i=0;i<count;i++){
        const unsigned char *row=p+SCENE_VEHICLE_PHYSICS_HEADER+nested+i*SCENE_VEHICLE_PHYSICS_ROW;
        scene_vehicle_physics_saved *v=stage->rows+i;scene_vehicle_physics_saved live;
        int status=scene_vehicle_physics_checkpoint_live(s,i,version,&live);
        if(status){scene_vehicle_physics_checkpoint_close(&stage);return status;}
        v->uid=scene_history_word(row);v->role=scene_history_word(row+4);v->scripted=scene_history_word(row+8);
        v->body_flags=scene_history_word(row+12);v->pending=scene_history_word(row+16);
        v->object_bits=scene_history_word(row+20);v->entity_bits=scene_history_word(row+24);v->skip_forces=scene_history_word(row+28);
        v->headlamp_bits=scene_history_word(row+68);
        for(uint32_t j=0;j<3;j++){v->velocity[j]=scene_history_float(row+32+j*4);
            v->force[j]=scene_history_float(row+44+j*4);v->torque[j]=scene_history_float(row+56+j*4);}
        if(v->uid!=live.uid||v->role!=live.role||scene_vehicle_physics_saved_valid(v,version)){
            scene_vehicle_physics_checkpoint_close(&stage);return RF_FORMAT;
        }
        for(uint32_t j=0;j<i;j++)if(stage->rows[j].uid==v->uid){scene_vehicle_physics_checkpoint_close(&stage);return RF_FORMAT;}
    }
    *out=stage;*inner=p+SCENE_VEHICLE_PHYSICS_HEADER;*inner_bytes=nested;return RF_OK;
}
static int scene_vehicle_physics_checkpoint_admit(const scene_stream *s,scene_vehicle_physics_checkpoint_stage *stage,
    const scene_vehicle_checkpoint_record *vehicle,const scene_passive_vehicle_checkpoint_stage *passive)
{
    if(!stage)return RF_OK;
    stage->admitted=0;
    if(!s||stage->count!=scene_vehicle_physics_checkpoint_count(s)||!stage->rows||
       (stage->version<1||stage->version>4))return RF_FORMAT;
    for(uint32_t i=0;i<stage->count;i++){
        const scene_vehicle_physics_saved *v=stage->rows+i;scene_vehicle_physics_saved live;
        int status=scene_vehicle_physics_checkpoint_live(s,i,stage->version,&live);if(status)return status;
        if(v->uid!=live.uid||v->role!=live.role)return RF_NOT_FOUND;
        if(!v->role){
            if(!vehicle||vehicle->profile!=rf_scene_vehicle_enabled)return RF_FORMAT;
            if(v->pending&&!vehicle->value.vehicle.player_occupied)return RF_FORMAT;
            if((v->object_bits&0x4000u)&&vehicle->value.vehicle.player_occupied)return RF_FORMAT;
            if(memcmp(v->velocity,vehicle->value.vehicle.velocity,12))return RF_FORMAT;
        }else{
            uint32_t slot=i-(s->driller_runtime?1u:0u);const scene_passive_vehicle_saved *saved;
            if(!passive||passive->version!=2||slot>=passive->count)return RF_FORMAT;
            saved=passive->rows+slot;
            if(!saved->loaded||!saved->has_damage||saved->uid!=v->uid||
               (saved->object_flags&scene_vehicle_physics_checkpoint_object_mask(stage->version))!=v->object_bits||
               (stage->version==4&&(saved->flags_810&0x4000u)!=v->headlamp_bits)||
               (saved->flags_814&0x800u)!=v->entity_bits)return RF_FORMAT;
        }
    }
    stage->admitted=1;return RF_OK;
}
/* Assignment only, AFTER final active host/gunner and RFVA publication. */
static void scene_vehicle_physics_checkpoint_assign(scene_stream *s,const scene_vehicle_physics_checkpoint_stage *stage)
{
    scene_driller_runtime *r=s?s->driller_runtime:NULL;uint32_t *probe=rf_scene_vehicle_physics_restore;
    memset(probe,0,sizeof(rf_scene_vehicle_physics_restore));
    if(stage&&!stage->admitted)return;
    /* Legacy codecs never had a scripted marker. Clear session-local state
     * without interpreting an ordinary constructor rest flag as event ON. */
    if(r){r->scripted_physics=0;r->scripted_exit_pending=0;
        r->damage.damage.object_flags&=~0x4000u; /* Legacy active saves were visible. */}
    for(uint32_t i=0;i<campaign_passive_vehicle_count;i++){
        campaign_passive_vehicles[i].scripted_physics=0;campaign_passive_vehicles[i].physics_body_flags=0;
    }
    if(stage)for(uint32_t i=0;i<stage->count;i++){
        const scene_vehicle_physics_saved *v=stage->rows+i;
        if(!v->role){
            r->scripted_physics=v->scripted;r->scripted_exit_pending=v->pending;r->physics.body_flags=v->body_flags;
            r->damage.damage.object_flags=(r->damage.damage.object_flags&
                ~(0x06004000u|(stage->version>=3?4u:0u)))|v->object_bits;
            if(stage->version==4)r->damage.damage.state.effects.flags_810=
                (r->damage.damage.state.effects.flags_810&~0x4000u)|v->headlamp_bits;
            r->damage.damage.state.effects.flags_814=(r->damage.damage.state.effects.flags_814&~0x800u)|v->entity_bits;
            r->physics.state.skip_forces=v->skip_forces;
            memcpy(r->physics.state.force,v->force,12);memcpy(r->physics.state.torque,v->torque,12);
        }else{
            scene_passive_vehicle *p=campaign_passive_vehicles+i-(r?1u:0u);
            p->scripted_physics=v->scripted;p->physics_body_flags=v->body_flags;memcpy(p->velocity,v->velocity,12);
            p->pose.flags|=v->object_bits&0x06000000u; /* Dirty parent publication remains intact; RFVA owns hidden. */
        }
    }
    if(r){
        r->entry.view.flags_7c=r->damage.damage.object_flags;r->entry.view.flags_814=r->damage.damage.state.effects.flags_814;
        r->entry.view.flags_810=(r->entry.view.flags_810&~0x4000u)|(r->damage.damage.state.effects.flags_810&0x4000u);
        probe[1]=(uint32_t)campaign_authored_vehicle_uid;probe[2]=r->scripted_physics;probe[3]=r->physics.body_flags;
        probe[4]=r->scripted_exit_pending;probe[5]=r->damage.damage.object_flags;probe[6]=r->damage.damage.state.effects.flags_814;
        probe[7]=r->physics.state.skip_forces;memcpy(probe+8,r->physics.state.velocity,12);
        memcpy(probe+11,r->physics.state.force,12);memcpy(probe+14,r->physics.state.torque,12);
    }
    probe[0]=1;probe[17]=campaign_passive_vehicle_count;
    if(campaign_passive_vehicle_count){const scene_passive_vehicle *p=campaign_passive_vehicles;
        probe[18]=p->uid;probe[19]=p->scripted_physics;probe[20]=p->physics_body_flags;memcpy(probe+21,p->velocity,12);}
    ++rf_scene_vehicle_physics_checkpoint[2];rf_scene_vehicle_physics_checkpoint[3]=probe[2];
    rf_scene_vehicle_physics_checkpoint[4]=probe[3];rf_scene_vehicle_physics_checkpoint[5]=probe[4];
    rf_scene_vehicle_physics_checkpoint[6]=0;
    for(uint32_t i=0;i<campaign_passive_vehicle_count;i++)rf_scene_vehicle_physics_checkpoint[6]+=
        campaign_passive_vehicles[i].scripted_physics&&!(campaign_passive_vehicles[i].physics_body_flags&0x80000000u);
}
#endif
