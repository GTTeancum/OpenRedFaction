#include "rf/effect.h"
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>
int main(int argc,char **argv)
{
    struct { int32_t index; uint32_t override_mode; int32_t enabled,now;
        rf_effect_switch objects[4]; int32_t slots[4]; } input;
    rf_effect_pair pair; unsigned i; int32_t status;
    _Static_assert(sizeof(input)==64,"Effect fixture layout");
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    if(argc==2 && !strcmp(argv[1],"--particle-cycle")) {
        struct {rf_particle_text_flags flags;unsigned initially_on,alternate;rf_particle_cycle cycle;} c;
        _Static_assert(sizeof(c)==36,"Particle cycle fixture layout");
        while(fread(&c,sizeof(c),1,stdin)==1) {
            if(rf_particle_cycle_read(&c.flags,c.initially_on,c.alternate,&c.cycle,&c.cycle))return 3;
            if(fwrite(&c.flags,sizeof(c.flags),1,stdout)!=1 || fwrite(&c.cycle,sizeof(c.cycle),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-pack")) {
        struct {rf_particle_text_flags flags;unsigned present;int values[4];} pack;
        _Static_assert(sizeof(pack)==32,"Particle packing fixture layout");
        while(fread(&pack,sizeof(pack),1,stdin)==1) {
            if(rf_particle_flags_pack(&pack.flags,pack.present,pack.values))return 3;
            if(fwrite(&pack.flags,sizeof(pack.flags),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-flags")) {
        char strings[2][512];rf_particle_text_flags flags;
        while(fread(strings,sizeof(strings),1,stdin)==1) {
            if(!memchr(strings[0],0,512) || !memchr(strings[1],0,512))return 2;
            if(rf_particle_flags_read(strings[0],strings[1],&flags))return 3;
            if(fwrite(&flags,sizeof(flags),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--vclip-lookup")) {
        char strings[65][128];const char *names[64];
        while(fread(strings,sizeof(strings),1,stdin)==1) {
            for(i=0;i<65;++i)if(!memchr(strings[i],0,128))return 2;
            for(i=0;i<64;++i)names[i]=strings[i];
            status=rf_vclip_name_lookup(names,strings[64]);
            if(fwrite(&status,4,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    while (fread(&input,sizeof(input),1,stdin)==1) {
        for (i=0;i<4;++i) pair.objects[i/2][i%2]=input.slots[i]>=0 && input.slots[i]<4 ? &input.objects[input.slots[i]] : NULL;
        status=rf_effect_set_enabled(&pair,1,input.index,input.override_mode,input.enabled,input.now);
        if (fwrite(&status,4,1,stdout)!=1 || fwrite(input.objects,sizeof(input.objects),1,stdout)!=1) return 1;
    }
    return ferror(stdin) ? 1 : 0;
}
