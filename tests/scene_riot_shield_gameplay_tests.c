/* Adapter contract fixture: actual installed class parser + real physical
 * triangle/damage services; pose lookup/fallback are isolated scene seams. */
#include "rf/entity_assets.h"
#include "rf/entity.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef struct campaign_npc_body {
    rf_entity_view view;rf_weapon_inventory inventory;
    struct {void *view;uint32_t handle;} registration;
    uint32_t combat_scripted,combat_target,combat_burst_remaining,combat_due,combat_reload_due,combat_navigation_due;
    int32_t combat_reload_weapon;
} campaign_npc_body;
static campaign_npc_body npc,*campaign_npc_bodies=&npc;
static uint32_t campaign_npc_body_count=1,campaign_model_owner_count=1,reset_calls,fallback_calls;
static int campaign_entities;
static struct {float basis[9];} model_owner,*campaign_model_owners=&model_owner;
static rf_weapon_supply_catalog campaign_weapon_supply;
static rf_weapon_model_owner campaign_weapon_models;
static rf_weapon_hand_placement hand;
static rf_entity_view *test_lookup(void *unused,int32_t handle){(void)unused;return (uint32_t)handle==npc.registration.handle?&npc.view:NULL;}
#define rf_entity_lookup test_lookup
static int rf_scene_npc_weapon_placement(uint32_t handle,int32_t which,rf_weapon_hand_placement *out)
{assert(handle==npc.registration.handle && which==0);*out=hand;return RF_OK;}
static int campaign_pain_reset_retained(void *ctx,uint32_t handle,int32_t weapon)
{(void)ctx;assert(handle==npc.registration.handle && weapon==0);++reset_calls;return RF_OK;}
static int campaign_enemy_broken_shield_fallback(uint32_t slot,int32_t shield,uint32_t *changed)
{assert(slot==0 && shield==0);++fallback_calls;*changed=0;if(npc.inventory.owned[1]){npc.view.weapons[0]=1;*changed=1;}return RF_OK;}
#include "../src/diagnostic/scene_riot_shield_gameplay.inc"
#include "../src/diagnostic/scene_riot_shield_query.inc"
int main(int argc,char **argv)
{
    rf_vpp tables={0};rf_vpp_entry entry;char *text;
    rf_weapon_static_model item={0};rf_model_part_metadata part={0};rf_static_render_lod lod={0};
    rf_model_vertex vertices[3]={0};int32_t reuse[3]={0};rf_model_triangle triangle={{0,1,2},0x20};
    rf_model_draw_batch batch={0,3,0,1,0};uint32_t accepted,broken;
    float start[3]={0,0,2},end[3]={0,0,-2};
    assert(argc==2 && rf_vpp_open(&tables,argv[1])==RF_OK);
    assert(rf_vpp_find(&tables,"clutter.tbl",&entry)==RF_OK);text=malloc(entry.size);assert(text);
    assert(rf_vpp_read(&tables,&entry,0,text,entry.size)==RF_OK);
    campaign_weapon_supply.names.count=2;strcpy(campaign_weapon_supply.names.names[0],"riot shield");
    assert(scene_npc_shields_open(text,entry.size,4096)==RF_OK);assert(scene_npc_shields.definition.life==1250);
    free(text);rf_vpp_close(&tables);
    vertices[0].position[0]=-1;vertices[0].position[1]=-1;vertices[1].position[0]=1;vertices[1].position[1]=-1;vertices[2].position[1]=1;
    lod.geometry.batches=&batch;lod.geometry.batch_count=1;lod.geometry.vertices=vertices;lod.geometry.vertex_count=3;
    lod.geometry.triangles=&triangle;lod.geometry.triangle_count=1;lod.geometry.reuse=reuse;
    item.render.parts=&part;item.render.part_count=1;item.render.lods=&lod;item.render.lod_count=1;
    campaign_weapon_models.items=&item;campaign_weapon_models.count=1;campaign_weapon_models.weapons[0].model=1;
    hand.basis[0]=hand.basis[4]=hand.basis[8]=1;memcpy(model_owner.basis,hand.basis,36);
    npc.registration.handle=5;npc.registration.view=&npc.view;npc.inventory.owned[0]=npc.inventory.owned[1]=1;
    {
        scene_npc_shield_candidate candidate;scene_riot_shield_owner before=scene_npc_shields.owners[0];
        uint32_t diagnostics[4];memcpy(diagnostics,rf_scene_riot_shield,sizeof(diagnostics));
        assert(scene_npc_shield_query(0,start,end,1,&candidate,&accepted)==RF_OK && accepted);
        assert(candidate.hit.time==.5f && candidate.limit==1 && candidate.handle==5);
        assert(!memcmp(&before,scene_npc_shields.owners,sizeof(before)));
        assert(!memcmp(diagnostics,rf_scene_riot_shield,sizeof(diagnostics)));
        assert(scene_npc_shield_query(0,start,end,.5f,&candidate,&accepted)==RF_OK && !accepted);
        assert(scene_npc_shield_query(0,end,start,1,&candidate,&accepted)==RF_OK && !accepted);
        assert(scene_npc_shield_query(0,start,end,1,&candidate,&accepted)==RF_OK && accepted);
        /* No actor-body contact was queried or required. */
        assert(scene_npc_shield_commit(&candidate,start,end,100,-1,&accepted,&broken)==RF_OK && accepted && !broken);
        candidate.handle=99;
        assert(scene_npc_shield_commit(&candidate,start,end,100,-1,&accepted,&broken)==RF_NOT_FOUND);
    }

    assert(scene_npc_shields.owners[0].damage.health==1150);
    npc.view.weapons[0]=1;
    assert(scene_npc_shield_receive(0,start,end,.75f,100,-1,&accepted,&broken)==RF_OK && !accepted);
    npc.view.weapons[0]=0;
    assert(scene_npc_shield_receive(0,end,start,.75f,100,-1,&accepted,&broken)==RF_OK && !accepted);
    assert(scene_npc_shields.owners[0].damage.health==1150);
    assert(scene_npc_shield_receive(0,start,end,.75f,1200,-1,&accepted,&broken)==RF_OK && accepted && broken);
    assert(!npc.inventory.owned[0] && npc.view.weapons[0]==1 && fallback_calls==1);
    npc.registration.handle=6;npc.inventory.owned[0]=1;npc.view.weapons[0]=0;
    npc.combat_scripted=1;npc.combat_target=77;npc.inventory.owned[1]=0;
    assert(scene_npc_shield_receive(0,start,end,.75f,100,-1,&accepted,&broken)==RF_OK && accepted && !broken);
    assert(scene_npc_shields.owners[0].damage.health==1150);
    assert(scene_npc_shield_receive(0,start,end,.75f,1200,-1,&accepted,&broken)==RF_OK && accepted && broken);
    assert(npc.combat_scripted==1 && npc.combat_target==77 && npc.view.weapons[0]==-1 && reset_calls==1);
    scene_npc_shields_close();assert(!scene_npc_shields.owners);
    puts("NPC shield adapter PASS: authored1250life, mesh contact, durability, selection, owner reset, break/order preservation");return 0;
}
