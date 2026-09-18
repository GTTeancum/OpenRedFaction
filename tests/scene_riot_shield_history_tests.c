#include "rf/campaign.h"
#include "rf/clutter_damage.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"shield history line%d: %s\n",__LINE__,#x);return 1;}}while(0)
typedef struct campaign_npc_body {
    struct {void *view;uint32_t handle;} registration;
    uint32_t persistence_registered,persistence_slot;
    struct {uint32_t owned[64];} inventory;
    struct {int32_t weapons[2];} view;
    uint32_t combat_burst_remaining,combat_due,combat_reload_due,combat_navigation_due,combat_scripted,combat_target;
    int32_t combat_reload_weapon;
} campaign_npc_body;
typedef struct scene_riot_shield_owner {uint32_t handle,initialized;rf_clutter_damage_state damage;} scene_riot_shield_owner;
static struct {scene_riot_shield_owner *owners;uint32_t count;int32_t weapon;} scene_npc_shields;
static campaign_npc_body bodies[2],*campaign_npc_bodies=bodies;static uint32_t campaign_npc_body_count=2;
static rf_campaign_actors rf_scene_defeated_actors;
static int campaign_enemy_broken_shield_fallback(uint32_t i,int32_t weapon,uint32_t *changed)
{*changed=0;if(bodies[i].view.weapons[0]==weapon && !bodies[i].inventory.owned[weapon] && bodies[i].inventory.owned[0]){bodies[i].view.weapons[0]=0;*changed=1;}return RF_OK;}
static int campaign_pain_reset_retained(void *c,uint32_t h,int32_t w){(void)c;(void)h;(void)w;return RF_OK;}
#include "../src/diagnostic/scene_riot_shield_history.inc"
static int bind(uint32_t i,const char *level,uint32_t uid,uint32_t handle)
{
    int status=rf_campaign_actor_register(&rf_scene_defeated_actors,level,uid,&bodies[i].persistence_slot);
    bodies[i].persistence_registered=1;bodies[i].registration.handle=handle;bodies[i].registration.view=&bodies[i].view;return status;
}
int main(void)
{
    scene_riot_shield_owner shield[2]={{0}};
    scene_npc_shields.owners=shield;scene_npc_shields.count=2;scene_npc_shields.weapon=13;
    CHECK(!bind(0,"a.rfl",42,100) && !bind(1,"a.rfl",43,101));
    shield[0]=(scene_riot_shield_owner){100,1,{37,0,-1}};shield[1]=(scene_riot_shield_owner){101,1,{-4,2,3}};
    CHECK(!scene_npc_shield_history_capture() && scene_shield_history_count==2);
    /* Revisit reordered actors, rebuilt handles and fresh constructor inventory. */
    CHECK(!bind(0,"a.rfl",43,200) && !bind(1,"a.rfl",42,201));memset(shield,0,sizeof(shield));
    bodies[0].inventory.owned[13]=bodies[0].inventory.owned[0]=1;bodies[0].view.weapons[0]=13;
    bodies[0].combat_scripted=1;bodies[0].combat_target=999;
    CHECK(!scene_npc_shield_history_restore());
    CHECK(shield[0].handle==200 && shield[0].damage.health==-4 && !bodies[0].inventory.owned[13] && bodies[0].view.weapons[0]==0);
    CHECK(bodies[0].combat_scripted==1 && bodies[0].combat_target==999);
    CHECK(shield[1].handle==201 && shield[1].damage.health==37);
    /* Same UID in a different level must not inherit either durability record. */
    CHECK(!bind(0,"b.rfl",43,300));memset(shield,0,sizeof(shield));
    CHECK(!scene_npc_shield_history_restore() && !shield[0].initialized && shield[1].damage.health==37);
    /* Broken shield with no fallback becomes unarmed; ownership stays absent. */
    CHECK(!bind(0,"a.rfl",43,400));bodies[0].inventory.owned[0]=0;bodies[0].inventory.owned[13]=1;bodies[0].view.weapons[0]=13;
    CHECK(!scene_npc_shield_history_restore() && !bodies[0].inventory.owned[13] && bodies[0].view.weapons[0]==-1);
    scene_npc_shield_history_reset();memset(shield,0,sizeof(shield));
    CHECK(!scene_npc_shield_history_restore() && !shield[0].initialized && !shield[1].initialized);
    /* Plain untouched NPCs consume no sparse entries, even without a history
     * registration. This is capture admission, not a shield-ray receive test. */
    bodies[0].persistence_registered=0;
    CHECK(!scene_npc_shield_history_capture() && !scene_shield_history_count);
    /* A touched live shield does require the caller's durable registration. */
    shield[0]=(scene_riot_shield_owner){400,1,{22,0,-1}};
    CHECK(scene_npc_shield_history_capture()==RF_NOT_FOUND && !scene_shield_history_count);
    /* Retired/unregistered entities no longer have equipment to restore. */
    bodies[0].registration.view=NULL;bodies[0].registration.handle=UINT32_MAX;
    CHECK(!scene_npc_shield_history_capture() && !scene_shield_history_count);
    CHECK(!scene_npc_shield_history_restore());
    puts("shield level/UID revisit, changed handles/order, broken ownership and new campaign reset passed");return 0;
}
