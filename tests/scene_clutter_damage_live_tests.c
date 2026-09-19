#include <stdio.h>
#include <stdlib.h>
#include "../src/diagnostic/scene_clutter_damage_live.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"clutter live line %d: %s\n",__LINE__,#x);return 1;}}while(0)
#define BASE "$Class Name: \"lamp\"\n$V3D Filename: \"lamp.v3d\"\n$Material: \"glass\"\n$Life: 80\n$Flags: (\"collide_weapon\")\n$Damage Type Factor: \"bullet\" 0.5\n"
int main(int argc,char **argv)
{
    rf_object_registry registry;rf_clutter_base_owner owner={0},before;
    scene_clutter_damage_profile profile,protected_profile;
    scene_clutter_damage_binding binding={0},saved_binding;
    scene_clutter_damage_live_result result,saved_result;
    rf_object_registry_init(&registry);
    CHECK(!scene_clutter_damage_profile_read(BASE,sizeof(BASE)-1,"lamp",&profile));
    CHECK(profile.life==80 && profile.damage_factors[1]==.5f && profile.ordinary);
    CHECK(!rf_object_registry_insert(&registry,&owner,&owner.state.handle));
    owner.state.class_index=3;owner.state.health=100;owner.state.flags=0x400010;
    CHECK(!scene_clutter_damage_bind(&registry,&owner,&profile,&binding));
    CHECK(owner.state.health==80 && owner.state.flags==0x400010 && !binding.break_pending);
    CHECK(!scene_clutter_damage_receive_live(&registry,&owner,&profile,&binding,20,1,0,&result));
    CHECK(result.accepted && result.applied && !result.retired && owner.state.health==70);
    CHECK(owner.state.flags&0x200000);
    CHECK(!scene_clutter_damage_bind(&registry,&owner,&profile,&binding));CHECK(owner.state.health==70);
    owner.state.flags|=0x4000;before=owner;
    CHECK(!scene_clutter_damage_receive_live(&registry,&owner,&profile,&binding,1000,1,1,&result));
    CHECK(!result.accepted && !memcmp(&owner,&before,sizeof(owner)));
    owner.state.flags&=~0x4000u;
    CHECK(!scene_clutter_damage_receive_live(&registry,&owner,&profile,&binding,140,1,0,&result));
    CHECK(result.retired && owner.state.health==0 && (owner.state.flags&2) && binding.killing_type==1 && binding.break_pending);
    binding.break_pending=0; /* Deferred consumer acknowledges exactly once. */
    before=owner;
    CHECK(!scene_clutter_damage_receive_live(&registry,&owner,&profile,&binding,-100,-1,1,&result));
    CHECK(!result.applied && !memcmp(&owner,&before,sizeof(owner)) && !binding.break_pending);
    CHECK(!scene_clutter_damage_bind(&registry,&owner,&profile,&binding));CHECK(owner.state.health==0);
    CHECK(!rf_object_registry_remove(&registry,owner.state.handle));
    saved_result=result;saved_binding=binding;
    CHECK(scene_clutter_damage_receive_live(&registry,&owner,&profile,&binding,1,1,0,&result)==RF_RANGE);
    CHECK(!memcmp(&result,&saved_result,sizeof(result)) && !memcmp(&binding,&saved_binding,sizeof(binding)));
    CHECK(!rf_object_registry_insert(&registry,&owner,&owner.state.handle));
    CHECK(scene_clutter_damage_bind(&registry,&owner,&profile,&binding)==RF_RANGE);
    memset(&binding,0,sizeof(binding));owner.state.flags=0;
    protected_profile=profile;protected_profile.life=-1;protected_profile.protected_object=1;
    CHECK(!scene_clutter_damage_bind(&registry,&owner,&protected_profile,&binding));CHECK(owner.state.health==100 && (owner.state.flags&4));
    CHECK(!scene_clutter_damage_receive_live(&registry,&owner,&protected_profile,&binding,10,1,0,&result));
    CHECK(result.accepted && !result.applied && owner.state.health==100 && (owner.state.flags&0x200000));
    CHECK(!scene_clutter_damage_receive_live(&registry,&owner,&protected_profile,&binding,10,1,1,&result));CHECK(owner.state.health==95);
    before=owner;saved_result=result;
    CHECK(scene_clutter_damage_receive_live(&registry,&owner,&protected_profile,&binding,1,11,0,&result)==RF_RANGE);
    CHECK(!memcmp(&owner,&before,sizeof(owner)) && !memcmp(&result,&saved_result,sizeof(result)));
    profile.ordinary=0;before=owner;
    CHECK(!scene_clutter_damage_receive_live(&registry,&owner,&profile,&binding,1000,-1,1,&result));
    CHECK(!result.accepted && !memcmp(&owner,&before,sizeof(owner)));
    if(argc>1){
        rf_vpp archive;rf_vpp_entry entry;void *text;
        CHECK(!rf_vpp_open(&archive,argv[1]));CHECK(!rf_vpp_find(&archive,"clutter.tbl",&entry));
        CHECK(entry.size && entry.size<=2*1024*1024);text=malloc(entry.size);CHECK(text);
        CHECK(!rf_vpp_read(&archive,&entry,0,text,entry.size));
        CHECK(!scene_clutter_damage_profile_read(text,entry.size,"lantern_box",&profile));CHECK(profile.life==80 && profile.ordinary && profile.break_yellboom && profile.break_radius==.2f);
        CHECK(!scene_clutter_damage_profile_read(text,entry.size,"riot_shield",&profile));CHECK(!profile.ordinary);
        free(text);rf_vpp_close(&archive);
    }
    puts("clutter live damage PASS");return 0;
}
