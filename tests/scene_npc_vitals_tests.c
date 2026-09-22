#include <stdio.h>
#include <string.h>
#include "../src/diagnostic/scene_npc_vitals.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"NPC authored vitals line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_entity_creation_vitals_class definition={80,30,0x12345678},before_class;
    rf_entity_creation_vitals_state state={0},saved;rf_level_entity_vitals authored={-1,-1};
    state.object_flags=0x6000000;rf_entity_creation_vitals(&state,&definition,0);saved=state;before_class=definition;
    CHECK(!scene_npc_vitals_apply(&authored,&state,&definition)&&!memcmp(&state,&saved,sizeof(state)));
    authored=(rf_level_entity_vitals){25,12};CHECK(!scene_npc_vitals_apply(&authored,&state,&definition));
    CHECK(state.health==25&&state.armor==12&&state.object_flags==saved.object_flags&&state.field_840==saved.field_840);
    authored=(rf_level_entity_vitals){-1,0};CHECK(!scene_npc_vitals_apply(&authored,&state,&definition)&&state.health==25&&state.armor==0);
    authored=(rf_level_entity_vitals){0,-3};CHECK(!scene_npc_vitals_apply(&authored,&state,&definition)&&state.health==0&&state.armor==0);
    authored=(rf_level_entity_vitals){900,800};CHECK(!scene_npc_vitals_apply(&authored,&state,&definition)&&state.health==80&&state.armor==30);
    CHECK(!memcmp(&definition,&before_class,sizeof(definition)));saved=state;
    authored=(rf_level_entity_vitals){10,NAN};CHECK(scene_npc_vitals_apply(&authored,&state,&definition)==RF_FORMAT&&!memcmp(&state,&saved,sizeof(state)));
    authored=(rf_level_entity_vitals){INFINITY,10};CHECK(scene_npc_vitals_apply(&authored,&state,&definition)==RF_FORMAT&&!memcmp(&state,&saved,sizeof(state)));
    definition.health=-5;rf_entity_creation_vitals(&state,&definition,0);CHECK(state.health==100&&(state.object_flags&4));saved=state;
    authored=(rf_level_entity_vitals){-1,-1};CHECK(!scene_npc_vitals_apply(&authored,&state,&definition)&&!memcmp(&state,&saved,sizeof(state)));
    authored.health=-2;CHECK(!scene_npc_vitals_apply(&authored,&state,&definition)&&state.health==0&&(state.object_flags&4));
    authored.health=10;CHECK(!scene_npc_vitals_apply(&authored,&state,&definition)&&state.health==-5&&(state.object_flags&4));
    CHECK(state.field_840==0x12345678);
    puts("PASS authored NPC vitals exact sentinel, finite overrides, lower-first class clamp and preserved factory flags");return 0;
}
