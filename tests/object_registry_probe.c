#include "rf/object_registry.h"
#include <fcntl.h>
#include <io.h>
#include <string.h>
static rf_object_registry registry;
static rf_object_list list;
static rf_object_link nodes[16];
static uint32_t token(const rf_object_link *node)
{
    uint32_t i;if(node==&list.sentinel)return 16;
    for(i=0;i<16;++i)if(node==nodes+i)return i;return UINT32_MAX;
}
static int list_probe(void)
{
    uint32_t command[3],out[36],i;rf_object_list_init(&list);
    while(fread(command,sizeof(command),1,stdin)==1) {
        if(command[0]<2 && command[1]>=16)return 2;
        if(command[0]==0)rf_object_list_append(&list,nodes+command[1]);
        else if(command[0]==1)rf_object_list_remove(&list,nodes+command[1]);
        else if(command[0]==2){list.count=command[1];list.peak=command[2];}
        else return 2;
        out[0]=list.count;out[1]=list.peak;out[2]=token(list.sentinel.next);out[3]=token(list.sentinel.previous);
        for(i=0;i<16;++i){out[4+i*2]=token(nodes[i].next);out[5+i*2]=token(nodes[i].previous);}
        if(fwrite(out,sizeof(out),1,stdout)!=1)return 3;
    }
    return ferror(stdin)?3:0;
}
int main(int argc,char **argv)
{
    uint32_t command[2],out[2];
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    if(argc==2 && !strcmp(argv[1],"--list"))return list_probe();
    rf_object_registry_init(&registry);
    while(fread(command,sizeof(command),1,stdin)==1) {
        out[0]=0;out[1]=0xabcdef01;
        switch(command[0]) {
        case 0:out[0]=(uint32_t)rf_object_registry_insert(&registry,(void *)(uintptr_t)command[1],&out[1]);break;
        case 1:out[0]=(uint32_t)rf_object_registry_remove(&registry,command[1]);break;
        case 2:out[1]=(uint32_t)(uintptr_t)rf_object_registry_lookup(&registry,command[1]);break;
        case 3:registry.generation=command[1];break; /* Boundary fixture only. */
        default:return 2;
        }
        if(fwrite(out,sizeof(out),1,stdout)!=1)return 3;
    }
    return ferror(stdin)?3:0;
}
