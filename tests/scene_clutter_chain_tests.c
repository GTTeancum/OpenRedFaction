#include "../src/diagnostic/scene.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"prop chain line%d\n",__LINE__);return 1;}}while(0)
int main(int argc,char **argv)
{
    rf_vpp tables={0};rf_vpp_entry entry;void *text;rf_vclip_definition clip;
    scene_stream stream={0};rf_geometry_collision_world world={0};
    rf_clutter_base_owner owners[3]={0},*pointers[3]={owners,owners+1,owners+2};
    scene_clutter_damage_profile profiles[2];scene_clutter_damage_binding bindings[3]={0};
    rf_clutter_class classes[2]={0};uint32_t i;
    CHECK(argc==2);CHECK(!rf_vpp_open(&tables,argv[1]));CHECK(!rf_vpp_find(&tables,"clutter.tbl",&entry));
    text=malloc(entry.size);CHECK(text);CHECK(!rf_vpp_read(&tables,&entry,0,text,entry.size));
    CHECK(!scene_clutter_damage_profile_read(text,entry.size,"Oil Drum",profiles));
    CHECK(!scene_clutter_damage_profile_read(text,entry.size,"gas_can",profiles+1));
    CHECK(!rf_vclip_definition_load(&tables,"oil drum explode",65536,&clip));free(text);rf_vpp_close(&tables);
    for(i=0;i<2;i++){profiles[i].break_damage=(float)((double)clip.damage*profiles[i].break_radius*profiles[i].break_damage);
        CHECK(profiles[i].life==19&&profiles[i].break_radius==3&&profiles[i].break_damage==90);classes[i].explosion=1;}
    campaign_clutter_classes.items=classes;campaign_clutter_classes.count=2;
    campaign_clutter_damage_profiles=profiles;campaign_clutter_damage_bindings=bindings;
    campaign_clutter_records.count=3;campaign_clutter_records.items=calloc(3,sizeof(*campaign_clutter_records.items));CHECK(campaign_clutter_records.items);
    campaign_clutter_bodies=pointers;rf_object_registry_init(&campaign_registry);stream.collision=&world;
    for(i=0;i<3;i++){
        owners[i].state.class_index=i==1?1:0;owners[i].state.definition=classes+owners[i].state.class_index;
        owners[i].matrix[0]=owners[i].matrix[4]=owners[i].matrix[8]=1;
        owners[i].state.position[0]=owners[i].body.state.position[0]=(float)i*2;
        campaign_clutter_records.items[i].uid=100+i;
        CHECK(!rf_object_registry_insert(&campaign_registry,&owners[i].state,&owners[i].state.handle));
        CHECK(!scene_clutter_damage_bind(&campaign_registry,owners+i,profiles+owners[i].state.class_index,bindings+i));
    }
    CHECK(!campaign_clutter_firearm_damage(0,20,3));CHECK(bindings[0].break_pending&&owners[1].state.health==19);
    CHECK(!scene_clutter_break_tick(&stream,60));
    for(i=0;i<3;i++)CHECK(owners[i].state.health<=0&&(owners[i].state.flags&2)&&!bindings[i].break_pending);
    CHECK(rf_scene_rocket_blast[0]==2); /* Third prop shares first class's cooldown. */
    CHECK(!scene_clutter_break_tick(&stream,61)&&rf_scene_rocket_blast[0]==2);
    free(campaign_clutter_records.items);
    puts("PASS authored oil/gas damage, two-hop chain, shared cooldown and one-shot deferred break");return 0;
}
