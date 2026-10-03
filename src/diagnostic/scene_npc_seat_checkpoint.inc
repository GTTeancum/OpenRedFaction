#ifndef RF_SCENE_NPC_SEAT_CHECKPOINT_INCLUDED
#define RF_SCENE_NPC_SEAT_CHECKPOINT_INCLUDED
/* RFNS1/2 live beneath RFTU wrappers and outside RFVA/vehicle bytes.
 * Include after seat runtime, turret/NPC stages and checkpoint world placement.
 * Explicit stages only: no temporary mutation of live links during admission.
 * Shared hooks and early type/prototype requirements are documented separately. */
enum {SCENE_NPC_SEAT_HEADER=16,SCENE_NPC_SEAT_ROW=24};
typedef struct scene_npc_seat_saved {
    scene_npc_seat_binding binding;
    uint32_t actor_uid,active,owns_host_flag,pose_ready,player_gunner;
    float pose[12];
} scene_npc_seat_saved;
struct scene_npc_seat_checkpoint_stage {
    uint32_t count,allocated_bytes,admitted;
    scene_npc_seat_saved rows[];
};
uint32_t rf_scene_npc_seat_checkpoint[6]; /* captures,restores,rows,active,placements,error */
static uint32_t scene_npc_seat_checkpoint_bytes(const scene_npc_seat_checkpoint_stage *s)
{return s?s->allocated_bytes:0;}

/* Narrow exceptions for RFNC linked_handle and RFTU occupant checks. */
static uint32_t scene_npc_seat_checkpoint_actor(uint32_t actor)
{return scene_npc_seat_attached(actor);}
static uint32_t scene_npc_seat_checkpoint_host(uint32_t host)
{
    uint32_t i,attached;
    for(i=0;i<scene_npc_seat_count;i++) {
        scene_npc_seat_binding *b=scene_npc_seats+i;
        if(b->active&&b->host==host)return !scene_npc_seat_pair(host,b->actor,b->tag,&attached)&&attached;
    }
    return 0;
}
static void scene_npc_seat_checkpoint_close(scene_npc_seat_checkpoint_stage **s)
{if(s&&*s){free(*s);*s=NULL;}}
static int scene_npc_seat_checkpoint_capture(unsigned char *p,uint32_t capacity,
    uint32_t inner_bytes,uint32_t *written)
{
    uint32_t i,attached,gunner_uid=0,gunner_matches=0;int status;
    uint64_t required=16u+(uint64_t)inner_bytes+(uint64_t)scene_npc_seat_count*24u;
    if(!p||!written||inner_bytes>capacity)return RF_RANGE;
    status=scene_jeep_gunner_checkpoint_capture_uid(scene_npc_seat_stream,&gunner_uid);if(status)return status;
    if(!scene_npc_seat_count){if(gunner_uid)return RF_FORMAT;*written=inner_bytes;return RF_OK;}
    if(scene_npc_seat_count>SCENE_NPC_SEAT_LIMIT)return RF_FORMAT;
    if(required>capacity||required>RF_CHECKPOINT_FILE_MAX)return RF_RANGE;
    for(i=0;i<scene_npc_seat_count;i++) {
        scene_npc_seat_binding *b=scene_npc_seats+i;
        if(b->slot>=campaign_npc_body_count||b->active>1||
           (b->kind!=SCENE_NPC_SEAT_JEEP&&b->kind!=SCENE_NPC_SEAT_TURRET))return RF_FORMAT;
        if(b->active) {
            int status=scene_npc_seat_pair(b->host,b->actor,b->tag,&attached);
            if(status||!attached)return status?status:RF_FORMAT;
            if(b->kind==SCENE_NPC_SEAT_TURRET) {
                scene_turret_operator_binding *op=scene_turret_operator_find(b->host);
                if(!op||op->actor!=b->actor||op->tag!=b->tag||op->owns_host_flag>1)return RF_FORMAT;
            }
            if(b->kind==SCENE_NPC_SEAT_JEEP&&gunner_uid&&
               (uint32_t)campaign_seeds.records.items[b->slot].record.uid==gunner_uid)++gunner_matches;
        }
    }
    if(gunner_matches!=(gunner_uid?1u:0u))return RF_FORMAT;
    memmove(p+16,p,inner_bytes);memcpy(p,"RFNS",4);
    scene_history_put(p+4,gunner_uid?2:1);scene_history_put(p+8,inner_bytes);scene_history_put(p+12,scene_npc_seat_count);
    for(i=0;i<scene_npc_seat_count;i++) {
        const scene_npc_seat_binding *b=scene_npc_seats+i;unsigned char *q=p+16+inner_bytes+i*24;
        scene_turret_operator_binding *op=b->active&&b->kind==SCENE_NPC_SEAT_TURRET?scene_turret_operator_find(b->host):NULL;
        scene_history_put(q,(uint32_t)campaign_seeds.records.items[b->slot].record.uid);
        scene_history_put(q+4,b->host_uid);scene_history_put(q+8,(uint32_t)b->tag);
        scene_history_put(q+12,b->kind);scene_history_put(q+16,b->active);
        scene_history_put(q+20,op?op->owns_host_flag:
            (b->active&&b->kind==SCENE_NPC_SEAT_JEEP&&gunner_uid&&
             (uint32_t)campaign_seeds.records.items[b->slot].record.uid==gunner_uid?1:0));
    }
    *written=(uint32_t)required;++rf_scene_npc_seat_checkpoint[0];return RF_OK;
}
static int scene_npc_seat_checkpoint_prepare(const unsigned char *p,uint32_t bytes,
    uint32_t budget,scene_npc_seat_checkpoint_stage **out,const unsigned char **inner,uint32_t *inner_bytes)
{
    scene_npc_seat_checkpoint_stage *s;uint32_t n,size,i,j,version;uint64_t allocated;
    if(!out||*out||!inner||!inner_bytes||(bytes&&!p))return RF_RANGE;
    if(bytes<4||memcmp(p,"RFNS",4)) {
        /* Legacy occupied-level saves lack an ownership contract. Keep them
         * rejected instead of silently retaining authored seats over saved NPCs. */
        if(scene_npc_seat_count)return RF_NOT_FOUND;
        *inner=p;*inner_bytes=bytes;return RF_OK;
    }
    if(bytes<16)return RF_FORMAT;version=scene_history_word(p+4);
    if(version!=1&&version!=2)return RF_FORMAT;
    size=scene_history_word(p+8);n=scene_history_word(p+12);
    if(!n||n!=scene_npc_seat_count||n>SCENE_NPC_SEAT_LIMIT||16u+(uint64_t)size+(uint64_t)n*24u!=bytes)return RF_FORMAT;
    allocated=sizeof(*s)+(uint64_t)n*sizeof(*s->rows);if(allocated>budget)return RF_RANGE;
    s=calloc(1,(size_t)allocated);if(!s)return RF_IO;s->count=n;s->allocated_bytes=(uint32_t)allocated;
    for(i=0;i<n;i++) {
        const unsigned char *q=p+16+size+i*24;scene_npc_seat_saved *v=s->rows+i;
        uint32_t uid=scene_history_word(q),host=scene_history_word(q+4),kind=scene_history_word(q+12);
        int32_t tag=(int32_t)scene_history_word(q+8);
        for(j=0;j<n;j++)if((uint32_t)campaign_seeds.records.items[scene_npc_seats[j].slot].record.uid==uid)break;
        if(j==n||host!=scene_npc_seats[j].host_uid||tag!=scene_npc_seats[j].tag||kind!=scene_npc_seats[j].kind)goto malformed;
        for(uint32_t k=0;k<i;k++)if(s->rows[k].actor_uid==uid)goto malformed;
        v->binding=scene_npc_seats[j];v->actor_uid=uid;v->active=scene_history_word(q+16);
        if(scene_jeep_gunner_checkpoint_control(version,kind,v->active,scene_history_word(q+20),
            &v->owns_host_flag,&v->player_gunner))goto malformed;
    }
    if(scene_jeep_gunner_checkpoint_wire_admit(s,version))goto malformed;
    *out=s;*inner=p+16;*inner_bytes=size;return RF_OK;
malformed:free(s);return RF_FORMAT;
}
static const scene_npc_seat_saved *scene_npc_seat_checkpoint_find(const scene_npc_seat_checkpoint_stage *s,uint32_t uid)
{
    uint32_t i;if(s)for(i=0;i<s->count;i++)if(s->rows[i].actor_uid==uid)return s->rows+i;
    return NULL;
}
static uint32_t scene_npc_seat_checkpoint_owns(const scene_npc_seat_checkpoint_stage *s,
    uint32_t host_uid,uint32_t actor_uid)
{
    const scene_npc_seat_saved *v=scene_npc_seat_checkpoint_find(s,actor_uid);
    return v&&v->active&&v->pose_ready&&v->binding.host_uid==host_uid;
}
/* Before NPC staging: construct expected seats from candidate host poses,
 * never the freshly spawned host pose. Detached rows need no candidate host. */
static int scene_npc_seat_checkpoint_hosts(scene_npc_seat_checkpoint_stage *s,
    const scene_turret_checkpoint_stage *turrets,const scene_vehicle_checkpoint_record *vehicle)
{
    uint32_t i,j;int status;if(!s)return RF_OK;
    for(i=0;i<s->count;i++) {
        scene_npc_seat_saved *v=s->rows+i;scene_npc_seat_host h;const float *position,*basis;
        if(!v->active)continue;
        /* Turret occupant pointers belong to the canonical live bank. The
         * staged binding is a value copy, never an occupant-word owner. */
        for(j=0;j<scene_npc_seat_count;j++)if(scene_npc_seats[j].actor==v->binding.actor&&
            scene_npc_seats[j].host==v->binding.host&&scene_npc_seats[j].tag==v->binding.tag)break;
        if(j==scene_npc_seat_count)return RF_FORMAT;
        status=scene_npc_seat_host_read(scene_npc_seats+j,&h);if(status)return status;
        if(v->binding.kind==SCENE_NPC_SEAT_JEEP) {
            status=scene_jeep_gunner_checkpoint_host(v,vehicle);if(status)return status;
            position=vehicle->value.jeep.vehicle.position;basis=vehicle->value.jeep.vehicle.orientation;
        }else {
            if(!turrets)return RF_FORMAT;
            for(j=0;j<turrets->count;j++)if(turrets->rows[j].uid==v->binding.host_uid)break;
            if(j==turrets->count||turrets->rows[j].dead||turrets->rows[j].health<=0||
               turrets->rows[j].handle!=v->binding.host||!(turrets->rows[j].flags_810&0x10000u)||
               (turrets->rows[j].flags&(2u|0x4000u)))return RF_FORMAT;
            position=turrets->rows[j].position;basis=turrets->rows[j].basis;
        }
        status=rf_static_model_tag_place(h.tags,v->binding.tag,basis,position,v->pose);if(status)return status;
        /* The current authored skeletal operators/driver have no basis lock;
         * a future independently aimed passenger needs its own saved basis. */
        if(campaign_npc_bodies[v->binding.slot].object_flags&0x100u)return RF_NOT_FOUND;
        v->pose_ready=1;
    }
    return RF_OK;
}
/* Called before NPC staging computes tensors/eye/body from its yaw-only row. */
static int scene_npc_seat_checkpoint_basis(const scene_npc_seat_checkpoint_stage *s,
    const rf_npc_checkpoint_record *row,float basis[9],uint32_t *seated)
{
    const scene_npc_seat_saved *v;uint32_t i;
    if(!row||!basis||!seated)return RF_RANGE;*seated=0;
    v=scene_npc_seat_checkpoint_find(s,row->uid);
    if(!v||!v->active)return row->ai_mode==13?RF_FORMAT:RF_OK;
    if(!v->pose_ready||row->retired||row->dead_pose||row->health<=0||row->support_uid||
       row->move.active||row->combat.active||row->shot_count||row->script_animation.active||
       (row->flags&(2u|0x4000u)))return RF_FORMAT;
    for(i=0;i<3;i++)if(!isfinite(row->position[i])||fabsf(row->position[i]-v->pose[9+i])>.001f)return RF_FORMAT;
    memcpy(basis,v->pose,36);*seated=1;return RF_OK;
}
/* Seat transform is proved above. Still run ordinary candidate-world solid,
 * mover/prop and room admission; only the requirement for floor contact lifts. */
static int scene_npc_seat_checkpoint_place(const scene_npc_seat_checkpoint_stage *s,uint32_t uid,
    const scene_checkpoint_world_context *world,const rf_checkpoint_placement *placement,
    rf_entity_room_state *room,rf_physics_support_contact *support,uint32_t *handled)
{
    const scene_npc_seat_saved *v=scene_npc_seat_checkpoint_find(s,uid);scene_checkpoint_world_context c;
    uint32_t i;int status;if(!world||!placement||!room||!support||!handled)return RF_RANGE;*handled=0;
    if(!v||!v->active)return RF_OK;*handled=1;if(!v->pose_ready)return RF_FORMAT;
    for(i=0;i<3;i++)if(fabsf(placement->position[i]-v->pose[9+i])>.001f)return RF_FORMAT;
    for(i=0;i<9;i++)if(fabsf(placement->basis[i]-v->pose[i])>.0001f)return RF_FORMAT;
    c=*world;c.allow_no_contact=1;c.authored_position=NULL;c.authored_basis=NULL;
    c.authored_rotating_npc=c.authored_center_shift_npc=c.support_uid=0;
    if(!campaign_npc_movement_configs||campaign_seeds.items[v->binding.slot].class_index>=campaign_seeds.class_count)return RF_FORMAT;
    c.class_speed=campaign_npc_movement_configs[campaign_seeds.items[v->binding.slot].class_index].base_speed;
    status=scene_checkpoint_world_place(&c,placement,room,support);if(status)return status;
    memset(support,0,sizeof(*support));support->material=-1;++rf_scene_npc_seat_checkpoint[4];return RF_OK;
}
static int scene_npc_seat_checkpoint_admit(scene_npc_seat_checkpoint_stage *s,
    const scene_npc_checkpoint_restore_stage *npcs)
{
    uint32_t i,j,attached;int status;if(!npcs)return RF_RANGE;
    /* Reject orphan mode13 even for legacy/nonseated levels. */
    for(i=0;i<npcs->count;i++)if(npcs->entries[i].saved.ai_mode==13) {
        const scene_npc_seat_saved *v=scene_npc_seat_checkpoint_find(s,npcs->entries[i].saved.uid);
        if(!v||!v->active)return RF_FORMAT;
    }
    if(!s)return RF_OK;if(s->count!=scene_npc_seat_count)return RF_FORMAT;
    for(i=0;i<s->count;i++) {
        scene_npc_seat_saved *v=s->rows+i;const scene_npc_checkpoint_restore_entry *e;
        for(j=0;j<npcs->count;j++)if(npcs->entries[j].saved.uid==v->actor_uid)break;
        if(j==npcs->count)return RF_FORMAT;e=npcs->entries+j;
        if(e->slot!=v->binding.slot||e->handle!=v->binding.actor||!scene_npc_seat_actor(&v->binding))return RF_FORMAT;
        if(v->binding.active) {
            status=scene_npc_seat_pair(v->binding.host,v->binding.actor,v->binding.tag,&attached);
            if(status||!attached)return status?status:RF_FORMAT;
        }
        if(!v->active){if(e->saved.ai_mode==13)return RF_FORMAT;continue;}
        if(!v->pose_ready||e->saved.retired||e->saved.dead_pose||e->saved.health<=0||
           e->saved.move.active||e->saved.combat.active||e->saved.shot_count||e->saved.script_animation.active||
           (e->saved.flags&(2u|0x4000u))||e->support.handle||e->support.material!=-1)return RF_FORMAT;
        for(j=0;j<3;j++)if(fabsf(e->published[j]-v->pose[9+j])>.001f)return RF_FORMAT;
        for(j=0;j<9;j++)if(fabsf(e->basis[j]-v->pose[j])>.0001f)return RF_FORMAT;
        if(v->binding.kind==SCENE_NPC_SEAT_TURRET) {
            scene_turret_owner *host=scene_turret_lookup(v->binding.host);
            if(!host||host->view.class_type!=4||i>=SCENE_TURRET_OWNER_LIMIT)return RF_FORMAT;
        }
    }
    s->admitted=1;return RF_OK;
}
/* Two assignment-only phases after ALL ordinary world validations succeed.
 * Detach before NPC/vehicle/turret assignment, then assign after them. Neither
 * calls gameplay bind/unbind, so saved cadence/AI actions remain untouched. */
static void scene_npc_seat_checkpoint_detach(const scene_npc_seat_checkpoint_stage *s)
{
    uint32_t i;if(!s||!s->admitted)return;
    for(i=0;i<scene_npc_seat_count;i++) {
        scene_npc_seat_binding *b=scene_npc_seats+i;scene_npc_seat_host h;campaign_npc_body *o;
        if(!b->active)continue;o=scene_npc_seat_actor(b);
        if(!scene_npc_seat_host_read(b,&h)) {
            if(*h.occupant==(int32_t)b->actor)*h.occupant=-1;
            if(h.driver&&*h.driver==b->actor)*h.driver=UINT32_MAX;
            if(b->kind==SCENE_NPC_SEAT_TURRET){h.view->occupants=NULL;h.view->occupant_count=0;}
        }
        if(o&&o->view.linked_handle==(int32_t)b->host)o->view.linked_handle=-1;
    }
    memset(scene_turret_operator_rows,0,sizeof(scene_turret_operator_rows));
    memset(scene_npc_seats,0,sizeof(scene_npc_seats));scene_npc_seat_count=0;rf_scene_npc_seats[6]=0;
}
static void scene_npc_seat_checkpoint_assign(const scene_npc_seat_checkpoint_stage *s)
{
    uint32_t i;if(!s||!s->admitted)return;
    scene_npc_seat_count=s->count;
    for(i=0;i<s->count;i++) {
        const scene_npc_seat_saved *v=s->rows+i;scene_npc_seat_binding *b=scene_npc_seats+i;
        campaign_npc_body *o=campaign_npc_bodies+v->binding.slot;
        *b=v->binding;b->active=v->active;b->last_frame=UINT32_MAX;b->occupant=-1;
        if(!v->active)continue;o->view.linked_handle=(int32_t)b->host;
        if(b->kind==SCENE_NPC_SEAT_JEEP) {
            scene_driller_entry_exit *e=&scene_npc_seat_stream->driller_runtime->entry;
            e->occupant=(int32_t)b->actor;e->host.driver=b->actor;
        }else {
            scene_turret_owner *host=scene_turret_lookup(b->host);
            scene_turret_operator_binding *op=scene_turret_operator_rows+i;
            b->occupant=(int32_t)b->actor;host->view.occupants=&b->occupant;host->view.occupant_count=1;
            op->active=1;op->host=b->host;op->actor=b->actor;op->host_uid=b->host_uid;
            op->actor_uid=v->actor_uid;op->tag=b->tag;op->owns_host_flag=v->owns_host_flag;
        }
        ++rf_scene_npc_seats[6];++rf_scene_npc_seat_checkpoint[3];
    }
    ++rf_scene_npc_seat_checkpoint[1];rf_scene_npc_seat_checkpoint[2]=s->count;
}
#endif
