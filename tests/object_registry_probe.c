#include "rf/object_registry.h"
#include <fcntl.h>
#include <io.h>
static rf_object_registry registry;
int main(void)
{
    uint32_t command[2],out[2];
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
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
