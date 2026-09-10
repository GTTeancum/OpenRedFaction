#include "rf/player.h"
#include <string.h>

int rf_player_bind_local(rf_player_local_binding *local,rf_player_entity_binding *entity,
    rf_player_select_weapon select_weapon,void *context)
{
    if(!local || !entity || !select_weapon)return RF_RANGE;
    local->entity=entity;
    local->inventory=&entity->inventory;
    entity->mode_1f8=2;
    if(entity->type_24==0)entity->state_560=-1;
    local->entity_handle=entity->handle_2c;
    select_weapon(context,local,local->inventory->primary_weapon);
    return RF_OK;
}

int rf_player_spawn_prepare(rf_player_spawn_state *player,int local_player,
    int32_t skin_count,rf_player_position_override *override,
    rf_player_spawn_request *request)
{
    if(!player || !override || !request || override->pending>255)return RF_RANGE;
    if(request->skin_index<0 || request->skin_index>=skin_count) {
        if(local_player)player->skin_f5c=0;
        request->skin_index=0;
    }
    if(local_player)player->flags_10&=~8u;
    if(override->pending==1) {
        memcpy(request->position,override->position,sizeof(request->position));
        override->pending=0;
    }
    return RF_OK;
}
