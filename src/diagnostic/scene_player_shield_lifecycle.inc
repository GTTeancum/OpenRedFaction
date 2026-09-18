/* Include after scene_riot_shield.inc. Player-owned durability, no inventory,
 * ammo, rendering or collision ownership. The caller supplies the authored
 * riot_shield definition and an already admitted physical shield contact.
 * Keep this state across weapon switches/level visits; reset for a new player.
 * A pickup while already owned does not repair it. Removal must be observed
 * through sync(owned=0) before a later acquisition can initialize new life. */
typedef struct scene_player_shield_state {
    rf_clutter_damage_state damage;
    uint32_t owned,selected,broken;
} scene_player_shield_state;

typedef struct scene_player_shield_hit {
    uint32_t intercepted,broken,remove_weapon,request_fallback;
} scene_player_shield_hit;

static void scene_player_shield_reset(scene_player_shield_state *state)
{
    scene_player_shield_state empty={0};
    if(state){empty.damage.killing_type=-1;*state=empty;}
}

static int scene_player_shield_sync(scene_player_shield_state *state,
    const rf_clutter_gameplay_definition *definition,uint32_t owned,
    uint32_t selected)
{
    if(!state || !definition || !isfinite(definition->life) ||
       definition->life<=0)return RF_RANGE;
    if(!owned){scene_player_shield_reset(state);return RF_OK;}
    if(!state->owned){
        scene_player_shield_reset(state);
        state->damage.health=definition->life;
        state->owned=1;
    }
    state->selected=selected!=0;
    return RF_OK;
}

static int scene_player_shield_receive(scene_player_shield_state *state,
    const rf_clutter_gameplay_definition *definition,uint32_t direct,
    uint32_t contact,const float forward[3],const float travel[3],
    float damage,int32_t damage_type,scene_player_shield_hit *out)
{
    scene_riot_shield_result result;
    scene_player_shield_hit hit={0};int status;
    if(!state || !out)return RF_RANGE;
    status=scene_riot_shield_intercept(definition,&state->damage,
        state->selected,state->owned && !state->broken,direct,contact,
        forward,travel,damage,damage_type,&result);
    if(status)return status;
    state->damage=result.state;
    hit.intercepted=result.intercepted;
    if(result.intercepted && result.broken){
        state->broken=1;
        hit.broken=hit.remove_weapon=hit.request_fallback=1;
    }
    /* Do not clear owned here: repeated sync against the not-yet-mutated
     * inventory would then resurrect a shield. Parent removes ownership,
     * chooses an existing weapon and acknowledges removal through sync. */
    *out=hit;return RF_OK;
}
