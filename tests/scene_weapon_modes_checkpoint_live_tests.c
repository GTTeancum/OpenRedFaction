/* Synthetic resource owners exercise actual live scene admission/publication. */
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"weapon modes line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    scene_stream s={0};rf_player_weapon views[3]={0};scene_weapon_custom_actions actions[3]={0};
    scene_undercover_resources undercover={0};rf_campaign_player_state player={0};
    rf_weapon_modes_checkpoint saved={3u,0x12345678u},captured={0},before;
    rf_weapon_inventory inventory;uint32_t i,j;
    campaign_extra_ids[0]=2;campaign_machine_special_id=3;campaign_extra_ids[3]=4;
    s.player_weapon[13]=views;s.player_weapon[17]=views+1;s.player_weapon[16]=views+2;
    s.machine_custom[0]=actions;s.machine_custom[1]=actions+1;s.machine_transition_ticks[0]=12;s.machine_transition_ticks[1]=15;
    s.undercover=&undercover;s.undercover_textures=2;undercover.actions=actions+2;
    undercover.render.part_count=1;undercover.materials.count=1;undercover.mode.attachment.parent=0;
    for(i=0;i<3;i++){
        views[i].bone_count=1;actions[i].view=views+i;actions[i].active=-1;actions[i].count=i==2?2:1;
        for(j=0;j<actions[i].count;j++)actions[i].payloads[j]=(void *)(uintptr_t)(1+i*2+j);
    }
    player.inventory.owned[2]=player.inventory.owned[4]=1;player.inventory.loaded[2]=11;player.inventory.loaded[3]=7;
    player.inventory.loaded[4]=9;player.inventory.reserve[0]=19;player.weapon=2;inventory=player.inventory;
    CHECK(!scene_weapon_modes_checkpoint_prepare(&s,&player,&saved));
    CHECK(!memcmp(&player.inventory,&inventory,sizeof(inventory)));
    campaign_player_inventory=player.inventory;campaign_equipped_slot=13;player_input.alt_fire=1;
    scene_weapon_modes_checkpoint_assign(&s,&saved);
    CHECK(campaign_machine_mode.special && campaign_machine_mode.target && !campaign_machine_mode.pending && !campaign_machine_mode.due && campaign_machine_mode.held);
    CHECK(undercover.mode.attached && undercover.mode.target && !undercover.mode.pending && s.undercover_alt_held);
    CHECK(campaign_conventional_random.value==saved.conventional_rng);
    CHECK(!scene_weapon_modes_checkpoint_capture(&s,&captured) && !memcmp(&captured,&saved,sizeof(saved)));
    CHECK(!memcmp(&campaign_player_inventory,&inventory,sizeof(inventory)));
    before=captured;campaign_machine_mode.pending=1;
    CHECK(scene_weapon_modes_checkpoint_capture(&s,&captured)==RF_RANGE && !memcmp(&captured,&before,sizeof(before)));campaign_machine_mode.pending=0;
    undercover.mode.pending=1;CHECK(scene_weapon_modes_checkpoint_prepare(&s,&player,&saved)==RF_RANGE);undercover.mode.pending=0;
    actions[1].active=0;CHECK(scene_weapon_modes_checkpoint_prepare(&s,&player,&saved)==RF_RANGE);actions[1].active=-1;
    views[0].initialized=1;views[0].current=2;CHECK(scene_weapon_modes_checkpoint_prepare(&s,&player,&saved)==RF_RANGE);views[0].current=0;
    s.undercover_textures=0;CHECK(scene_weapon_modes_checkpoint_prepare(&s,&player,&saved)==RF_RANGE);s.undercover_textures=2;
    player.inventory.owned[2]=0;CHECK(scene_weapon_modes_checkpoint_prepare(&s,&player,&saved)==RF_FORMAT);player.inventory.owned[2]=1;
    player.inventory.owned[3]=1;CHECK(scene_weapon_modes_checkpoint_prepare(&s,&player,&saved)==RF_FORMAT);player.inventory.owned[3]=0;
    player.weapon=3;CHECK(scene_weapon_modes_checkpoint_prepare(&s,&player,&saved)==RF_FORMAT);player.weapon=2;
    player.inventory.owned[4]=0;CHECK(scene_weapon_modes_checkpoint_prepare(&s,&player,&saved)==RF_FORMAT);player.inventory.owned[4]=1;
    saved.flags=0;player_input.alt_fire=0;scene_weapon_modes_checkpoint_assign(&s,&saved);
    CHECK(!campaign_machine_mode.special && !campaign_machine_mode.target && !campaign_machine_mode.held && !undercover.mode.attached && !s.undercover_alt_held);
    puts("PASS live RFWM settled ownership/resources, completed mode/RNG publication, no ammo mutation, pending/custom/reload rejection");return 0;
}
