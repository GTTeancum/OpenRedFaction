#ifndef RF_SCENE_BLIND_PURSUIT_METADATA_INCLUDED
#define RF_SCENE_BLIND_PURSUIT_METADATA_INCLUDED
/* Original class718, read from the already charged constructor entity.tbl
 * scratch. -1 marks a class outside this ordinary handheld consumer. */
typedef struct scene_blind_class_metadata {uint32_t valid,count;float seconds[640];} scene_blind_class_metadata;
static scene_blind_class_metadata scene_blind_classes;
_Static_assert(sizeof(scene_blind_class_metadata)==2568,"Bounded blind pursuit class metadata");
static int scene_blind_classes_read(const void *text,uint32_t bytes)
{
    uint32_t i;int status;float value;
    memset(&scene_blind_classes,0,sizeof(scene_blind_classes));
    if(!text||!bytes||campaign_seeds.class_count>640||!campaign_seeds.classes)return RF_RANGE;
    for(i=0;i<640;i++)scene_blind_classes.seconds[i]=-1;
    for(i=0;i<campaign_seeds.class_count;i++){
        const rf_entity_seed_class *c=campaign_seeds.classes+i;const char *name;
        if((c->physics.flags&0x20019u)!=0x20019u||c->model_kind!=2||
           (c->physics.flags&(0x200u|0x2000u|0x400000u))||(c->physics.flags2&0x100u)||
           c->physics.use_kind==1||!isfinite(c->physics.mass)||c->physics.mass<=0)continue;
        status=scene_turret_class_name(&campaign_seeds,i,&name);if(status)return status;
        status=rf_entity_blind_pursuit_read(text,bytes,name,&value);if(status)return status;
        /* One-day input horizon keeps frame-age arithmetic below2^23 and
         * an expired two-frame margin representable in binary32 seconds. */
        if(!isfinite(value)||value<0||value>86400.f)return RF_RANGE;
        scene_blind_classes.seconds[i]=value;
    }
    scene_blind_classes.count=campaign_seeds.class_count;scene_blind_classes.valid=1;return RF_OK;
}
#endif
