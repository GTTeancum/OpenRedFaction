#include "rf/effect.h"
#include "rf/entity_assets.h"
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>
#include <stdlib.h>
int main(int argc,char **argv)
{
    struct { int32_t index; uint32_t override_mode; int32_t enabled,now;
        rf_effect_switch objects[4]; int32_t slots[4]; } input;
    rf_effect_pair pair; unsigned i; int32_t status;
    _Static_assert(sizeof(input)==64,"Effect fixture layout");
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    if(argc==2 && !strcmp(argv[1],"--explosion-clock")) {
        struct {rf_explosion_recipe recipe;float dt;rf_explosion_clock clock;} in;
        struct {int32_t status;rf_explosion_clock clock;rf_explosion_clock_actions actions;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.clock=in.clock;
            out.status=rf_explosion_clock_tick(&in.recipe,in.dt,&out.clock,&out.actions);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--explosion-scale")) {
        struct {rf_explosion_definition definition;uint32_t slot;float size;} in;
        struct {int32_t status;rf_particle_definition particle;float extent;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_explosion_central_prepare(&in.definition,in.slot,in.size,&out.particle,&out.extent);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==5 && !strcmp(argv[1],"--explosion-definition")) {
        rf_vpp archive;rf_explosion_definition definition;
        _Static_assert(sizeof(definition)==2380,"Explosion definition fixture layout");
        memset(&definition,0xa5,sizeof(definition));status=rf_vpp_open(&archive,argv[2]);
        if(!status) {status=rf_explosion_definition_load(&archive,argv[3],(uint32_t)strtoul(argv[4],NULL,10),&definition);rf_vpp_close(&archive);}
        return fwrite(&status,4,1,stdout)==1 && fwrite(&definition,sizeof(definition),1,stdout)==1?0:1;
    }
    if(argc==5 && !strcmp(argv[1],"--explosion-load")) {
        rf_vpp archive;rf_explosion_recipe recipe;
        _Static_assert(sizeof(recipe)==712,"Explosion recipe fixture layout");
        memset(&recipe,0xa5,sizeof(recipe));status=rf_vpp_open(&archive,argv[2]);
        if(!status) {status=rf_explosion_recipe_load(&archive,argv[3],(uint32_t)strtoul(argv[4],NULL,10),&recipe);rf_vpp_close(&archive);}
        return fwrite(&status,4,1,stdout)==1 && fwrite(&recipe,sizeof(recipe),1,stdout)==1?0:1;
    }
    if(argc==5 && !strcmp(argv[1],"--vclip-load")) {
        rf_vpp archive;rf_vclip_definition definition;
        _Static_assert(sizeof(definition)==500,"Vclip fixture layout");
        memset(&definition,0xa5,sizeof(definition));status=rf_vpp_open(&archive,argv[2]);
        if(!status) {status=rf_vclip_definition_load(&archive,argv[3],(uint32_t)strtoul(argv[4],NULL,10),&definition);rf_vpp_close(&archive);}
        return fwrite(&status,4,1,stdout)==1 && fwrite(&definition,sizeof(definition),1,stdout)==1?0:1;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-prepare")) {
        rf_particle_definition definition;
        while(fread(&definition,sizeof(definition),1,stdin)==1) {
            if(rf_particle_definition_prepare(&definition,&definition))return 3;
            if(fwrite(&definition,sizeof(definition),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==5 && !strcmp(argv[1],"--emitter-load")) {
        rf_vpp archive;rf_particle_definition definition;
        memset(&definition,0xa5,sizeof(definition));status=rf_vpp_open(&archive,argv[2]);
        if(!status) {status=rf_emitter_definition_load(&archive,argv[3],(uint32_t)strtoul(argv[4],NULL,10),&definition);rf_vpp_close(&archive);}
        return fwrite(&status,4,1,stdout)==1 && fwrite(&definition,sizeof(definition),1,stdout)==1?0:1;
    }
    if(argc==2 && (!strcmp(argv[1],"--particle-definition") || !strcmp(argv[1],"--emitter-definition"))) {
        uint32_t bytes;rf_particle_definition definition;
        int named=!strcmp(argv[1],"--emitter-definition");char name[128];
        _Static_assert(sizeof(definition)==184,"Particle definition fixture layout");
        while(fread(&bytes,4,1,stdin)==1) {
            if(named && (fread(name,1,128,stdin)!=128 || !memchr(name,0,128)))return 2;
            void *text;if(bytes>65536)return 2;text=malloc(bytes?bytes:1);if(!text)return 2;
            if(fread(text,1,bytes,stdin)!=bytes){free(text);return 2;}
            memset(&definition,0xa5,sizeof(definition));
            status=named?rf_emitter_definition_read(text,bytes,name,&definition):rf_particle_definition_read(text,bytes,&definition);free(text);
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(&definition,sizeof(definition),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
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
