#include "rf/level_particles.h"
#include <stdlib.h>
#include <string.h>
void rf_level_particles_close(rf_level_particles *particles)
{
    if(!particles)return;
    rf_level_particle_materials_close(&particles->materials);
    free(particles->state);memset(particles,0,sizeof(*particles));
}
int rf_level_particles_open(rf_level_particles *particles,const rf_level *level,
    const rf_geometry_collision_world *world,rf_vpp *archives,uint32_t archive_count,
    uint32_t seed,int32_t now_ms,uint32_t budget)
{
    rf_level_particles value={0};rf_level_emitter_reader reader;rf_level_emitter record;
    uint32_t i,j;int status;
    if(!particles || !level || !world || (!archives && archive_count) || now_ms<0 || now_ms>RF_TIMER_PERIOD)return RF_RANGE;
    if((uint64_t)sizeof(value)+sizeof(*value.state)>budget)return RF_RANGE;
    value.resident_bytes=(uint32_t)(sizeof(value)+sizeof(*value.state));
    value.state=calloc(1,sizeof(*value.state));if(!value.state)return RF_RANGE;
    status=rf_particle_pool_init(&value.state->particles,value.state->records,value.state->lists,133);if(status)goto failed;
    status=rf_emitter_pool_init(&value.state->emitters,value.state->slots,&value.state->particles);if(status)goto failed;
    value.state->random.value=seed;
    status=rf_level_particle_materials_open(&value.materials,level,archives,archive_count,
        budget-value.resident_bytes+(uint32_t)sizeof(value.materials));if(status)goto failed;
    value.resident_bytes+=value.materials.resident_bytes-(uint32_t)sizeof(value.materials);
    status=rf_level_emitters_begin(level,&reader);
    if(status==RF_NOT_FOUND){*particles=value;return RF_OK;}
    if(status)goto failed;
    for(i=0;i<value.materials.count;i++) {
        rf_particle_emitter_template source={0};rf_collision_room_location location;
        uint32_t index,texture=value.materials.bindings[i].texture,room;
        status=rf_level_emitter_next(&reader,&record);if(status)goto failed;
        if(record.uid!=value.materials.bindings[i].uid){status=RF_FORMAT;goto failed;}
        for(j=0;j<i;j++)if(value.materials.bindings[j].uid==record.uid){status=RF_FORMAT;goto failed;}
        status=rf_geometry_collision_world_locate(world,record.position,&location);if(status)goto failed;
        room=location.room==UINT32_MAX?0:location.room+1;
        status=rf_level_emitter_template(&record,texture,value.materials.textures[texture].bitmap.frames,&source);if(status)goto failed;
        status=rf_emitter_pool_create(&value.state->emitters,&source,0,room,record.enabled!=0,now_ms,NULL,&value.state->random,&index);
        if(status)goto failed;
        if(index!=i){status=RF_FORMAT;goto failed;}
    }
    *particles=value;return RF_OK;
failed:
    rf_level_particles_close(&value);return status;
}
