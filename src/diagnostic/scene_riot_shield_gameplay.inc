/* NPC held shield adapter. Include after scene_ai_ammo_fallback.inc and scene_ai_broken_shield.inc.
 * Parent calls open with clutter.tbl bytes after NPC allocation, close before
 * NPC teardown, and receive only for a resolved direct-ray NPC body candidate.
 * No generic-damage hook: blasts/scripted damage bypass this adapter.
 * State is level-local, keyed by registered handle, not serialized yet.
 */
#include "scene_riot_shield.inc"
#include "scene_riot_shield_contact.inc"
#include <stdlib.h>

typedef struct scene_riot_shield_owner {
    uint32_t handle,initialized;rf_clutter_damage_state damage;
} scene_riot_shield_owner;
uint32_t rf_scene_riot_shield[4]; /* intercepted, broken, latest health bits, initialized owners */
static struct {
    scene_riot_shield_owner *owners;uint32_t count;
    int32_t weapon;rf_clutter_gameplay_definition definition;
} scene_npc_shields;
static void scene_npc_shields_close(void)
{
    free(scene_npc_shields.owners);memset(&scene_npc_shields,0,sizeof(scene_npc_shields));
    scene_npc_shields.weapon=-1;
}
static int scene_npc_shields_open(const void *clutter_text,uint32_t bytes,uint32_t budget)
{
    rf_clutter_gameplay_definition definition;scene_riot_shield_owner *owners;
    int32_t weapon;int status;
    if(scene_npc_shields.owners)return RF_RANGE;
    status=rf_clutter_gameplay_read(clutter_text,bytes,"riot_shield",&definition);if(status)return status;
    if(!(definition.life>0) || definition.protected_object)return RF_FORMAT;
    weapon=rf_weapon_name_find(&campaign_weapon_supply.names,"riot shield");
    if(weapon<0 || weapon>=64)return RF_NOT_FOUND;
    if(budget<sizeof(scene_npc_shields) || campaign_npc_body_count>(budget-sizeof(scene_npc_shields))/sizeof(*owners))return RF_RANGE;
    owners=campaign_npc_body_count?calloc(campaign_npc_body_count,sizeof(*owners)):NULL;
    if(campaign_npc_body_count && !owners)return RF_IO;
    memset(rf_scene_riot_shield,0,sizeof(rf_scene_riot_shield));
    scene_npc_shields.owners=owners;scene_npc_shields.count=campaign_npc_body_count;
    scene_npc_shields.weapon=weapon;scene_npc_shields.definition=definition;return RF_OK;
}
/* accepted=0 preserves damage routing. broken=1 reports the retired shield.
 * The broken-shield fallback preserves
 * combat_scripted/combat_target and never silently cancels that order.
 * Missing selected-shield resources are errors, never fictional protection. */
static int scene_npc_shield_receive(uint32_t slot,const float start[3],const float end[3],
    float actor_fraction,float damage,int32_t type,uint32_t *accepted,uint32_t *broken)
{
    campaign_npc_body *npc;scene_riot_shield_owner *owner;rf_weapon_hand_placement pose;
    rf_collision_model_response_hit hit;scene_riot_shield_result result;
    float travel[3];uint32_t i,model,contact=0,changed=0;int status;
    if(!accepted || !broken || !start || !end || slot>=campaign_npc_body_count ||
       !campaign_npc_bodies || slot>=scene_npc_shields.count || !scene_npc_shields.owners)return RF_RANGE;
    *accepted=*broken=0;npc=campaign_npc_bodies+slot;
    if(!npc->registration.view || rf_entity_lookup(&campaign_entities,(int32_t)npc->registration.handle)!=&npc->view)return RF_NOT_FOUND;
    if(npc->view.weapons[0]!=scene_npc_shields.weapon || !npc->inventory.owned[scene_npc_shields.weapon])return RF_OK;
    owner=scene_npc_shields.owners+slot;
    if(!owner->initialized || owner->handle!=npc->registration.handle){
        memset(owner,0,sizeof(*owner));owner->initialized=1;owner->handle=npc->registration.handle;
        ++rf_scene_riot_shield[3];owner->damage.health=scene_npc_shields.definition.life;owner->damage.killing_type=-1;
    }
    if(npc->view.weapons[0]!=scene_npc_shields.weapon || !npc->inventory.owned[scene_npc_shields.weapon] || owner->damage.health<=0)return RF_OK;
    if(slot>=campaign_model_owner_count || !campaign_model_owners)return RF_NOT_FOUND;
    model=campaign_weapon_models.weapons[scene_npc_shields.weapon].model;
    if(!model || model>campaign_weapon_models.count)return RF_NOT_FOUND;
    status=rf_scene_npc_weapon_placement(npc->registration.handle,0,&pose);if(status)return status;
    status=scene_riot_shield_contact(&campaign_weapon_models.items[model-1].render,&pose,start,end,actor_fraction,&hit,&contact);
    if(status)return status;
    for(i=0;i<3;++i)travel[i]=end[i]-start[i];
    status=scene_riot_shield_intercept(&scene_npc_shields.definition,&owner->damage,1,1,1,contact,
        campaign_model_owners[slot].basis+6,travel,damage,type,&result);if(status)return status;
    if(!result.intercepted)return RF_OK;
    owner->damage=result.state;*accepted=1;*broken=result.broken;
    ++rf_scene_riot_shield[0];rf_scene_riot_shield[1]+=result.broken;memcpy(rf_scene_riot_shield+2,&owner->damage.health,4);
    if(result.broken){
        /*4031a0 NPC path clears ownership; player mark pool is irrelevant. */
        npc->inventory.owned[scene_npc_shields.weapon]=0;
        status=campaign_enemy_broken_shield_fallback(slot,scene_npc_shields.weapon,&changed);if(status)return status;
        if(!changed){
            status=campaign_pain_reset_retained(NULL,npc->registration.handle,scene_npc_shields.weapon);if(status)return status;
            npc->view.weapons[0]=-1;npc->combat_burst_remaining=npc->combat_due=npc->combat_reload_due=0;
            npc->combat_reload_weapon=-1;npc->combat_navigation_due=0;
        }
    }
    return RF_OK;
}
