/* Two-player split screen, stage 3b (docs/SPLIT-SCREEN-2P.md): the world's
 * view of both humans. One human's state is live in the player globals, the
 * other's is parked (scene_player_context.inc); these read either one by its
 * entity handle so bots, scoring and pickups can treat both as players, as
 * the PS2 does with its player list. Index 0 is the first human, 1 the
 * second; scene_human_handle[] holds their handles for the whole match. */
static uint32_t scene_human_handle[2];
/* Second human: kills, deaths, streak, longest streak, bots fighting it,
 * its health, frames any bot fought it, its handle; in its pass: bot
 * updates, bot shots, rounds flown, rounds that hurt it. */
uint32_t rf_scene_mp_human2[12];
static void *scene_player_other(void *global);extern uint32_t rf_scene_player_context[4];
extern float rf_scene_gameplay_eye[3];static uint32_t scene_mp_player_class_flags;
typedef struct {
    uint32_t index,handle,live,affiliation,class_flags;float health,armor,radius;
    const float *position,*eye;campaign_player_damage_owner *damage;
} scene_human_ref;
/* The human with index 0 or 1, live or parked; 0 when absent or not yet
 * registered (the second human exists only in a split-screen match). */
static int scene_human_get(uint32_t index,scene_human_ref *out)
{
    uint32_t live=index==(rf_scene_player_context[0]&1u);
    rf_registered_entity_view *object=&campaign_player_object;
    campaign_player_damage_owner *damage=&campaign_player_damage;
    const float *position=scene_actor_body.state.position,*eye=rf_scene_gameplay_eye;
    __typeof__(campaign_player_geometry) *geometry=&campaign_player_geometry;
    uint32_t *flags=&scene_mp_player_class_flags;
    if(index>1 || (index==1 && !scene_player2_live))return 0;
    if(!live){
        object=scene_player_other(object);damage=scene_player_other(damage);
        position=((__typeof__(scene_actor_body) *)scene_player_other(&scene_actor_body))->state.position;
        eye=scene_player_other(rf_scene_gameplay_eye);geometry=scene_player_other(geometry);flags=scene_player_other(flags);
    }
    if(!object->view)return 0;
    out->index=index;out->handle=object->handle;out->live=live;
    out->affiliation=damage->state.effects.affiliation&3;out->class_flags=*flags;
    out->health=damage->state.effects.health;out->armor=damage->state.effects.armor;
    out->radius=geometry->model_radius;out->position=position;out->eye=eye;out->damage=damage;
    return 1;
}
/* The human whose entity handle this is; 0 for anything else. */
static int scene_human_find(uint32_t handle,scene_human_ref *out)
{
    uint32_t i;
    if(!handle)return 0;
    for(i=0;i<2;i++)if(scene_human_get(i,out) && out->handle==handle)return 1;
    return 0;
}
/* Bots fighting the second human run their update with that human live
 * (stage 3b): the primary pass skips them, the second human's pass runs only
 * them. The set is taken once per frame at the primary pass. */
static uint8_t scene_human2_targeted[256];
static void scene_human2_targets_take(void)
{
    uint32_t i;
    memset(scene_human2_targeted,0,sizeof(scene_human2_targeted));
    if(!scene_player2_live || !scene_human_handle[1])return;
    for(i=0;i<campaign_npc_body_count && i<256;i++)
        scene_human2_targeted[i]=campaign_npc_bodies[i].combat_alert && campaign_npc_bodies[i].combat_target==scene_human_handle[1];
    rf_scene_mp_human2[4]=0;for(i=0;i<256;i++)rf_scene_mp_human2[4]+=scene_human2_targeted[i];
    if(rf_scene_mp_human2[4])++rf_scene_mp_human2[6];rf_scene_mp_human2[7]=scene_human_handle[1];
}
static int scene_human2_pass_skip(uint32_t slot)
{
    if(!scene_player2_live)return 0;
    if(slot>=256)return !scene_follow_primary;
    return scene_follow_primary?scene_human2_targeted[slot]:!scene_human2_targeted[slot];
}
