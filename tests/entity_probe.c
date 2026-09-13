#include "rf/entity.h"
#include "rf/collision.h"
#include "rf/player.h"
#include "rf/level.h"
#include "rf/lightmap.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
#include "impact_process_probe.h"
#include "land_process_probe.h"
#include "navigation_select_probe.h"
#include "collision_pool_probe.h"
#include "actor_response_probe.h"
#include "actor_general_response_probe.h"
#include "actor_solid_response_probe.h"
#include "actor_model_response_probe.h"
#include "collision_process_probe.h"
#include "collision_discovery_probe.h"
#include "damage_effect_probe.h"
#include "pain_probe.h"
#include "burn_probe.h"
#include "burn_body_probe.h"
#include "burn_retirement_probe.h"
#include "burn_resource_probe.h"
#include "burn_retarget_probe.h"
#include "dying_probe.h"
#include "finalizer_probe.h"
#include "corpse_update_probe.h"
#include "corpse_surface_probe.h"
#include "corpse_delete_probe.h"
#include "corpse_create_probe.h"
#include "death_motion_probe.h"
#include "death_drop_probe.h"
#include "death_link_probe.h"
#include "death_tail_probe.h"
#include "death_early_probe.h"
static uint32_t death_clearance(void *context,uint32_t direction)
{uint32_t *v=context;++v[2];v[3]=direction;return v[direction];}
typedef struct death_ray_trace {uint32_t responses[4],count,points[24];} death_ray_trace;
static uint32_t death_ray(void *context,const float start[3],const float end[3])
{death_ray_trace *t=context;uint32_t i=t->count++;memcpy(t->points+6*i,start,12);memcpy(t->points+6*i+3,end,12);return t->responses[i];}
static rf_damage_object dispatch_object;
static uint32_t dispatch_facts[3],dispatch_present,dispatch_trace[8],dispatch_count,dispatch_effect[6];
static float dispatch_after;
static rf_damage_object *dispatch_lookup(void *context,uint32_t handle)
{(void)context;(void)handle;dispatch_trace[dispatch_count++]=0x40a0e0;return dispatch_present?&dispatch_object:NULL;}
static uint32_t dispatch_predicate(void *context,uint32_t stage,uint32_t handle,const rf_damage_object *object)
{static const uint32_t addresses[3]={0x426fc0,0x42cca0,0x48aaf0};(void)context;(void)handle;(void)object;dispatch_trace[dispatch_count++]=addresses[stage];return dispatch_facts[stage];}
static float dispatch_effect_call(void *context,rf_damage_object *object,float amount,uint32_t source,int32_t kind,uint32_t extra)
{uint32_t address=object->type==0?0x41a350:object->type==4?0x410270:0x417c60;(void)context;dispatch_trace[dispatch_count++]=address;dispatch_effect[0]=address;dispatch_effect[1]=0x30000000;memcpy(dispatch_effect+2,&amount,4);dispatch_effect[3]=source;dispatch_effect[4]=(uint32_t)kind;dispatch_effect[5]=extra;object->health=dispatch_after;return 7.25f;}
static uint32_t damage_sound_trace[3][8],damage_sound_count,damage_sound_mutation;
static int32_t damage_sound_sample,damage_sound_playing;
static rf_entity_damage_sound_state *damage_sound_state;
static int32_t damage_sound_resolve(void *context,int32_t sound_class)
{(void)context;if(damage_sound_count<3){damage_sound_trace[damage_sound_count][0]=0;damage_sound_trace[damage_sound_count][1]=(uint32_t)sound_class;}++damage_sound_count;return damage_sound_sample;}
static int32_t damage_sound_poll(void *context,int32_t voice)
{(void)context;if(damage_sound_count<3){damage_sound_trace[damage_sound_count][0]=1;damage_sound_trace[damage_sound_count][1]=(uint32_t)voice;}++damage_sound_count;return damage_sound_playing;}
static void damage_sound_play(void *context,const float position[3],int32_t sample)
{(void)context;if(damage_sound_count<3){uint32_t *r=damage_sound_trace[damage_sound_count];r[0]=2;memcpy(r+1,position,12);r[4]=(uint32_t)sample;r[5]=0x3f800000;}++damage_sound_count;damage_sound_state->flags^=damage_sound_mutation;}
static rf_entity_room_result room_result;
static uint32_t room_queries,room_notices,room_notice_kind;
static void binding_select(void *context,rf_player_local_binding *local,int32_t weapon)
{
    uint32_t *out=context;
    memcpy(out,local->entity,20);out[5]=(uint32_t)local->entity_handle;
    out[6]=(uint32_t)weapon;out[7]=local->inventory==&local->entity->inventory;
    ++out[8];
}
static int room_locate(void *context,const float position[3],rf_entity_room_result *result)
{(void)context;(void)position;++room_queries;*result=room_result;return RF_OK;}
static void room_notify(void *context,const char *name)
{(void)context;++room_notices;room_notice_kind=!strcmp(name,"underwater")?2:1;}
static void climb_sound(void *context,const rf_player_climb_state *state,const rf_player_sound_request *sound)
{
    uint32_t *out=context;
    out[0]++;out[1]=state->previous_region==NULL;out[2]=state->region!=NULL;
    out[3]=state->speed.mode;out[4]=sound->sound_id;out[5]=sound->spatial;
}
static int climb_stand(void *context,uint32_t *stood)
{uint32_t *v=context;++v[1];*stood=!v[0];return RF_OK;}
static void jump_sound(void *context,const rf_player_jump_state *state,int32_t sound)
{uint32_t *out=context;++out[7];out[8]=state->jump_time;out[9]=(uint32_t)sound;}
typedef struct landing_trace {uint32_t special_xor,stance_xor,count,trace[6];} landing_trace;
static void landing_effect(void *context,rf_entity_landing_state *state,uint32_t request)
{
    landing_trace *t=context;uint32_t *v=t->trace+3*t->count++;
    v[0]=request;v[1]=state->actor_flags;v[2]=state->body_flags;
    if(request==RF_ENTITY_LAND_SPECIAL)state->actor_flags^=t->special_xor;
    else state->body_flags^=t->stance_xor;
}
typedef struct slow_context {rf_player_climb_state *state;uint32_t *flags,blocked,calls;} slow_context;
static int slow_stand(void *context,uint32_t *stood)
{slow_context *v=context;++v->calls;v->state->speed.response=9;*stood=!v->blocked;if(*stood)*v->flags&=~0x400u;return RF_OK;}
#include "corpse_owned_delete_probe.h"
#include "corpse_owned_create_probe.h"
#include "corpse_owned_abort_probe.h"
static int action_name_probe(const char *path)
{
    FILE *file=fopen(path,"rb");uint32_t h[5],i;char data[46][64];const char *names[45];int32_t result;
    if(!file)return 2;_setmode(_fileno(stdout),_O_BINARY);
    while(fread(h,1,sizeof(h),file)==sizeof(h)) {
        if(fread(data,1,sizeof(data),file)!=sizeof(data)){fclose(file);return 3;}
        for(i=0;i<46;++i)if(!memchr(data[i],0,64)){fclose(file);return 4;}
        for(i=0;i<45;++i)names[i]=(h[2+i/32]&(1u<<(i%32)))?data[i]:NULL;
        result=rf_entity_action_name_lookup(h[0],h[1],names,h[4]?data[45]:NULL);
        if(fwrite(&result,4,1,stdout)!=1){fclose(file);return 5;}
    }
    fclose(file);return 0;
}
typedef struct loader_creation_fixture {
    uint32_t calls,success[2],class_flags,trace[6];rf_entity_loader_created actors[2];
    const float *position,*orientation;
} loader_creation_fixture;
static rf_entity_loader_created *loader_creation_create(void *context,const rf_entity_loader_create_request *r)
{
    loader_creation_fixture *f=context;uint32_t i=f->calls++;
    if(i>=2 || r->uid!=-1 || r->player_index!=-1 || r->position!=f->position || r->orientation!=f->orientation)return NULL;
    f->trace[i*3]=(uint32_t)r->class_id;f->trace[i*3+1]=!strcmp(r->name,"main")?1:!strcmp(r->name,"masako_endgame")?2:0;
    f->trace[i*3+2]=r->flags;return f->success[i]?f->actors+i:NULL;
}
static int loader_creation_probe(void)
{
    uint32_t wire[10],out[13];float transform[12]={0};
    while(fread(wire,sizeof(wire),1,stdin)==1) {
        loader_creation_fixture f={0};rf_entity_loader_creation input={0};rf_entity_loader_created *result;
        input.class_id=(int32_t)wire[0];input.multiplayer=wire[1];input.excluded=wire[2];input.hidden=wire[3];input.other_flag=wire[4];
        input.special_name=wire[6];input.matching_level=wire[7];input.special_class=(int32_t)wire[8];input.name="main";input.position=transform;input.orientation=transform+3;
        f.position=input.position;f.orientation=input.orientation;f.success[0]=wire[5];f.success[1]=wire[9];f.class_flags=0xcccccccc;
        memset(f.actors,0xa5,sizeof(f.actors));f.actors[0].class_flags_728=f.actors[1].class_flags_728=&f.class_flags;f.actors[1].handle=12345;
        result=rf_entity_loader_create(&input,loader_creation_create,&f);
        out[0]=result!=NULL;out[1]=f.calls;memcpy(out+2,f.trace,24);out[8]=f.actors[0].linked_146c;
        out[9]=f.actors[1].field_7c8;out[10]=f.actors[1].flags_814;memcpy(out+11,&f.actors[1].field_7cc,4);out[12]=f.class_flags;
        if(fwrite(out,sizeof(out),1,stdout)!=1)return 1;
    }
    return ferror(stdin)?1:0;
}
#include "ai_select_probe.h"
#include "ai_recovery_probe.h"
#include "ai_arbitration_probe.h"
#include "ai_destination_probe.h"
#include "ai_route_limit_probe.h"
#include "ai_prepare_probe.h"
#include "ai_direct_probe.h"
#include "ai_search_probe.h"
#include "ai_endpoint_probe.h"
#include "ai_nearest_probe.h"
int main(int argc,char **argv)
{
    if(argc==3 && !strcmp(argv[1],"--action-name"))return action_name_probe(argv[2]);
    if(argc==2 && !strcmp(argv[1],"--corpse-owned-abort"))return corpse_owned_abort_probe();
    if(argc==2 && !strcmp(argv[1],"--corpse-owned-create"))return corpse_owned_create_probe();
    if(argc==2 && !strcmp(argv[1],"--finalize-owned-create"))return finalize_owned_create_probe();
    if(argc==2 && !strcmp(argv[1],"--corpse-owned-delete"))return corpse_owned_delete_probe();
    if(argc==2 && !strcmp(argv[1],"--corpse-name")) {
        static rf_corpse_owners owners;rf_corpse_physics_seed seed={0};uint32_t input[3],slot,values[4],base;char text[256];const char *name;int status;
        seed.flags=0x33;seed.radius=1;seed.basis[0]=seed.basis[4]=seed.basis[8]=1;
        if(rf_corpse_owners_init(&owners,sizeof(owners)+1024) || rf_corpse_owners_acquire(&owners,&seed,.25f,.5f,2,&slot))return 2;
        base=owners.allocated_bytes;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(input,sizeof(input),1,stdin)==1) {
            if(input[0]>1 || input[1]>255 || fread(text,1,input[1],stdin)!=input[1])return 3;
            text[input[1]]=0;name=input[2]==1?NULL:input[2]==2?owners.slots[slot].names[input[0]].bytes:text;
            status=rf_corpse_name_assign(&owners,slot,input[0],name);
            values[0]=(uint32_t)status;values[1]=owners.slots[slot].names[input[0]].length;
            values[2]=owners.slots[slot].names[input[0]].bytes!=NULL;values[3]=owners.allocated_bytes-base;
            fwrite(values,4,4,stdout);if(values[2])fwrite(owners.slots[slot].names[input[0]].bytes,1,values[1]+1,stdout);
        }
        rf_corpse_name_assign(&owners,slot,0,NULL);rf_corpse_name_assign(&owners,slot,1,NULL);
        if(rf_corpse_owners_recycle(&owners,slot) || owners.allocated_bytes!=sizeof(owners))return 4;
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--corpse-base")) {
        static rf_corpse_owners owners;static rf_object_registry registry;
        uint32_t input[23],index,count,values[10];rf_corpse_list_link head,previous;rf_corpse *c;rf_physics_body *body;
        rf_corpse_physics_seed seed;rf_physics_sphere spheres[4];float material[3];
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(input,sizeof(input),1,stdin)==1) {
            if(input[19]>4 || input[21]>=1024 || fread(spheres,24,input[19],stdin)!=input[19])return 2;
            rf_corpse_owners_init(&owners,sizeof(owners)+96);rf_object_registry_init(&registry);
            registry.generation=input[20];registry.head=input[21];registry.count=1;
            memset(&owners.slots[0].corpse,0xa5,sizeof(rf_corpse));head.next=head.previous=&head;count=input[21]%2;
            if(count) {head.next=head.previous=&previous;previous.next=previous.previous=&head;}
            memset(&seed,0,sizeof(seed));seed.word_0c=input[0];seed.word_14=input[1];
            memcpy(seed.position,input+2,12);memcpy(seed.basis,input+5,36);memcpy(&seed.radius,input+14,4);
            seed.flags=input[15];memcpy(material,input+16,12);seed.sphere_count=input[19];seed.spheres=spheres;
            if(rf_corpse_base_acquire(&owners,&registry,&head,&count,&seed,material[0],material[1],material[2],input[22],&index) || index)return 3;
            c=&owners.slots[0].corpse;body=&owners.slots[0].body;
            if(count!=1+input[21]%2 || head.previous!=&c->deletion.object_link ||
               c->deletion.object_link.next!=&head || c->deletion.object_link.previous!=(input[21]%2?&previous:&head) ||
               c->deletion.object_link.previous->next!=&c->deletion.object_link ||
               rf_object_registry_lookup(&registry,c->deletion.handle)!=c || registry.count ||
               c->deletion.update!=&c->update || c->deletion.registered_object!=c || c->deletion.lifecycle ||
               c->deletion.emitters || c->deletion.corpse_link.next || c->deletion.corpse_link.previous)return 4;
            values[0]=c->deletion.handle;memcpy(values+1,&c->update.fade.health_34,4);
            values[2]=c->update.fade.object_flags_7c;values[3]=c->update.model;
            memcpy(values+4,&c->model_radius,4);memcpy(values+5,&c->physics_radius,4);
            values[6]=c->physics_flags;values[7]=c->attachment_index;values[8]=c->word_1fc;values[9]=(uint32_t)c->update.item_2cc;
            fwrite(&owners.slots[0].room,sizeof(owners.slots[0].room),1,stdout);fwrite(values,4,10,stdout);fwrite(c->update.position,4,3,stdout);fwrite(c->update.basis,4,9,stdout);
            fwrite(&body->state,sizeof(body->state),1,stdout);fwrite(&body->spheres.count,4,1,stdout);fwrite(body->spheres.items,24,body->spheres.count,stdout);
            /* Fixture teardown; only the base subset has run, no constructor resources. */
            head.next=head.previous=&head;rf_object_registry_remove(&registry,c->deletion.handle);rf_corpse_owners_recycle(&owners,index);
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--corpse-owners")) {
        static rf_corpse_owners owners;rf_corpse_physics_seed seed={0};rf_physics_sphere sphere={{1,2,3},1,-1,7};
        uint32_t cycle,i,index=99,base=sizeof(owners);float mass=3,response=10;
        seed.flags=0x33;seed.radius=1;seed.basis[0]=seed.basis[4]=seed.basis[8]=1;
        memcpy(&seed.word_0c,&response,4);memcpy(&seed.word_14,&mass,4);seed.spheres=&sphere;seed.sphere_count=1;
        if(rf_corpse_owners_init(&owners,base+30*24))return 2;
        for(cycle=0;cycle<64;cycle++) {
            for(i=0;i<30;i++) {
                if(rf_corpse_owners_acquire(&owners,&seed,.25f,.5f,2,&index) || index!=i)return 3;
                if(memcmp(owners.slots[i].body.spheres.items,&sphere,24))return 4;
                owners.slots[i].corpse.uid=cycle*30+i;
            }
            index=99;if(rf_corpse_owners_acquire(&owners,&seed,.25f,.5f,2,&index)!=RF_NOT_FOUND || index!=99)return 5;
            if(owners.allocated_bytes!=base+720)return 6;
            for(i=30;i--;) {
                if(rf_corpse_owners_recycle(&owners,i) || owners.slots[i].corpse.uid!=cycle*30+i)return 7;
                if(rf_corpse_owners_recycle(&owners,i)!=RF_RANGE)return 8;
            }
            if(owners.allocated_bytes!=base || owners.pool.live || owners.pool.active_mask)return 9;
        }
        owners.budget=base+23;index=99;
        if(rf_corpse_owners_acquire(&owners,&seed,.25f,.5f,2,&index)!=RF_RANGE || index!=99 || owners.pool.live)return 10;
        owners.budget=base+24;seed.flags=0;
        if(rf_corpse_owners_acquire(&owners,&seed,.25f,.5f,2,&index)!=RF_RANGE || index!=99 || owners.pool.live)return 11;
        seed.flags=0x33;
        if(rf_corpse_owners_acquire(&owners,&seed,.25f,.5f,2,&index) || index)return 12;
        memset(&sphere,0xa5,sizeof(sphere));if(owners.slots[0].body.spheres.items[0].opaque_14!=7)return 13;
        if(rf_corpse_owners_recycle(&owners,0) || owners.allocated_bytes!=base)return 14;
        printf("PASS 1921 owned corpse acquisitions; 64 full pools; payload retention and budget recovery\n");return 0;
    }
    if(argc==2 && (!strcmp(argv[1],"--corpse-body") || !strcmp(argv[1],"--creation-body"))) {
        uint32_t input[20];rf_physics_sphere spheres[8];rf_corpse_physics_seed seed;rf_physics_body body;float material[3];int status;
        int (*open_body)(const rf_physics_creation_seed *,float,float,float,uint32_t,rf_physics_body *)=
            !strcmp(argv[1],"--creation-body")?rf_physics_creation_body_open:rf_corpse_body_open;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(input,sizeof(input),1,stdin)==1) {
            if(input[19]>8 || fread(spheres,24,input[19],stdin)!=input[19])return 2;
            memset(&seed,0,sizeof(seed));memset(&body,0,sizeof(body));seed.word_0c=input[0];seed.word_14=input[1];
            memcpy(seed.position,input+2,12);memcpy(seed.basis,input+5,36);memcpy(&seed.radius,input+14,4);seed.flags=input[15];
            memcpy(material,input+16,12);seed.spheres=spheres;seed.sphere_count=input[19];
            status=open_body(&seed,material[0],material[1],material[2],0,&body);
            if(status!=RF_RANGE || body.allocated_bytes || body.spheres.items)return 3;
            status=open_body(&seed,material[0],material[1],material[2],sizeof(body)+(input[19]?input[19]:1)*24,&body);
            fwrite(&status,4,1,stdout);fwrite(&body.state,sizeof(body.state),1,stdout);fwrite(&body.spheres.count,4,1,stdout);fwrite(body.spheres.items,24,body.spheres.count,stdout);
            rf_physics_body_close(&body);rf_physics_body_close(&body);
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--corpse-create-guard")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return corpse_create_probe(4);
    }
    if(argc==2 && !strcmp(argv[1],"--corpse-create-trace")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return corpse_create_probe(2);
    }
    if(argc==2 && !strcmp(argv[1],"--corpse-create")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return corpse_create_probe(0);
    }
    if(argc==2 && !strcmp(argv[1],"--corpse-create-list")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return corpse_create_probe(1);
    }
    if(argc==2 && !strcmp(argv[1],"--corpse-pool")) {
        rf_corpse_pool pool;uint32_t input[2],index;int status;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);rf_corpse_pool_init(&pool);
        while(fread(input,sizeof(input),1,stdin)==1) {
            index=UINT32_MAX;
            if(input[0]==0)status=rf_corpse_pool_acquire(&pool,&index);
            else if(input[0]==1)status=rf_corpse_pool_release(&pool,input[1]);
            else return 2;
            fwrite(&status,4,1,stdout);fwrite(&index,4,1,stdout);fwrite(&pool,sizeof(pool),1,stdout);
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--corpse-delete")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return corpse_delete_probe();
    }
    if(argc==2 && !strcmp(argv[1],"--corpse-surface-draw")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return corpse_surface_draw_probe();
    }
    if(argc==2 && !strcmp(argv[1],"--corpse-surface-collect")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return corpse_surface_collect_probe();
    }
    if(argc==2 && !strcmp(argv[1],"--corpse-surface-quad")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return corpse_surface_quad_probe();
    }
    if(argc==2 && !strcmp(argv[1],"--corpse-surface-pool")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return corpse_surface_pool_probe();
    }
    if(argc==2 && !strcmp(argv[1],"--corpse-surface-guards"))return corpse_surface_guards();
    if(argc==2 && !strcmp(argv[1],"--lightmap-projection-read")) {
        unsigned char record[96];struct {int32_t status;rf_lightmap_projection projection;} out;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(record,sizeof(record),1,stdin)==1) {
            memset(&out,0xa5,sizeof(out));out.status=rf_lightmap_projection_read(record,sizeof(record),&out.projection);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 3;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--lightmap-pack")) {
        struct {uint32_t width,height,pitch,rgb_bytes,packed_bytes,double_rgb,available;unsigned char rgb[512],packed[512];} in;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            int status;if(in.rgb_bytes>512 || in.packed_bytes>512)return 2;
            status=rf_lightmap_pack_1555(in.rgb,in.rgb_bytes,in.width,in.height,in.double_rgb,in.available?in.packed:NULL,in.pitch,in.packed_bytes);
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(in.rgb,512,1,stdout)!=1 || fwrite(in.packed,512,1,stdout)!=1)return 3;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--lightmap-project")) {
        struct {rf_lightmap_projection projection;float point[3];uint32_t alias;} in;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            struct {int32_t status;float uv[2];} out;
            out.uv[0]=in.point[0];out.uv[1]=in.point[1];
            out.status=rf_lightmap_project(&in.projection,in.point,in.alias?in.point:out.uv);
            if(in.alias)memcpy(out.uv,in.point,8);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 3;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--lightmap-sample")) {
        struct {uint32_t width,height,pitch,bytes,available;float uv[2];unsigned char pixels[512];} in;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_lightmap_1555_view view={in.available?in.pixels:NULL,in.width,in.height,in.pitch,in.bytes};
            uint32_t out[2]={0,0x12345678};
            if(in.bytes>512)return 2;
            out[0]=(uint32_t)rf_lightmap_sample_1555(&view,in.uv,out+1);
            if(fwrite(out,sizeof(out),1,stdout)!=1)return 3;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--corpse-surface")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return corpse_surface_probe();
    }
    if(argc==2 && !strcmp(argv[1],"--corpse-update")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return corpse_update_probe();
    }
    if(argc==2 && !strcmp(argv[1],"--corpse-fade")) {
        struct {rf_corpse_fade_state state;float dt;} s;uint32_t next;int status;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&s,sizeof(s),1,stdin)==1) {
            next=0xaaaaaaaa;status=rf_corpse_fade_step(&s.state,s.dt,&next);
            fwrite(&status,4,1,stdout);fwrite(&s.state,sizeof(s.state),1,stdout);fwrite(&next,4,1,stdout);
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--corpse-retention")) {
        uint32_t count,i,faded;rf_corpse_retention_node nodes[32];int status;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&count,4,1,stdin)==1) {
            if(count>32)return 2;
            for(i=0;i<count;i++) {
                if(fread(&nodes[i].object_flags_7c,16,1,stdin)!=1)return 2;
                nodes[i].next=i+1<count?nodes+i+1:NULL;
            }
            faded=UINT32_MAX;status=rf_corpse_retention_apply(count?nodes:NULL,count,&faded);
            fwrite(&status,4,1,stdout);fwrite(&faded,4,1,stdout);
            for(i=0;i<count;i++)fwrite(&nodes[i].object_flags_7c,16,1,stdout);
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--burn-retarget")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return burn_retarget_probe();
    }
    if(argc==2 && !strcmp(argv[1],"--player-detach")) {
        struct {rf_player_entity_link link;uint32_t activity_word;} s;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&s,sizeof(s),1,stdin)==1) {
            rf_player_detach_sp(&s.link,(uint8_t *)&s.activity_word);fwrite(&s,sizeof(s),1,stdout);
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--finalize-sp")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return finalizer_probe_main();
    }
    if(argc==2 && !strcmp(argv[1],"--dying-update")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return dying_probe_main();
    }
    if(argc==2 && !strcmp(argv[1],"--pain"))return pain_probe();
    if(argc==2 && !strcmp(argv[1],"--navigation-select"))return navigation_select_probe();
    if(argc==2 && !strcmp(argv[1],"--navigation-pair")) {
        struct {float point[3],radius;rf_entity_navigation_candidate a,b;} wire;
        uint32_t classification;float score;int32_t status;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&wire,sizeof(wire),1,stdin)==1) {
            classification=99;memset(&score,0xa5,4);status=rf_entity_navigation_pair(wire.point,wire.radius,&wire.a,&wire.b,&score,&classification);
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(&classification,4,1,stdout)!=1 || fwrite(&score,4,1,stdout)!=1)return 3;
        }
        return ferror(stdin)?3:0;
    }
    if(argc==2 && !strcmp(argv[1],"--navigation-basis")) {
        float direction[3],matrix[3][3];int32_t status;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(direction,sizeof(direction),1,stdin)==1) {
            memset(matrix,0xa5,sizeof(matrix));status=rf_entity_navigation_basis(direction,matrix);
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(matrix,sizeof(matrix),1,stdout)!=1)return 3;
        }
        return ferror(stdin)?3:0;
    }
    if(argc==2 && !strcmp(argv[1],"--navigation-closest")) {
        float wire[9],output[4];int32_t status;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(wire,sizeof(wire),1,stdin)==1) {
            memset(output,0xa5,sizeof(output));status=rf_entity_navigation_closest_point(wire,wire+3,wire+6,output,output+3);
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(output,sizeof(output),1,stdout)!=1)return 3;
        }
        return ferror(stdin)?3:0;
    }
    if(argc==2 && !strcmp(argv[1],"--navigation-single")) {
        struct {float position[3],radius,height;uint32_t mode;rf_entity_navigation_candidate candidate;} wire;
        uint32_t result;int32_t status;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&wire,sizeof(wire),1,stdin)==1) {
            result=99;status=rf_entity_navigation_single(wire.position,wire.radius,wire.height,wire.mode,&wire.candidate,&result);
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(&result,4,1,stdout)!=1 || fwrite(&wire.candidate,sizeof(wire.candidate),1,stdout)!=1)return 3;
        }
        return ferror(stdin)?3:0;
    }
    if(argc==2 && !strcmp(argv[1],"--navigation-candidate")) {
        struct {float radius,height;uint32_t mode;float candidate_radius,candidate_height;uint32_t word_40;} wire;uint32_t result;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&wire,sizeof(wire),1,stdin)==1) {
            result=rf_entity_navigation_candidate_allowed(wire.radius,wire.height,wire.mode,wire.candidate_radius,wire.candidate_height,wire.word_40);
            if(fwrite(&result,4,1,stdout)!=1)return 3;
        }
        return ferror(stdin)?3:0;
    }
    if(argc==2 && !strcmp(argv[1],"--ai-node-prepare")) {
        struct {uint32_t count,mode;float radius,height;rf_entity_navigation_candidate nodes[8];} in;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            rf_entity_navigation_reference refs[8];uint32_t i;int32_t status;if(in.count>8)return 3;
            for(i=0;i<in.count;++i)refs[i]=(rf_entity_navigation_reference){in.nodes+i,i+1,NULL,0};
            status=rf_entity_navigation_search_prepare(refs,in.count,in.radius,in.height,in.mode);
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(in.nodes,sizeof(in.nodes),1,stdout)!=1)return 3;
        }
        return ferror(stdin)?3:0;
    }
    if(argc==2 && !strcmp(argv[1],"--ai-edge")) {
        float in[10];uint32_t out[2];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(in,sizeof(in),1,stdin)==1) {
            out[1]=99;out[0]=(uint32_t)rf_entity_navigation_edge_allowed(in,in+3,in+6,in[9],out+1);
            if(fwrite(out,sizeof(out),1,stdout)!=1)return 3;
        }
        return ferror(stdin)?3:0;
    }
    if(argc==2 && !strcmp(argv[1],"--ai-nearest"))return ai_nearest_probe();
    if(argc==2 && !strcmp(argv[1],"--ai-endpoint"))return ai_endpoint_probe();
    if(argc==2 && !strcmp(argv[1],"--ai-search"))return ai_search_probe();
    if(argc==2 && !strcmp(argv[1],"--ai-direct"))return ai_direct_probe();
    if(argc==2 && !strcmp(argv[1],"--ai-prepare"))return ai_prepare_probe();
    if(argc==2 && !strcmp(argv[1],"--ai-route-limit"))return ai_route_limit_probe();
    if(argc==2 && !strcmp(argv[1],"--ai-weapon-limit")) {
        uint32_t in[70],out[2];int32_t weapons[2];float override_value,scalars[64],result;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(in,sizeof(in),1,stdin)==1) {
            memcpy(weapons,in,8);memcpy(&override_value,in+3,4);memcpy(scalars,in+6,256);out[1]=0x12345678;memcpy(&result,out+1,4);
            out[0]=(uint32_t)rf_entity_ai_weapon_limit(weapons,in[2],override_value,in[4],scalars,in[5],&result);memcpy(out+1,&result,4);
            if(fwrite(out,sizeof(out),1,stdout)!=1)return 3;
        }
        return ferror(stdin)?3:0;
    }
    if(argc==2 && !strcmp(argv[1],"--ai-destination"))return ai_destination_probe();
    if(argc==2 && !strcmp(argv[1],"--ai-arbitration"))return ai_arbitration_probe();
    if(argc==2 && !strcmp(argv[1],"--ai-recovery"))return ai_recovery_probe();
    if(argc==2 && !strcmp(argv[1],"--ai-reset"))return ai_reset_probe();
    if(argc==2 && !strcmp(argv[1],"--ai-select"))return ai_select_probe();
    if(argc==2 && !strcmp(argv[1],"--ai-transition")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        struct {rf_entity_ai_transition_state state;uint32_t operation;int32_t requested;uint32_t a,b;float clock;uint32_t network_a,network_b;} input;
        while(fread(&input,sizeof(input),1,stdin)==1) {
            int32_t status=input.operation?rf_entity_ai_set_state(&input.state,input.requested,input.clock):
                rf_entity_ai_set_action(&input.state,input.requested,input.a,input.b,input.clock,input.network_a,input.network_b);
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(&input.state,sizeof(input.state),1,stdout)!=1)return 3;
        }
        return ferror(stdin)?3:0;
    }
    if(argc==2 && !strcmp(argv[1],"--navigation-reset")) {
        struct {rf_entity_navigation_route route;int32_t now;} wire;int32_t result;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&wire,sizeof(wire),1,stdin)==1) {
            result=rf_entity_navigation_reset(&wire.route,wire.now);
            if(fwrite(&result,4,1,stdout)!=1 || fwrite(&wire.route,sizeof(wire.route),1,stdout)!=1)return 3;
        }
        return ferror(stdin)?3:0;
    }
    if(argc==2 && !strcmp(argv[1],"--land-process"))return land_process_probe();
    if(argc==2 && !strcmp(argv[1],"--impact-process-sp"))return impact_process_probe();
    if(argc==2 && !strcmp(argv[1],"--impact-damage")) {
        struct {float speed;uint32_t falling;int32_t material;uint32_t kind,flags;} input;
        struct {int32_t status;float amount;uint32_t eligible;} output;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            memset(&output,0xa5,sizeof(output));
            output.status=rf_entity_impact_damage(input.speed,input.falling,input.material,input.kind,input.flags,&output.amount,&output.eligible);
            fwrite(&output,sizeof(output),1,stdout);
        }
        return ferror(stdin)?1:0;
    }

    if(argc==2 && (!strcmp(argv[1],"--slow-enter") || !strcmp(argv[1],"--normal-enter"))) {
        uint32_t v[7];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(v,sizeof(v),1,stdin)==1) {
            rf_movement_descriptor table[16]={0};rf_player_movement_region region={0};float identity[3][3]={{1,0,0},{0,1,0},{0,0,1}};
            rf_movement_config config={v[0],3.5f,.5f,0,1,2,3};
            rf_player_climb_state state={&region,&region,table+2,NULL,123,{2,7,1},5};
            uint32_t flags=v[1],selected=77,out[12];slow_context context={&state,&flags,v[3],0};
            rf_player_slow_input input={&config,table,identity,(int32_t)v[5],1,v[2],(uint8_t)v[6]};
            table[0].index=0;table[1].index=1;table[1].enabled=v[4];table[2].index=2;
            if(!strcmp(argv[1],"--normal-enter")) {
                rf_player_climb_exit_input normal={&config,table,identity,1,(int32_t)v[5],1,!!(flags&0x400u),(uint8_t)v[6]};
                out[0]=(uint32_t)rf_player_climb_exit(&state,&normal,&selected,slow_stand,&context);
            } else out[0]=(uint32_t)rf_player_slow_enter(&state,&flags,&input,&selected,slow_stand,&context);
            out[1]=state.previous_region==&region;out[2]=state.region==&region;out[3]=state.movement->index;out[4]=state.orientation==identity;
            out[5]=flags;out[6]=selected;out[7]=context.calls;memcpy(out+8,&state.speed,12);memcpy(out+11,&state.vertical_velocity,4);
            fwrite(out,sizeof(out),1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--landing-finish")) {
        struct {rf_entity_landing_state state;uint32_t special_xor,stance_xor;} input;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            landing_trace t={0};int32_t status;t.special_xor=input.special_xor;t.stance_xor=input.stance_xor;
            status=rf_entity_landing_finish(&input.state,landing_effect,&t);
            fwrite(&status,4,1,stdout);fwrite(&input.state,16,1,stdout);fwrite(&t.count,28,1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--support-contact-route")) {
        struct {float fraction,dot;uint32_t resolved,type,flags,falling;} input;int32_t route;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            route=rf_entity_support_contact_route(input.fraction,(double)input.dot,input.resolved,input.type,input.flags,input.falling);
            fwrite(&route,4,1,stdout);
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--position-snapshot")) {
        struct {uint32_t flags;float previous[3],published[3];} input;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            rf_entity_position_snapshot(&input.flags,input.previous,input.published);
            fwrite(&input,sizeof(input),1,stdout);
        }
        return 0;
    }

    if(argc==2 && !strcmp(argv[1],"--support-moved")) {
        struct {uint32_t flags;float previous[3],current[3];} input;int32_t moved;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            moved=rf_entity_support_moved(&input.flags,input.previous,input.current);
            fwrite(&moved,4,1,stdout);fwrite(&input.flags,4,1,stdout);
        }
        return 0;
    }

    if(argc==2 && !strcmp(argv[1],"--support-route")) {
        rf_entity_support_gate input;int32_t result;
        _Static_assert(sizeof(input)==28,"support route wire");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            result=rf_entity_support_route(&input);fwrite(&result,4,1,stdout);
        }
        return 0;
    }

    if(argc==2 && !strcmp(argv[1],"--animation-gate")) {
        rf_entity_animation_gate input;int32_t result;
        _Static_assert(sizeof(input)==40,"animation gate wire size");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            result=rf_entity_animation_should_advance(&input);fwrite(&result,4,1,stdout);
        }
        return 0;
    }

    if(argc==2 && !strcmp(argv[1],"--creation-vitals")) {
        uint32_t wire[8];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(wire,sizeof(wire),1,stdin)==1) {
            rf_entity_creation_vitals_state state;rf_entity_creation_vitals_class definition;
            memcpy(&state,wire,16);memcpy(&definition,wire+4,12);
            rf_entity_creation_vitals(&state,&definition,wire[7]);
            if(fwrite(&state,sizeof(state),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?3:0;
    }

    if(argc==2 && !strcmp(argv[1],"--burn-attachments"))return burn_attachment_probe();
    if(argc==2 && !strcmp(argv[1],"--burn-spread"))return burn_spread_probe();
    if(argc==2 && !strcmp(argv[1],"--burn-body"))return burn_body_probe();
    if(argc==2 && !strcmp(argv[1],"--burn-retirement"))return burn_retirement_probe();
    if(argc==2 && !strcmp(argv[1],"--burn-owner"))return burn_owner_probe();
    if(argc==2 && !strcmp(argv[1],"--burn-update"))return burn_update_probe();
    if(argc==2 && !strcmp(argv[1],"--burn-fade"))return burn_fade_probe();
    if(argc==2 && !strcmp(argv[1],"--burn-fade-resolved"))return burn_fade_resolved_probe();
    if(argc==2 && !strcmp(argv[1],"--burn-resource-release"))return burn_resource_probe();
    if(argc==2 && !strcmp(argv[1],"--burn-create"))return burn_create_probe();
    if(argc==2 && !strcmp(argv[1],"--burn-pool"))return burn_pool_probe();
    if(argc==2 && !strcmp(argv[1],"--damage-full"))return damage_full_probe();
    if(argc==2 && !strcmp(argv[1],"--damage-effects"))return damage_effect_probe();
    if(argc==2 && !strcmp(argv[1],"--armor-immunity")) {
        uint32_t wire[3],result;float armor;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(wire,sizeof(wire),1,stdin)==1) {
            memcpy(&armor,wire,4);result=rf_entity_armor_immunity(armor,wire[1],wire[2]);fwrite(&result,4,1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--damage-dispatch")) {
        uint32_t wire[15];rf_damage_request request;float multiplier,result;int status;
        rf_damage_backend backend={dispatch_lookup,dispatch_predicate,dispatch_effect_call,NULL};
        _Static_assert(sizeof(request)==24,"Damage request wire");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(wire,sizeof(wire),1,stdin)==1) {
            memcpy(&dispatch_object,wire,12);memcpy(&request,wire+3,24);memcpy(&multiplier,wire+9,4);
            dispatch_present=wire[10];memcpy(dispatch_facts,wire+11,12);memcpy(&dispatch_after,wire+14,4);
            dispatch_count=0;memset(dispatch_trace,0,sizeof(dispatch_trace));memset(dispatch_effect,0,sizeof(dispatch_effect));memset(&result,0xa5,4);
            status=rf_damage_dispatch_sp(0x12340001,&request,multiplier,&backend,&result);
            fwrite(&status,4,1,stdout);fwrite(&dispatch_object,12,1,stdout);fwrite(&result,4,1,stdout);fwrite(&dispatch_count,4,1,stdout);fwrite(dispatch_trace,32,1,stdout);fwrite(dispatch_effect,24,1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--damage-sound")) {
        uint32_t wire[19];rf_entity_damage_sound_state state;float fraction;int32_t now,status;
        rf_entity_damage_sound_backend backend={damage_sound_resolve,damage_sound_poll,damage_sound_play,NULL};
        _Static_assert(sizeof(state)==48,"Damage sound wire");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(wire,sizeof(wire),1,stdin)==1) {
            memcpy(&state,wire,48);memcpy(&fraction,wire+12,4);memcpy(&now,wire+15,4);
            memcpy(&damage_sound_sample,wire+16,4);memcpy(&damage_sound_playing,wire+17,4);damage_sound_mutation=wire[18];
            damage_sound_count=0;memset(damage_sound_trace,0,sizeof(damage_sound_trace));damage_sound_state=&state;
            status=rf_entity_damage_sound(&state,fraction,wire[13],wire[14],now,&backend);
            if(damage_sound_count>3)return 4;
            fwrite(&status,4,1,stdout);fwrite(&state,48,1,stdout);fwrite(&damage_sound_count,4,1,stdout);fwrite(damage_sound_trace,sizeof(damage_sound_trace),1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--damage-credit")) {
        uint32_t wire[16];rf_entity_damage_credit state;rf_entity_damage_uid entities[4];int32_t kind,uid,status;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(wire,sizeof(wire),1,stdin)==1) {
            memcpy(&state,wire,16);memcpy(&kind,wire+4,4);memcpy(&uid,wire+6,4);memcpy(entities,wire+8,32);
            if(wire[7]>4)return 4;
            status=rf_entity_damage_credit_sp(&state,kind,wire[5],uid,entities,wire[7]);
            fwrite(&status,4,1,stdout);fwrite(&state,16,1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--damage-vitals")) {
        uint32_t wire[7];rf_entity_damage_vitals state;float amount,multiplier,scaled;int32_t kind,status;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(wire,sizeof(wire),1,stdin)==1) {
            memcpy(&state,wire,12);memcpy(&amount,wire+3,4);memcpy(&kind,wire+4,4);memcpy(&multiplier,wire+5,4);
            memset(&scaled,0xa5,4);status=rf_entity_damage_vitals_sp(&state,amount,kind,multiplier,wire[6],&scaled);
            fwrite(&status,4,1,stdout);fwrite(&state,12,1,stdout);fwrite(&scaled,4,1,stdout);
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--support-route")) {
        rf_player_support_input input;uint32_t result;
        _Static_assert(sizeof(input)==32,"Support route wire layout");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            result=rf_player_support_route(&input);
            if(fwrite(&result,sizeof(result),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--jump-gate")) {
        rf_player_jump_gate input;uint32_t result;
        _Static_assert(sizeof(input)==28,"Jump gate wire layout");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            result=rf_player_jump_enabled(&input);
            if(fwrite(&result,sizeof(result),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--jump")) {
        uint32_t v[12],out[10];
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(v,sizeof(v),1,stdin)==1) {
            rf_movement_descriptor table[16]={{0}},old={0};float identity[3][3]={{0}};
            rf_player_jump_state state={0};rf_player_jump_input input={0};uint32_t selected=77;
            memset(out,0,sizeof(out));old.index=v[0];table[3].enabled=table[8].enabled=v[8];
            state.actor_flags=v[1];state.physics_flags=v[2];memcpy(&state.vertical_velocity,v+3,4);
            state.movement=&old;state.jump_time=v[9];input.descriptors=table;input.identity=identity;
            memcpy(&input.strength,v+4,4);memcpy(&input.frame_dt,v+5,4);
            input.parent_blocked=v[6];input.alternate_fall=v[7];input.class_sound=(int32_t)v[10];input.now=v[11];
            if(rf_player_jump(&state,&input,&selected,jump_sound,out))return 3;
            out[0]=state.actor_flags;out[1]=state.physics_flags;memcpy(out+2,&state.vertical_velocity,4);
            out[3]=state.movement==&old?UINT32_MAX:(uint32_t)(state.movement-table);
            out[4]=state.orientation==identity;out[5]=state.jump_time;out[6]=selected;
            if(fwrite(out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==4 && !strcmp(argv[1],"--owned-level-regions")) {
        rf_vpp archive;rf_level level;rf_level_owned_regions regions={0};int status;
        _setmode(_fileno(stdout),_O_BINARY);
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]))return 3;
        status=rf_level_owned_regions_open(&level,64*1024,&regions);rf_vpp_close(&archive);
        if(status==RF_NOT_FOUND)return 0;if(status)return 4;
        memset(&level,0xa5,sizeof(level)); /* Owned data must outlive the loader. */
        if(fwrite(regions.items,sizeof(*regions.items),regions.count,stdout)!=regions.count)return 5;
        rf_level_owned_regions_close(&regions);rf_level_owned_regions_close(&regions);
        return regions.items || regions.count || regions.allocated_bytes?6:0;
    }
    if(argc==2 && !strcmp(argv[1],"--climb-exit")) {
        int32_t v[6];uint32_t out[10];
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(v,sizeof(v),1,stdin)==1) {
            rf_player_movement_region region={0};rf_movement_descriptor table[16]={{0}};
            rf_movement_config config={0};rf_player_climb_state state={0};rf_player_climb_exit_input input={0};
            float identity[3][3]={{1,0,0},{0,1,0},{0,0,1}};uint32_t selected=77,standing[2]={(uint32_t)v[2],0};
            config.flags=v[0];config.base_speed=3.5f;config.slow_factor=.5f;
            if(v[3]>=0)table[v[3]].enabled=v[4];
            state.previous_region=state.region=&region;state.movement=table+2;state.contact_handle=123;state.vertical_velocity=7;
            input.config=&config;input.descriptors=table;input.identity=identity;input.default_index=v[3];
            input.forced_action=v[5];input.entity_scale=1;input.crouched=v[1];
            if(rf_player_climb_exit(&state,&input,&selected,climb_stand,standing))return 3;
            out[0]=state.previous_region==NULL;out[1]=state.region==&region;
            out[2]=state.movement?(uint32_t)(state.movement-table):UINT32_MAX;out[3]=state.orientation==identity;
            out[4]=state.contact_handle;out[5]=state.speed.mode;out[6]=selected;memcpy(out+7,&state.vertical_velocity,4);
            out[8]=standing[1];memcpy(out+9,&state.speed.speed,4);
            if(fwrite(out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--climb-enter")) {
        uint32_t values[4],out[12];
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(values,sizeof(values),1,stdin)==1) {
            rf_player_movement_region region={0};rf_movement_descriptor descriptors[16]={{0}};
            rf_movement_config config={0};rf_player_climb_input input={0};rf_player_climb_state state={0};
            uint32_t selected=77;memset(out,0,sizeof(out));region.kind=values[2];descriptors[2].enabled=values[3];
            config.flags=values[0]*4;config.base_speed=3.5f;
            state.previous_region=&region;state.movement=descriptors+1;state.contact_handle=123;
            input.region=&region;input.descriptors=descriptors;input.config=&config;input.forced_action=-1;
            input.entity_scale=1;input.free_motion=values[1];input.sound.owner_present=1;
            if(rf_player_climb_enter(&state,&input,&selected,climb_sound,out))return 3;
            out[6]=state.previous_region==NULL;out[7]=state.region==&region;
            out[8]=state.movement-descriptors;out[9]=state.orientation==region.matrix;
            out[10]=state.speed.mode;out[11]=selected;
            if(fwrite(out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--actor-pair")) {
        uint32_t words[20],result;rf_collision_actor_pair_view a,b;const rf_collision_actor_pair_view *left,*right;
        _Static_assert(sizeof(a)==32,"Actor pair wire layout");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(words,sizeof(words),1,stdin)==1) {
            memcpy(&a,words,32);memcpy(&b,words+8,32);left=words[19]==1?NULL:&a;right=words[19]==2?NULL:words[19]==3?&a:&b;
            result=rf_collision_actor_pair_reject(left,right,words[16],words[17],words+18);
            if(fwrite(&result,4,1,stdout)!=1 || fwrite(words+18,4,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--collision-discovery")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return collision_discovery_probe();
    }
    if(argc==2 && !strcmp(argv[1],"--loader-create")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return loader_creation_probe();
    }
    if(argc==2 && !strcmp(argv[1],"--pair-classify")) {
        rf_collision_pair_class_view views[2];uint32_t words[7],result;
        _Static_assert(sizeof(views)==192,"Pair classification wire layout");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(views,sizeof(views),1,stdin)==1) {
            if(fread(words,sizeof(words),1,stdin)!=1)return 1;
            result=rf_collision_pair_reject(words[6]&1?NULL:views,words[6]&2?NULL:words[6]&4?views:views+1,words[0],words[1],words[2],words[3],words[4],words+5);
            if(fwrite(&result,4,1,stdout)!=1 || fwrite(words+5,4,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--projectile-planes")) {
        rf_collision_projectile_plane_source state;float planes[4][4];
        _Static_assert(sizeof(state)==52,"Projectile plane source wire layout");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&state,sizeof(state),1,stdin)==1) {
            rf_collision_projectile_planes(&state,planes);if(fwrite(planes,sizeof(planes),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--projectile-eligible")) {
        rf_collision_projectile_eligibility state;uint32_t result;
        _Static_assert(sizeof(state)==152,"Projectile eligibility wire layout");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&state,sizeof(state),1,stdin)==1) {
            result=rf_collision_projectile_eligible(&state);if(fwrite(&result,4,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--actor-model-response")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return actor_model_response_probe();
    }
    if(argc==2 && !strcmp(argv[1],"--actor-solid-response")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return actor_solid_response_probe();
    }
    if(argc==2 && !strcmp(argv[1],"--actor-general-response")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return actor_general_response_probe();
    }
    if(argc==2 && !strcmp(argv[1],"--actor-response")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return actor_response_probe();
    }
    if(argc==2 && !strcmp(argv[1],"--segment-sphere")) {
        float wire[13];uint32_t result;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(wire,sizeof(wire),1,stdin)==1) {
            result=rf_collision_segment_sphere(wire,wire+3,wire+6,wire[9],wire+10);
            if(fwrite(&result,4,1,stdout)!=1 || fwrite(wire+10,12,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--ray-sphere")) {
        float wire[15];uint32_t result;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(wire,sizeof(wire),1,stdin)==1) {
            result=rf_collision_ray_sphere(wire,wire[6],wire+7,wire[10],wire+11,wire+14);
            if(fwrite(&result,4,1,stdout)!=1 || fwrite(wire+11,16,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--pair-expired")) {
        rf_collision_pair_expiration state;uint32_t result;
        _Static_assert(sizeof(state)==60,"Pair expiration wire layout");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&state,sizeof(state),1,stdin)==1) {
            result=rf_collision_pair_expired(&state);if(fwrite(&result,4,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--collision-process")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return collision_process_probe();
    }
    if(argc==2 && !strcmp(argv[1],"--collision-create")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return collision_pool_probe();
    }
    if(argc==2 && !strcmp(argv[1],"--collision-retire")) {
        uint32_t words[101],i;rf_collision_pair nodes[32];rf_collision_pair_list active,available;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(words,sizeof(words),1,stdin)==1) {
            for(i=0;i<32;i++) {
                nodes[i].next=words[5+i*3]==UINT32_MAX?NULL:&nodes[words[5+i*3]];
                nodes[i].first=(const void *)(uintptr_t)words[6+i*3];
                nodes[i].second=(const void *)(uintptr_t)words[7+i*3];
            }
            active.head=words[0]==UINT32_MAX?NULL:&nodes[words[0]];active.count=words[1];
            available.head=words[2]==UINT32_MAX?NULL:&nodes[words[2]];available.count=words[3];
            rf_collision_pairs_retire(&active,&available,(const void *)(uintptr_t)words[4]);
            words[0]=active.head?(uint32_t)(active.head-nodes):UINT32_MAX;words[1]=active.count;
            words[2]=available.head?(uint32_t)(available.head-nodes):UINT32_MAX;words[3]=available.count;
            for(i=0;i<32;i++)words[5+i*3]=nodes[i].next?(uint32_t)(nodes[i].next-nodes):UINT32_MAX;
            if(fwrite(words,sizeof(words),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--death-select")) {
        uint32_t words[52],facts[4],out[5];rf_entity_death_selection state;rf_random_state random;int32_t selected;
        _Static_assert(sizeof(state)==196,"Death selection wire layout");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(words,sizeof(words),1,stdin)==1) {
            memcpy(&state,words,sizeof(state));random.value=words[49];facts[0]=words[50];facts[1]=words[51];facts[2]=0;facts[3]=UINT32_MAX;
            selected=12345;out[0]=(uint32_t)rf_entity_death_select(&state,death_clearance,facts,&random,&selected);
            out[1]=(uint32_t)selected;out[2]=random.value;out[3]=facts[2];out[4]=facts[3];
            if(fwrite(out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--death-clearance")) {
        uint32_t words[39],out[26];rf_entity_death_clearance_state state;rf_entity_death_obstacle actors[4];death_ray_trace trace;
        _Static_assert(sizeof(state)==56 && sizeof(actors)==80,"Death clearance wire layout");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(words,sizeof(words),1,stdin)==1) {
            memcpy(&state,words,56);memcpy(actors,words+15,80);memset(&trace,0,sizeof(trace));memcpy(trace.responses,words+35,16);
            out[0]=rf_entity_death_clearance(&state,words[14],actors,4,death_ray,&trace);out[1]=trace.count;memcpy(out+2,trace.points,96);
            if(fwrite(out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--death-drop")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return death_drop_probe();
    }
    if(argc==2 && !strcmp(argv[1],"--player-mode")) {
        rf_player_mode_state state;uint32_t active;
        _Static_assert(sizeof(state)==8,"player mode ABI");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&state,8,1,stdin)==1) {
            active=rf_player_mode_active(&state);rf_player_mode_stop(&state);
            if(fwrite(&active,4,1,stdout)!=1 || fwrite(&state,8,1,stdout)!=1)return 3;
        }
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--death-early")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return death_early_probe();
    }
    if(argc==2 && !strcmp(argv[1],"--death-tail")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return death_tail_probe();
    }
    if(argc==2 && !strcmp(argv[1],"--death-link")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return death_link_probe();
    }
    if(argc==2 && !strcmp(argv[1],"--death-motion")) {
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);return death_motion_probe();
    }
    if(argc==2 && !strcmp(argv[1],"--falling")) {
        uint32_t words[3],result;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(words,sizeof(words),1,stdin)==1) {
            result=rf_entity_falling((int32_t)words[0],words[1],(int32_t)words[2]);
            if(fwrite(&result,4,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--death-entry")) {
        uint32_t words[12],entered;rf_entity_death_entry_state state;
        _Static_assert(sizeof(state)==44,"Death entry wire layout");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(words,sizeof(words),1,stdin)==1) {
            memcpy(&state,words,sizeof(state));
            entered=rf_entity_death_entry_sp(&state,words[11]);
            if(fwrite(&entered,4,1,stdout)!=1 || fwrite(&state,sizeof(state),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--player-phase")) {
        uint32_t words[6],output[2];rf_entity_registry registry;rf_entity_view view;rf_player_entity_link link;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(words,sizeof(words),1,stdin)==1) {
            uint32_t slot=words[1]&0xffffu;
            memset(&registry,0,sizeof(registry));memset(&view,0,sizeof(view));
            link.entity_handle=(int32_t)words[1];view.handle=(int32_t)words[2];view.type=(int32_t)words[3];view.flags_810=words[4];
            if(words[5] && slot<RF_OBJECT_SLOTS)registry.slots[slot]=&view;
            output[0]=rf_player_is_dead(&registry,words[0]?&link:NULL);
            output[1]=rf_player_is_dying(&registry,words[0]?&link:NULL);
            fwrite(output,sizeof(output),1,stdout);
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--player-sound")) {
        rf_player_sound_input input;rf_player_sound_request result;
        _Static_assert(sizeof(input)==36 && sizeof(result)==32,"Player sound wire layout");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            if(rf_player_sound_route(&input,&result))return 3;
            if(fwrite(&result,sizeof(result),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==4 && !strcmp(argv[1],"--level-regions")) {
        rf_vpp archive;rf_level level;rf_level_entity_reader reader;rf_player_movement_region region;int status;
        _setmode(_fileno(stdout),_O_BINARY);
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]))return 3;
        status=rf_level_regions_begin(&level,&reader);
        if(status==RF_NOT_FOUND){rf_vpp_close(&archive);return 0;}if(status)return 4;
        while((status=rf_level_region_next(&reader,&region))==RF_OK)
            if(fwrite(&region,sizeof(region),1,stdout)!=1)return 5;
        rf_vpp_close(&archive);return status==RF_NOT_FOUND?0:6;
    }
    if(argc==2 && !strcmp(argv[1],"--player-regions")) {
        struct {uint32_t count;float point[3];rf_player_movement_region regions[3];} input;
        _Static_assert(sizeof(input)==208,"Movement region wire input");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            uint32_t index;
            if(input.count>3 || rf_player_movement_region_find(input.regions,input.count,input.point,&index))return 3;
            if(fwrite(&index,4,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--player-stance-gate")) {
        rf_player_stance_gate input;
        _Static_assert(sizeof(input)==32,"Player stance gate wire input");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            uint32_t result=rf_player_stance_enabled(&input);
            if(fwrite(&result,sizeof(result),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--player-motion")) {
        rf_player_motion_input input;int32_t state;
        _Static_assert(sizeof(input)==52,"Player motion wire input");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            if(rf_player_motion_choose(&input,&state))return 3;
            if(fwrite(&state,sizeof(state),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--player-crouch")) {
        rf_player_crouch_input input;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            uint32_t result=rf_player_can_crouch(&input);
            if(fwrite(&result,sizeof(result),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--player-bind")) {
        rf_player_entity_binding entity;rf_player_local_binding local;
        uint32_t out[9];
        _Static_assert(sizeof(entity)==20,"Player binding wire input");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&entity,sizeof(entity),1,stdin)==1) {
            memset(&local,0,sizeof(local));memset(out,0,sizeof(out));
            if(rf_player_bind_local(&local,&entity,binding_select,out))return 3;
            if(fwrite(out,sizeof(out),1,stdout)!=1)return 4;
        }
        return ferror(stdin)?1:0;
    }
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
