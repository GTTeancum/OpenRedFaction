#include "rf/entity.h"
#include "rf/player.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
static rf_entity_room_result room_result;
static uint32_t room_queries,room_notices,room_notice_kind;
static int room_locate(void *context,const float position[3],rf_entity_room_result *result)
{(void)context;(void)position;++room_queries;*result=room_result;return RF_OK;}
static void room_notify(void *context,const char *name)
{(void)context;++room_notices;room_notice_kind=!strcmp(name,"underwater")?2:1;}
int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--player-spawn")) {
        struct {rf_player_spawn_state player;int32_t local,count;
            rf_player_position_override override;rf_player_spawn_request request;} in;
        _Static_assert(sizeof(in)==48,"Player spawn wire input");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            if(rf_player_spawn_prepare(&in.player,in.local,in.count,&in.override,&in.request))return 3;
            if(fwrite(&in,sizeof(in),1,stdout)!=1)return 4;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--room-refresh")) {
        struct {rf_entity_room_state state;float position[3];uint32_t local,room,liquid;float minimum_y,depth;} in;
        uint32_t out[8];
        _Static_assert(sizeof(in)==52,"Room wire input");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            room_queries=room_notices=room_notice_kind=0;
            room_result.room=in.room;room_result.liquid=in.liquid;room_result.minimum_y=in.minimum_y;
            room_result.liquid_depth=in.depth;room_result.name="test-room";
            if(rf_entity_room_refresh(&in.state,in.position,in.local,room_locate,room_notify,0))return 3;
            memcpy(out,&in.state,20);out[5]=room_queries;out[6]=room_notices;out[7]=room_notice_kind;
            if(fwrite(out,sizeof(out),1,stdout)!=1)return 4;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--sphere-overrides")) {
        rf_entity_class_sphere spheres[8];rf_entity_sphere_override overrides[8];uint32_t header[3];
        _Static_assert(sizeof(*spheres)==64 && sizeof(*overrides)==44,"Sphere override wire layout");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(header,sizeof(header),1,stdin)==1) {
            if(header[0]>8 || header[1]>8 || fread(spheres,64,header[0],stdin)!=header[0] || fread(overrides,44,header[1],stdin)!=header[1])return 2;
            if(rf_entity_sphere_overrides(spheres,header[0],overrides,header[1],(uint8_t)header[2]))return 3;
            if(fwrite(spheres,64,header[0],stdout)!=header[0])return 4;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--physics-flags")) {
        uint32_t in[5],out;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(in,sizeof(in),1,stdin)==1) {
            out=rf_entity_creation_physics_flags(in[0],in[1],in[2],in[3],(uint8_t)in[4]);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--creation-flags")) {
        uint32_t in[2],out;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(in,sizeof(in),1,stdin)==1) {
            out=rf_entity_creation_object_flags(in[0],in[1]);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    struct { int32_t target,query,attached[8]; struct {
        int32_t slot,handle,type,kind; uint32_t flags[3];
        int32_t action,linked,weapons[2]; float speed;
        int32_t owner,occupants[3];
    } nodes[8]; } input;
    rf_entity_registry registry; rf_entity_view nodes[8];
    int32_t output[6]; unsigned i;
    _Static_assert(sizeof(input)==552,"Entity predicate fixture layout");
    _setmode(_fileno(stdin),_O_BINARY); _setmode(_fileno(stdout),_O_BINARY);
    while (fread(&input,sizeof(input),1,stdin)==1) {
        const rf_entity_view *p;
        memset(&registry,0,sizeof(registry)); memset(nodes,0,sizeof(nodes));
        for (i=0;i<8;++i) {
            nodes[i].handle=input.nodes[i].handle; nodes[i].type=input.nodes[i].type; nodes[i].class_type=input.nodes[i].kind;
            nodes[i].flags_7c=input.nodes[i].flags[0]; nodes[i].flags_810=input.nodes[i].flags[1]; nodes[i].flags_7d0=input.nodes[i].flags[2];
            nodes[i].action_520=input.nodes[i].action; nodes[i].linked_handle=input.nodes[i].linked;
            memcpy(nodes[i].weapons,input.nodes[i].weapons,8); nodes[i].base_speed=input.nodes[i].speed;
            nodes[i].weapon_owner=input.nodes[i].owner>=0 && input.nodes[i].owner<8 ? &nodes[input.nodes[i].owner] : 0;
            nodes[i].occupants=input.nodes[i].occupants; nodes[i].occupant_count=3;
            if (input.nodes[i].slot>=0 && input.nodes[i].slot<RF_OBJECT_SLOTS) registry.slots[input.nodes[i].slot]=&nodes[i];
        }
        p=rf_object_lookup(&registry,input.query); output[0]=p ? (int32_t)(p-nodes) : -1;
        p=rf_entity_lookup(&registry,input.query); output[1]=p ? (int32_t)(p-nodes) : -1;
        p=input.target>=0 && input.target<8 ? &nodes[input.target] : 0;
        output[3]=output[4]=-99;
        output[2]=rf_entity_combat_predicates(&registry,p,input.attached,8,&output[3],&output[4]);
        output[5]=-99;
        if (rf_entity_has_weapon(&registry,p,&output[5])!=RF_OK) output[5]=-2;
        if (fwrite(output,sizeof(output),1,stdout)!=1) return 1;
    }
    return ferror(stdin) ? 1 : 0;
}
