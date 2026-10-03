#ifndef RF_SCENE_TURRET_PLAYER_CHECKPOINT_INCLUDED
#define RF_SCENE_TURRET_PLAYER_CHECKPOINT_INCLUDED
/* After world_player_restore. RFTP1 wraps existing vehicle/turret bytes,
 * inside RFCL. RFPL retains the real standing body; RFTU retains turret aim.
 * No mount/use/death callback, teleport, resource load or allocation at commit. */
enum {SCENE_TURRET_PLAYER_CP_HEADER=16,SCENE_TURRET_PLAYER_CP_ROW=64};
typedef struct scene_turret_player_checkpoint_stage {
    uint32_t uid,class_id,remaining,burst,rng,allocated_bytes,admitted;
    float desired[3],anchor[3],eye_offset[3];
    scene_turret_player_state player;
    scene_turret_owner *host;
} scene_turret_player_checkpoint_stage;
static uint32_t scene_turret_player_checkpoint_scope_active;
uint32_t rf_scene_turret_player_checkpoint[20]; /* capture,restore,UID,handle,player,due,burst,RNG,desiredXYZ,anchorXYZ,eyeoffsetXYZ,actual aimXYZ */
static void scene_turret_player_checkpoint_scope(uint32_t enabled)
{scene_turret_player_checkpoint_scope_active=!!enabled;}
static void scene_turret_player_checkpoint_reset(void)
{scene_turret_player_checkpoint_scope_active=0;memset(rf_scene_turret_player_checkpoint,0,sizeof(rf_scene_turret_player_checkpoint));}
static int scene_turret_player_checkpoint_live(uint32_t frame)
{
    const scene_turret_player_state *p=&scene_turret_player;
    const scene_turret_owner *o;uint32_t i;
    if(!p->active||frame==UINT32_MAX||!scene_turret_player_controls(p->host))return RF_NOT_FOUND;
    o=scene_turret_lookup(p->host);
    if(!o||o<scene_turrets||o>=scene_turrets+scene_turret_authored_count||
       !o->uid||o->uid==UINT32_MAX||o->class_index>=campaign_seeds.class_count||
       o->dead||o->damage.effects.health<=0||o->view.linked_handle!=-1||
       (o->view.flags_7c&(2u|0x4000u))||!scene_turret_primary_for(o->view.weapons[0])||
       campaign_seeds.classes[o->class_index].physics.use_kind!=4||
       scene_turret_operator_find(p->host)||scene_turret_generated_find(p->host)||
       p->saved_link!=-1||campaign_player_damage.state.effects.health<=0||
       p->use_held||p->fire_held||p->input.use||p->input.fire||p->input.alt_fire||
       p->release_mask||p->burst>scene_turret_primary_for(o->view.weapons[0])->burst_count||
       (p->due>frame&&p->due-frame>36000))return RF_NOT_FOUND;
    for(i=0;i<3;i++)if(!isfinite(p->desired[i])||!isfinite(p->anchor[i])||
        !isfinite(p->eye_offset[i])||p->desired[i]<campaign_seeds.classes[o->class_index].eye_limits.minimum[i]-.0001f||
        p->desired[i]>campaign_seeds.classes[o->class_index].eye_limits.maximum[i]+.0001f)return RF_NOT_FOUND;
    if(p->input.look[0]||p->input.look[1])return RF_NOT_FOUND;
    return RF_OK;
}
/* Narrow ordinary-snapshot exceptions; standalone RFCP/RFTU stays guarded. */
static uint32_t scene_turret_player_checkpoint_player_allowed(void)
{return scene_turret_player_checkpoint_scope_active&&!scene_turret_player_checkpoint_live(combat_frame);}
static uint32_t scene_turret_player_checkpoint_host_allowed(uint32_t host)
{return host==scene_turret_player.host&&scene_turret_player_checkpoint_player_allowed();}
static int scene_turret_player_checkpoint_capture(unsigned char *p,uint32_t capacity,
    uint32_t inner_bytes,uint32_t frame,uint32_t *written)
{
    const scene_turret_player_state *live=&scene_turret_player;const scene_turret_owner *o;
    unsigned char *q;uint64_t total=80u+(uint64_t)inner_bytes;int status;
    if(!p||!written||inner_bytes>capacity)return RF_RANGE;
    if(!live->active){*written=inner_bytes;return RF_OK;}
    status=scene_turret_player_checkpoint_live(frame);if(status)return status;
    if(total>capacity||total>RF_CHECKPOINT_FILE_MAX)return RF_RANGE;
    o=scene_turret_lookup(live->host);
    memmove(p+16,p,inner_bytes);memcpy(p,"RFTP",4);
    scene_history_put(p+4,1);scene_history_put(p+8,inner_bytes);scene_history_put(p+12,64);
    q=p+16+inner_bytes;scene_history_put(q,o->uid);scene_history_put(q+4,o->class_index);
    scene_history_put(q+8,live->due>frame?live->due-frame:0);scene_history_put(q+12,live->burst);
    scene_history_put(q+16,campaign_enemy_spread_random.value);
    memcpy(q+20,live->desired,12);memcpy(q+32,live->anchor,12);memcpy(q+44,live->eye_offset,12);
    scene_history_put(q+56,0);scene_history_put(q+60,0);
    *written=(uint32_t)total;++rf_scene_turret_player_checkpoint[0];return RF_OK;
}
static void scene_turret_player_checkpoint_close(scene_turret_player_checkpoint_stage **s)
{if(s&&*s){free(*s);*s=NULL;}}
static int scene_turret_player_checkpoint_prepare(const unsigned char *p,uint32_t bytes,
    uint32_t budget,scene_turret_player_checkpoint_stage **out,const unsigned char **inner,uint32_t *inner_bytes)
{
    scene_turret_player_checkpoint_stage *s;const unsigned char *q;uint32_t size,i;
    if(!out||*out||!inner||!inner_bytes||(bytes&&!p))return RF_RANGE;
    if(bytes<4||memcmp(p,"RFTP",4)){*inner=p;*inner_bytes=bytes;return RF_OK;}
    if(bytes<16||scene_history_word(p+4)!=1||scene_history_word(p+12)!=64)return RF_FORMAT;
    size=scene_history_word(p+8);if(80u+(uint64_t)size!=bytes)return RF_FORMAT;
    if(sizeof(*s)>budget)return RF_RANGE;s=calloc(1,sizeof(*s));if(!s)return RF_IO;
    q=p+16+size;s->allocated_bytes=sizeof(*s);s->uid=scene_history_word(q);s->class_id=scene_history_word(q+4);
    s->remaining=scene_history_word(q+8);s->burst=scene_history_word(q+12);s->rng=scene_history_word(q+16);
    memcpy(s->desired,q+20,12);memcpy(s->anchor,q+32,12);memcpy(s->eye_offset,q+44,12);
    if(!s->uid||s->uid==UINT32_MAX||s->remaining>36000||scene_history_word(q+56)||scene_history_word(q+60))goto malformed;
    for(i=0;i<3;i++)if(!isfinite(s->desired[i])||!isfinite(s->anchor[i])||!isfinite(s->eye_offset[i]))goto malformed;
    *out=s;*inner=p+16;*inner_bytes=size;return RF_OK;
malformed:free(s);return RF_FORMAT;
}
/* After RFTU and ordinary standing-player/world placement admission, before
 * any assignment. Shared RNG must agree with every saved NPC combat owner. */
static int scene_turret_player_checkpoint_admit(scene_turret_player_checkpoint_stage *s,
    const scene_turret_checkpoint_stage *turrets,const scene_world_player_stage *player,uint32_t frame)
{
    const scene_turret_saved *v=NULL;scene_turret_owner *o;const rf_entity_seed_class *cls;
    float distance=0;uint32_t i;
    if(!s)return RF_OK;
    if(!turrets||!player||!player->npcs||frame==UINT32_MAX||s->remaining>UINT32_MAX-frame)return RF_RANGE;
    if(scene_turret_player_active()||campaign_player_view.linked_handle!=-1||
       player->player.player.health<=0||!campaign_player_object.view||
       rf_entity_lookup(&campaign_entities,(int32_t)campaign_player_object.handle)!=&campaign_player_view||
       (player->vehicle&&player->vehicle->value.vehicle.player_occupied))return RF_NOT_FOUND;
    for(i=0;i<turrets->count;i++)if(turrets->rows[i].uid==s->uid){if(v)return RF_FORMAT;v=turrets->rows+i;}
    if(!v||v->class_index!=s->class_id||v->dead||v->health<=0||(v->flags&(2u|0x4000u))||
       !scene_turret_primary_for(v->primary)||v->target_uid!=UINT32_MAX||v->remaining||v->burst||
       s->class_id>=campaign_seeds.class_count||s->burst>scene_turret_primary_for(v->primary)->burst_count)return RF_FORMAT;
    o=scene_turret_lookup(v->handle);cls=campaign_seeds.classes+s->class_id;
    if(!o||cls->physics.use_kind!=4||!isfinite(cls->physics.use_radius)||cls->physics.use_radius<=0||
       o->view.linked_handle!=-1||o->view.occupants||o->view.occupant_count||o->view.weapon_owner||
       scene_turret_operator_find(v->handle)||scene_turret_generated_find(v->handle))return RF_NOT_FOUND;
    for(i=0;i<3;i++){
        float d=player->player.position[i]-v->position[i];distance+=d*d;
        if(s->desired[i]<cls->eye_limits.minimum[i]-.0001f||s->desired[i]>cls->eye_limits.maximum[i]+.0001f)return RF_FORMAT;
    }
    if(!isfinite(distance)||distance>(cls->physics.use_radius+.05f)*(cls->physics.use_radius+.05f))return RF_NOT_FOUND;
    for(i=0;i<player->npcs->count;i++)if(player->npcs->entries[i].saved.combat.active&&
        player->npcs->entries[i].saved.combat.spread_rng!=s->rng)return RF_FORMAT;
    s->player.active=1;s->player.host=v->handle;s->player.player=campaign_player_object.handle;
    s->player.occupant=(int32_t)s->player.player;s->player.saved_link=-1;s->player.last_frame=UINT32_MAX;
    s->player.due=frame+s->remaining;s->player.burst=s->burst;s->player.use_held=1;
    memcpy(s->player.desired,s->desired,12);memcpy(s->player.anchor,s->anchor,12);memcpy(s->player.eye_offset,s->eye_offset,12);
    s->host=o;s->admitted=1;return RF_OK;
}
/* After ordinary player, RFTU and RFNS publication. Exact preadmitted handles;
 * no input edge, attach sound or fire callback is replayed. */
static void scene_turret_player_checkpoint_assign(const scene_turret_player_checkpoint_stage *s)
{
    scene_turret_owner *o;uint32_t *p=rf_scene_turret_player_checkpoint;
    if(!s||!s->admitted)return;o=s->host;
    scene_turret_player=s->player;o->view.occupants=&scene_turret_player.occupant;
    o->view.occupant_count=1;o->view.weapon_owner=&campaign_player_view;
    campaign_player_view.linked_handle=(int32_t)s->player.host;
    campaign_enemy_spread_random.value=s->rng;
    rf_scene_turret_player[2]=1;rf_scene_turret_player[3]=s->uid;
    rf_scene_turret_player[8]=s->player.player;rf_scene_turret_player[9]=s->player.host;
    ++p[1];p[2]=s->uid;p[3]=s->player.host;p[4]=s->player.player;
    p[5]=s->player.due;p[6]=s->player.burst;p[7]=s->rng;
    memcpy(p+8,scene_turret_player.desired,12);memcpy(p+11,scene_turret_player.anchor,12);
    memcpy(p+14,scene_turret_player.eye_offset,12);
    memcpy(p+17,scene_turret_combat_states[o-scene_turrets].angles,12);
}
#endif
