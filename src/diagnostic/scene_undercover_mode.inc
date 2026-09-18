#ifndef RF_SCENE_UNDERCOVER_MODE_INC
#define RF_SCENE_UNDERCOVER_MODE_INC
/* Include after scene_weapon_custom_actions.inc. Authored weapons.tbl:
 * fp_ugun, separate silencer.v3d, Silencer On/Off and Silencer Launch.
 * Port policy: commit suppression at actual animation completion; reject fire
 * during transition. Authored sub-animation visibility markers are unresolved.
 * Draw separate silencer.v3m using pose below whenever visible; do NOT hide a
 * guessed handgun batch. The optional resource owner below supplies them. */
typedef struct scene_undercover_mode {
    rf_model_attachment attachment;uint32_t attached,pending,target;
} scene_undercover_mode;
static int scene_undercover_mode_init(scene_undercover_mode *m,const rf_player_weapon *w,
    rf_vpp *meshes,uint32_t initially_attached)
{
    rf_model_file file;scene_undercover_mode value={0};uint32_t i;int status;
    if(!m || !w || !meshes || initially_attached>1)return RF_RANGE;
    status=rf_model_file_open(&file,meshes,"fp_ugun.v3c");if(status)return status;
    if(!file.lod_count)return RF_FORMAT;
    for(i=0;i<file.lods[0].attachment_count;i++) {
        status=rf_model_file_attachment(&file,0,i,&value.attachment);if(status)return status;
        if(!strcmp(value.attachment.name,"silencer"))break;
    }
    if(i==file.lods[0].attachment_count)return RF_NOT_FOUND;
    if(value.attachment.parent<0 || (uint32_t)value.attachment.parent>=w->bone_count)return RF_FORMAT;
    /* Installed fp_ugun silencer tag follows gun bone16, not the loose
     * bdbn-silencer01 hand-animation bone. Resolve through the authored parent. */
    if(strcmp(w->bones[value.attachment.parent].name,"bdbn-gun01"))return RF_FORMAT;
    value.attached=initially_attached;value.target=initially_attached;*m=value;return RF_OK;
}
static uint32_t scene_undercover_mode_visible(const scene_undercover_mode *m)
{return m && (m->attached || m->pending);}
static uint32_t scene_undercover_mode_can_fire(const scene_undercover_mode *m)
{return m && !m->pending;}
static const char *scene_undercover_mode_launch(const scene_undercover_mode *m)
{return m && m->attached?"Silencer Launch":"Glock Launch";}
static const char *scene_undercover_mode_transition_sound(const scene_undercover_mode *m)
{return m && m->target?"Silencer On":"Silencer Off";}
/* Caller admits a toggle edge while selected, idle and not reloading. */
static int scene_undercover_mode_toggle(scene_undercover_mode *m,
    scene_weapon_custom_actions *actions,rf_player_weapon *w,uint32_t *started)
{
    int status;if(!m || !actions || !w || !started)return RF_RANGE;*started=0;
    if(m->pending || (w->initialized && w->current!=0))return RF_OK;
    status=scene_weapon_custom_actions_start(actions,w,m->attached?1:0);if(status)return status;
    m->target=!m->attached;m->pending=1;*started=1;return RF_OK;
}
/* Call after the one ordinary view step; do not advance animation twice.
 * Other actions interrupt/cancel rather than silently committing suppression. */
static int scene_undercover_mode_poll(scene_undercover_mode *m,
    scene_weapon_custom_actions *actions,rf_player_weapon *w,uint32_t *changed)
{
    int status;if(!m || !actions || !w || !changed)return RF_RANGE;*changed=0;
    if(!m->pending || w->current==3)return RF_OK;
    if(!w->current){m->attached=m->target;*changed=1;}else m->target=m->attached;
    status=scene_weapon_custom_actions_detach(actions,w);if(status)return status;
    m->pending=0;return RF_OK;
}
static int scene_undercover_mode_cancel(scene_undercover_mode *m,
    scene_weapon_custom_actions *actions,rf_player_weapon *w)
{
    int status;if(!m || !actions || !w)return RF_RANGE;
    status=scene_weapon_custom_actions_detach(actions,w);if(status)return status;
    m->pending=0;m->target=m->attached;return RF_OK;
}
/* Exact authored attachment composed with evaluated pose, model space.
 * The renderer applies its ordinary FP model-camera/FOV/depth path afterwards. */
static int scene_undercover_mode_pose(const scene_undercover_mode *m,const rf_player_weapon *w,float out[12])
{
    float local[12];int status;
    if(!m || !w || !out || !w->initialized || m->attachment.parent<0 ||
        (uint32_t)m->attachment.parent>=w->bone_count)return RF_RANGE;
    status=rf_model_attachment_transform(m->attachment.rotation,m->attachment.position,local);if(status)return status;
    return rf_model_compose_transform(local,w->pose[m->attachment.parent],out);
}

/* Separate authored prop owner; returned geometry/materials are directly usable
 * by the FP renderer with scene_undercover_mode_pose. No world-camera placement
 * or static approximation of the animated attachment is introduced. */
typedef struct scene_undercover_resources {
    scene_undercover_mode mode;scene_weapon_custom_actions *actions;
    rf_static_render_resource render;rf_model_materials materials;
    uint32_t resident_bytes,peak_bytes;
} scene_undercover_resources;
static int scene_undercover_resources_close(scene_undercover_resources **owner,rf_player_weapon *w)
{
    scene_undercover_resources *o;int status;if(!owner || !(o=*owner))return RF_OK;
    status=scene_weapon_custom_actions_close(&o->actions,w);if(status)return status;
    rf_model_materials_close(&o->materials);rf_static_render_resource_close(&o->render);
    free(o);*owner=NULL;return RF_OK;
}
static int scene_undercover_resources_open(rf_player_weapon *w,rf_vpp *meshes,rf_vpp *motions,
    rf_vpp *maps,uint32_t map_count,uint32_t initially_attached,uint32_t budget,scene_undercover_resources **result)
{
    scene_undercover_resources *o;rf_model_file *file;uint32_t used;int status;
    if(!w || !meshes || !motions || !maps || !map_count || !result || *result ||
        budget<sizeof(*o)+sizeof(*file))return RF_RANGE;
    o=calloc(1,sizeof(*o));file=malloc(sizeof(*file));if(!o || !file){free(o);free(file);return RF_IO;}
    used=sizeof(*o)+sizeof(*file);
    status=scene_undercover_mode_init(&o->mode,w,meshes,initially_attached);if(status)goto fail;
    status=scene_weapon_custom_actions_open(w,motions,2,budget-used,&o->actions);if(status)goto fail;
    used+=o->actions->resident_bytes;
    status=rf_model_file_open(file,meshes,"silencer.v3m");if(status)goto fail;
    status=rf_static_render_resource_open(file,budget-used,&o->render);if(status)goto fail;
    used+=o->render.allocated_bytes;
    status=rf_model_materials_open(&o->materials,file,maps,map_count,budget-used);if(status)goto fail;
    o->peak_bytes=used+o->materials.peak_bytes;
    o->resident_bytes=used+o->materials.resident_bytes-sizeof(*file);
    free(file);*result=o;return RF_OK;
fail:
    free(file);scene_undercover_resources_close(&o,w);return status;
}
#endif

