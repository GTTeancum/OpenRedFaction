#include "rf/weapon.h"
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>
int main(int argc,char **argv)
{
    struct { int32_t weapon; rf_weapon_reset_state state; rf_weapon_descriptor descriptors[64];
        rf_weapon_reset_context context; rf_motion_playback_state playback;
        rf_motion_playback_resource resources[32]; } input;
    int32_t status; unsigned i;
    _Static_assert(sizeof(input)==2284,"Weapon reset wire layout");
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    if (argc==2 && !strcmp(argv[1],"--current")) {
        struct { int32_t handle; uint32_t local;
            struct { int32_t slot,handle,type,kind,linked,weapon; } nodes[2]; } data;
        _Static_assert(sizeof(data)==56,"Current weapon fixture layout");
        while (fread(&data,sizeof(data),1,stdin)==1) {
            rf_entity_registry registry={0}; rf_entity_view nodes[2]={0};
            int32_t output[2]={0,-99};
            for (i=0;i<2;++i) {
                nodes[i].handle=data.nodes[i].handle; nodes[i].type=data.nodes[i].type;
                nodes[i].class_type=data.nodes[i].kind; nodes[i].linked_handle=data.nodes[i].linked;
                nodes[i].weapons[0]=data.nodes[i].weapon;
                if (data.nodes[i].slot>=0 && data.nodes[i].slot<1024) registry.slots[data.nodes[i].slot]=&nodes[i];
            }
            output[0]=rf_weapon_current(&registry,data.handle,data.local,NULL,NULL,&output[1]);
            if (fwrite(output,sizeof(output),1,stdout)!=1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    if (argc==2 && !strcmp(argv[1],"--selection")) {
        struct { rf_weapon_selection_state state; rf_weapon_selection_input input;
            uint8_t owned[64]; uint32_t flags[64],count; } data;
        _Static_assert(sizeof(data)==380,"Selection fixture layout");
        while (fread(&data,sizeof(data),1,stdin)==1) {
            status=rf_weapon_finish_selection(&data.state,&data.input,data.owned,data.flags,data.count,NULL,NULL);
            if (fwrite(&status,4,1,stdout)!=1 || fwrite(&data.state,sizeof(data.state),1,stdout)!=1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    if (argc==2 && !strcmp(argv[1],"--queue")) {
        struct { rf_weapon_selection_state state; int32_t weapon; } data;
        _Static_assert(sizeof(data)==20,"Queue fixture layout");
        while (fread(&data,sizeof(data),1,stdin)==1) {
            status=rf_weapon_queue_selection(&data.state,data.weapon);
            if (fwrite(&status,4,1,stdout)!=1 || fwrite(&data.state,sizeof(data.state),1,stdout)!=1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    if (argc==2 && !strcmp(argv[1],"--empty")) {
        struct { rf_weapon_inventory primary,linked; rf_weapon_supply supply[64]; uint32_t flags[64],count;
            int32_t preference[32]; rf_weapon_empty_input input; } data;
        _Static_assert(sizeof(data)==2108,"Empty weapon fixture layout");
        while (fread(&data,sizeof(data),1,stdin)==1) {
            struct { int32_t status; rf_weapon_empty_action action; } output={0,{-99,-99}};
            output.status=rf_weapon_decide_empty(&data.primary,&data.linked,data.supply,data.flags,data.count,
                data.preference,&data.input,&output.action);
            if (fwrite(&output,sizeof(output),1,stdout)!=1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    if (argc==2 && !strcmp(argv[1],"--inventory")) {
        struct { uint32_t present,defer_flag; int32_t weapon;
            rf_weapon_inventory inventory; rf_weapon_supply supply[64]; int32_t preference[32]; } data;
        _Static_assert(sizeof(data)==1356,"Inventory fixture layout");
        while (fread(&data,sizeof(data),1,stdin)==1) {
            int32_t output[4]={0,-99,0,-99};
            output[0]=rf_weapon_reserve(data.present ? &data.inventory : NULL,data.supply,data.weapon,&output[1]);
            output[2]=rf_weapon_choose_available(data.present ? &data.inventory : NULL,data.supply,data.preference,data.defer_flag,&output[3]);
            if (fwrite(output,sizeof(output),1,stdout)!=1) return 1;
        }
        return ferror(stdin) ? 1 : 0;
    }
    while (fread(&input,sizeof(input),1,stdin)==1) {
        status=rf_weapon_reset(&input.state,input.weapon,input.descriptors,&input.context,
            &input.playback,input.resources,32,NULL,NULL);
        if (fwrite(&status,4,1,stdout)!=1 || fwrite(&input.state,sizeof(input.state),1,stdout)!=1 ||
            fwrite(&input.playback,sizeof(input.playback),1,stdout)!=1) return 1;
        for (i=0;i<32;++i) if (fwrite(&input.resources[i].references,4,1,stdout)!=1) return 1;
    }
    return ferror(stdin) ? 1 : 0;
}
