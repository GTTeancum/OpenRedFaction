#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"scene remote checkpoint line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static unsigned char saved_remote[RF_REMOTE_CHECKPOINT_MAX];
int main(void)
{
    scene_stream stream={0};float p[3]={1,2,3},d[3]={0,0,1};uint32_t bytes;
    rf_collision_solid_view movers[2]={0};int32_t uids[2]={123,456};rf_remote_charge_host host={0};
    strcpy(campaign_current_level,"glass_house.rfl");campaign_weapon_supply.names.count=1;
    strcpy(campaign_weapon_supply.names.names[0],"Remote Charge");campaign_remote_id=0;
    campaign_player_object.handle=7;campaign_equipped_slot=9;campaign_player_inventory.owned[0]=1;
    scene_remote_reset();CHECK(!rf_remote_charge_launch(scene_remote_charges+3,7,0,p,d,10,.051f,20));
    scene_remote_held=2;scene_remote_cooldown=17;
    CHECK(!scene_remote_checkpoint_capture(saved_remote,sizeof(saved_remote),&bytes));
    campaign_player_object.handle=999;campaign_remote_id=-1;
    CHECK(!scene_remote_checkpoint_preflight(&stream,saved_remote,bytes));
    CHECK(scene_remote_charges[3].owner==7); /* No live mutation before publication. */
    scene_remote_checkpoint_publish();CHECK(scene_remote_charges[3].owner==999 && scene_remote_held==2 && scene_remote_cooldown==17);
    campaign_remote_id=0;campaign_import_applied=1;campaign_player_import.weapon=0;campaign_equipped_slot=8;
    scene_remote_checkpoint_frame0();CHECK(campaign_equipped_slot==9 && scene_remote_charges[3].flight.lifecycle.active);
    CHECK(!scene_remote_checkpoint_keep_restored());
    /* Bind mover, save UID, reorder list/change handles, restore new identity. */
    campaign_movers.count=2;campaign_movers.views=movers;campaign_movers.uids=uids;
    movers[0].object_id=77;movers[1].object_id=88;
    for(unsigned i=0;i<3;i++){movers[0].output_matrix[i][i]=movers[1].output_matrix[i][i]=1;host.orientation[i*4]=1;}
    scene_remote_charges[3].attached=scene_remote_charges[3].flight.resting=1;
    scene_remote_charges[3].contact.object=SCENE_MOVER_ROCKET_OWNER;scene_remote_charges[3].contact.hit.normal[2]=1;
    memcpy(scene_remote_charges[3].contact.hit.point,p,12);host.handle=77;host.found=1;
    CHECK(!rf_remote_charge_bind_host(scene_remote_charges+3,&host));scene_remote_host_tags[3]=SCENE_MOVER_ROCKET_OWNER;
    CHECK(!scene_remote_checkpoint_capture(saved_remote,sizeof(saved_remote),&bytes));
    uids[0]=456;uids[1]=123;movers[1].object_id=222;
    CHECK(!scene_remote_checkpoint_preflight(&stream,saved_remote,bytes));scene_remote_checkpoint_publish();
    CHECK(scene_remote_charges[3].host==222 && scene_remote_host_tags[3]==(SCENE_MOVER_ROCKET_OWNER|1));
    uids[1]=777;CHECK(scene_remote_checkpoint_preflight(&stream,saved_remote,bytes)==RF_FORMAT);
    scene_remote_checkpoint_publish();CHECK(scene_remote_charges[3].host==222); /* Failed stage cannot publish. */
    CHECK(!scene_remote_checkpoint_preflight(&stream,NULL,0));scene_remote_checkpoint_publish();
    CHECK(!scene_remote_charges[3].flight.lifecycle.active && !scene_remote_pending && !scene_remote_held);
    puts("scene remote checkpoint player remap, selected mode/reset guard, mover UID reorder and absent/failed stage passed");return 0;
}
