#ifndef RF_SCENE_NPC_VITALS_INC
#define RF_SCENE_NPC_VITALS_INC
#include "rf/level.h"
#include "rf/entity.h"
#include <math.h>
/*464010: exact-1 sentinel at589510;40a4c0 clamps lower first and returns,
 * then tests the class maximum. Preserve that order for negative class life
 * (factory uses it to mark invulnerability); do not rewrite flags/class data.
 * Apply after creation and before persistent restoration. Error atomic. */
static int scene_npc_vitals_apply(const rf_level_entity_vitals *authored,
    rf_entity_creation_vitals_state *state,const rf_entity_creation_vitals_class *definition)
{
    rf_entity_creation_vitals_state next;
    if(!authored||!state||!definition)return RF_RANGE;
    if(!isfinite(authored->health)||!isfinite(authored->armor)||
       !isfinite(definition->health)||!isfinite(definition->armor))return RF_FORMAT;
    next=*state;
    if(authored->health!=-1.f)next.health=authored->health<0?0:authored->health>definition->health?definition->health:authored->health;
    if(authored->armor!=-1.f)next.armor=authored->armor<0?0:authored->armor>definition->armor?definition->armor:authored->armor;
    *state=next;return RF_OK;
}
#endif
