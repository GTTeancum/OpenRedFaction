#ifndef RF_SCENE_AI_DROP_SUPPLY_INCLUDED
#define RF_SCENE_AI_DROP_SUPPLY_INCLUDED
/* First-pass corpse drop policy: preserve the existing one-magazine maximum,
 * but do not manufacture rounds that the NPC has already spent. Only held,
 * owned, implemented weapons participate; no mutation before persistent drop
 * publication. An exhausted held weapon retains ownership value: quantity zero is
 * persisted without manufacturing ammunition. */
static int campaign_enemy_drop_supply(const rf_weapon_inventory *inventory,
    const rf_weapon_acquire_definition *definitions,uint32_t count,
    const int32_t ids[8],int32_t held,int32_t *quantity)
{
    static const uint32_t supported[6]={0,1,2,3,6,7};uint32_t i;
    int32_t loaded,reserve,type,magazine;int64_t available;
    if(!inventory || !definitions || !ids || !quantity)return RF_RANGE;
    if(held<0 || held>=64 || (uint32_t)held>=count || !inventory->owned[held])return RF_NOT_FOUND;
    for(i=0;i<6;i++)if(ids[supported[i]]==held)break;
    if(i==6)return RF_NOT_FOUND;
    type=definitions[held].ammo_type;magazine=definitions[held].magazine;
    if(type<0 || type>=32 || magazine<=0)return RF_RANGE;
    loaded=inventory->loaded[held];reserve=inventory->reserve[type];
    if(loaded<0 || loaded>magazine || reserve<0)return RF_RANGE;
    available=(int64_t)loaded+reserve;
    *quantity=available>magazine?magazine:(int32_t)available;
    return RF_OK;
}
#endif
