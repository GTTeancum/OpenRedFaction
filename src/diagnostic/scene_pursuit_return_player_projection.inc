#ifndef RF_SCENE_PURSUIT_RETURN_PLAYER_PROJECTION_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_PLAYER_PROJECTION_INCLUDED
/* Uncalled proposal for the independent late-close lane. The caller must
 * authenticate the world/NPC/player stage lifetimes before each invocation,
 * including all candidate seat/vehicle pointers, and retain the complete
 * source-to-player return relation in its own publication receipt. This is
 * only the on-foot player assignment projection, not that relation seal.
 * No resource demand, allocation, callback or gameplay write occurs here. */
typedef struct scene_pursuit_return_player_projection {
    scene_npc_reload_player_receipt identity;
    const scene_world_player_stage *stage;
    rf_physics_body_state body;
    rf_entity_room_state room;
    float position[3],eye[9],support_velocity[3];
    float health,armor;
    uint32_t support;
} scene_pursuit_return_player_projection;
_Static_assert(sizeof(void*)!=4||sizeof(scene_pursuit_return_player_projection)<=480,
    "Return player projection exceeds proposed Xbox32 allowance");

/* Prepare before publication against an independently joined on-foot target.
 * Do not substitute the outgoing player's health or position for the saved
 * candidate. Inputs and output storage must be disjoint and stable. */
static int scene_pursuit_return_player_projection_prepare(
    const scene_stream *stream,const scene_world_player_stage *player,
    uint32_t gunner,uint32_t turret,scene_pursuit_return_player_projection *out)
{
    scene_pursuit_return_player_projection p={0};int status;
    if(!stream||!player||!out||!player->npcs||
       player->npcs->pursuit_return_joined!=1||gunner>1||turret>1)return RF_RANGE;
    status=scene_npc_reload_incoming_vehicle(player->vehicle,player->seats,gunner,turret);
    if(status)return status;
    if(!isfinite(player->player.player.health)||player->player.player.health<=0||
       !isfinite(player->player.player.armor)||(campaign_player_view.flags_810&1u))return RF_FORMAT;
    status=scene_npc_reload_player_admit(stream,&p.identity);if(status)return status;
    p.stage=player;p.body=player->body;p.room=player->room;
    p.body.flags&=~(0x200000u|0x400000u);
    if(player->support.handle)p.body.flags|=0x400000u;
    p.support=player->support.handle;
    p.health=player->player.player.health;p.armor=player->player.player.armor;
    memcpy(p.position,player->player.position,sizeof(p.position));
    memcpy(p.eye,player->eye,sizeof(p.eye));
    memcpy(p.support_velocity,player->support_velocity,sizeof(p.support_velocity));
    for(uint32_t i=0;i<3;i++)if(!isfinite(p.position[i])||!isfinite(p.support_velocity[i]))return RF_FORMAT;
    for(uint32_t i=0;i<9;i++)if(!isfinite(p.eye[i]))return RF_FORMAT;
    *out=p;return RF_OK;
}

/* Recheck the projection-relevant candidate fields before NPC/player
 * assignment, after the caller repeats candidate lifetime authentication.
 * Source-to-player relation and unrelated world candidate fields remain in
 * the outer receipt. This does not update the retained expected projection. */
static inline int scene_pursuit_return_player_projection_candidate(
    const scene_stream *stream,const scene_world_player_stage *player,
    uint32_t gunner,uint32_t turret,const scene_pursuit_return_player_projection *p)
{
    scene_pursuit_return_player_projection current;int status;
    if(!p||p->stage!=player)return RF_RANGE;
    status=scene_pursuit_return_player_projection_prepare(stream,player,gunner,turret,&current);
    if(status)return status;
    return memcmp(&current,p,sizeof(current))?RF_FORMAT:RF_OK;
}

/* Run after all player/vehicle/turret publication and before storage close.
 * Candidate pointers are deliberately not dereferenced: the outer receipt
 * must separately seal candidate contents and the incoming return relation.
 * Ordinary on-foot identity is checked again before reading live body state.
 * This does not seal sphere contents, inventory, animation or other world
 * ownership, which remain the responsibility of the composed transaction. */
static int scene_pursuit_return_player_projection_seal(
    const scene_stream *stream,const scene_world_player_stage *player,
    const scene_pursuit_return_player_projection *p)
{
    extern rf_entity_room_state rf_scene_actor_room_state;
    int status;
    if(!stream||!player||!p||p->stage!=player)return RF_RANGE;
    status=scene_npc_reload_player_seal(stream,&p->identity);if(status)return status;
    if((campaign_player_view.flags_810&1u)||
       !isfinite(campaign_player_damage.state.effects.health)||
       campaign_player_damage.state.effects.health<=0||
       campaign_player_damage.state.effects.health!=p->health||
       campaign_player_damage.state.effects.armor!=p->armor||
       campaign_support_handle!=p->support||
       memcmp(&scene_actor_body.state,&p->body,sizeof(p->body))||
       memcmp(&rf_scene_actor_room_state,&p->room,sizeof(p->room))||
       memcmp(campaign_position,p->position,sizeof(p->position))||
       memcmp(campaign_orientation,p->eye,sizeof(p->eye))||
       memcmp(campaign_support_velocity,p->support_velocity,sizeof(p->support_velocity)))return RF_FORMAT;
    return RF_OK;
}
#endif
