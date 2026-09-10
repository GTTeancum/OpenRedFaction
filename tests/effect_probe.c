#include "rf/effect.h"
#include "rf/particle_pool.h"
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
    if(argc==2 && !strcmp(argv[1],"--particle-emission-parent")) {
        struct {rf_particle_emitter emitter;uint32_t seed,now,empty;rf_particle_emitter_parent parent;uint32_t present;} in;
        struct {int32_t status;uint32_t seed,index;rf_particle_emitter emitter;rf_particle particle;} out;
        static rf_particle particles[RF_PARTICLE_CAPACITY];rf_particle_list lists[6];rf_particle_pool pool;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_random_state rng={in.seed};rf_particle_pool_init(&pool,particles,lists,6);
            if(in.empty)lists[1].next=lists[1].previous=RF_PARTICLE_CAPACITY+1;
            memset(&particles[500],0xa5,sizeof(particles[500]));
            particles[500].next=501;particles[500].previous=RF_PARTICLE_CAPACITY+1;
            memset(&out,0xa5,sizeof(out));out.emitter=in.emitter;
            out.status=rf_particle_emitter_emit_parent(&pool,&out.emitter,1,(int32_t)in.now,in.present?&in.parent:NULL,&rng,&out.index);
            out.seed=rng.value;out.particle=particles[500];
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-emission")) {
        struct {rf_particle_emitter emitter;uint32_t seed,now,empty;} in;
        struct {int32_t status;uint32_t seed,index;rf_particle_emitter emitter;rf_particle particle;} out;
        static rf_particle particles[RF_PARTICLE_CAPACITY];rf_particle_list lists[6];rf_particle_pool pool;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_random_state rng={in.seed};rf_particle_pool_init(&pool,particles,lists,6);
            if(in.empty)lists[1].next=lists[1].previous=RF_PARTICLE_CAPACITY+1;
            memset(&particles[500],0xa5,sizeof(particles[500]));
            particles[500].next=501;particles[500].previous=RF_PARTICLE_CAPACITY+1;
            memset(&out,0xa5,sizeof(out));out.emitter=in.emitter;
            out.status=rf_particle_emitter_emit(&pool,&out.emitter,1,(int32_t)in.now,&rng,&out.index);
            out.seed=rng.value;out.particle=particles[500];
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-render-mode")) {
        uint32_t in[3],out;
        while(fread(in,sizeof(in),1,stdin)==1) {
            out=rf_particle_render_mode(in[0],in[1],in[2]);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-texture-states")) {
        uint32_t in[2];struct {int32_t status;rf_particle_texture_states states;} out;
        _Static_assert(sizeof(out)==152,"Particle texture-state output");
        while(fread(in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_particle_texture_decode(in[0],in[1],&out.states);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-render-states")) {
        struct {uint32_t mode;rf_particle_render_environment environment;rf_particle_render_states states;} in;
        struct {int32_t status;rf_particle_render_states states;} out;
        _Static_assert(sizeof(in)==116,"Particle render-state input");
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.states=in.states;out.status=rf_particle_render_decode(in.mode,&in.environment,&out.states);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-cone-oriented")) {
        struct {float axis[3],cosine_min;rf_random_state random;} in;
        struct {int32_t status;rf_random_state random;float direction[3];} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.random=in.random;
            out.status=rf_particle_cone_oriented(in.axis,in.cosine_min,&out.random,out.direction);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-cone")) {
        struct {float cosine_min;rf_random_state random;} in;
        struct {int32_t status;rf_random_state random;float direction[3];} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.random=in.random;
            out.status=rf_particle_cone_sample(in.cosine_min,&out.random,out.direction);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-vertex-encode")) {
        struct {rf_particle_vertex_environment environment;rf_particle_screen_vertex vertex;} in;
        struct {int32_t status;rf_particle_draw_vertex vertex;} out;
        _Static_assert(sizeof(in)==80 && sizeof(out)==36,"Particle draw vertex fixture layout");
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_particle_vertex_encode(&in.environment,&in.vertex,&out.vertex);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-billboard-project")) {
        struct {rf_particle_projection projection;rf_particle_clip_environment environment;rf_particle_billboard_packet packet;} in;
        struct {int32_t status;rf_particle_screen_polygon polygon;} out;
        _Static_assert(sizeof(in)==148 && sizeof(out)==392,"Particle screen fixture layout");
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_particle_billboard_project(&in.projection,&in.environment,&in.packet,&out.polygon);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-billboard-clip")) {
        struct {rf_particle_clip_environment environment;rf_particle_billboard_packet packet;} in;
        struct {int32_t status;rf_particle_clipped_polygon polygon;} out;
        _Static_assert(sizeof(in)==124 && sizeof(out)==308,"Clipped particle fixture layout");
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_particle_billboard_clip(&in.environment,&in.packet,&out.polygon);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-billboard-prepare")) {
        struct {float center[3],angle,radius;uint32_t width,height;float scale[3];rf_particle_clip_environment clip;} in;
        struct {int32_t status;rf_particle_billboard_packet packet;} out;
        _Static_assert(sizeof(in)==56 && sizeof(out)==112,"Particle prepared billboard fixtures");
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_particle_billboard_prepare(in.center,in.angle,in.radius,in.width,in.height,in.scale,&in.clip,&out.packet);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-project")) {
        struct {rf_particle_projection projection;rf_particle_projected_point point;} in;
        struct {int32_t status;rf_particle_projected_point point;} out;
        _Static_assert(sizeof(in)==52,"Particle projection input");
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.point=in.point;out.status=rf_particle_project(&in.projection,&out.point);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-billboard")) {
        struct {float center[3],angle,radius;uint32_t width,height;float scale[2];} in;
        struct {int32_t status;rf_particle_billboard_vertex vertices[4];} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_particle_billboard_build(in.center,in.angle,in.radius,in.width,in.height,in.scale,out.vertices);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-frame")) {
        rf_particle in;struct {int32_t status;uint32_t frame;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.frame=0xa5a5a5a5;out.status=rf_particle_frame_index(&in,&out.frame);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-step-free")) {
        struct {float dt;rf_particle particle;} in;
        struct {int32_t status;uint32_t live;rf_particle particle;} out;
        static rf_particle records[RF_PARTICLE_CAPACITY];rf_particle_list lists[5];rf_particle_pool pool;
        rf_particle_pool_init(&pool,records,lists,5);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            records[0]=in.particle;pool.live[0]=1;pool.live[1]=0;
            lists[0].next=lists[0].previous=1600;lists[2].next=lists[2].previous=0;
            out.status=rf_particle_pool_step_free(&pool,0,in.dt);out.live=pool.live[0];out.particle=records[0];
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-pool")) {
        static rf_particle records[RF_PARTICLE_CAPACITY];rf_particle_list lists[8];rf_particle_pool pool;
        struct {uint32_t operation,kind,owner,room,emitter,index,seed;rf_particle_spawn spawn;} in;
        struct {int32_t status;uint32_t index,seed,live[2];} out;
        _Static_assert(sizeof(in)==104,"Pool command layout");
        rf_particle_pool_init(&pool,records,lists,8);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_random_state rng={in.seed};out.index=in.index;
            if(in.operation==0)out.status=rf_particle_pool_create(&pool,in.kind,&in.spawn,in.owner,in.room,in.emitter,&rng,&out.index);
            else if(in.operation==1)out.status=rf_particle_pool_detach(&pool,in.emitter);
            else if(in.operation==2)out.status=rf_particle_pool_recycle(&pool,in.index);
            else out.status=in.operation==3?RF_OK:RF_RANGE;
            out.seed=rng.value;out.live[0]=pool.live[0];out.live[1]=pool.live[1];
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
            if(in.operation==3 && (fwrite(records,sizeof(records),1,stdout)!=1 || fwrite(lists,sizeof(lists),1,stdout)!=1))return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-initialize")) {
        struct {rf_particle_spawn spawn;uint32_t pool,owner,room,emitter;rf_random_state random;rf_particle particle;} in;
        struct {int32_t status;rf_random_state random;rf_particle particle;} out;
        _Static_assert(sizeof(rf_particle_spawn)==76,"Particle spawn layout");
        _Static_assert(sizeof(rf_particle)==120,"Particle record layout");
        _Static_assert(sizeof(in)==216,"Particle initialization fixture");
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.random=in.random;out.particle=in.particle;
            out.status=rf_particle_initialize(&in.spawn,in.pool,in.owner,in.room,in.emitter,&out.random,&out.particle);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-tick")) {
        struct {rf_particle_cycle cycle;uint32_t global_enabled;float dt;uint32_t due;rf_random_state random;rf_particle_emitter_clock clock;} in;
        struct {int32_t status;rf_random_state random;rf_particle_emitter_clock clock;rf_particle_emitter_actions actions;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.random=in.random;out.clock=in.clock;
            out.status=rf_particle_emitter_tick(&in.cycle,in.global_enabled,in.dt,in.due,&out.random,&out.clock,&out.actions);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--particle-duration")) {
        struct {rf_particle_cycle cycle;unsigned enabled;rf_random_state random;} in;
        struct {int32_t status;rf_random_state random;float duration;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.random=in.random;
            out.status=rf_particle_cycle_duration(&in.cycle,in.enabled,&out.random,&out.duration);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
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
