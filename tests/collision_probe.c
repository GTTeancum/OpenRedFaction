#include "campaign_particle_fixture.h"
#include "rf/event.h"
#include "rf/object_registry.h"
#include "rf/collision.h"
#include "rf/geometry.h"
#include "rf/preview.h"
#include "rf/material.h"
#include "rf/level_particles.h"
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>
#include <stdlib.h>
#include "burn_resolved_probe.h"
static int32_t group_sound_probe(void *context,int32_t sample,const float position[3],float volume,uint32_t flags)
{
    uint32_t *trace=context,words[6],i;memcpy(words,&sample,4);memcpy(words+1,position,12);
    memcpy(words+4,&volume,4);words[5]=flags;
    ++trace[0];for(i=0;i<6;i++)trace[1]=(trace[1]^words[i])*16777619u;
    return sample==-1?-1:(int32_t)((uint32_t)sample^0x12340000u);
}
typedef struct activation_trace {uint32_t *source,count,hash;} activation_trace;
static void activation_record(void *context,uint32_t kind,uint32_t value,const float position[3],float scalar)
{
    activation_trace *t=context;uint32_t words[7]={kind,value,*t->source},i;
    memcpy(words+3,position,12);memcpy(words+6,&scalar,4);++t->count;
    for(i=0;i<7;i++)t->hash=(t->hash^words[i])*16777619u;
}
static int32_t activation_play(void *context,int32_t sample,const float position[3],float volume,uint32_t flags)
{ if(flags)abort();activation_record(context,1,(uint32_t)sample,position,volume);return sample+100; }
static void activation_alert(void *context,uint32_t actor,const float position[3],float radius)
{activation_record(context,2,actor,position,radius);}
typedef struct body_fixture {
    float heights[3];uint32_t enabled[3],flags,spheres;
} body_fixture;
typedef struct trigger_link_trace {uint32_t count,words[16];} trigger_link_trace;
static int trigger_link_record(void *context,uint32_t kind,uint32_t handle,uint32_t source,uint32_t actor)
{
    trigger_link_trace *t=context;uint32_t *p;
    if(t->count==4)return RF_RANGE;p=t->words+4*t->count++;p[0]=kind;p[1]=handle;p[2]=source;p[3]=actor;return RF_OK;
}
typedef struct body_fixture_context {body_fixture input;uint32_t count;rf_collision_body_request trace[6];} body_fixture_context;
static int body_fixture_geometry(void *context,const rf_collision_body_request *q,rf_collision_body_candidate *out,uint32_t *matched)
{
    body_fixture_context *c=context;uint32_t i=q->solid==UINT32_MAX?2:q->solid;
    float z=c->input.heights[i],vertices[4][3]={{-3,-3,0},{3,-3,0},{3,3,0},{-3,3,0}};
    rf_collision_face face={0};rf_collision_sweep_tree_hit hit;int status;uint32_t j;
    if(c->count>=6)return RF_RANGE;c->trace[c->count++]=*q;
    for(j=0;j<4;j++)vertices[j][2]=z;
    face.plane[2]=1;face.plane[3]=-z;
    face.minimum[0]=face.minimum[1]=-3;face.maximum[0]=face.maximum[1]=3;
    face.minimum[2]=z-.0001f;face.maximum[2]=z+.0001f;face.vertices=vertices;face.count=4;
    status=rf_collision_flat_faces(&face,c->input.enabled[i],q->flags,q->start,q->delta,NULL,NULL,q->radius,q->limit,&hit,matched);
    if(!status && *matched) {out->hit=hit.hit;out->texture=UINT32_MAX;out->material=0;out->face_flags=0;out->face_token=i+1;}
    return status;
}
static int body_fixture_metadata(void *context,uint32_t solid,uint32_t face,uint32_t *texture,uint32_t *material)
{
    uint32_t *calls=context;++*calls;
    *texture=(solid==UINT32_MAX?2000:1000+solid)+face;*material=3000+face;return RF_OK;
}
static int geometry_body_fixture_run(const body_fixture *input,rf_geometry_body_hit *result,uint32_t *matched,uint32_t *calls)
{
    rf_geometry_collision_world world={0};rf_geometry_collision_room room={0};rf_collision_room_view room_view={0};
    rf_geometry_collision_movers movers={0};rf_geometry_collision_flat flat[2]={{0}};
    rf_collision_solid_view views[2]={{0}};rf_group_attached_pose poses[2]={{0}};
    rf_collision_face faces[3]={{0}};float vertices[3][4][3];rf_collision_body_mover scratch[2];
    rf_collision_body_query q={0};rf_collision_body_sphere spheres[2]={{{0,0,0},.5f},{{1,0,0},.5f}};
    uint32_t i,j,primary=0;int status;
    for(i=0;i<3;i++) {
        float z=input->heights[i];float v[4][3]={{-3,-3,0},{3,-3,0},{3,3,0},{-3,3,0}};
        memcpy(vertices[i],v,sizeof(v));for(j=0;j<4;j++)vertices[i][j][2]=z;
        faces[i].plane[2]=1;faces[i].plane[3]=-z;faces[i].minimum[0]=faces[i].minimum[1]=-3;
        faces[i].maximum[0]=faces[i].maximum[1]=3;faces[i].minimum[2]=z-.0001f;faces[i].maximum[2]=z+.0001f;
        faces[i].vertices=vertices[i];faces[i].count=4;
        if(i<2) {
            flat[i].faces=faces+i;flat[i].count=input->enabled[i];views[i].object_id=100+i;
            memcpy(poses[i].minimum,faces[i].minimum,12);memcpy(poses[i].maximum,faces[i].maximum,12);
            poses[i].flags=i?0:input->flags;
            /* Deliberately different ray poses: body collision must ignore them. */
            for(j=0;j<3;j++){poses[i].input_matrix[j*4]=1;poses[i].public_position[j]=50;poses[i].velocity[j]=(float)(j+1+i);}
        }
    }
    status=rf_collision_tree_open(faces+2,input->enabled[2],65536,&room.tree);if(status)return status;
    if(input->enabled[2])room.tree.source_indices[0]=37;
    room_view.tree=&room.tree;memcpy(room_view.minimum,faces[2].minimum,12);memcpy(room_view.maximum,faces[2].maximum,12);
    world.rooms=&room;world.views=&room_view;world.room_count=1;world.primary=&primary;world.primary_count=1;
    movers.count=2;movers.owned=flat;movers.views=views;movers.poses=poses;
    q.start[2]=8;q.end[2]=-8;q.radius=.5f;q.limit=1;q.flags=0x460;q.spheres=spheres;q.count=input->spheres;
    for(i=0;i<3;i++)q.matrix[i][i]=1;
    status=rf_geometry_collision_body_sweep(&world,&movers,&q,scratch,2,body_fixture_metadata,calls,result,matched);
    rf_collision_tree_close(&room.tree);return status;
}
int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--group-ramp")) {
        struct {rf_group_motion_state motion;float elapsed,acceleration,deceleration,dt;} input;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            float scale=123;int status=rf_group_rotation_ramp(&input.motion,&input.elapsed,input.acceleration,input.deceleration,input.dt,&scale);
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(&input,28,1,stdout)!=1 || fwrite(&scale,4,1,stdout)!=1)return 141;
        }
        return ferror(stdin)?142:0;
    }
    if(argc==2 && !strcmp(argv[1],"--group-stop")) {
        struct {rf_group_motion_state motion;float speed,control;} input;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            int status=rf_group_motion_stop(&input.motion,&input.speed,&input.control);
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(&input,sizeof(input),1,stdout)!=1)return 139;
        }
        return ferror(stdin)?140:0;
    }
    struct {float lo[3],hi[3],start[3],end[3],point[3];} input;
    struct {int32_t status;uint32_t hit;float point[3];} output;
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    if(argc==2 && !strcmp(argv[1],"--entity-registration")) {
        rf_object_registry objects,before;rf_entity_registry entities={0};rf_entity_view views[1025]={{0}};
        rf_registered_entity_view wrappers[1025]={{0}},duplicate={0};uint32_t i,handle;
        rf_object_registry_init(&objects);
        for(i=0;i<1024;i++) {
            if(rf_entity_view_register(&objects,&entities,views+i,wrappers+i))return 3;
            if(wrappers[i].handle!=((i+1)<<16|i) || rf_entity_lookup(&entities,views[i].handle)!=views+i ||
               rf_object_registry_lookup(&objects,wrappers[i].handle)!=wrappers+i)return 4;
        }
        before=objects;
        if(rf_entity_view_register(&objects,&entities,views+1024,wrappers+1024)!=RF_RANGE || memcmp(&objects,&before,sizeof(objects)))return 5;
        for(i=0;i<1024;i++)if(rf_entity_view_unregister(&objects,&entities,wrappers+i) || views[i].handle!=-1)return 6;
        if(rf_entity_view_register(&objects,&entities,views,wrappers))return 7;
        before=objects;handle=wrappers[0].handle;
        if(rf_entity_view_register(&objects,&entities,views,&duplicate)!=RF_RANGE || memcmp(&objects,&before,sizeof(objects)))return 8;
        views[0].handle^=0x10000;
        if(rf_entity_view_unregister(&objects,&entities,wrappers)!=RF_NOT_FOUND || rf_object_registry_lookup(&objects,handle)!=wrappers)return 9;
        views[0].handle=(int32_t)handle;if(rf_entity_view_unregister(&objects,&entities,wrappers))return 10;
        puts("PASS 1025 registrations, exhaustion, duplicate and stale guards");return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--trigger-links")) {
        uint32_t input[5];
        while(fread(input,sizeof(input),1,stdin)==1) {
            rf_object_registry registry;rf_event_links links;uint32_t handles[4],i;
            struct {int status;trigger_link_trace trace;} output={0};rf_object_registry_init(&registry);
            for(i=0;i<4;i++){if(rf_object_registry_insert(&registry,input+i,handles+i))return 3;}
            links.count=4;links.handles=handles;
            output.status=rf_trigger_links_dispatch(&registry,&links,0x23450020,0x34560021,input[4],trigger_link_record,&output.trace);
            fwrite(&output,sizeof(output),1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--body-surface")) {
        struct {uint32_t solid,face,texture,slot;char name[64];} in;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            unsigned char data[160]={0};uint32_t texture_offset=64,face_offset=0,offsets[4]={0,1,2,3},slots[3]={4,2,7};
            rf_geometry g={0};const rf_geometry *sources[3]={&g,&g,&g};rf_geometry_materials mapping={0};
            rf_surface_materials palette={0};rf_geometry_body_surfaces c={sources,3,&mapping,&palette};
            struct {int status;uint32_t texture,material;} out={0,0xa5a5a5a5,0xa5a5a5a5};
            size_t len;in.name[63]=0;len=strlen(in.name);data[64]=(unsigned char)len;memcpy(data+66,in.name,len);
            memcpy(data+16,&in.texture,4);g.data=data;g.faces=g.textures=1;g.face_offsets=&face_offset;g.texture_offsets=&texture_offset;
            mapping.count=3;mapping.offsets=offsets;mapping.slots=slots;mapping.textures.count=8;
            if(in.slot)slots[0]=slots[1]=slots[2]=in.slot;
            palette.count=3;strcpy(palette.prefixes[0].name,"rock");palette.prefixes[0].material=1;
            strcpy(palette.prefixes[1].name,"metal");palette.prefixes[1].material=2;
            strcpy(palette.prefixes[2].name,"ice");palette.prefixes[2].material=8;
            out.status=rf_geometry_body_surface(&c,in.solid,in.face,&out.texture,&out.material);fwrite(&out,sizeof(out),1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--geometry-body-sweep")) {
        body_fixture fixture;
        while(fread(&fixture,sizeof(fixture),1,stdin)==1) {
            struct {int status;uint32_t matched;rf_geometry_body_hit hit;uint32_t calls;} out;
            memset(&out,0xa5,sizeof(out));out.calls=0;
            out.status=geometry_body_fixture_run(&fixture,&out.hit,&out.matched,&out.calls);
            fwrite(&out,sizeof(out),1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--body-sweep")) {
        body_fixture_context c;
        while(fread(&c.input,sizeof(c.input),1,stdin)==1) {
            rf_collision_body_query q={0};rf_collision_body_mover movers[2]={{0}};
            rf_collision_body_sphere spheres[2]={{{0,0,0},.5f},{{1,0,0},.5f}};
            struct {int status;uint32_t matched;rf_collision_body_hit hit;uint32_t count;rf_collision_body_request trace[6];} out;
            uint32_t i,j;memset(&out,0xa5,sizeof(out));c.count=0;memset(c.trace,0,sizeof(c.trace));
            q.start[2]=8;q.end[2]=-8;q.radius=.5f;q.limit=1;q.flags=0x460;q.spheres=spheres;q.count=c.input.spheres;
            for(i=0;i<3;i++)q.matrix[i][i]=1;
            for(i=0;i<2;i++) {
                movers[i].minimum[0]=movers[i].minimum[1]=-3;movers[i].maximum[0]=movers[i].maximum[1]=3;
                movers[i].minimum[2]=c.input.heights[i]-.0001f;movers[i].maximum[2]=c.input.heights[i]+.0001f;
                movers[i].object_id=100+i;movers[i].flags=i?0:c.input.flags;
                for(j=0;j<3;j++)movers[i].matrix[j][j]=1;
            }
            out.hit.fraction=1;
            out.status=rf_collision_body_sweep(&q,movers,2,body_fixture_geometry,&c,&out.hit,&out.matched);
            out.count=c.count;memcpy(out.trace,c.trace,sizeof(c.trace));fwrite(&out,sizeof(out),1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--oriented-box")) {
        float values[24];
        while(fread(values,sizeof(values),1,stdin)==1) {
            output.hit=0xa5a5a5a5;memcpy(output.point,values+21,12);
            output.status=rf_collision_segment_oriented_box(values,(const float (*)[3])(values+3),
                values+12,values+15,values+18,output.point,&output.hit);
            fwrite(&output,sizeof(output),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==4 && !strcmp(argv[1],"--level-emitters")) {
        rf_vpp archive;rf_level level;rf_level_emitter_reader reader;rf_level_emitter emitter;int status;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]))return 3;
        status=rf_level_emitters_begin(&level,&reader);if(status)return 4;
        while((status=rf_level_emitter_next(&reader,&emitter))==RF_OK) {
            rf_level_emitter_reader truncated=reader,saved;rf_level_emitter guard,original;
            truncated.cursor=emitter.offset;truncated.index--;truncated.section.size=emitter.offset+emitter.bytes-1;saved=truncated;
            memset(&guard,0xa5,sizeof(guard));original=guard;
            if(rf_level_emitter_next(&truncated,&guard)!=RF_FORMAT || memcmp(&truncated,&saved,sizeof(saved)) || memcmp(&guard,&original,sizeof(guard)))return 5;
            if(fwrite(&emitter,sizeof(emitter),1,stdout)!=1)return 6;
        }
        rf_vpp_close(&archive);return status==RF_NOT_FOUND?0:7;
    }
    if(argc==5 && !strcmp(argv[1],"--level-emitter-bind")) {
        rf_vpp archive,maps[4];rf_level level;rf_geometry geometry;rf_geometry_collision_world world={0};
        rf_level_emitter_reader reader;rf_level_emitter emitter;uint32_t i;int status;char path[1024];
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]) ||
           rf_geometry_open(&geometry,&level,8u*1024u*1024u))return 3;
        if(rf_geometry_collision_world_open(&geometry,8u*1024u*1024u,&world))return 4;
        rf_geometry_close(&geometry);
        for(i=0;i<4;i++) {
            if(snprintf(path,sizeof(path),"%s/maps%u.vpp",argv[4],i+1)<0 || rf_vpp_open(&maps[i],path))return 5;
        }
        status=rf_level_emitters_begin(&level,&reader);if(status)return 6;
        while((status=rf_level_emitter_next(&reader,&emitter))==RF_OK) {
            struct {uint32_t uid;int32_t room_status,image_status;rf_collision_room_location location;
                uint32_t width,height,resident,format,frames,archive;} out={0};
            rf_particle_definition definition={0};rf_particle_bitmap bitmap={0};
            if(strlen(emitter.bitmap)>=sizeof(definition.bitmap))return 7;
            strcpy(definition.bitmap,emitter.bitmap);out.uid=emitter.uid;
            out.room_status=rf_geometry_collision_world_locate(&world,emitter.position,&out.location);
            out.image_status=rf_particle_bitmap_open(&bitmap,&definition,maps,4,0,1024u*1024u);
            if(!out.image_status) {
                out.width=bitmap.image.width;out.height=bitmap.image.height;out.resident=bitmap.resident_bytes;
                out.format=bitmap.image.source_format;out.frames=bitmap.frames;out.archive=bitmap.archive_index;
            }
            rf_particle_bitmap_close(&bitmap);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 8;
        }
        for(i=0;i<4;i++)rf_vpp_close(&maps[i]);rf_geometry_collision_world_close(&world);rf_vpp_close(&archive);
        return status==RF_NOT_FOUND?0:9;
    }
    if(argc==6 && !strcmp(argv[1],"--level-emitter-materials")) {
        rf_vpp archive,maps[4];rf_level level;rf_level_particle_materials materials={0},empty={0};
        uint32_t i,budget=(uint32_t)strtoul(argv[5],NULL,10);int status;char path[1024];
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]))return 3;
        for(i=0;i<4;i++) {
            if(snprintf(path,sizeof(path),"%s/maps%u.vpp",argv[4],i+1)<0 || rf_vpp_open(&maps[i],path))return 4;
        }
        status=rf_level_particle_materials_open(&materials,&level,maps,4,budget);
        for(i=0;i<4;i++)rf_vpp_close(&maps[i]);rf_vpp_close(&archive);
        fwrite(&status,4,1,stdout);
        if(status) {if(memcmp(&materials,&empty,sizeof(empty)))return 5;return 0;}
        fwrite(&materials.count,4,1,stdout);fwrite(&materials.texture_count,4,1,stdout);fwrite(&materials.resident_bytes,4,1,stdout);
        fwrite(materials.bindings,sizeof(*materials.bindings),materials.count,stdout);
        {
            uint32_t sizes[3]={sizeof(materials),sizeof(rf_level_particle_texture),sizeof(rf_image)};
            fwrite(sizes,sizeof(sizes),1,stdout);
        }
        for(i=0;i<materials.texture_count;i++) {
            rf_level_particle_texture *texture=&materials.textures[i];uint32_t f;
            fwrite(texture->name,64,1,stdout);
            fwrite(&texture->animation.count,4,1,stdout);
            for(f=0;f<texture->animation.count;f++) {
                rf_image *image=texture->animation.images+f;
                uint32_t data[5]={image->width,image->height,image->bytes,texture->animation.archive_index,2166136261u},x,y,k;
                for(y=0;y<image->height;y++)for(x=0;x<image->width;x++) {
                    const unsigned char *pixel=rf_image_pixel(image,x,y);
                    for(k=0;k<4;k++){data[4]^=pixel[k];data[4]*=16777619u;}
                }
                fwrite(data,sizeof(data),1,stdout);
            }
        }
        rf_level_particle_materials_close(&materials);rf_level_particle_materials_close(&materials);
        if(memcmp(&materials,&empty,sizeof(empty)))return 6;
        return ferror(stdout)?7:0;
    }
    if(argc==6 && (!strcmp(argv[1],"--level-particles") || !strcmp(argv[1],"--campaign-particle-events"))) {
        rf_vpp archive,maps[4];rf_level level;rf_geometry geometry;rf_geometry_collision_world world={0};
        rf_level_particles particles={0},empty={0};uint32_t i,budget=(uint32_t)strtoul(argv[5],NULL,10);int status;char path[1024];
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]) ||
           rf_geometry_open(&geometry,&level,8u*1024u*1024u))return 3;
        if(rf_geometry_collision_world_open(&geometry,8u*1024u*1024u,&world))return 4;
        rf_geometry_close(&geometry);
        for(i=0;i<4;i++) {
            if(snprintf(path,sizeof(path),"%s/maps%u.vpp",argv[4],i+1)<0 || rf_vpp_open(&maps[i],path))return 5;
        }
        status=rf_level_particles_open(&particles,&level,&world,maps,4,123,0,budget);
        if(!strcmp(argv[1],"--campaign-particle-events")) {
            if(!status)status=campaign_particle_events(&level,&particles);
            if(rf_campaign_particle_text_size>=sizeof(rf_campaign_particle_text))status=RF_RANGE;
            if(!status)printf("%s",rf_campaign_particle_text);
            rf_level_particles_close(&particles);
            for(i=0;i<4;i++)rf_vpp_close(&maps[i]);rf_geometry_collision_world_close(&world);rf_vpp_close(&archive);
            return status?11:0;
        }
        for(i=0;i<4;i++)rf_vpp_close(&maps[i]);rf_geometry_collision_world_close(&world);rf_vpp_close(&archive);
        fwrite(&status,4,1,stdout);
        if(status){if(memcmp(&particles,&empty,sizeof(empty)))return 6;return 0;}
        {
            uint32_t header[6]={particles.state->emitters.live,particles.materials.texture_count,particles.resident_bytes,
                particles.state->particles.live[1],particles.state->random.value,sizeof(*particles.state)};
            if(particles.state->emitters.slots!=particles.state->slots || particles.state->emitters.particles!=&particles.state->particles ||
               particles.state->particles.particles!=particles.state->records || particles.state->particles.lists!=particles.state->lists)return 7;
            fwrite(header,sizeof(header),1,stdout);
            for(i=0;i<particles.materials.count;i++) {
                fwrite(&particles.materials.bindings[i],sizeof(particles.materials.bindings[i]),1,stdout);
                fwrite(&particles.state->slots[i],sizeof(particles.state->slots[i]),1,stdout);
                if(!rf_image_pixel(particles.materials.textures[particles.materials.bindings[i].texture].animation.images,0,0))return 8;
            }
        }
        rf_level_particles_close(&particles);rf_level_particles_close(&particles);
        return memcmp(&particles,&empty,sizeof(empty))?9:(ferror(stdout)?10:0);
    }
    /* Process-local batch rays for inspecting authored static geometry. */
    if(argc==4 && (!strcmp(argv[1],"--world-rays") || !strcmp(argv[1],"--scene-rays"))) {
        rf_vpp archive;rf_level level;rf_geometry geometry;rf_geometry_collision_world world={0};rf_geometry_collision_movers movers={0};float ray[6];
        int combined=!strcmp(argv[1],"--scene-rays");
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]) ||
           rf_geometry_open(&geometry,&level,8u*1024u*1024u))return 3;
        if(rf_geometry_collision_world_open(&geometry,8u*1024u*1024u,&world))return 4;
        if(combined) {
            rf_geometry_movers source={0};uint32_t *ids,i;
            if(rf_geometry_movers_open(&level,8u*1024u*1024u,&source))return 4;
            ids=malloc((source.count?source.count:1)*4);if(!ids)return 4;
            for(i=0;i<source.count;++i)ids[i]=i;
            i=rf_geometry_collision_movers_open(&source,ids,8u*1024u*1024u,&movers);
            free(ids);rf_geometry_movers_close(&source);if(i)return 4;
        }
        rf_geometry_close(&geometry);rf_vpp_close(&archive);
        while(fread(ray,sizeof(ray),1,stdin)==1) {
            struct {int32_t status;uint32_t matched;rf_geometry_world_hit hit;} result={0};
            if(combined) {
                rf_collision_solid_hit hit={0};float end[3];uint32_t i;
                for(i=0;i<3;++i)end[i]=ray[i]+ray[i+3];
                result.status=rf_geometry_collision_ray(&world,&movers,ray,end,0x26,&hit,&result.matched);
                result.hit.hit=hit.hit;result.hit.face=hit.face_index;result.hit.room=hit.room;
                result.hit.hits=hit.object_id; /* Synthetic mover index; UINT32_MAX for static. */
            } else result.status=rf_geometry_collision_world_ray(&world,0x460,ray,ray+3,1,&result.hit,&result.matched);
            if(fwrite(&result,sizeof(result),1,stdout)!=1)return 5;
        }
        rf_geometry_collision_movers_close(&movers);rf_geometry_collision_world_close(&world);return ferror(stdin)?6:0;
    }
    if(argc==4 && !strcmp(argv[1],"--burn-resolved")) {
        rf_vpp archive;rf_level level;rf_geometry geometry;rf_geometry_collision_world world={0};int status;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]) || rf_geometry_open(&geometry,&level,8u*1024u*1024u))return 3;
        if(rf_geometry_collision_world_open(&geometry,8u*1024u*1024u,&world))return 4;
        rf_geometry_close(&geometry);status=burn_resolved_probe(&world);
        rf_geometry_collision_world_close(&world);rf_vpp_close(&archive);return status;
    }
    if(argc==4 && !strcmp(argv[1],"--world-track")) {
        rf_vpp archive;rf_level level;rf_geometry geometry;rf_geometry_collision_world world={0};
        struct {float points[6];uint32_t old,flags;} query;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]) || rf_geometry_open(&geometry,&level,8u*1024u*1024u))return 3;
        if(rf_geometry_collision_world_open(&geometry,8u*1024u*1024u,&world))return 4;
        rf_geometry_close(&geometry);
        while(fread(&query,sizeof(query),1,stdin)==1) {
            struct {int32_t status;uint32_t room;int32_t adapter_status;uint32_t token;} result;
            result.room=result.token=0xa5a5a5a5u;
            result.status=rf_geometry_collision_world_track(&world,query.old,query.points,query.points+3,query.flags,&result.room);
            result.adapter_status=rf_geometry_collision_world_track_emitter(&world,query.old==UINT32_MAX?0:query.old+1,
                query.points,query.points+3,query.flags,&result.token);
            if(fwrite(&result,sizeof(result),1,stdout)!=1)return 5;
        }
        rf_geometry_collision_world_close(&world);rf_vpp_close(&archive);return ferror(stdin)?6:0;
    }
    if(argc==4 && !strcmp(argv[1],"--world-locate-dump")) {
        rf_vpp archive;rf_level level;rf_geometry geometry;rf_geometry_collision_world world={0};
        uint32_t i,j,k,p,n,bytes;void *poison;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]) || rf_geometry_open(&geometry,&level,8u*1024u*1024u))return 3;
        if(rf_geometry_collision_world_open(&geometry,8u*1024u*1024u,&world))return 4;
        bytes=geometry.bytes;rf_geometry_close(&geometry);poison=malloc(bytes);if(!poison)return 5;memset(poison,0xdd,bytes);
        fwrite(&world.room_count,4,1,stdout);fwrite(&world.primary_count,4,1,stdout);fwrite(world.minimum,4,6,stdout);fwrite(world.primary,4,world.primary_count,stdout);
        for(i=0;i<world.room_count;++i) {
            const rf_collision_tree *tree=&world.rooms[i].tree;
            fwrite(&world.views[i].skip,4,1,stdout);fwrite(&tree->node_count,4,1,stdout);fwrite(&tree->face_count,4,1,stdout);
            fwrite(tree->nodes,sizeof(*tree->nodes),tree->node_count,stdout);
            for(j=0;j<tree->face_count;++j) {
                const rf_collision_face *face=tree->faces+j;
                fwrite(face->plane,4,10,stdout);fwrite(&face->count,4,1,stdout);fwrite(&face->filter.face_flags,4,1,stdout);fwrite(tree->source_indices+j,4,1,stdout);
                fwrite(face->vertices,12,face->count,stdout);
            }
        }
        n=world.primary_count*3;fwrite(&n,4,1,stdout);
        for(p=0;p<world.primary_count;++p)for(k=0;k<3;++k) {
            const rf_geometry_collision_room *room=&world.rooms[world.primary[p]];const rf_collision_tree *tree=&room->tree;
            float point[3]={0};rf_collision_room_location result,again;
            if(tree->face_count) {
                const rf_collision_face *face=tree->faces;
                for(i=0;i<face->count;++i)for(j=0;j<3;++j)point[j]+=face->vertices[i][j];
                for(j=0;j<3;++j)point[j]=point[j]/face->count+((int)k-1)*.25f*face->plane[j];
            } else for(j=0;j<3;++j)point[j]=(room->minimum[j]+room->maximum[j])*.5f;
            if(rf_geometry_collision_world_locate(&world,point,&result) || rf_geometry_collision_world_locate(&world,point,&again) || memcmp(&result,&again,sizeof(result)))return 6;
            if(result.room!=UINT32_MAX) {
                const rf_collision_tree *owner=&world.rooms[result.room].tree;uint32_t found=0;
                for(j=0;j<owner->face_count;++j)if(owner->source_indices[j]==result.face)found=1;
                if(!found)return 7;
            }
            fwrite(point,4,3,stdout);fwrite(&result,sizeof(result),1,stdout);
        }
        free(poison);rf_geometry_collision_world_close(&world);rf_vpp_close(&archive);return ferror(stdout)?8:0;
    }
    if(argc==2 && !strcmp(argv[1],"--cross-rooms")) {
        struct {float start[3],end[3];uint32_t flags,cached,tree;} in;
        const float lo[3]={-2,-2,-2},hi[3]={2,2,2};
        struct {int32_t status;rf_collision_crossing location;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_face faces[6]={{0}};float vertices[6][4][3];rf_collision_node nodes[3]={{0}};
            rf_collision_tree tree={0};rf_collision_room_view room={0};uint32_t primary=0,stack[3],a,b,k;
            const int corners[4][2]={{0,0},{1,0},{1,1},{0,1}};
            for(a=0;a<6;++a) {
                uint32_t axis=a/2,other[2],n=0;
                for(k=0;k<3;++k)if(k!=axis)other[n++]=k;
                faces[a].plane[axis]=(a&1)?-1.0f:1.0f;
                faces[a].plane[3]=(a&1)?hi[axis]:-lo[axis];
                for(b=0;b<4;++b) {
                    vertices[a][b][axis]=(a&1)?hi[axis]:lo[axis];
                    for(k=0;k<2;++k)vertices[a][b][other[k]]=corners[b][k]?hi[other[k]]:lo[other[k]];
                }
                memcpy(faces[a].minimum,lo,12);memcpy(faces[a].maximum,hi,12);
                faces[a].minimum[axis]=faces[a].maximum[axis]=vertices[a][0][axis];
                faces[a].vertices=vertices[a];faces[a].count=4;faces[a].filter.face_flags=in.flags;
            }
            for(a=0;a<3;++a) {memcpy(nodes[a].minimum,lo,12);memcpy(nodes[a].maximum,hi,12);nodes[a].first_face=a*2;nodes[a].face_count=2;nodes[a].left=nodes[a].right=UINT32_MAX;}
            nodes[0].left=1;nodes[0].right=2;tree.faces=faces;tree.face_count=6;
            tree.nodes=nodes;tree.node_count=in.tree?3:1;tree.node_capacity=3;tree.stack=stack;
            if(!in.tree){nodes[0].face_count=6;nodes[0].left=nodes[0].right=UINT32_MAX;}
            room.tree=&tree;room.skip=0;memset(&out,0xa5,sizeof(out));
            out.status=rf_collision_cross_rooms(&room,1,&primary,1,in.cached?0:UINT32_MAX,in.start,in.end,&out.location);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--locate-room")) {
        struct {float position[3],lo[3],hi[3];uint32_t flags[6],skip,tree;} in;
        struct {int32_t status;rf_collision_room_location location;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_face faces[6]={{0}};float vertices[6][4][3];rf_collision_node nodes[3]={{0}};
            rf_collision_tree tree={0};rf_collision_room_view room={0};uint32_t primary=0,stack[3],a,b,k;
            const int corners[4][2]={{0,0},{1,0},{1,1},{0,1}};
            for(a=0;a<6;++a) {
                uint32_t axis=a/2,other[2],n=0;
                for(k=0;k<3;++k)if(k!=axis)other[n++]=k;
                faces[a].plane[axis]=(a&1)?-1.0f:1.0f;
                faces[a].plane[3]=(a&1)?in.hi[axis]:-in.lo[axis];
                for(b=0;b<4;++b) {
                    vertices[a][b][axis]=(a&1)?in.hi[axis]:in.lo[axis];
                    for(k=0;k<2;++k)vertices[a][b][other[k]]=corners[b][k]?in.hi[other[k]]:in.lo[other[k]];
                }
                memcpy(faces[a].minimum,in.lo,12);memcpy(faces[a].maximum,in.hi,12);
                faces[a].minimum[axis]=faces[a].maximum[axis]=vertices[a][0][axis];
                faces[a].vertices=vertices[a];faces[a].count=4;faces[a].filter.face_flags=in.flags[a];
            }
            for(a=0;a<3;++a) {memcpy(nodes[a].minimum,in.lo,12);memcpy(nodes[a].maximum,in.hi,12);nodes[a].first_face=a*2;nodes[a].face_count=2;nodes[a].left=nodes[a].right=UINT32_MAX;}
            nodes[0].left=1;nodes[0].right=2;tree.faces=faces;tree.face_count=6;
            if(in.tree){tree.nodes=nodes;tree.node_count=3;tree.node_capacity=3;tree.stack=stack;}
            room.tree=&tree;room.skip=in.skip;memset(&out,0xa5,sizeof(out));
            out.status=rf_collision_locate_room(&room,1,&primary,1,in.lo,in.hi,in.position,&out.location);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--room-direction")) {
        float in[4];struct {int32_t status;float direction[3];} out;
        while(fread(in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_room_direction(in,in[3],out.direction);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--room-face")) {
        struct {rf_collision_room_query query;float plane[4],lo[3],hi[3],vertices[4][3];uint32_t token;} in;
        struct {int32_t status;uint32_t retry;rf_collision_room_query query;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_face face={0};memcpy(face.plane,in.plane,16);memcpy(face.minimum,in.lo,12);memcpy(face.maximum,in.hi,12);
            face.vertices=in.vertices;face.count=4;out.query=in.query;out.retry=99;
            out.status=rf_collision_room_query_face(&out.query,&face,in.token,&out.retry);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?1:0;
    }
    if((argc==2 || argc==3) && (!strcmp(argv[1],"--door-cycle") || !strcmp(argv[1],"--door-cycle-smooth"))) {
        struct {rf_group_translation_runtime runtime;rf_group_attached_pose controller,mover;float keys[2][8];} in;
        struct {int32_t status;rf_group_translation_runtime runtime;rf_group_attached_pose controller,mover;} out;
        rf_level_group_key keys[2]={{0}};rf_group_controller_view binding={0};rf_group_pose_slot slot;
        uint32_t i,frame,handle=0x12340000;
        int smooth=!strcmp(argv[1],"--door-cycle-smooth");float dt=smooth?1.0f/60.0f:.25f;
        uint32_t steps=argc==3?(uint32_t)strtoul(argv[2],NULL,10):smooth?600:40;
        if(!steps || steps>600)return 2;
        if(fread(&in,sizeof(in),1,stdin)!=1)return 2;
        out.runtime=in.runtime;out.controller=in.controller;out.mover=in.mover;
        for(i=0;i<2;i++) {memcpy(keys[i].position,in.keys[i],12);memcpy(keys[i].timing,in.keys[i]+3,20);}
        binding.runtime=&out.runtime;binding.first_key=keys;binding.mover_handles=&handle;binding.mover_count=1;
        slot.handle=handle;slot.pose=&out.mover;
        if(rf_group_motion_activate(&out.runtime.motion,2))return 3;
        /* Controlled translation diagnostic: no trigger obstruction or event links. */
        for(frame=0;frame<steps;frame++) {
            rf_group_translation_frame tick;uint32_t sounds=0;
            out.status=rf_group_translation_tick_begin(&out.runtime,keys,2,dt,(int32_t)(smooth?frame*1000/60:frame*250),&tick);
            if(!out.status && tick.stage==RF_GROUP_TICK_GATES)out.status=rf_group_translation_tick_move(&out.runtime,&tick);
            if(!out.status && tick.stage==RF_GROUP_TICK_ARRIVAL)out.status=rf_group_translation_tick_finish(&out.runtime,&tick,2,&sounds);
            if(!out.status) {
                out.controller.flags=out.runtime.object_flags;memcpy(out.controller.pending,out.runtime.pending,12);memcpy(out.controller.velocity,out.runtime.velocity,12);
                out.status=rf_group_translation_bind_pose(&out.mover,handle,&binding,1,dt,0);
            }
            if(!out.status)out.status=rf_group_commit_positions(&out.runtime.motion.flags,&out.controller,&binding,&slot,1);
            if(!out.status) {memcpy(out.runtime.position,out.controller.position,12);out.runtime.object_flags=out.controller.flags;}
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 4;if(out.status)return 5;
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--controller-pose")) {
        float in[12];rf_level_group_key first={0};struct {int32_t status;rf_group_attached_pose pose;} out;
        while(fread(in,sizeof(in),1,stdin)==1) {
            memcpy(first.position,in,12);memcpy(first.orientation,in+3,36);memset(&out.pose,0xa5,sizeof(out.pose));
            out.status=rf_group_controller_pose(&first,&out.pose);if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--group-initialize")) {
        struct {rf_group_attached_pose pose;uint32_t flags,mode,index,count;int32_t now;float position[3];} in;
        struct {int32_t status;rf_group_translation_runtime runtime;rf_group_attached_pose pose;} out;
        rf_level_group_key selected={0};
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out.runtime,0xa5,sizeof(out.runtime));out.pose=in.pose;memcpy(selected.position,in.position,12);
            out.status=rf_group_translation_initialize(&out.runtime,&out.pose,in.flags,in.mode,&selected,in.index,in.count,in.now);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--group-commit")) {
        struct {uint32_t flags,counts[2],handles[2][16];rf_group_attached_pose poses[9];} in;
        struct {int32_t status;uint32_t flags;rf_group_attached_pose poses[9];} out;
        rf_group_controller_view bindings={0};rf_group_pose_slot slots[9];uint32_t i;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.flags=in.flags;memcpy(out.poses,in.poses,sizeof(out.poses));
            for(i=0;i<9;i++) {slots[i].handle=0x12340000+i;slots[i].pose=out.poses+i;}
            bindings.mover_handles=in.handles[0];bindings.mover_count=in.counts[0];bindings.general_handles=in.handles[1];bindings.general_count=in.counts[1];
            out.status=in.counts[0]>16 || in.counts[1]>16?RF_RANGE:rf_group_commit_positions(&out.flags,out.poses,&bindings,slots,9);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--pose-position")) {
        struct {rf_group_attached_pose pose;float position[3];uint32_t alias;} in;
        struct {int32_t status;rf_group_attached_pose pose;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.pose=in.pose;out.status=rf_group_pose_set_position(&out.pose,in.alias?out.pose.pending:in.position);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--group-bind")) {
        struct {uint32_t count,force;float dt;rf_group_attached_pose pose;struct {float first[3],pending[3];uint32_t flags,mover,general;} controllers[2];} in;
        struct {int32_t status;rf_group_attached_pose pose;} out;
        rf_group_controller_view views[2];rf_group_translation_runtime runtime[2];rf_level_group_key keys[2];uint32_t i;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(runtime,0,sizeof(runtime));memset(keys,0,sizeof(keys));
            for(i=0;i<2;i++) {
                runtime[i].motion.flags=in.controllers[i].flags;memcpy(runtime[i].pending,in.controllers[i].pending,12);memcpy(keys[i].position,in.controllers[i].first,12);
                views[i].runtime=runtime+i;views[i].first_key=keys+i;views[i].mover_handles=&in.controllers[i].mover;views[i].mover_count=1;views[i].general_handles=&in.controllers[i].general;views[i].general_count=1;
            }
            out.pose=in.pose;out.status=in.count>2?RF_RANGE:rf_group_translation_bind_pose(&out.pose,0x12340000,views,in.count,in.dt,in.force);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--group-propagate")) {
        struct {uint32_t count,force;float dt;rf_group_attached_pose pose;rf_group_translation_contribution contributions[4];} in;
        struct {int32_t status;rf_group_attached_pose pose;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.pose=in.pose;out.status=rf_group_translation_propagate(&out.pose,in.contributions,in.count,in.dt,in.force);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--group-tick")) {
        struct {rf_group_translation_runtime runtime;struct {float position[3],timing[5];} keys[2];float dt;int32_t now;} in;
        struct {int32_t status;rf_group_translation_runtime runtime;uint32_t sounds,stage;} out;
        rf_level_group_key keys[2];rf_group_translation_frame frame;uint32_t i;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.runtime=in.runtime;out.sounds=0;out.stage=0;
            memset(keys,0,sizeof(keys));
            for(i=0;i<2;i++) {memcpy(keys[i].position,in.keys[i].position,12);memcpy(keys[i].timing,in.keys[i].timing,20);}
            out.status=rf_group_translation_tick_begin(&out.runtime,keys,2,in.dt,in.now,&frame);
            if(!out.status && frame.stage==RF_GROUP_TICK_GATES)out.status=rf_group_translation_tick_move(&out.runtime,&frame);
            if(!out.status && frame.stage==RF_GROUP_TICK_ARRIVAL)out.status=rf_group_translation_tick_finish(&out.runtime,&frame,2,&out.sounds);
            if(!out.status)out.stage=frame.stage;
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--group-position")) {
        struct {rf_group_translation_step step;rf_group_translation_progress progress;float position[3];} in;
        struct {int32_t status;uint32_t arrival;float pending[3];} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_group_translation_position(&in.step,&in.progress,in.position,out.pending,&out.arrival);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--group-integrate")) {
        rf_group_translation_step in;
        struct {int32_t status;rf_group_translation_progress result;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_group_translation_integrate(&in,&out.result);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--group-arrive")) {
        struct {uint32_t count;rf_group_motion_state state;} in;
        struct {int32_t status;uint32_t sounds;rf_group_motion_state state;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.state=in.state;out.sounds=0xa5a5a5a5;
            out.status=rf_group_translation_arrive(&out.state,in.count,&out.sounds);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--group-activate")) {
        struct {uint32_t count;rf_group_motion_state state;} in;
        struct {int32_t status;rf_group_motion_state state;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.state=in.state;out.status=rf_group_motion_activate(&out.state,in.count);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--attach-movers")) {
        struct {uint32_t count,controller,flags,mode,refs_count,handles_count,capacity;float rotation;
            rf_group_object objects[16];uint32_t refs[32],handles[32];} in;
        struct {int32_t status;uint32_t refs_count,handles_count;float rotation;
            rf_group_object objects[16];uint32_t refs[32],handles[32];} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.refs_count=in.refs_count;out.handles_count=in.handles_count;out.rotation=in.rotation;
            memcpy(out.objects,in.objects,sizeof(out.objects));memcpy(out.refs,in.refs,sizeof(out.refs));memcpy(out.handles,in.handles,sizeof(out.handles));
            out.status=in.count>16 || in.refs_count>32 || in.capacity>32?RF_RANGE:
                rf_group_attach_movers(out.objects,in.count,in.controller,in.flags,in.mode,out.refs,&out.refs_count,out.handles,&out.handles_count,in.capacity,&out.rotation);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--group-flags")) {
        struct {uint32_t count;uint8_t flags[6],padding[2];float timing[2];} in;
        struct {int32_t status;uint32_t flags;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_level_group group={0};rf_level_group_key key={0};
            group.key_count=in.count;memcpy(group.flags,in.flags,6);memcpy(key.timing+3,in.timing,8);
            memset(&out,0xa5,sizeof(out));out.status=rf_level_group_initial_flags(&group,&key,&out.flags);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==4 && !strcmp(argv[1],"--mover-preview")) {
        rf_vpp archive;rf_level level;rf_geometry_movers movers={0};uint32_t i,j,k,pass,vertices=0,cases=0,shifted=0;
        const float identity[3][3]={{1,0,0},{0,1,0},{0,0,1}};
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]) || rf_geometry_movers_open(&level,8*1024*1024,&movers))return 2;
        for(i=0;i<movers.count;i++) {
            rf_geometry *g=&movers.items[i].geometry,baked=*g;rf_preview_mesh prior={0};baked.data=malloc(g->bytes);if(!baked.data)return 3;
            for(pass=0;pass<3;pass++) {
                rf_preview_mesh mesh={0},reference={0},exact={0},short_mesh={0};float origin[3]={0};const float (*matrix)[3]=pass?movers.items[i].orientation:identity;
                memcpy(baked.data,g->data,g->bytes);memcpy(level.player_orientation,identity,36);
                for(k=0;k<3;k++) {origin[k]=pass?movers.items[i].position[k]:0;level.player_position[k]=origin[k];if(pass==2)origin[k]+=(float)(k+1);}
                level.player_position[2]-=20;
                for(j=0;j<g->vertices;j++) {
                    rf_collision_ray_hit local={0},world;if(rf_geometry_vertex(g,j,local.point) || rf_collision_contact_world(&local,origin,matrix,&world))return 4;
                    memcpy(baked.data+g->vertices_offset+12*j,world.point,12);
                }
                for(j=0;j<g->faces;j++) {
                    rf_geometry_face face;rf_collision_ray_hit local={0},world;if(rf_geometry_get_face(g,j,&face))return 4;memcpy(local.normal,face.plane,12);
                    if(rf_collision_contact_world(&local,origin,matrix,&world))return 4;memcpy(baked.data+g->face_offsets[j],world.normal,12);
                }
                if(rf_preview_build_transformed(&mesh,g,&level,origin,matrix,17,8*1024*1024) || rf_preview_build(&reference,&baked,&level,8*1024*1024))return 5;
                for(j=0;j<reference.count;j++)reference.vertices[j].material+=17;
                if(mesh.count!=reference.count || mesh.bytes!=reference.bytes || (mesh.bytes && memcmp(mesh.vertices,reference.vertices,mesh.bytes)))return 6;
                if(rf_preview_build_transformed(&exact,g,&level,origin,matrix,17,mesh.bytes) || exact.bytes!=mesh.bytes || (mesh.bytes && memcmp(exact.vertices,mesh.vertices,mesh.bytes)))return 7;
                if(mesh.bytes && rf_preview_build_transformed(&short_mesh,g,&level,origin,matrix,17,mesh.bytes-1)!=RF_RANGE)return 8;
                if(pass==2 && (prior.count!=mesh.count || (mesh.bytes && memcmp(prior.vertices,mesh.vertices,mesh.bytes))))shifted++;
                vertices+=mesh.count;cases++;rf_preview_close(&prior);prior=mesh;
                rf_preview_close(&reference);rf_preview_close(&exact);rf_preview_close(&short_mesh);
            }
            rf_preview_close(&prior);free(baked.data);
        }
        printf("%u %u %u %u\n",movers.count,cases,vertices,shifted);rf_geometry_movers_close(&movers);rf_vpp_close(&archive);return 0;
    }
    if(argc==4 && (!strcmp(argv[1],"--member-groups") || !strcmp(argv[1],"--registered-member-groups"))) {
        rf_object_registry registry;int registered=!strcmp(argv[1],"--registered-member-groups");
        rf_vpp archive;rf_level level;rf_level_owned_groups source={0};rf_group_runtime_collection runtime={0};rf_geometry_movers movers={0};
        rf_group_mover_memberships members={0},exact={0},guard;rf_group_object *objects,*before;uint32_t *controllers,i,object_count;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]) || rf_level_owned_groups_open(&level,4*1024*1024,&source) || rf_geometry_movers_open(&level,8*1024*1024,&movers))return 2;
        objects=calloc(movers.count?movers.count:1,sizeof(*objects));before=calloc(movers.count?movers.count:1,sizeof(*before));controllers=calloc(source.count?source.count:1,4);if(!objects || !before || !controllers)return 3;
        for(i=0;i<movers.count;i++) {objects[i].uid=movers.items[i].uid;objects[i].type=9;objects[i].handle=0x12340000+i;objects[i].parent=UINT32_MAX;objects[i].flags=0x6000000;}
        for(i=0;i<source.count;i++)controllers[i]=0x23450000+movers.count+i;
        object_count=movers.count;rf_geometry_movers_close(&movers);rf_vpp_close(&archive);
        if(rf_group_runtime_open(&source,0,1024*1024,&runtime))return 4;
        if(registered) {
            rf_object_registry_init(&registry);
            for(i=0;i<object_count;i++)if(rf_object_registry_insert(&registry,objects+i,&objects[i].handle))return 4;
            for(i=0;i<runtime.count;i++)if(rf_object_registry_insert(&registry,runtime.items+i,controllers+i))return 4;
            for(i=0;i<object_count;i++)if(rf_object_registry_lookup(&registry,objects[i].handle)!=objects+i)return 4;
        }
        {
         if(rf_group_mover_memberships_open(&runtime,objects,object_count,controllers,0,1024*1024,&members))return 5;
         if(rf_group_mover_memberships_open(&runtime,objects,object_count,controllers,0,members.peak_bytes,&exact))return 6;
         rf_group_mover_memberships_close(&exact);memcpy(before,objects,object_count*sizeof(*objects));memset(&guard,0xa5,sizeof(guard));exact=guard;
         if(rf_group_mover_memberships_open(&runtime,objects,object_count,controllers,0,members.peak_bytes-1,&exact)!=RF_RANGE || memcmp(&guard,&exact,sizeof(guard)) || memcmp(before,objects,object_count*sizeof(*objects)))return 7;
         if(fwrite(&members.count,4,1,stdout)!=1 || fwrite(&object_count,4,1,stdout)!=1 || fwrite(&members.allocated_bytes,4,1,stdout)!=1 || fwrite(&members.peak_bytes,4,1,stdout)!=1 || fwrite(objects,sizeof(*objects),object_count,stdout)!=object_count)return 8;
        }
        for(i=0;i<members.count;i++) {
            rf_group_mover_membership *m=members.items+i;
            if(fwrite(&m->count,4,1,stdout)!=1 || fwrite(&m->rotation_sign,4,1,stdout)!=1 || fwrite(m->handles,4,m->count,stdout)!=m->count)return 8;
        }
        rf_group_mover_memberships_close(&members);rf_group_mover_memberships_close(&members);rf_group_runtime_close(&runtime);rf_level_owned_groups_close(&source);free(objects);free(before);free(controllers);return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--mover-contact")) {
        struct {rf_collision_ray_hit hit;float origin[3],matrix[3][3],velocity[3];uint32_t object,texture,material,flags,face;} input;
        struct {int32_t status;rf_collision_body_hit hit;} output;
        while(fread(&input,sizeof(input),1,stdin)==1) {
            memset(&output,0xa5,sizeof(output));
            output.status=rf_collision_mover_contact(&input.hit,input.origin,input.matrix,input.velocity,input.object,input.texture,input.material,input.flags,input.face,&output.hit);
            fwrite(&output,sizeof(output),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--mover-sphere-local")) {
        float input[30];struct {int32_t status;float values[6];} output;
        while(fread(input,sizeof(input),1,stdin)==1) {
            memset(&output,0xa5,sizeof(output));
            output.status=rf_collision_mover_sphere_local(input,(const float (*)[3])(input+3),input+12,input+15,input+18,(const float (*)[3])(input+21),output.values,output.values+3);
            fwrite(&output,sizeof(output),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--group-activation-run")) {
        uint32_t input[10],output[18];static rf_object_registry registry;static rf_entity_registry entities;
        while(fread(input,sizeof(input),1,stdin)==1) {
            rf_group_motion_state motion;rf_group_attached_pose pose={0},mover_pose={0};
            rf_group_sound_state sounds={{11,12,13,14},{21,22,23,24}};
            rf_entity_view actor={0};rf_group_registered_mover mover={9,0,&mover_pose};
            rf_group_wake_object prop={0};rf_group_activation_context c={0};
            uint32_t source=77,backlink=55,started=0xa5a5a5a5,handle,dummy[2]={8,0},i;
            activation_trace trace={&source,0,2166136261u};
            memcpy(&motion,input,24);rf_object_registry_init(&registry);memset(&entities,0,sizeof(entities));
            for(i=0;i<2;i++)if(rf_object_registry_insert(&registry,dummy+i,&handle))return 4;
            if(rf_object_registry_insert(&registry,&mover,&mover.handle))return 4;
            actor.handle=0x20001;actor.type=0;actor.flags_7c=input[6];actor.flags_810=input[7];actor.linked_handle=-1;entities.slots[1]=&actor;
            for(i=0;i<3;i++) {pose.public_position[i]=(float)i+1;mover_pose.minimum[i]=-1;mover_pose.maximum[i]=1;prop.minimum[i]=-.5f;prop.maximum[i]=.5f;}
            prop.family=1;c.motion=&motion;c.pose=&pose;c.sounds=&sounds;c.source=&source;c.entities=&entities;c.local=&actor;c.registry=&registry;
            c.objects=&prop;c.object_count=1;c.movers=&mover.handle;c.mover_count=1;c.play=activation_play;c.alert=activation_alert;c.context=&trace;c.gate_7cabd4=input[8];c.gate_7cabb0=input[9];
            output[0]=(uint32_t)rf_group_activation_run(&c,0x10000,2,99,0x20001,&backlink,&started);
            memcpy(output+1,&motion,24);output[7]=backlink;output[8]=source;memcpy(output+9,sounds.handles,16);
            output[13]=prop.flags;output[14]=prop.physics_flags;output[15]=started;output[16]=trace.count;output[17]=trace.hash;
            fwrite(output,sizeof(output),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--group-wake-bounds")) {
        struct {uint32_t count,handles[34];struct {uint32_t kind;rf_group_wake_bounds bounds;} movers[4];} input;
        struct {int32_t status;uint32_t count;rf_group_wake_bounds bounds[32];} output;
        static rf_object_registry registry;rf_group_registered_mover movers[4];rf_group_attached_pose poses[4];uint32_t i;
        while(fread(&input,sizeof(input),1,stdin)==1) {
            if(input.count>34)return 3;
            rf_object_registry_init(&registry);memset(movers,0,sizeof(movers));memset(poses,0,sizeof(poses));
            for(i=0;i<4;i++) {
                movers[i].object_kind=input.movers[i].kind;movers[i].pose=poses+i;
                memcpy(poses[i].minimum,input.movers[i].bounds.minimum,12);memcpy(poses[i].maximum,input.movers[i].bounds.maximum,12);
                if(rf_object_registry_insert(&registry,movers+i,&movers[i].handle))return 4;
            }
            memset(&output,0xa5,sizeof(output));
            output.status=rf_group_wake_bounds_collect(&registry,input.handles,input.count,output.bounds,&output.count);
            fwrite(&output,sizeof(output),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--group-wake")) {
        struct {uint32_t count;rf_group_wake_object objects[8];rf_group_wake_bounds bounds[34];uint32_t attached[4],parents[4];} input;
        struct {int32_t status;rf_group_wake_object objects[8];} output;
        while(fread(&input,sizeof(input),1,stdin)==1) {
            if(input.count>34)return 3;
            output.status=rf_group_wake_objects(input.objects,8,input.bounds,input.count,input.attached,4,input.parents,4);
            memcpy(output.objects,input.objects,sizeof(output.objects));fwrite(&output,sizeof(output),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--group-sound-start")) {
        struct {rf_group_sound_state sounds;uint32_t flags;int32_t next_key;float position[3];} input;
        struct {int32_t status;rf_group_sound_state sounds;uint32_t trace[2];} output;
        while(fread(&input,sizeof(input),1,stdin)==1) {
            output.trace[0]=0;output.trace[1]=2166136261u;
            output.status=rf_group_sound_start(&input.sounds,input.flags,input.next_key,input.position,group_sound_probe,output.trace);
            output.sounds=input.sounds;fwrite(&output,sizeof(output),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--group-activation-begin")) {
        struct {uint32_t count,handle;rf_group_motion_state state;rf_group_activation_actor actor;} input;
        struct {int32_t status;rf_group_motion_state state;rf_group_activation_actor actor;uint32_t started;} output;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            output.started=0xa5a5a5a5;
            output.status=rf_group_activation_begin(&input.state,input.count,input.handle,&input.actor,&output.started);
            output.state=input.state;output.actor=input.actor;fwrite(&output,sizeof(output),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==4 && !strcmp(argv[1],"--registered-groups")) {
        rf_vpp archive;rf_level level;rf_level_owned_groups source={0};rf_group_runtime_collection runtime={0};
        rf_group_registration registration={0},exact={0};static rf_object_registry registry,saved;
        uint32_t unrelated=6,handle,i,budget,last=UINT32_MAX;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]) ||
            rf_level_owned_groups_open(&level,1024*1024,&source))return 80;
        rf_vpp_close(&archive);if(rf_group_runtime_open(&source,0,1024*1024,&runtime))return 81;
        rf_object_registry_init(&registry);if(rf_object_registry_insert(&registry,&unrelated,&handle))return 82;
        if(rf_group_registration_open(&runtime,&registry,1024*1024,&registration))return 83;
        printf("%u %u %u\n",registration.count,registration.key_count,registration.allocated_bytes);
        for(i=0;i<registration.count;i++) {
            rf_group_registered_controller *c=registration.controllers+i;
            if(c->object_kind!=8 || rf_object_registry_lookup(&registry,c->handle)!=c || !c->runtime)return 84;
            printf("OBJECT %u %u %u\n",registration.objects[i].uid,registration.objects[i].handle,registration.objects[i].flags);last=c->handle;
        }
        for(i=0;i<registration.key_count;i++)printf("KEY %u %u\n",registration.keys[i].uid,registration.keys[i].handle);
        budget=registration.allocated_bytes;rf_group_registration_close(&registration);rf_group_registration_close(&registration);
        if(rf_object_registry_lookup(&registry,handle)!=&unrelated || rf_object_registry_lookup(&registry,last))return 85;
        saved=registry;
        if(rf_group_registration_open(&runtime,&registry,budget-1,&exact)!=RF_RANGE || exact.storage || memcmp(&registry,&saved,sizeof(saved)))return 86;
        if(rf_group_registration_open(&runtime,&registry,budget,&exact))return 87;
        rf_group_registration_close(&exact);if(registry.count!=RF_OBJECT_CAPACITY-1)return 88;
        rf_group_runtime_close(&runtime);rf_level_owned_groups_close(&source);return 0;
    }
    if(argc==4 && !strcmp(argv[1],"--runtime-groups")) {
        rf_vpp archive;rf_level level;rf_level_owned_groups source={0};rf_group_runtime_collection runtime={0},exact={0},guard;
        uint32_t i,translation=0,rotation=0;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]) || rf_level_owned_groups_open(&level,4*1024*1024,&source))return 2;
        rf_vpp_close(&archive);memset(&level,0xdd,sizeof(level));
        if(rf_group_runtime_open(&source,0,1024*1024,&runtime))return 3;
        if(rf_group_runtime_open(&source,0,runtime.allocated_bytes,&exact))return 4;rf_group_runtime_close(&exact);
        memset(&guard,0xa5,sizeof(guard));exact=guard;
        if(rf_group_runtime_open(&source,0,runtime.allocated_bytes-1,&exact)!=RF_RANGE || memcmp(&guard,&exact,sizeof(guard)))return 5;
        for(i=source.count;i>0;i--)if(runtime.items[i-1].kind==RF_GROUP_RUNTIME_TRANSLATION) {
            uint32_t saved=source.groups[i-1].record.unknown;source.groups[i-1].record.unknown=UINT32_MAX;exact=guard;
            if(rf_group_runtime_open(&source,0,1024*1024,&exact)!=RF_RANGE || memcmp(&guard,&exact,sizeof(guard)))return 6;
            source.groups[i-1].record.unknown=saved;break;
        }
        for(i=0;i<runtime.count;i++) {translation+=runtime.items[i].kind==RF_GROUP_RUNTIME_TRANSLATION;rotation+=runtime.items[i].kind==RF_GROUP_RUNTIME_ROTATION_PENDING;}
        if(fwrite(&runtime.count,4,1,stdout)!=1 || fwrite(&runtime.allocated_bytes,4,1,stdout)!=1 || fwrite(&translation,4,1,stdout)!=1 || fwrite(&rotation,4,1,stdout)!=1)return 7;
        for(i=0;i<runtime.count;i++) {
            rf_group_runtime_entry *e=runtime.items+i;if(e->source!=source.groups+i)return 8;
            if(fwrite(&e->kind,4,1,stdout)!=1 || fwrite(&e->initial_flags,4,1,stdout)!=1 || fwrite(&e->translation,76,1,stdout)!=1 || fwrite(&e->pose,236,1,stdout)!=1)return 7;
        }
        rf_group_runtime_close(&runtime);rf_group_runtime_close(&runtime);rf_level_owned_groups_close(&source);return 0;
    }
    if(argc==4 && !strcmp(argv[1],"--owned-groups")) {
        rf_vpp archive;rf_level level;rf_level_owned_groups owned={0},exact={0},guard;
        uint32_t i,list;int status;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]))return 2;
        status=rf_level_owned_groups_open(&level,4*1024*1024,&owned);if(status)return 3;
        if(rf_level_owned_groups_open(&level,owned.allocated_bytes,&exact))return 4;
        rf_level_owned_groups_close(&exact);memset(&guard,0xa5,sizeof(guard));exact=guard;
        if(rf_level_owned_groups_open(&level,owned.allocated_bytes-1,&exact)!=RF_RANGE || memcmp(&exact,&guard,sizeof(guard)))return 5;
        {
            rf_level cut=level;
            for(i=0;i<cut.section_count;i++)if(cut.sections[i].type==0x3000) {
                if(!cut.sections[i].size)return 7;cut.sections[i].size--;exact=guard;
                if(rf_level_owned_groups_open(&cut,4*1024*1024,&exact)!=RF_FORMAT || memcmp(&exact,&guard,sizeof(guard)))return 8;
                break;
            }
        }
        rf_vpp_close(&archive);memset(&level,0xdd,sizeof(level));
        if(fwrite(&owned.count,4,1,stdout)!=1 || fwrite(&owned.allocated_bytes,4,1,stdout)!=1)return 6;
        for(i=0;i<owned.count;i++) {
            const rf_level_owned_group *g=owned.groups+i;
            if(fwrite(&g->record,sizeof(g->record),1,stdout)!=1 ||
               fwrite(g->keys,sizeof(*g->keys),g->record.key_count,stdout)!=g->record.key_count ||
               fwrite(g->legacy,sizeof(*g->legacy),g->record.legacy_count,stdout)!=g->record.legacy_count)return 6;
            for(list=0;list<2;list++)if(fwrite(g->ids[list],4,g->record.ids_count[list],stdout)!=g->record.ids_count[list])return 6;
        }
        rf_level_owned_groups_close(&owned);rf_level_owned_groups_close(&owned);return 0;
    }
    if(argc==4 && !strcmp(argv[1],"--groups")) {
        rf_vpp archive;rf_level level;rf_level_group_reader reader;rf_level_group g;int status;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]) || rf_level_groups_begin(&level,&reader))return 2;
        while((status=rf_level_group_next(&reader,&g))==RF_OK) {
            uint32_t i,j;rf_level_group_key key;
            if(fwrite(&g,sizeof(g),1,stdout)!=1)return 3;
            for(i=0;i<g.key_count;i++) {
                if(rf_level_group_key_at(&level,&g,i,&key))return 4;
                if(fwrite(&key,sizeof(key),1,stdout)!=1)return 3;
            }
            for(i=0;i<2;i++)for(j=0;j<g.ids_count[i];j++) {
                uint32_t uid;if(rf_level_group_id_at(&level,&g,i,j,&uid))return 5;
                if(fwrite(&uid,4,1,stdout)!=1)return 3;
            }
            {rf_level_group guard={0},before;rf_level_group_reader cut=reader,prior;
                cut.cursor=g.offset;cut.index--;cut.section.size=g.offset+g.bytes-1;prior=cut;
                memset(&guard,0xa5,sizeof(guard));before=guard;
                if(rf_level_group_next(&cut,&guard)!=RF_FORMAT || memcmp(&guard,&before,sizeof(guard)) || memcmp(&cut,&prior,sizeof(cut)))return 6;
            }
        }
        rf_vpp_close(&archive);return status==RF_NOT_FOUND?0:7;
    }
    if(argc==4 && (!strcmp(argv[1],"--combined-world") || !strcmp(argv[1],"--registered-combined-world"))) {
        rf_object_registry registry;int registered=!strcmp(argv[1],"--registered-combined-world");
        rf_vpp archive;rf_level level;rf_geometry geometry={0};rf_geometry_movers source={0};
        rf_geometry_collision_world world={0};rf_geometry_collision_movers movers={0},empty={0};
        uint32_t *ids,i,j,k,pass,group,queries=0,hits=0,moving_hits=0,static_hits=0,hash[2]={2166136261u,2166136261u},bytes;
        void *poison=NULL;int status;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]) || rf_geometry_open(&geometry,&level,8*1024*1024))return 2;
        if(rf_geometry_collision_world_open(&geometry,8*1024*1024,&world))return 3;
        status=rf_geometry_movers_open(&level,8*1024*1024,&source);if(status && status!=RF_NOT_FOUND)return 4;
        ids=(uint32_t *)malloc(source.count?source.count*4:4);if(!ids)return 5;
        for(i=0;i<source.count;i++)ids[i]=0x12340000+i;
        if(rf_geometry_collision_movers_open(&source,ids,8*1024*1024,&movers))return 6;
        if(registered) {
            rf_object_registry_init(&registry);
            for(i=0;i<movers.count;i++)if(rf_object_registry_insert(&registry,movers.poses+i,&movers.views[i].object_id))return 6;
        }
        free(ids);bytes=geometry.bytes+source.allocated_bytes;
        for(pass=0;pass<2;pass++) {
            for(group=0;group<2;group++)for(i=0;i<(group?movers.count:world.room_count);i++) {
                const rf_collision_face *faces=group?movers.owned[i].faces:world.rooms[i].tree.faces;
                uint32_t count=group?movers.owned[i].count:(world.rooms[i].tree.face_count?1:0);
                for(j=0;j<count;j++) {
                    const rf_collision_face *face=faces+j;float start[3],end[3],delta[3];uint32_t v,visible;
                    rf_collision_ray_hit local={0},global;
                    struct {int32_t status;uint32_t matched;rf_collision_solid_hit hit;} out;
                    const unsigned char *raw=(const unsigned char *)&out;
                    for(v=0;v<face->count;v++)for(k=0;k<3;k++)local.point[k]+=face->vertices[v][k];
                    for(k=0;k<3;k++) {local.point[k]/=face->count;local.normal[k]=face->plane[k];}
                    global=local;
                    if(group && rf_collision_contact_world(&local,movers.views[i].output_origin,movers.views[i].output_matrix,&global))return 7;
                    for(k=0;k<3;k++) {
                        start[k]=global.point[k]+global.normal[k]+.0037f*(k+1);
                        end[k]=global.point[k]-global.normal[k]+.005f*(k+1);delta[k]=end[k]-start[k];
                    }
                    if(!group) {
                        rf_geometry_world_hit reference;rf_collision_solid_hit mapped;uint32_t a,b;
                        if(rf_geometry_collision_world_ray(&world,0x464,start,delta,1,&reference,&a) ||
                            rf_geometry_collision_ray(&world,&empty,start,end,0x26,&mapped,&b) || a!=b)return 8;
                        if(a && (memcmp(&reference.hit,&mapped.hit,28) || reference.face!=mapped.face_index || reference.room!=mapped.room || mapped.solid_index!=UINT32_MAX))return 9;
                    }
                    memset(&out,0xa5,sizeof(out));out.status=rf_geometry_collision_ray(&world,&movers,start,end,0x26,&out.hit,&out.matched);
                    if(out.status || rf_geometry_collision_ray(&world,&movers,start,end,0x26,NULL,&visible) || out.matched!=visible)return 10;
                    if(!pass) {queries++;hits+=out.matched;}
                    if(out.matched) {
                        if(out.hit.solid_index!=UINT32_MAX) {
                            if(out.hit.solid_index>=movers.count || out.hit.face_index>=movers.owned[out.hit.solid_index].count ||
                                out.hit.object_id!=movers.views[out.hit.solid_index].object_id || out.hit.room!=UINT32_MAX)return 11;
                            if(!pass)moving_hits++;
                        } else {
                            const rf_collision_tree *tree;uint32_t found=0;
                            if(out.hit.room>=world.room_count || out.hit.object_id!=UINT32_MAX)return 12;
                            tree=&world.rooms[out.hit.room].tree;
                            for(v=0;v<tree->face_count;v++)if(tree->source_indices[v]==out.hit.face_index)found=1;
                            if(!found)return 13;
                            if(!pass)static_hits++;
                        }
                    }
                    for(k=0;k<sizeof(out);k++)hash[pass]=(hash[pass]^raw[k])*16777619u;
                }
            }
            if(!pass) {rf_geometry_close(&geometry);rf_geometry_movers_close(&source);rf_vpp_close(&archive);poison=malloc(bytes);if(!poison)return 14;memset(poison,0xdd,bytes);}
        }
        if(hash[0]!=hash[1])return 15;
        printf("%u %u %u %u %u %u %u %u\n",world.room_count,movers.count,queries,hits,moving_hits,static_hits,hash[0],world.allocated_bytes+movers.allocated_bytes);
        free(poison);rf_geometry_collision_world_close(&world);rf_geometry_collision_movers_close(&movers);return 0;
    }
    if(argc==4 && (!strcmp(argv[1],"--bound-movers") || !strcmp(argv[1],"--shifted-movers") || !strcmp(argv[1],"--committed-movers"))) {
        rf_vpp archive;rf_level level;rf_geometry_movers source={0};
        rf_geometry_collision_movers owned={0},exact={0},guard;uint32_t *ids,i;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]))return 2;
        if(rf_geometry_movers_open(&level,8*1024*1024,&source))return 3;
        ids=(uint32_t *)malloc(source.count?source.count*4:4);if(!ids)return 4;
        for(i=0;i<source.count;i++)ids[i]=0x12340000+i; /* Explicit diagnostic runtime handles. */
        if(rf_geometry_collision_movers_open(&source,ids,8*1024*1024,&owned))return 5;
        if(rf_geometry_collision_movers_open(&source,ids,owned.peak_bytes,&exact))return 6;
        rf_geometry_collision_movers_close(&exact);
        memset(&guard,0xa5,sizeof(guard));exact=guard;
        if(rf_geometry_collision_movers_open(&source,ids,owned.peak_bytes-1,&exact)!=RF_RANGE || memcmp(&guard,&exact,sizeof(guard)))return 7;
        rf_geometry_movers_close(&source);rf_vpp_close(&archive);
        if(strcmp(argv[1],"--bound-movers")) {
            rf_group_translation_runtime runtime={0};rf_level_group_key first={0};rf_group_controller_view controller={0};
            runtime.motion.flags=8;runtime.pending[0]=1;runtime.pending[1]=2;runtime.pending[2]=3;
            controller.runtime=&runtime;controller.first_key=&first;controller.mover_handles=ids;controller.mover_count=owned.count;
            if(owned.count) {
                rf_group_controller_view bad[2]={controller,controller};rf_group_translation_runtime rotation=runtime;
                size_t pose_bytes=owned.count*sizeof(*owned.poses),view_bytes=owned.count*sizeof(*owned.views);
                unsigned char *snapshot=malloc(pose_bytes+view_bytes);if(!snapshot)return 11;
                memcpy(snapshot,owned.poses,pose_bytes);memcpy(snapshot+pose_bytes,owned.views,view_bytes);
                rotation.motion.flags|=4;bad[1].runtime=&rotation;bad[1].mover_handles=ids+owned.count-1;bad[1].mover_count=1;
                if(rf_geometry_collision_movers_propagate(&owned,bad,2,.25f,1)!=RF_RANGE ||
                    memcmp(snapshot,owned.poses,pose_bytes) || memcmp(snapshot+pose_bytes,owned.views,view_bytes))return 12;
                free(snapshot);
            }
            if(!strcmp(argv[1],"--committed-movers")) {
                rf_group_attached_pose controller_pose={0};rf_group_pose_slot *slots;
                if(owned.count>1024)return 13;
                slots=calloc(owned.count?owned.count:1,sizeof(*slots));if(!slots)return 14;
                for(i=0;i<owned.count;i++) {slots[i].handle=ids[i];slots[i].pose=owned.poses+i;}
                if(rf_geometry_collision_movers_propagate(&owned,&controller,1,.25f,0))return 15;
                for(i=0;i<owned.count;i++)if(memcmp(owned.views[i].input_origin,owned.poses[i].base_position,12))return 16;
                memcpy(controller_pose.pending,runtime.pending,12);
                if(rf_group_commit_positions(&runtime.motion.flags,&controller_pose,&controller,slots,owned.count) || runtime.motion.flags)return 17;
                memcpy(runtime.position,controller_pose.position,12);runtime.object_flags=controller_pose.flags;
                if(rf_geometry_collision_movers_sync(&owned))return 18;
                free(slots);
            } else if(rf_geometry_collision_movers_propagate(&owned,&controller,1,.25f,1))return 10;
        }
        free(ids);
        if(fwrite(&owned.count,4,1,stdout)!=1 || fwrite(&owned.allocated_bytes,4,1,stdout)!=1 || fwrite(&owned.peak_bytes,4,1,stdout)!=1)return 8;
        for(i=0;i<owned.count;i++) {
            rf_collision_solid_view *view=owned.views+i;
            if(view->flat_faces!=owned.owned[i].faces || view->flat_count!=owned.owned[i].count ||
                view->rooms || view->room_count || view->primary || view->primary_count || view->children || view->child_count)return 9;
            if(fwrite(owned.uids+i,4,1,stdout)!=1 || fwrite(&view->object_id,4,1,stdout)!=1 ||
                fwrite(&view->flat_count,4,1,stdout)!=1 || fwrite(view->minimum,120,1,stdout)!=1 ||
                fwrite(owned.poses+i,sizeof(*owned.poses),1,stdout)!=1)return 8;
        }
        rf_geometry_collision_movers_close(&owned);rf_geometry_collision_movers_close(&owned);return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--vertex-bounds")) {
        uint32_t count;float (*vertices)[3];struct {int32_t status;rf_collision_bounds bounds;} out;
        while(fread(&count,4,1,stdin)==1) {
            if(count>65536)return 2;
            vertices=(float(*)[3])malloc(count?count*12:12);if(!vertices)return 3;
            if(fread(vertices,12,count,stdin)!=count)return 4;
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_vertex_bounds(vertices,count,&out.bounds);free(vertices);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 5;
        }
        return ferror(stdin)?6:0;
    }
    if(argc==4 && !strcmp(argv[1],"--mover-queries")) {
        rf_vpp archive;rf_level level;rf_geometry_movers movers={0};
        rf_geometry_collision_flat *owned;uint32_t i,count;
        struct {uint32_t mover,flags;float start[3],delta[3],origin[3],matrix[3][3],radius,limit;} in;
        struct {int32_t status;uint32_t matched;rf_collision_sweep_tree_hit hit;} out;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]))return 2;
        if(rf_geometry_movers_open(&level,8*1024*1024,&movers))return 3;
        count=movers.count;owned=(rf_geometry_collision_flat *)calloc(count?count:1,sizeof(*owned));if(!owned)return 4;
        for(i=0;i<count;i++)if(rf_geometry_collision_flat_open(&movers.items[i].geometry,8*1024*1024,owned+i))return 5;
        rf_geometry_movers_close(&movers);rf_vpp_close(&archive);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));
            out.status=in.mover>=count?RF_RANGE:rf_collision_flat_faces(owned[in.mover].faces,owned[in.mover].count,
                in.flags,in.start,in.delta,in.origin,in.matrix,in.radius,in.limit,&out.hit,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 6;
        }
        for(i=0;i<count;i++)rf_geometry_collision_flat_close(owned+i);
        free(owned);return ferror(stdin)?7:0;
    }
    if(argc==4 && !strcmp(argv[1],"--mover-faces")) {
        rf_vpp archive;rf_level level;rf_geometry_movers movers={0};
        rf_geometry_collision_flat *owned;uint32_t i,j,count,peak=0;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]))return 2;
        if(rf_geometry_movers_open(&level,8*1024*1024,&movers))return 3;
        count=movers.count;owned=(rf_geometry_collision_flat *)calloc(count?count:1,sizeof(*owned));if(!owned)return 4;
        for(i=0;i<count;i++) {
            rf_geometry_collision_flat exact={0},guard;
            if(rf_geometry_collision_flat_open(&movers.items[i].geometry,8*1024*1024,owned+i))return 5;
            if(owned[i].allocated_bytes>peak)peak=owned[i].allocated_bytes;
            if(rf_geometry_collision_flat_open(&movers.items[i].geometry,owned[i].allocated_bytes,&exact))return 6;
            rf_geometry_collision_flat_close(&exact);
            memset(&guard,0xa5,sizeof(guard));exact=guard;
            if(rf_geometry_collision_flat_open(&movers.items[i].geometry,owned[i].allocated_bytes-1,&exact)!=RF_RANGE || memcmp(&guard,&exact,sizeof(guard)))return 7;
        }
        rf_geometry_movers_close(&movers);rf_vpp_close(&archive);
        if(fwrite(&count,4,1,stdout)!=1 || fwrite(&peak,4,1,stdout)!=1)return 8;
        for(i=0;i<count;i++) {
            if(fwrite(&owned[i].count,4,1,stdout)!=1)return 8;
            for(j=0;j<owned[i].count;j++) {
                rf_collision_face *face=owned[i].faces+j;
                if(fwrite(&j,4,1,stdout)!=1 || fwrite(&face->count,4,1,stdout)!=1 ||
                    fwrite(face->plane,40,1,stdout)!=1 || fwrite(&face->filter,24,1,stdout)!=1 ||
                    fwrite(face->vertices,12,face->count,stdout)!=face->count)return 8;
            }
            rf_geometry_collision_flat_close(owned+i);
        }
        free(owned);return 0;
    }
    if(argc==4 && !strcmp(argv[1],"--movers")) {
        rf_vpp archive;rf_level level;rf_geometry_movers m={0},exact={0},guard;
        uint32_t i,budget;int status;
        if(rf_vpp_open(&archive,argv[2]))return 2;
        status=rf_level_open(&level,&archive,argv[3]);
        if(!status)status=rf_geometry_movers_open(&level,8*1024*1024,&m);
        if(status)return 3;
        budget=m.allocated_bytes;
        if(rf_geometry_movers_open(&level,budget,&exact))return 4;
        rf_geometry_movers_close(&exact);
        memset(&guard,0xa5,sizeof(guard));exact=guard;
        if(rf_geometry_movers_open(&level,budget-1,&exact)!=RF_RANGE || memcmp(&exact,&guard,sizeof(exact)))return 5;
        for(i=0;i<m.count;i++) {
            rf_level truncated=level;uint32_t j,k;
            uint32_t cuts[4]={m.items[i].offset,m.items[i].geometry_offset-1,
                m.items[i].geometry_offset+m.items[i].geometry.bytes-1,
                m.items[i].offset+m.items[i].bytes-1};
            for(j=0;j<4;j++) {
                for(k=0;k<truncated.section_count;k++)if(truncated.sections[k].type==0x2000)truncated.sections[k].size=cuts[j];
                exact=guard;
                if(rf_geometry_movers_open(&truncated,8*1024*1024,&exact)!=RF_FORMAT || memcmp(&exact,&guard,sizeof(exact)))return 8;
            }
        }
        rf_vpp_close(&archive); /* All geometry access below must be owned. */
        printf("%u %u\n",m.count,budget);
        for(i=0;i<m.count;i++) {
            rf_geometry_mover *item=m.items+i;rf_geometry *g=&item->geometry;uint32_t j;
            printf("%d %u %u %u %u %u %u %u %u %u",item->uid,item->offset,item->bytes,item->geometry_offset,g->bytes,g->textures,g->rooms,g->vertices,g->faces,g->mappings);
            for(j=0;j<3;j++)printf(" %u",item->trailer[j]);
            for(j=0;j<3;j++)printf(" %.9g",item->position[j]);
            for(j=0;j<9;j++)printf(" %.9g",item->orientation[j/3][j%3]);
            printf("\n");
            for(j=0;j<g->faces;j++) {
                rf_geometry_face face;uint32_t k;
                if(rf_geometry_get_face(g,j,&face))return 6;
                for(k=0;k<face.corners;k++) {
                    rf_geometry_corner corner;float vertex[3];
                    if(rf_geometry_get_corner(g,j,k,&corner) || rf_geometry_vertex(g,corner.vertex,vertex))return 7;
                }
            }
        }
        rf_geometry_movers_close(&m);rf_geometry_movers_close(&m);return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--flat-query")) {
        struct {float z[4];uint32_t count,flags;float start[3],delta[3],radius,limit,origin[3],matrix[3][3];} in;
        struct {int32_t status;uint32_t matched;rf_collision_sweep_tree_hit hit;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_face faces[4]={0};float vertices[4][4][3];uint32_t i,j;
            for(i=0;i<4;i++) {
                faces[i].plane[2]=1;faces[i].plane[3]=-in.z[i];faces[i].vertices=vertices[i];faces[i].count=4;
                for(j=0;j<3;j++) {faces[i].minimum[j]=j==2?in.z[i]-.0001f:-2.0001f;faces[i].maximum[j]=j==2?in.z[i]+.0001f:2.0001f;}
                for(j=0;j<4;j++) {vertices[i][j][0]=(j==0 || j==3)?-2:2;vertices[i][j][1]=j<2?-2:2;vertices[i][j][2]=in.z[i];}
            }
            memset(&out,0xa5,sizeof(out));out.status=in.count>4?RF_RANGE:rf_collision_flat_faces(faces,in.count,in.flags,in.start,in.delta,in.origin,in.matrix,in.radius,in.limit,&out.hit,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--solid-ray-posed")) {
        struct {float z[3];uint32_t enabled[3],flags;float start[3],end[3],poses[2][30];uint32_t want_result;} in;
        struct {int32_t status;uint32_t matched;rf_collision_solid_hit hit;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_solid_view solids[3]={0};rf_collision_room_view rooms[3]={0};rf_collision_tree trees[3]={0};
            rf_collision_node nodes[3]={0};rf_collision_face faces[3]={0};float vertices[3][4][3];uint32_t primary=0,work[3],i,j;
            for(i=0;i<3;i++) {
                float z=in.z[i];rf_collision_solid_view *solid=solids+i;
                solid->rooms=rooms+i;solid->room_count=1;solid->primary=&primary;solid->primary_count=1;solid->object_id=100+i;
                solid->input_matrix[0][0]=solid->input_matrix[1][1]=solid->input_matrix[2][2]=1;
                memcpy(solid->output_matrix,solid->input_matrix,36);
                for(j=0;j<3;j++) {
                    solid->minimum[j]=rooms[i].minimum[j]=nodes[i].minimum[j]=faces[i].minimum[j]=j==2?z-.0001f:-3;
                    solid->maximum[j]=rooms[i].maximum[j]=nodes[i].maximum[j]=faces[i].maximum[j]=j==2?z+.0001f:3;
                }
                rooms[i].tree=trees+i;trees[i].nodes=nodes+i;trees[i].node_count=trees[i].node_capacity=1;trees[i].faces=faces+i;trees[i].face_count=1;trees[i].stack=work+i;
                nodes[i].face_count=in.enabled[i];nodes[i].left=nodes[i].right=UINT32_MAX;
                faces[i].plane[2]=1;faces[i].plane[3]=-z;faces[i].vertices=vertices[i];faces[i].count=4;
                for(j=0;j<4;j++) {vertices[i][j][0]=(j==0 || j==3)?-3:3;vertices[i][j][1]=j<2?-3:3;vertices[i][j][2]=z;}
            }
            for(i=0;i<2;i++)memcpy(solids[i].minimum,in.poses[i],120);
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_ray_solids(solids,2,solids+2,in.start,in.end,in.flags,in.want_result?&out.hit:NULL,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--solid-ray-flat")) {
        struct {float z[3];uint32_t enabled[3],flags;float start[3],end[3];} in;
        struct {int32_t status;uint32_t matched;rf_collision_solid_hit hit;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_solid_view solids[3]={0};rf_collision_room_view rooms[3]={0};rf_collision_tree trees[3]={0};
            rf_collision_node nodes[3]={0};rf_collision_face faces[3]={0};float vertices[3][4][3];uint32_t primary=0,work[3],i,j;
            for(i=0;i<3;i++) {
                float z=in.z[i];rf_collision_solid_view *solid=solids+i;
                solid->rooms=rooms+i;solid->room_count=1;solid->primary=&primary;solid->primary_count=1;solid->object_id=100+i;
                solid->input_matrix[0][0]=solid->input_matrix[1][1]=solid->input_matrix[2][2]=1;
                memcpy(solid->output_matrix,solid->input_matrix,36);
                for(j=0;j<3;j++) {
                    solid->minimum[j]=rooms[i].minimum[j]=nodes[i].minimum[j]=faces[i].minimum[j]=j==2?z-.0001f:-3;
                    solid->maximum[j]=rooms[i].maximum[j]=nodes[i].maximum[j]=faces[i].maximum[j]=j==2?z+.0001f:3;
                }
                rooms[i].tree=trees+i;trees[i].nodes=nodes+i;trees[i].node_count=trees[i].node_capacity=1;trees[i].faces=faces+i;trees[i].face_count=1;trees[i].stack=work+i;
                nodes[i].face_count=in.enabled[i];nodes[i].left=nodes[i].right=UINT32_MAX;
                faces[i].plane[2]=1;faces[i].plane[3]=-z;faces[i].vertices=vertices[i];faces[i].count=4;
                for(j=0;j<4;j++) {vertices[i][j][0]=(j==0 || j==3)?-3:3;vertices[i][j][1]=j<2?-3:3;vertices[i][j][2]=z;}
            }
            for(i=0;i<3;i++) {solids[i].room_count=0;solids[i].flat_faces=faces+i;solids[i].flat_count=in.enabled[i];}
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_ray_solids(solids,2,solids+2,in.start,in.end,in.flags,&out.hit,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--solid-ray")) {
        struct {float z[3];uint32_t enabled[3],flags;float start[3],end[3];} in;
        struct {int32_t status;uint32_t matched;rf_collision_solid_hit hit;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_solid_view solids[3]={0};rf_collision_room_view rooms[3]={0};rf_collision_tree trees[3]={0};
            rf_collision_node nodes[3]={0};rf_collision_face faces[3]={0};float vertices[3][4][3];uint32_t primary=0,work[3],i,j;
            for(i=0;i<3;i++) {
                float z=in.z[i];rf_collision_solid_view *solid=solids+i;
                solid->rooms=rooms+i;solid->room_count=1;solid->primary=&primary;solid->primary_count=1;solid->object_id=100+i;
                solid->input_matrix[0][0]=solid->input_matrix[1][1]=solid->input_matrix[2][2]=1;
                memcpy(solid->output_matrix,solid->input_matrix,36);
                for(j=0;j<3;j++) {
                    solid->minimum[j]=rooms[i].minimum[j]=nodes[i].minimum[j]=faces[i].minimum[j]=j==2?z-.0001f:-3;
                    solid->maximum[j]=rooms[i].maximum[j]=nodes[i].maximum[j]=faces[i].maximum[j]=j==2?z+.0001f:3;
                }
                rooms[i].tree=trees+i;trees[i].nodes=nodes+i;trees[i].node_count=trees[i].node_capacity=1;trees[i].faces=faces+i;trees[i].face_count=1;trees[i].stack=work+i;
                nodes[i].face_count=in.enabled[i];nodes[i].left=nodes[i].right=UINT32_MAX;
                faces[i].plane[2]=1;faces[i].plane[3]=-z;faces[i].vertices=vertices[i];faces[i].count=4;
                for(j=0;j<4;j++) {vertices[i][j][0]=(j==0 || j==3)?-3:3;vertices[i][j][1]=j<2?-3:3;vertices[i][j][2]=z;}
            }
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_ray_solids(solids,2,solids+2,in.start,in.end,in.flags,&out.hit,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--contact-world")) {
        struct {rf_collision_ray_hit local;float origin[3],matrix[3][3];} in;
        struct {int32_t status;rf_collision_ray_hit world;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_contact_world(&in.local,in.origin,in.matrix,&out.world);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--query-local")) {
        struct {float start[3],delta[3],origin[3],matrix[3][3];uint32_t flags;} in;
        struct {int32_t status;uint32_t active;float start[3],delta[3];} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_query_local(in.start,in.delta,in.origin,in.matrix,in.flags,out.start,out.delta,&out.active);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--sphere-edge")) {
        struct {float start[3],delta[3],radius,a[3],b[3],limit;} in;
        struct {int32_t status;uint32_t hit;float fraction,point[3];} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_sphere_edge(in.start,in.delta,in.radius,in.a,in.b,in.limit,&out.fraction,out.point,&out.hit);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--sphere-plane")) {
        struct {float start[3],delta[3],radius,plane[4];} in;
        struct {int32_t status;uint32_t hit;float fraction,point[3];} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_sphere_plane(in.start,in.delta,in.radius,in.plane,&out.fraction,out.point,&out.hit);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==4 && !strcmp(argv[1],"--world-ray-at")) {
        rf_vpp archive;rf_level level;rf_geometry geometry;rf_geometry_collision_world world={0};
        float start[3],delta[3];unsigned flags;int fields;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]) ||
           rf_geometry_open(&geometry,&level,8u*1024u*1024u) ||
           rf_geometry_collision_world_open(&geometry,8u*1024u*1024u,&world))return 3;
        while((fields=scanf("%f %f %f %f %f %f %u",start,start+1,start+2,delta,delta+1,delta+2,&flags))==7) {
            rf_geometry_world_hit hit={0};rf_geometry_face face={0};uint32_t matched=0;
            int status=rf_geometry_collision_world_ray(&world,flags,start,delta,1,&hit,&matched);
            if(!status && matched && rf_geometry_get_face(&geometry,hit.face,&face))return 4;
            printf("%d %u %u %u %.9g %.9g %.9g %.9g %.9g %.9g %.9g %u %u\n",
                status,matched,hit.face,hit.room,hit.hit.fraction,hit.hit.point[0],hit.hit.point[1],hit.hit.point[2],
                hit.hit.normal[0],hit.hit.normal[1],hit.hit.normal[2],face.flags,face.portal);
        }
        rf_geometry_collision_world_close(&world);rf_geometry_close(&geometry);rf_vpp_close(&archive);
        return fields==EOF?0:5;
    }
    if(argc==4 && !strcmp(argv[1],"--world")) {
        rf_vpp archive;rf_level level;rf_geometry geometry;rf_geometry_collision_world world={0},guard,sentinel;
        uint32_t i,j,k,pass,faces=0,queries=0,hits=0,errors=0,hashes[2]={2166136261u,2166136261u},geometry_bytes;void *poison=NULL;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]) || rf_geometry_open(&geometry,&level,8u*1024u*1024u))return 3;
        if(rf_geometry_collision_world_open(&geometry,8u*1024u*1024u,&world))return 4;
        memset(&guard,0xa5,sizeof(guard));sentinel=guard;
        if(rf_geometry_collision_world_open(&geometry,world.peak_bytes-1,&guard)!=RF_RANGE || memcmp(&guard,&sentinel,sizeof(guard)))return 5;
        if(rf_geometry_collision_world_open(&geometry,world.peak_bytes,&guard))return 6;
        rf_geometry_collision_world_close(&guard);
        for(i=0;i<world.room_count;i++) {
            const rf_collision_tree *tree=&world.rooms[i].tree;
            if(world.views[i].skip!=geometry.data[geometry.room_offsets[i]+28])return 7;
            if(world.views[i].tree!=tree || memcmp(world.views[i].minimum,world.rooms[i].minimum,24))return 7;
            faces+=tree->face_count;
            for(j=0;j<tree->face_count;j++) {rf_geometry_face face;if(rf_geometry_get_face(&geometry,tree->source_indices[j],&face) || face.room!=i)return 8;}
        }
        if(faces!=geometry.faces)return 9;
        geometry_bytes=geometry.bytes;
        for(pass=0;pass<2;pass++) {
            for(i=0;i<world.room_count;i++) {
                const rf_collision_tree *tree=&world.rooms[i].tree;const rf_collision_face *face;float start[3]={0},delta[3];
                struct {int32_t status;uint32_t matched;rf_geometry_world_hit hit;} out;const unsigned char *bytes=(const unsigned char*)&out;
                if(!tree->face_count)continue;face=tree->faces;
                for(j=0;j<face->count;j++)for(k=0;k<3;k++)start[k]+=face->vertices[j][k];
                for(k=0;k<3;k++) {start[k]=start[k]/face->count+face->plane[k]+.0037f*(k+1);delta[k]=-2*face->plane[k]+.0013f*(k+1);}
                memset(&out,0xa5,sizeof(out));out.status=rf_geometry_collision_world_ray(&world,0x460,start,delta,1,&out.hit,&out.matched);
                if(!pass) {queries++;errors+=out.status!=0;if(!out.status)hits+=out.matched;}
                if(!out.status && out.matched && (out.hit.face>=faces || out.hit.room>=world.room_count))return 10;
                for(j=0;j<sizeof(out);j++)hashes[pass]=(hashes[pass]^bytes[j])*16777619u;
            }
            if(!pass) {rf_geometry_close(&geometry);poison=malloc(geometry_bytes);if(!poison)return 11;memset(poison,0xdd,geometry_bytes);}
        }
        if(hashes[0]!=hashes[1])return 12;
        printf("%u %u %u %u %u %u %u %u %u %u\n",world.room_count,faces,world.primary_count,world.child_count,world.allocated_bytes,world.peak_bytes,queries,hits,errors,hashes[0]);
        free(poison);rf_geometry_collision_world_close(&world);rf_vpp_close(&archive);return 0;
    }
    if(argc==4 && !strcmp(argv[1],"--world-sweep")) {
        rf_vpp archive;rf_level level;rf_geometry geometry;rf_geometry_collision_world world={0},guard,sentinel;
        uint32_t i,j,k,q,pass,edge_hits=0,faces=0,queries=0,hits=0,errors=0,hashes[2]={2166136261u,2166136261u},geometry_bytes;void *poison=NULL;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]) || rf_geometry_open(&geometry,&level,8u*1024u*1024u))return 3;
        if(rf_geometry_collision_world_open(&geometry,8u*1024u*1024u,&world))return 4;
        memset(&guard,0xa5,sizeof(guard));sentinel=guard;
        if(rf_geometry_collision_world_open(&geometry,world.peak_bytes-1,&guard)!=RF_RANGE || memcmp(&guard,&sentinel,sizeof(guard)))return 5;
        if(rf_geometry_collision_world_open(&geometry,world.peak_bytes,&guard))return 6;
        rf_geometry_collision_world_close(&guard);
        for(i=0;i<world.room_count;i++) {
            const rf_collision_tree *tree=&world.rooms[i].tree;
            if(world.views[i].tree!=tree || memcmp(world.views[i].minimum,world.rooms[i].minimum,24))return 7;
            faces+=tree->face_count;
            for(j=0;j<tree->face_count;j++) {rf_geometry_face face;if(rf_geometry_get_face(&geometry,tree->source_indices[j],&face) || face.room!=i)return 8;}
        }
        if(faces!=geometry.faces)return 9;
        geometry_bytes=geometry.bytes;
        for(pass=0;pass<2;pass++) {
            for(i=0;i<world.room_count;i++)for(q=0;q<3;q++) {
                const rf_collision_tree *tree=&world.rooms[i].tree;const rf_collision_face *face;float start[3]={0},delta[3];
                struct {int32_t status;uint32_t matched;rf_geometry_world_sweep_hit hit;} out;const unsigned char *bytes=(const unsigned char*)&out;
                if(!tree->face_count)continue;face=tree->faces;
                for(j=0;j<face->count;j++)for(k=0;k<3;k++)start[k]+=face->vertices[j][k];
                for(k=0;k<3;k++) {
                    float anchor=q==0?start[k]/face->count:q==1?face->vertices[0][k]:(face->vertices[0][k]+face->vertices[1%face->count][k])*.5f;
                    start[k]=anchor+face->plane[k]+.0037f*(k+1);delta[k]=-2*face->plane[k]+.0013f*(k+1);
                }
                memset(&out,0xa5,sizeof(out));out.status=rf_geometry_collision_world_sweep(&world,0x460,start,delta,.25f*(q+1),1,&out.hit,&out.matched);
                if(!pass) {queries++;errors+=out.status!=0;if(!out.status) {hits+=out.matched;if(out.matched)edge_hits+=out.hit.edge!=0;}}
                if(!out.status && out.matched) {
                    const rf_collision_tree *target;uint32_t found=0;
                    if(out.hit.face>=faces || out.hit.room>=world.room_count)return 10;
                    target=&world.rooms[out.hit.room].tree;
                    for(j=0;j<target->face_count;j++)if(target->source_indices[j]==out.hit.face)found=1;
                    if(!found)return 13;
                }
                for(j=0;j<sizeof(out);j++)hashes[pass]=(hashes[pass]^bytes[j])*16777619u;
            }
            if(!pass) {rf_geometry_close(&geometry);poison=malloc(geometry_bytes);if(!poison)return 11;memset(poison,0xdd,geometry_bytes);}
        }
        if(hashes[0]!=hashes[1])return 12;
        printf("%u %u %u %u %u %u %u %u %u %u %u\n",world.room_count,faces,world.primary_count,world.child_count,world.allocated_bytes,world.peak_bytes,queries,hits,errors,hashes[0],edge_hits);
        free(poison);rf_geometry_collision_world_close(&world);rf_vpp_close(&archive);return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--transformed-rooms")) {
        struct {struct {float bounds[6],z;uint32_t skip,first,count;} rooms[4];uint32_t primary[2],children[4];float start[3],delta[3],limit;uint32_t flags;float radius,origin[3],matrix[3][3];} in;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_room_view rooms[4];rf_collision_tree trees[4];rf_collision_node nodes[4];rf_collision_face faces[4];float vertices[4][4][3];uint32_t stacks[4],i,j;
            struct {int32_t status;uint32_t matched;rf_collision_sweep_room_hit hit;} out;
            memset(trees,0,sizeof(trees));memset(faces,0,sizeof(faces));
            for(i=0;i<4;i++) {
                float z=in.rooms[i].z;
                memcpy(rooms[i].minimum,in.rooms[i].bounds,24);rooms[i].skip=in.rooms[i].skip;rooms[i].first_child=in.rooms[i].first;rooms[i].child_count=in.rooms[i].count;rooms[i].tree=trees+i;
                trees[i].nodes=nodes+i;trees[i].node_count=trees[i].node_capacity=1;trees[i].faces=faces+i;trees[i].face_count=1;trees[i].stack=stacks+i;
                for(j=0;j<3;j++) {nodes[i].minimum[j]=faces[i].minimum[j]=j==2?z-.0001f:-2.0001f;nodes[i].maximum[j]=faces[i].maximum[j]=j==2?z+.0001f:2.0001f;}
                nodes[i].first_face=0;nodes[i].face_count=1;nodes[i].left=nodes[i].right=UINT32_MAX;
                faces[i].plane[2]=1;faces[i].plane[3]=-z;faces[i].vertices=vertices[i];faces[i].count=4;
                for(j=0;j<4;j++) {vertices[i][j][0]=(j==0 || j==3)?-2:2;vertices[i][j][1]=j<2?-2:2;vertices[i][j][2]=z;}
            }
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_transformed_rooms(rooms,4,in.primary,2,in.children,4,in.flags,in.start,in.delta,in.origin,in.matrix,in.radius,in.limit,&out.hit,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--sweep-rooms")) {
        struct {struct {float bounds[6],z;uint32_t skip,first,count;} rooms[4];uint32_t primary[2],children[4];float start[3],delta[3],limit;uint32_t flags;float radius;} in;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_room_view rooms[4];rf_collision_tree trees[4];rf_collision_node nodes[4];rf_collision_face faces[4];float vertices[4][4][3];uint32_t stacks[4],i,j;
            struct {int32_t status;uint32_t matched;rf_collision_sweep_room_hit hit;} out;
            memset(trees,0,sizeof(trees));memset(faces,0,sizeof(faces));
            for(i=0;i<4;i++) {
                float z=in.rooms[i].z;
                memcpy(rooms[i].minimum,in.rooms[i].bounds,24);rooms[i].skip=in.rooms[i].skip;rooms[i].first_child=in.rooms[i].first;rooms[i].child_count=in.rooms[i].count;rooms[i].tree=trees+i;
                trees[i].nodes=nodes+i;trees[i].node_count=trees[i].node_capacity=1;trees[i].faces=faces+i;trees[i].face_count=1;trees[i].stack=stacks+i;
                for(j=0;j<3;j++) {nodes[i].minimum[j]=faces[i].minimum[j]=j==2?z-.0001f:-2.0001f;nodes[i].maximum[j]=faces[i].maximum[j]=j==2?z+.0001f:2.0001f;}
                nodes[i].first_face=0;nodes[i].face_count=1;nodes[i].left=nodes[i].right=UINT32_MAX;
                faces[i].plane[2]=1;faces[i].plane[3]=-z;faces[i].vertices=vertices[i];faces[i].count=4;
                for(j=0;j<4;j++) {vertices[i][j][0]=(j==0 || j==3)?-2:2;vertices[i][j][1]=j<2?-2:2;vertices[i][j][2]=z;}
            }
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_sweep_rooms(rooms,4,in.primary,2,in.children,4,in.flags,in.start,in.delta,in.radius,in.limit,&out.hit,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--room-query")) {
        struct {struct {float bounds[6],z;uint32_t skip,first,count;} rooms[4];uint32_t primary[2],children[4];float start[3],delta[3],limit;uint32_t flags;} in;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_room_view rooms[4];rf_collision_tree trees[4];rf_collision_node nodes[4];rf_collision_face faces[4];float vertices[4][4][3];uint32_t stacks[4],i,j;
            struct {int32_t status;uint32_t matched;rf_collision_room_hit hit;} out;
            memset(trees,0,sizeof(trees));memset(faces,0,sizeof(faces));
            for(i=0;i<4;i++) {
                float z=in.rooms[i].z;
                memcpy(rooms[i].minimum,in.rooms[i].bounds,24);rooms[i].skip=in.rooms[i].skip;rooms[i].first_child=in.rooms[i].first;rooms[i].child_count=in.rooms[i].count;rooms[i].tree=trees+i;
                trees[i].nodes=nodes+i;trees[i].node_count=trees[i].node_capacity=1;trees[i].faces=faces+i;trees[i].face_count=1;trees[i].stack=stacks+i;
                for(j=0;j<3;j++) {nodes[i].minimum[j]=faces[i].minimum[j]=j==2?z-.0001f:-2.0001f;nodes[i].maximum[j]=faces[i].maximum[j]=j==2?z+.0001f:2.0001f;}
                nodes[i].first_face=0;nodes[i].face_count=1;nodes[i].left=nodes[i].right=UINT32_MAX;
                faces[i].plane[2]=1;faces[i].plane[3]=-z;faces[i].vertices=vertices[i];faces[i].count=4;
                for(j=0;j<4;j++) {vertices[i][j][0]=(j==0 || j==3)?-2:2;vertices[i][j][1]=j<2?-2:2;vertices[i][j][2]=z;}
            }
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_thin_rooms(rooms,4,in.primary,2,in.children,4,in.flags,in.start,in.delta,in.limit,&out.hit,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==4 && (!strcmp(argv[1],"--rooms") || !strcmp(argv[1],"--room-layout") || !strcmp(argv[1],"--room-layout-tight"))) {
        int tight=!strcmp(argv[1],"--room-layout-tight"),layout=strcmp(argv[1],"--rooms")!=0;
        rf_vpp archive;rf_level level;rf_geometry geometry;uint32_t room,total=0,nodes=0,peak=0,queries=0,hits=0;
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]) || rf_geometry_open(&geometry,&level,8u*1024u*1024u))return 3;
        for(room=0;room<geometry.rooms;room++) {
            rf_geometry_collision_room owned={0},guard,sentinel;uint32_t i,j,k;
            if(tight)memset(geometry.data+geometry.room_offsets[room]+4,0,24);
            if(rf_geometry_collision_room_open(&geometry,room,8u*1024u*1024u,&owned))return 4;
            memset(&guard,0xa5,sizeof(guard));sentinel=guard;
            if(rf_geometry_collision_room_open(&geometry,room,owned.peak_bytes-1,&guard)!=RF_RANGE || memcmp(&guard,&sentinel,sizeof(guard)))return 5;
            if(rf_geometry_collision_room_open(&geometry,room,owned.peak_bytes,&guard))return 6;
            rf_geometry_collision_room_close(&guard);
            total+=owned.tree.face_count;nodes+=owned.tree.node_count;if(owned.peak_bytes>peak)peak=owned.peak_bytes;
            if(layout) {
                if(fwrite(geometry.data+geometry.room_offsets[room]+4,24,1,stdout)!=1 || fwrite(owned.minimum,24,1,stdout)!=1 || fwrite(&owned.tree.face_count,4,1,stdout)!=1)return 14;
                for(i=0;i<owned.tree.face_count;i++)if(fwrite(owned.tree.source_indices+i,4,1,stdout)!=1 || fwrite(owned.tree.faces[i].minimum,24,1,stdout)!=1)return 14;
                rf_geometry_collision_room_close(&owned);continue;
            }
            for(i=0;i<owned.tree.face_count;i++) {
                rf_geometry_face original;rf_collision_face *face=owned.tree.faces+i;
                float start[3]={0},delta[3],limit=1;uint32_t matched,brute=0;
                rf_collision_tree_hit result;rf_collision_ray_hit hit;
                if(rf_geometry_get_face(&geometry,owned.tree.source_indices[i],&original) || original.room!=room || original.corners!=face->count)return 7;
                for(j=0;j<i;j++)if(owned.tree.source_indices[j]==owned.tree.source_indices[i])return 8;
                for(j=0;j<face->count;j++) {
                    rf_geometry_corner corner;float position[3];
                    if(rf_geometry_get_corner(&geometry,owned.tree.source_indices[i],j,&corner) || rf_geometry_vertex(&geometry,corner.vertex,position) || memcmp(position,face->vertices[j],12))return 9;
                    for(k=0;k<3;k++)start[k]+=position[k];
                }
                /* Skew synthetic rays to avoid coplanar parallel rays against
                 * adjacent faces (the primitive intentionally preserves NaN). */
                for(j=0;j<3;j++) {start[j]=start[j]/face->count+face->plane[j]+0.0037f*(j+1);delta[j]=-2*face->plane[j]+0.0013f*(j+1);}
                /* Nearest mode: equal-depth identities can differ with traversal order. */
                for(j=0;j<owned.tree.face_count;j++) {
                    rf_collision_face candidate=owned.tree.faces[j];uint32_t accepted;
                    candidate.filter.query_flags=0x460;
                    {int status=rf_collision_thin_face(&candidate,start,delta,limit,&hit,&accepted);if(status) {fprintf(stderr,"room %u ray %u candidate %u status %d\n",room,i,j,status);return 10;}}
                    if(accepted) {brute=1;limit=hit.fraction;}
                }
                if(rf_collision_thin_tree(owned.tree.nodes,owned.tree.node_count,owned.tree.faces,owned.tree.face_count,0x460,start,delta,1,owned.tree.stack,owned.tree.node_capacity,&result,&matched))return 11;
                if(matched!=brute || (matched && result.hit.fraction!=limit))return 12;
                queries++;hits+=matched;
            }
            rf_geometry_collision_room_close(&owned);
        }
        if(total!=geometry.faces)return 13;
        if(!layout)printf("%u %u %u %u %u %u\n",geometry.rooms,total,nodes,peak,queries,hits);
        rf_geometry_close(&geometry);rf_vpp_close(&archive);return 0;
    }
    if(argc==4 && (!strcmp(argv[1],"--level") || !strcmp(argv[1],"--level-initial"))) {
        int initial=!strcmp(argv[1],"--level-initial");
        rf_vpp archive;rf_level level;rf_geometry geometry;float scratch[256][3];
        rf_collision_face_filter filter={0x461,0,0,0,0,0};uint32_t index,j,k;
        if(rf_vpp_open(&archive,argv[2]))return 3;
        if(rf_level_open(&level,&archive,argv[3])) {rf_vpp_close(&archive);return 3;}
        if(rf_geometry_open(&geometry,&level,8u*1024u*1024u)) {rf_vpp_close(&archive);return 3;}
        for(index=0;index<geometry.faces;index++) {
            rf_collision_face face,guard,sentinel;float start[3]={0},delta[3];
            struct {int32_t status;uint32_t matched;rf_collision_ray_hit hit;} out;
            if(initial && rf_geometry_initial_collision_filter(&geometry,index,0x461,&filter))return 6;
            if(rf_geometry_collision_face(&geometry,index,&filter,scratch,256,&face))return 4;
            memset(&guard,0xa5,sizeof(guard));memcpy(&sentinel,&guard,sizeof(guard));
            if(rf_geometry_collision_face(&geometry,index,&filter,scratch,face.count-1,&guard)!=RF_RANGE || memcmp(&guard,&sentinel,sizeof(guard)))return 5;
            for(j=0;j<3;j++) {
                for(k=0;k<face.count;k++)start[j]+=scratch[k][j];
                start[j]=start[j]/face.count+face.plane[j];delta[j]=-2*face.plane[j];
            }
            memset(&out.hit,0xa5,sizeof(out.hit));out.matched=0xa5a5a5a5;
            out.status=rf_collision_thin_face(&face,start,delta,1,&out.hit,&out.matched);
            if(initial && fwrite(&filter,sizeof(filter),1,stdout)!=1)return 2;
            if(fwrite(&index,4,1,stdout)!=1 || fwrite(&face.count,4,1,stdout)!=1 || fwrite(face.plane,4,4,stdout)!=4 ||
               fwrite(face.minimum,4,3,stdout)!=3 || fwrite(face.maximum,4,3,stdout)!=3 || fwrite(scratch,12,face.count,stdout)!=face.count ||
               fwrite(start,4,3,stdout)!=3 || fwrite(delta,4,3,stdout)!=3 || fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        rf_geometry_close(&geometry);rf_vpp_close(&archive);return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--build")) {
        struct {float faces[16][6];uint32_t count,budget;} in;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_face faces[16];rf_collision_tree tree={0};uint32_t i;int32_t status;
            memset(faces,0,sizeof(faces));for(i=0;i<16;i++)memcpy(faces[i].minimum,in.faces[i],24);
            status=in.count>16?RF_RANGE:rf_collision_tree_open(faces,in.count,in.budget,&tree);
            if(status) {
                rf_collision_tree guard,sentinel;
                memset(&guard,0xa5,sizeof(guard));memcpy(&sentinel,&guard,sizeof(guard));
                if(in.count<=16 && (rf_collision_tree_open(faces,in.count,in.budget,&guard)!=status || memcmp(&guard,&sentinel,sizeof(guard))))return 7;
            }
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(&tree.node_count,4,1,stdout)!=1 || fwrite(&tree.peak_bytes,4,1,stdout)!=1)return 2;
            if(!status) {
                if(fwrite(tree.nodes,sizeof(*tree.nodes),tree.node_count,stdout)!=tree.node_count || fwrite(tree.source_indices,4,in.count,stdout)!=in.count)return 2;
            }
            rf_collision_tree_close(&tree);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--partition")) {
        struct {float bounds[6],faces[8][6];} in;
        struct {int32_t status;uint32_t axis,counts[3];uint8_t labels[8];} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_node node;rf_collision_face faces[8];unsigned i;
            memcpy(node.minimum,in.bounds,24);
            for(i=0;i<8;i++)memcpy(faces[i].minimum,in.faces[i],24);
            memset(&out,0xa5,sizeof(out));out.status=rf_collision_partition(&node,faces,8,out.labels,&out.axis,out.counts);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--sweep-tree")) {
        struct {rf_collision_node nodes[3];float z[3],start[3],delta[3],limit;uint32_t flags;float normal_delta[3],radius;} in;
        struct {int32_t status;uint32_t matched;rf_collision_sweep_tree_hit result;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_face faces[3];float vertices[3][4][3];uint32_t stack[3],i,j;
            for(i=0;i<3;i++) {
                static const float xy[4][2]={{-2,-2},{2,-2},{2,2},{-2,2}};
                memset(faces+i,0,sizeof(faces[i]));faces[i].plane[2]=1;faces[i].plane[3]=-in.z[i];
                faces[i].minimum[0]=faces[i].minimum[1]=-2.0001f;faces[i].maximum[0]=faces[i].maximum[1]=2.0001f;
                faces[i].minimum[2]=in.z[i]-.0001f;faces[i].maximum[2]=in.z[i]+.0001f;
                for(j=0;j<4;j++) {vertices[i][j][0]=xy[j][0];vertices[i][j][1]=xy[j][1];vertices[i][j][2]=in.z[i];}
                faces[i].vertices=vertices[i];faces[i].count=4;
            }
            memset(&out.result,0xa5,sizeof(out.result));out.matched=0xa5a5a5a5;
            out.status=rf_collision_sweep_tree(in.nodes,3,faces,3,in.flags,in.start,in.delta,in.normal_delta,in.radius,in.limit,stack,3,&out.result,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--tree")) {
        struct {rf_collision_node nodes[3];float z[3],start[3],delta[3],limit;uint32_t flags;} in;
        struct {int32_t status;uint32_t matched;rf_collision_tree_hit result;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_face faces[3];float vertices[3][4][3];uint32_t stack[3],i,j;
            for(i=0;i<3;i++) {
                static const float xy[4][2]={{-2,-2},{2,-2},{2,2},{-2,2}};
                memset(faces+i,0,sizeof(faces[i]));faces[i].plane[2]=1;faces[i].plane[3]=-in.z[i];
                faces[i].minimum[0]=faces[i].minimum[1]=-2.0001f;faces[i].maximum[0]=faces[i].maximum[1]=2.0001f;
                faces[i].minimum[2]=in.z[i]-.0001f;faces[i].maximum[2]=in.z[i]+.0001f;
                for(j=0;j<4;j++) {vertices[i][j][0]=xy[j][0];vertices[i][j][1]=xy[j][1];vertices[i][j][2]=in.z[i];}
                faces[i].vertices=vertices[i];faces[i].count=4;
            }
            memset(&out.result,0xa5,sizeof(out.result));out.matched=0xa5a5a5a5;
            out.status=rf_collision_thin_tree(in.nodes,3,faces,3,in.flags,in.start,in.delta,in.limit,stack,3,&out.result,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--sweep")) {
        struct {float plane[4],lo[3],hi[3],vertices[8][3],start[3],delta[3],limit;rf_collision_face_filter filter;uint32_t count;float normal_delta[3],radius;} in;
        struct {int32_t status;uint32_t matched;rf_collision_sweep_hit hit;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_face face;
            memcpy(face.plane,in.plane,16);memcpy(face.minimum,in.lo,12);memcpy(face.maximum,in.hi,12);
            face.vertices=in.vertices;face.count=in.count;face.filter=in.filter;
            memset(&out.hit,0xa5,sizeof(out.hit));out.matched=0xa5a5a5a5;
            out.status=in.count>8?RF_RANGE:rf_collision_sweep_face(&face,in.start,in.delta,in.normal_delta,in.radius,in.limit,&out.hit,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--thin")) {
        struct {float plane[4],lo[3],hi[3],vertices[8][3],start[3],delta[3],limit;rf_collision_face_filter filter;uint32_t count;} in;
        struct {int32_t status;uint32_t matched;rf_collision_ray_hit hit;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_collision_face face;
            memcpy(face.plane,in.plane,16);memcpy(face.minimum,in.lo,12);memcpy(face.maximum,in.hi,12);
            face.vertices=in.vertices;face.count=in.count;face.filter=in.filter;
            memset(&out.hit,0xa5,sizeof(out.hit));out.matched=0xa5a5a5a5;
            out.status=in.count>8?RF_RANGE:rf_collision_thin_face(&face,in.start,in.delta,in.limit,&out.hit,&out.matched);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--filter")) {
        rf_collision_face_filter in;struct {int32_t status;uint32_t accepted;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.accepted=0xa5a5a5a5;out.status=rf_collision_face_accept(&in,&out.accepted);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--polygon")) {
        struct {float normal[3],point[3],vertices[16][3];uint32_t count;} in;
        struct {int32_t status;uint32_t inside;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.inside=0xa5a5a5a5;
            out.status=in.count>16?RF_RANGE:rf_collision_polygon_contains(in.normal,in.point,in.vertices,in.count,&out.inside);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--plane")) {
        struct {float start[3],delta[3],plane[4],fraction;} in;
        struct {int32_t status;uint32_t hit;float fraction;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.hit=0xa5a5a5a5;out.fraction=in.fraction;
            out.status=rf_collision_segment_plane(in.start,in.delta,in.plane,&out.fraction,&out.hit);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    while(fread(&input,sizeof(input),1,stdin)==1) {
        unsigned i;output.hit=0xa5a5a5a5;for(i=0;i<3;i++)output.point[i]=input.point[i];
        output.status=rf_collision_segment_box(input.lo,input.hi,input.start,input.end,output.point,&output.hit);
        if(fwrite(&output,sizeof(output),1,stdout)!=1)return 2;
    }
    return ferror(stdin)?2:0;
}
