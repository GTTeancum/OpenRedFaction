#ifndef RF_SCENE_JEEP_NPC_GUNNER_INCLUDED
#define RF_SCENE_JEEP_NPC_GUNNER_INCLUDED
/* Include after scene_driller_entry_exit, before Jeep seats and damage runtime.
 * The real seat bank is entry.occupants[2]: NPC driver0, player gunner1.
 * A local control token adapts the existing player transaction; the physical
 * host.driver NEVER changes to the gunner. Admission/exit use the real entry's
 * world queries, registered identity, authored tag and preboarding body pose.
 * Callers publish returned player state using the ordinary player adapter.
 */
/* entries,exits,blocked switches,gunner frames,active,host,driver,seat0,seat1,
 * player,player parent,role,live NPC pair,player steering,error,host alive */
uint32_t rf_scene_jeep_npc_gunner[16];
static uint32_t scene_jeep_npc_gunner_active(const scene_driller_entry_exit *e)
{
    return e&&e->resource&&!strcmp(e->resource->model,"Jeep01.v3m")&&
        e->session.active&&e->gunner_occupant==(int32_t)e->player.handle;
}
static uint32_t scene_jeep_npc_gunner_driver(const scene_driller_entry_exit *e)
{
    return e&&e->host.driver!=UINT32_MAX&&e->host.driver!=e->player.handle&&
        e->occupant==(int32_t)e->host.driver&&
        scene_npc_seat_related(e->host.handle,e->host.driver);
}
static int scene_jeep_npc_gunner_valid(const scene_driller_entry_exit *e)
{
    if(!e||!e->resource||strcmp(e->resource->model,"Jeep01.v3m")||
       e->view.occupants!=e->occupants||e->view.occupant_count!=2||
       e->registration.view!=&e->view||e->registration.handle!=e->host.handle||
       rf_entity_lookup(e->entities,(int32_t)e->host.handle)!=&e->view||
       rf_entity_lookup(e->entities,(int32_t)e->player.handle)!=e->player_view)return RF_NOT_FOUND;
    /* Driver death may legitimately leave the gunner alone. Do not promote
     * the player or restore the absent authored driver to acquire control. */
    if(e->host.driver==UINT32_MAX){if(e->occupant!=-1)return RF_NOT_FOUND;}
    else if(!scene_jeep_npc_gunner_driver(e))return RF_NOT_FOUND;
    if(e->session.active&&(!scene_jeep_npc_gunner_active(e)||
       e->session.host!=e->host.handle||e->session.driver!=e->player.handle||
       e->player.host!=e->host.handle||e->player.control!=e->host.handle||
       e->player_view->linked_handle!=(int32_t)e->host.handle))return RF_NOT_FOUND;
    return RF_OK;
}
/* Also used by destroyed-host release. reason1 retains collision-safe living
 * exit; reason2 retires a dead player's ownership without guessed placement. */
static int scene_jeep_npc_gunner_unpossess(scene_driller_entry_exit *e,
    uint32_t reason,uint32_t *changed)
{
    scene_vehicle_host_control control;scene_vehicle_possession session;
    scene_vehicle_player_control player;scene_vehicle_possession_ops ops;int status;
    if(!changed)return RF_RANGE;*changed=0;
    if(!scene_jeep_npc_gunner_active(e))return RF_NOT_FOUND;
    status=scene_jeep_npc_gunner_valid(e);if(status)return status;
    control=e->host;control.driver=e->player.handle;session=e->session;player=e->player;
    ops=(scene_vehicle_possession_ops){scene_driller_entry_allowed,scene_driller_entry_seat,scene_driller_exit_pose,e};
    status=scene_vehicle_unpossess(&session,&player,&control,reason,&ops,changed);if(status)return status;
    if(*changed){e->session=session;e->player=player;e->gunner_occupant=-1;
        e->player_view->linked_handle=-1;++rf_scene_jeep_npc_gunner[1];}
    return RF_OK;
}
/* handled0 leaves even the held-edge state untouched for the old Use path.
 * handled1 owns this input sample and never writes the NPC driver slot. */
static int scene_jeep_npc_gunner_use(scene_driller_entry_exit *e,
    scene_driller_resources *resource,uint32_t *role,uint32_t held,uint32_t alive,
    uint32_t *changed,uint32_t *handled)
{
    scene_vehicle_host_control control;scene_vehicle_possession session;
    scene_vehicle_player_control player;scene_vehicle_possession_ops ops;
    uint32_t edge,i;int32_t old_seat,gunner=-1;int status;
    if(!e||!resource||!role||!changed||!handled||held>1||alive>1)return RF_RANGE;
    *changed=*handled=0;
    if(strcmp(resource->model,"Jeep01.v3m")||e->resource!=resource)return RF_OK;
    if(!scene_jeep_npc_gunner_active(e)&&
       (e->session.active||!scene_jeep_npc_gunner_driver(e)))return RF_OK;
    *handled=1;edge=held&&!e->held;e->held=held;
    status=scene_jeep_npc_gunner_valid(e);if(status)return status;
    if(e->session.active){
        if(alive&&!edge&&e->host.alive)return RF_OK;
        if(edge&&alive&&e->host.alive&&(e->view.flags_814&0x80u))return RF_OK;
        return scene_jeep_npc_gunner_unpossess(e,alive?0:2,changed);
    }
    if(!edge||!alive||!e->host.alive||e->gunner_occupant!=-1||
       e->player_view->linked_handle!=-1)return RF_OK;
    for(i=0;i<resource->tags.count;i++)if(!strcmp(resource->tags.items[i].name,"interface_2")){gunner=(int32_t)i;break;}
    if(gunner<0)return RF_NOT_FOUND;
    session=e->session;player=e->player;control=e->host;control.driver=UINT32_MAX;
    memcpy(player.pose.position,e->player_body->state.position,12);
    memcpy(player.pose.basis,e->player_body->state.orientation,36);
    player.weapon=e->player_view->weapons[0];player.control_flags=e->player_view->flags_810;
    ops=(scene_vehicle_possession_ops){scene_driller_entry_allowed,scene_driller_entry_seat,scene_driller_exit_pose,e};
    old_seat=resource->seat;resource->seat=gunner;
    status=scene_vehicle_possess(&session,&player,&control,alive,&ops,changed);
    if(status||!*changed){resource->seat=old_seat;return status;}
    e->session=session;e->player=player;e->gunner_occupant=(int32_t)player.handle;
    e->player_view->linked_handle=(int32_t)e->host.handle;*role=1;
    ++rf_scene_jeep_npc_gunner[0];return RF_OK;
}
/* Invoked AFTER the seat-switch edge has been consumed, before its single
 * driver invariants. No mid-session promotion even after NPC driver death. */
static uint32_t scene_jeep_npc_gunner_blocks_switch(const scene_driller_entry_exit *e)
{
    if(!scene_jeep_npc_gunner_active(e))return 0;
    ++rf_scene_jeep_npc_gunner[2];return 1;
}
static uint32_t scene_vehicle_player_fire_owner(const scene_driller_entry_exit *e)
{
    return e->session.active?e->session.driver:e->host.driver;
}
/* Once at the end of the ordinary vehicle tick. This is observation, not a
 * fixture override: no player, host, route or seat state is changed. */
static void scene_jeep_npc_gunner_probe(const scene_driller_entry_exit *e,uint32_t role)
{
    uint32_t active=scene_jeep_npc_gunner_active(e);if(active)++rf_scene_jeep_npc_gunner[3];
    rf_scene_jeep_npc_gunner[4]=active;rf_scene_jeep_npc_gunner[5]=e->host.handle;
    rf_scene_jeep_npc_gunner[6]=e->host.driver;rf_scene_jeep_npc_gunner[7]=(uint32_t)e->occupant;
    rf_scene_jeep_npc_gunner[8]=(uint32_t)e->gunner_occupant;rf_scene_jeep_npc_gunner[9]=e->player.handle;
    rf_scene_jeep_npc_gunner[10]=(uint32_t)e->player_view->linked_handle;rf_scene_jeep_npc_gunner[11]=role;
    rf_scene_jeep_npc_gunner[12]=scene_jeep_npc_gunner_driver(e);
    rf_scene_jeep_npc_gunner[13]=e->session.active&&e->host.alive&&role==0;
    rf_scene_jeep_npc_gunner[14]=active?(uint32_t)scene_jeep_npc_gunner_valid(e):0;
    rf_scene_jeep_npc_gunner[15]=e->host.alive;
}
#endif
