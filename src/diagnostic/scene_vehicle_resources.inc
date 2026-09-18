#ifndef RF_SCENE_VEHICLE_RESOURCES_INC
#define RF_SCENE_VEHICLE_RESOURCES_INC
#include "scene_driller_resources.inc"
/* Parameterized static vehicle resource owner, deliberately retaining the
 * existing Driller layout for shared drawing/material/physics consumers.
 * Movement5/7/9 is metadata admission, not a claim that all driving modes
 * have live solvers. Only authored use_kind1 vehicles are admitted. Required
 * seat_name is exact and has no fallback; callers choose the actual driver
 * seat, never infer ownership from tag order. Same bounded sequential loading
 * and resident/peak accounting as Driller. Close with
 * scene_driller_resources_close; place with scene_driller_seat_pose.
 * Source archives may be closed once loading succeeds. */
static int scene_vehicle_resources_open(const char *tables_path,const char *class_name,const char *seat_name,rf_vpp *meshes,
    rf_vpp *maps,uint32_t map_count,uint32_t budget,scene_driller_resources **result)
{
    scene_driller_resources *o=NULL;rf_model_file *file=NULL;rf_vpp tables={0};rf_vpp_entry entry;
    const char *overrides[64];void *table_text=NULL;uint32_t used,i,table_bytes;int status;
    if(!tables_path || !class_name || !*class_name || !seat_name || !*seat_name || !meshes || !maps || !map_count || !result || *result || budget<sizeof(*o)+sizeof(*file))return RF_RANGE;
    o=calloc(1,sizeof(*o));if(!o)return RF_IO;o->seat=-1;used=sizeof(*o);
    status=rf_vpp_open(&tables,tables_path);if(status)goto fail;
    status=rf_vpp_find(&tables,"entity.tbl",&entry);if(status)goto fail;table_bytes=entry.size;
    if(table_bytes>budget-used){status=RF_RANGE;goto fail;}
    o->peak_bytes=used+table_bytes;
    table_text=malloc(table_bytes);if(!table_text){status=RF_IO;goto fail;}
    status=rf_vpp_read(&tables,&entry,0,table_text,table_bytes);if(status)goto fail;
    status=rf_entity_rotation_values_read(table_text,table_bytes,class_name,&o->rotation);if(status)goto fail;
    free(table_text);table_text=NULL;
    status=rf_entity_assets_load(tables_path,class_name,"",&o->assets,budget-used);if(status)goto fail;
    status=rf_model_compiled_filename(o->assets.model,o->model,".v3m");if(status)goto fail;
    status=rf_entity_physics_config_load(&tables,class_name,budget-used,&o->physics);if(status)goto fail;
    status=rf_entity_movement_load(&tables,class_name,budget-used,&o->movement);if(status)goto fail;
    status=rf_entity_vitals_config_load(&tables,class_name,budget-used,&o->vitals);if(status)goto fail;
    if(o->physics.authored.use_kind!=1 || (o->physics.authored.movement_index!=5 && o->physics.authored.movement_index!=7 && o->physics.authored.movement_index!=9)){status=RF_FORMAT;goto fail;}
    rf_vpp_close(&tables);
    file=malloc(sizeof(*file));if(!file){status=RF_IO;goto fail;}used+=sizeof(*file);
    status=rf_model_file_open(file,meshes,o->model);if(status)goto fail;
    status=rf_static_render_resource_open(file,budget-used,&o->render);if(status)goto fail;used+=o->render.allocated_bytes;
    status=rf_static_model_tags_open(file,budget-used,&o->tags);if(status)goto fail;used+=o->tags.allocated_bytes;
    for(i=0;i<o->tags.count;i++)if(!strcmp(o->tags.items[i].name,seat_name)){o->seat=(int32_t)i;break;}
    if(o->seat<0){status=RF_NOT_FOUND;goto fail;}
    if(o->assets.texture_count>64){status=RF_RANGE;goto fail;}
    for(i=0;i<o->assets.texture_count;i++)overrides[i]=o->assets.textures[i];
    status=rf_model_materials_open_skin(&o->materials,file,overrides,o->assets.texture_count,maps,map_count,budget-used);if(status)goto fail;
    if(used+o->materials.peak_bytes>o->peak_bytes)o->peak_bytes=used+o->materials.peak_bytes;
    o->resident_bytes=used+o->materials.resident_bytes-sizeof(*file);
    free(file);*result=o;return RF_OK;
fail:
    rf_vpp_close(&tables);free(table_text);free(file);scene_driller_resources_close(&o);return status;
}
#endif
