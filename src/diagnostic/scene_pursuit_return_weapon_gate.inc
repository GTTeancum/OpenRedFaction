#ifndef RF_SCENE_PURSUIT_RETURN_WEAPON_GATE_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_WEAPON_GATE_INCLUDED
/* UNCALLED read-only ordinary standalone return gates, 408d90/408dc0.
 * The caller supplies a current catalog snapshot and proves that the owner
 * is standalone. Unarmed owners return RF_NOT_FOUND: linked-owner recursion
 * is not implemented here and must not become a fabricated true predicate.
 * Only the primary flags_264 melee bit controls the prospective state3 gate;
 * flags_268, practical firing ranges and live movement state are unrelated.
 * Output remains unchanged on errors or unsupported unarmed ownership. */
static int scene_pursuit_return_weapon_gate(int32_t primary,int32_t secondary,
    const rf_weapon_descriptor *definitions,uint32_t weapon_count,
    uint32_t *melee_only)
{
    uint32_t result;
    if(!definitions||!melee_only)return RF_RANGE;
    if(!weapon_count||weapon_count>64||primary < -1||secondary < -1||
       (primary>=0&&(uint32_t)primary>=weapon_count)||
       (secondary>=0&&(uint32_t)secondary>=weapon_count))return RF_FORMAT;
    if(primary==-1&&secondary==-1)return RF_NOT_FOUND;
    result=primary!=-1&&secondary==-1&&
        !!(definitions[primary].flags_264&0x20u);
    *melee_only=result;return RF_OK;
}
#endif
