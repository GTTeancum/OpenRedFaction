#include "rf/burn.h"
/* Four owned active emitters, with four available particle nodes. */
static int burn_resolved_probe(rf_geometry_collision_world *world)
{
    struct {rf_particle_emitter_runtime emitters[4];float pose[4][12];
        rf_particle_emitter_parent parent;int32_t owner,now;uint32_t room,enabled;
        float dt,elapsed;} query;
    struct {int32_t status;rf_burn_attachment_result placement;uint32_t random;
        rf_particle_emitter_runtime emitters[4];rf_particle particles[4];} result;
    rf_level_particle_state *state=calloc(1,sizeof(*state));uint32_t i;
    if(!state)return 10;
    while(fread(&query,sizeof(query),1,stdin)==1) {
        rf_burn_record record={0};rf_burn_attachment_runtime runtime={0};
        rf_particle_pool_init(&state->particles,state->records,state->lists,133);
        rf_emitter_pool_init(&state->emitters,state->slots,&state->particles);
        state->lists[1]=(rf_particle_list){500,503};
        for(i=0;i<4;++i) {
            state->records[500+i].next=i==3?1601:501+i;
            state->records[500+i].previous=i?499+i:1601;
            state->slots[i].runtime=query.emitters[i];state->slots[i].active=1;
            state->slots[i].next=i==3?129:i+1;state->slots[i].previous=i?i-1:129;
            record.emitters[i]=i+1;record.attachments[i]=(int32_t)i;
        }
        state->emitters.lists[0].next=4;state->slots[4].previous=128;
        state->emitters.lists[1]=(rf_particle_list){0,3};state->emitters.live=4;
        record.elapsed=query.elapsed;state->random.value=123;
        runtime.pose=(const float (*)[12])query.pose;runtime.bone_count=4;
        runtime.emitters=&state->emitters;runtime.world=world;runtime.parent=query.owner<0?NULL:&query.parent;
        runtime.random=&state->random;runtime.owner=query.owner;runtime.now_ms=query.now;
        runtime.parent_room=query.room;runtime.global_enabled=query.enabled;runtime.frame_seconds=query.dt;
        memset(&result,0,sizeof(result));
        result.status=rf_burn_attachments_resolved(&record,&runtime,&result.placement);
        result.random=state->random.value;
        for(i=0;i<4;++i)result.emitters[i]=state->slots[i].runtime;
        memcpy(result.particles,state->records+500,sizeof(result.particles));
        if(fwrite(&result,sizeof(result),1,stdout)!=1){free(state);return 11;}
    }
    free(state);return ferror(stdin)?12:0;
}
