#include "rf/animation_check.h"
#include "rf/model.h"
#include "rf/model_file.h"
#include "rf/turn.h"
#include "rf/entity.h"
#include "rf/weapon.h"
#include "rf/effect.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static uint32_t hash_bytes(uint32_t hash, const void *bytes, size_t count)
{
    const unsigned char *p=bytes;
    while (count--) hash=(hash ^ *p++)*16777619u;
    return hash;
}
typedef struct animation_reset {
    rf_weapon_reset_state *state; rf_weapon_descriptor *descriptors;
    rf_weapon_reset_context *context; rf_motion_playback_state *playback;
    rf_motion_playback_resource *resources; rf_turn_actor *actor;
    rf_effect_pair *effects;
} animation_reset;
static int stop_reset_effect(void *user,int32_t handle)
{
    animation_reset *r=user;
    return rf_effect_set_enabled(r->effects,1,handle,0,0,0);
}
static int reset_loaded_weapon(void *user)
{
    animation_reset *r=user;
    const rf_weapon_reset_ops ops={NULL,NULL,stop_reset_effect,NULL};
    return rf_weapon_reset(r->state,r->actor->weapon,r->descriptors,r->context,r->playback,r->resources,4,&ops,r);
}
static int animation_run(const char *meshes_path,const char *motions_path,uint32_t out[8],rf_preview_mesh *preview,uint32_t preview_frame,uint32_t budget,rf_animation_frame_sink sink,void *sink_context,const rf_animation_placement *placement)
{
    static const char *names[4]={"ult2_stand.rfa","ult2_crouch.rfa",
        "ult2_sidestep_left.rfa","ult2_sidestep_right.rfa"};
    rf_vpp meshes, archive; rf_model_file model; rf_motion_file files[4];
    const rf_motion_file *handles[4]={&files[0],&files[1],&files[2],&files[3]};
    rf_motion_playback_resource resources[4]={0}; rf_motion_playback_state state={0};
    rf_turn_effects effects={0}; rf_turn_actor actor={0};
    rf_locomotion_candidate_input selection={0,1,{0,0,0},0};
    rf_locomotion_candidates candidates;
    rf_entity_registry registry={0}; rf_entity_view entity={0};
    rf_weapon_reset_state weapon_state={0}; rf_weapon_descriptor descriptors[64]={0};
    rf_weapon_reset_context weapon_context={0};
    rf_effect_switch effect_objects[2]={{1,{0,0,0},77},{1,{0,0,0},99}};
    rf_effect_pair effect_pair={{{&effect_objects[0],&effect_objects[1]},{NULL,NULL}}};
    animation_reset reset={&weapon_state,descriptors,&weapon_context,&state,resources,&actor,&effect_pair};
    uint32_t weapon_flags[64]={6};
    rf_weapon_inventory inventory={0}; rf_weapon_supply supply[64]={0};
    int32_t preference[32],replacement,reserve;
    rf_weapon_empty_input empty_input={0,-1,-1,-1,-1,-1,1,0,0,0,0,1,0,0};
    rf_weapon_empty_action empty_action;
    rf_weapon_selection_state weapon_selection={-1,2000,1,255,{0xab,0xcd},99};
    rf_weapon_selection_input weapon_request={0,-1,-1,32,0,-1,0,0,0,1};
    int ready,eligible;
    rf_turn_context context={{0x800,6,.3f,1.5f,20,7,9},-1,1,0,0};
    int32_t actions[45],sounds[45],sound_class;
    rf_motion_controller controller={0,-1,0,0,0,0}; int32_t motions[23];
    struct pose_workspace {rf_model_bone bones[256];float matrices[256][12];} *workspace=NULL;
    rf_model_bone *bones;rf_model_attachment eye;
    float (*matrices)[12], local[12], tag[12], displacement[3]={.125f,-.25f,.5f};
    uint16_t generations[256]={0}; uint32_t count=0,i,frame; int status,opened=0,found=0;
    void *payload=NULL;
    float (*stored)[12]=NULL,(*prepared)[12]=NULL;uint16_t prepared_generations[256]={0};
    rf_model_geometry geometry={0};rf_model_vertex *vertices=NULL;uint32_t vertex_count=0,vertex_index,selected_lod;
    uint8_t *render_memory=NULL;uint32_t render_capacity=0,render_batch,render_bytes=0,emitted_indices=0;
    uint16_t *render_indices=NULL;rf_model_clip_pool *clip_pool=NULL;
    rf_model_clip_projection clip_projection={{320,240},{0,0},0,1};rf_model_clip_planes clip_planes={0};
    rf_model_render_buffers render_buffers={0};rf_model_projection render_view={0};rf_model_lighting render_lights={0};
    rf_model_render_output render_output={1,{255,255,255},255,1,1};
    if (!out) return RF_RANGE;
    memset(out,0,8*4); out[0]=1;
    /* Fixed 27 KiB workspace, separate from the caller's output-mesh budget.
     * Keep this live across callbacks without exhausting the Xbox's 64 KiB stack. */
    workspace=malloc(sizeof(*workspace));if(!workspace) {status=RF_IO;goto done;}
    bones=workspace->bones;matrices=workspace->matrices;
    status=rf_vpp_open(&meshes,meshes_path); if (status!=RF_OK) goto done;
    opened=1; status=rf_vpp_open(&archive,motions_path); if (status!=RF_OK) goto done;
    opened=2; status=rf_model_file_open(&model,&meshes,"miner.v3c"); if (status!=RF_OK) goto done;
    for (i=0;i<model.section_count;++i) if (model.sections[i].type==0x424f4e45) {
        if (model.sections[i].size>4+256*56) { status=RF_RANGE; goto done; }
        out[7]=model.sections[i].size; payload=malloc(out[7]);
        if (!payload) { status=RF_RANGE; goto done; }
        status=rf_vpp_read(&meshes,&model.entry,model.sections[i].offset,payload,out[7]);
        if (status==RF_OK) status=rf_model_decode_bones(payload,out[7],bones,256,&count);
        free(payload); payload=NULL; if (status!=RF_OK) goto done;
    }
    if (!count || !model.lod_count) { status=RF_FORMAT; goto done; }
    stored=calloc(count*2,sizeof(*stored));if(!stored) { status=RF_RANGE;goto done; }
    prepared=stored+count;
    for(i=0;i<count;++i) {
        status=rf_model_bone_transform(bones[i].rotation,bones[i].position,stored[i]);if(status)goto done;
    }
    /* Fixed highest-detail diagnostic: use the original alternate-mode gate.
     * A camera-derived metric belongs to the live model draw path. */
    status=rf_model_file_select_lod(&model,0,0,1,0,0,1,0,&selected_lod);if(status)goto done;
    status=rf_model_geometry_open(&geometry,&model,selected_lod,1024*1024);if(status)goto done;
    vertices=geometry.vertices;vertex_count=geometry.vertex_count;
    if(!vertex_count) { status=RF_FORMAT;goto done; }
    for(i=0;i<geometry.batch_count;++i)if(geometry.batches[i].vertices>render_capacity)render_capacity=geometry.batches[i].vertices;
    if(!render_capacity || render_capacity>4096) {status=RF_RANGE;goto done;}
    render_bytes=render_capacity*56+4096*40;
    render_memory=malloc(render_bytes);render_indices=malloc(24576*sizeof(*render_indices));clip_pool=malloc(sizeof(*clip_pool));
    if(!render_memory || !render_indices || !clip_pool) {status=RF_IO;goto done;}
    memset(clip_pool,0,sizeof(*clip_pool));
    render_buffers.cache=(rf_model_render_cache*)render_memory;
    render_buffers.clip=(float(*)[3])(render_memory+render_capacity*32);
    render_buffers.second=(float(*)[3])(render_memory+render_capacity*44);
    render_buffers.vertices=(uint8_t(*)[40])(render_memory+render_capacity*56);render_buffers.capacity=render_capacity;
    /* Fixed view and ambient fixture; real animation poses feed the renderer.
     * Triangle submission and world-derived lighting remain separate. */
    render_view.camera[2]=-100;render_view.rotation[0]=render_view.rotation[4]=render_view.rotation[8]=1;
    render_view.perspective=render_view.compute_clip=render_view.clipping=1;
    render_view.screen[0]=320;render_view.screen[1]=-240;render_view.screen[2]=320;render_view.screen[3]=240;
    if(preview) {render_view.camera[2]=2.2f;render_view.rotation[0]=render_view.rotation[8]=-1;}
    render_lights.ambient[0]=40;render_lights.ambient[1]=50;render_lights.ambient[2]=60;
    out[1]=count;
    for (i=0;i<model.lods[0].attachment_count;++i) {
        status=rf_model_file_attachment(&model,0,i,&eye); if (status!=RF_OK) goto done;
        if (!strcmp(eye.name,"eye")) { found=1; break; }
    }
    if (!found || eye.parent<0 || (uint32_t)eye.parent>=count) { status=RF_FORMAT; goto done; }
    status=rf_model_attachment_transform(eye.rotation,eye.position,local); if (status!=RF_OK) goto done;
    state.completion.active.freeze_slot=-1;
    state.completion.active.primary_slot=state.completion.active.dominant_slot=-1;
    state.phase=.25f; state.generation=1;
    for (i=0;i<4;++i) {
        rf_motion_track track;
        status=rf_motion_file_open(&files[i],&archive,names[i]); if (status!=RF_OK) goto done;
        status=rf_motion_file_track(&files[i],0,&track); if (status!=RF_OK) goto done;
        resources[i].comparison=track.envelope; resources[i].looping=i<2;
        resources[i].markers[0]=3200; resources[i].markers[1]=6400;
    }
    for (i=0;i<23;++i) motions[i]=-1;
    motions[0]=0; motions[8]=1;
    for (i=0;i<45;++i) actions[i]=sounds[i]=-1;
    /* Original 0x4181d0 names actions 17/18 sidestep_left/right. Roll actions
     * 19/20 are absent; preparation still invokes its weapon-reset adapter. */
    actions[17]=2; actions[18]=3;
    actor.info_flags=context.movement.flags; actor.weapon=0;
    actor.direction.entity_flags=10;
    entity.handle=0x10000; entity.linked_handle=-1;
    entity.weapons[0]=0; entity.weapons[1]=-1; entity.weapon_owner=&entity;
    weapon_state.sound_81c=weapon_state.sound_820=-1;
    weapon_state.flags_7d0=10;
    weapon_state.character_present=1; weapon_context.weapon_count=1;
    descriptors[0].flags_264=6; descriptors[0].flags_268=0x40; descriptors[0].release_sound_class=-1;
    inventory.owned[0]=inventory.owned[1]=1;
    supply[0].ammo_type=0; supply[0].capacity=10;
    supply[1].ammo_type=-1; supply[1].flags_268=0x100;
    for (i=0;i<32;++i) preference[i]=-1;
    preference[0]=1; preference[1]=0;
    entity.flags_7d0=10; registry.slots[0]=&entity;
    actor.direction.orientation[0]=actor.direction.orientation[4]=actor.direction.orientation[8]=1;
    for (i=3;i<=6;++i) out[i]=2166136261u;
    for (frame=0;frame<64;++frame) {
        if(sink)preview->count=0;
        if(placement) {
            status=rf_model_local_view(&placement->world_view,placement->position,placement->orientation,&render_view);if(status)goto done;
            clip_projection=placement->clip_projection;clip_planes=placement->planes;
        }
        inventory.reserve[0]=frame<32 ? 1 : 0;
        status=rf_weapon_reserve(&inventory,supply,0,&reserve); if (status!=RF_OK) goto done;
        status=rf_weapon_choose_available(&inventory,supply,preference,1,&replacement); if (status!=RF_OK) goto done;
        /* This rig is a nonlocal player: original presentation returns without
         * model work. Local-player presentation remains an explicit adapter. */
        status=rf_weapon_current(&registry,entity.handle,0,NULL,NULL,&empty_input.current); if (status!=RF_OK) goto done;
        status=rf_weapon_decide_empty(&inventory,NULL,supply,weapon_flags,1,preference,&empty_input,&empty_action); if (status!=RF_OK) goto done;
        if (empty_action.kind==RF_WEAPON_EMPTY_SELECT) {
            /* Earlier selection gates are not assembled yet; this diagnostic
             * enters the recovered tail with the empty-handler request flags. */
            weapon_request.requested=empty_action.weapon;
            status=rf_weapon_finish_selection(&weapon_selection,&weapon_request,inventory.owned,weapon_flags,1,NULL,NULL);
            if (status!=RF_OK) goto done;
        }
        /* Scripted diagnostic requests, not the unrecovered locomotion selector. */
        if (frame==4 || frame==7 || frame==20 || frame==40) {
            status=rf_motion_request_state(&controller,motions,(frame==4 || frame==20) ? 8 : 0,.25f);
            if (status!=RF_OK) goto done;
        }
        controller.override_enabled=frame>=22 && frame<26;
        status=rf_motion_apply_controller(&controller,motions,1.0f/30.0f,&state,resources,4); if (status!=RF_OK) goto done;
        actor.direction.count=frame>=8 && frame<56;
        actor.direction.vector[0]=frame<32 ? -1.0f : 1.0f;
        context.now_ms=(int32_t)frame*33;
        selection.action=frame>=62 ? 12 : frame>=60 ? 7 : frame>=58 ? 17 : 0;
        selection.velocity[0]=frame>=32 ? 1.0f : 0;
        actor.behavior=frame>=48;
        if (frame==48) weapon_state.active[0]=1;
        status=rf_locomotion_prepare(&effects.deadlines[3],-1,&context,&actor,weapon_flags,1,reset_loaded_weapon,&reset); if (status!=RF_OK) goto done;
        entity.action_520=selection.action;
        status=rf_entity_combat_predicates(&registry,&entity,NULL,0,&ready,&eligible); if (status!=RF_OK) goto done;
        selection.combat_eligible=(uint32_t)eligible;
        status=rf_locomotion_choose_candidates(&candidates,&selection,motions,&effects,&state,resources,4,
            actions,sounds,&context,&actor,NULL,NULL,&sound_class); if (status!=RF_OK) goto done;
        status=rf_motion_update(&state,resources,4,1.0f/30.0f); if (status!=RF_OK) goto done;
        status=rf_model_evaluate_playback(bones,count,&state,handles,resources,4,displacement,matrices,generations,256); if (status!=RF_OK) goto done;
        status=rf_model_compose_transform(local,matrices[eye.parent],tag); if (status!=RF_OK) goto done;
        out[3]=hash_bytes(out[3],matrices,count*48); out[4]=hash_bytes(out[4],&state,sizeof(state)); out[6]=hash_bytes(out[6],tag,48);
        out[4]=hash_bytes(out[4],&controller,sizeof(controller));
        for (i=0;i<4;++i) out[4]=hash_bytes(out[4],&resources[i].references,4);
        out[4]=hash_bytes(out[4],&effects,sizeof(effects));
        out[4]=hash_bytes(out[4],&sound_class,4);
        out[4]=hash_bytes(out[4],&candidates,sizeof(candidates));
        out[4]=hash_bytes(out[4],&ready,4); out[4]=hash_bytes(out[4],&eligible,4);
        out[4]=hash_bytes(out[4],&weapon_state,sizeof(weapon_state));
        out[4]=hash_bytes(out[4],effect_objects,sizeof(effect_objects));
        out[4]=hash_bytes(out[4],&reserve,4); out[4]=hash_bytes(out[4],&replacement,4);
        out[4]=hash_bytes(out[4],&empty_action,sizeof(empty_action));
        out[4]=hash_bytes(out[4],&weapon_selection,sizeof(weapon_selection));
        displacement[0]=1;
        status=rf_model_evaluate_playback(bones,count,&state,handles,resources,4,displacement,matrices,generations,256); if (status!=RF_OK) goto done;
        if (displacement[0]!=1) { status=RF_FORMAT; goto done; }
        out[5]=hash_bytes(out[5],matrices,count*48); out[5]=hash_bytes(out[5],displacement,12);
        out[5]=hash_bytes(out[5],generations,count*2); displacement[0]=0;
        status=rf_model_prepare_skinning(stored,matrices,count,(uint16_t)state.generation,prepared,prepared_generations,count);if(status)goto done;
        out[5]=hash_bytes(out[5],prepared,count*48);out[5]=hash_bytes(out[5],prepared_generations,count*2);
        for(vertex_index=0;vertex_index<vertex_count;++vertex_index) {
            rf_model_vertex *v=vertices+vertex_index;float position[3];
            status=rf_model_collision_vertex(v->position,v->weights,v->bones,prepared,count,position);if(status)goto done;
            out[5]=hash_bytes(out[5],position,sizeof(position));
        }
        for(render_batch=0;render_batch<geometry.batch_count;++render_batch) {
            const rf_model_draw_batch *draw=geometry.batches+render_batch;uint32_t n;
            rf_model_triangle_output triangle_output={render_buffers.vertices,render_indices,draw->vertices,4096,0,24576};
            memset(render_memory,0xa5,render_bytes);
            status=rf_model_geometry_render_batch(&geometry,render_batch,prepared,count,&render_view,&render_lights,&render_output,&render_buffers);if(status)goto done;
            for(n=0;n<draw->vertices;++n) {
                uint32_t index=draw->first_vertex+n;int32_t distance=geometry.reuse[index];
                uint8_t *v=render_buffers.vertices[n];
                if(render_view.screen_clip && render_buffers.cache[n].clip)continue;
                if(distance>0) {
                    if(memcmp(render_buffers.cache[n].world,render_buffers.cache[n-distance].world,12) ||
                        render_buffers.cache[n].clip!=render_buffers.cache[n-distance].clip) {status=RF_FORMAT;goto done;}
                    if(render_buffers.cache[n].clip)continue;
                }
                if(memcmp(v+24,vertices[index].uv,8) || v[16]!=60 || v[17]!=50 || v[18]!=40 || v[19]!=255 ||
                    v[20]!=0xa5 || v[21]!=0xa5 || v[22]!=0xa5 || v[32]!=0xa5 || v[39]!=0xa5) {status=RF_FORMAT;goto done;}
            }
            if(placement && clip_planes.near_depth>0) {
                status=rf_model_geometry_clip_near(&geometry,render_batch,&render_buffers,clip_planes.near_depth);if(status)goto done;
            }
            status=rf_model_geometry_emit_batch(&geometry,render_batch,&render_buffers,&render_view,&clip_planes,&clip_projection,
                &render_output,0,clip_pool,&triangle_output);if(status)goto done;
            if(triangle_output.index_count%3) {status=RF_FORMAT;goto done;}
            for(n=0;n<triangle_output.index_count;++n)if(render_indices[n]>=triangle_output.vertex_count) {status=RF_FORMAT;goto done;}
            emitted_indices+=triangle_output.index_count;
            if(preview && (sink || frame==preview_frame)) {
                if(triangle_output.index_count>budget/sizeof(rf_preview_vertex)-preview->count) {status=RF_RANGE;goto done;}
                for(n=0;n<triangle_output.index_count;++n) {
                    const uint8_t *v=render_buffers.vertices[render_indices[n]];float xy[2],q,uv[2];
                    rf_preview_vertex *p=preview->vertices+preview->count++;
                    memcpy(xy,v,8);memcpy(&q,v+12,4);memcpy(uv,v+24,8);
                    if(!isfinite(xy[0]) || !isfinite(xy[1]) || !isfinite(q) || q<=0) {status=RF_FORMAT;goto done;}
                    p->position[0]=floorf(xy[0]*16)/16;p->position[1]=floorf(xy[1]*16)/16;
                    p->position[2]=(1000.0f/999.9f)*(1-.1f*q)*16777215;
                    p->color[0]=p->color[1]=p->color[2]=1;
                    p->texture[0]=uv[0]*q;p->texture[1]=uv[1]*q;p->texture[2]=q;p->material=draw->material;
                    p->lightmap_texture[0]=p->lightmap_texture[1]=0;p->lightmap_texture[2]=q;p->lightmap=UINT32_MAX;
                }
            }
        }
        out[2]=frame+1;
        if(sink) {preview->bytes=preview->count*sizeof(rf_preview_vertex);status=sink(sink_context,frame,preview);if(status)goto done;}
    }
    if(!emitted_indices && !placement)status=RF_FORMAT;
done:
    free(workspace);
    free(clip_pool);free(render_indices);
    free(render_memory);
    rf_model_geometry_close(&geometry);
    free(stored);
    free(payload);
    if (opened==2) rf_vpp_close(&archive);
    if (opened) rf_vpp_close(&meshes);
    out[0]=status==RF_OK ? 2u : 0x80000000u | (uint32_t)(-status);
    return status;
}

int rf_animation_check(const char *meshes_path,const char *motions_path,uint32_t out[8])
{ return animation_run(meshes_path,motions_path,out,NULL,0,0,NULL,NULL,NULL); }

int rf_animation_placement_from_level(const rf_level *level,const rf_level_entity *entity,
    rf_animation_placement *placement)
{
    rf_animation_placement value={0};uint32_t i,j;
    if(!level || !entity || !placement)return RF_RANGE;
    for(i=0;i<3;++i) {
        if(!isfinite(level->player_position[i]) || !isfinite(entity->position[i]))return RF_FORMAT;
        value.world_view.camera[i]=level->player_position[i];value.position[i]=entity->position[i];
        for(j=0;j<3;++j) {
            float scale=i==1?4.0f/3.0f:1.0f;
            if(!isfinite(level->player_orientation[i][j]) || !isfinite(entity->orientation[i][j]))return RF_FORMAT;
            value.world_view.rotation[i*3+j]=level->player_orientation[i][j]*scale;
            if(!isfinite(value.world_view.rotation[i*3+j]))return RF_FORMAT;
            value.orientation[i*3+j]=entity->orientation[i][j];
        }
    }
    value.world_view.perspective=value.world_view.compute_clip=value.world_view.clipping=1;
    value.world_view.far_clip=1;value.world_view.far_depth=1000;
    value.world_view.screen[0]=320;value.world_view.screen[1]=-240;
    value.world_view.screen[2]=320;value.world_view.screen[3]=240;
    value.planes.near_depth=.1f;value.planes.far_depth=1000;
    value.clip_projection.scale[0]=320;value.clip_projection.scale[1]=240;value.clip_projection.clamp=1;
    *placement=value;return RF_OK;
}
int rf_animation_preview_placed(const char *meshes_path,const char *motions_path,
    const rf_animation_placement *placement,uint32_t frame,rf_preview_mesh *mesh,uint32_t budget)
{
    uint32_t out[8];int status;
    if(!placement || !mesh || mesh->vertices || frame>=64 || budget<sizeof(rf_preview_vertex))return RF_RANGE;
    memset(mesh,0,sizeof(*mesh));mesh->vertices=malloc(budget);if(!mesh->vertices)return RF_IO;
    status=animation_run(meshes_path,motions_path,out,mesh,frame,budget,NULL,NULL,placement);
    if(status) {rf_preview_close(mesh);return status;}
    mesh->bytes=mesh->count*sizeof(rf_preview_vertex);return RF_OK;
}

int rf_animation_preview(const char *meshes_path,const char *motions_path,uint32_t frame,rf_preview_mesh *mesh,uint32_t budget)
{
    uint32_t out[8];int status;
    if(!mesh || mesh->vertices || frame>=64 || budget<sizeof(rf_preview_vertex))return RF_RANGE;
    memset(mesh,0,sizeof(*mesh));mesh->vertices=malloc(budget);if(!mesh->vertices)return RF_IO;
    status=animation_run(meshes_path,motions_path,out,mesh,frame,budget,NULL,NULL,NULL);
    if(status || !mesh->count) {rf_preview_close(mesh);return status?status:RF_FORMAT;}
    mesh->bytes=mesh->count*sizeof(rf_preview_vertex);return RF_OK;
}
int rf_animation_stream(const char *meshes_path,const char *motions_path,uint32_t budget,rf_animation_frame_sink sink,void *context)
{
    rf_preview_mesh mesh={0};uint32_t out[8];int status;
    if(!sink || budget<sizeof(rf_preview_vertex))return RF_RANGE;
    mesh.vertices=malloc(budget);if(!mesh.vertices)return RF_IO;
    status=animation_run(meshes_path,motions_path,out,&mesh,0,budget,sink,context,NULL);
    rf_preview_close(&mesh);return status;
}
int rf_animation_stream_placed(const char *meshes_path,const char *motions_path,uint32_t budget,
    const rf_animation_placement *placement,rf_animation_frame_sink sink,void *context)
{
    rf_preview_mesh mesh={0};uint32_t out[8];int status;
    if(!placement || !sink || budget<sizeof(rf_preview_vertex))return RF_RANGE;
    mesh.vertices=malloc(budget);if(!mesh.vertices)return RF_IO;
    status=animation_run(meshes_path,motions_path,out,&mesh,0,budget,sink,context,placement);
    rf_preview_close(&mesh);return status;
}
