#ifndef RF_SCENE_NOISE_INVESTIGATION_METADATA_INCLUDED
#define RF_SCENE_NOISE_INVESTIGATION_METADATA_INCLUDED
/* Constructor-only class720 lookup, sharing the already charged entity.tbl
 * scratch. 255 is an absent/unsupported default, never a guessed Direct. */
typedef struct scene_noise_class_metadata {uint32_t valid,count;uint8_t style[640];} scene_noise_class_metadata;
static scene_noise_class_metadata scene_noise_classes;
_Static_assert(sizeof(scene_noise_class_metadata)==648,"Bounded noise class metadata");
static int scene_noise_classes_load(rf_vpp *tables,const rf_vpp_entry *entry,uint32_t budget)
{
    unsigned char *text;char token[256];uint32_t at=0,quoted,selected=UINT32_MAX,seen=0;int status;
    memset(&scene_noise_classes,0,sizeof(scene_noise_classes));
    if(campaign_seeds.class_count>640||!entry||!entry->size||entry->size>budget)return RF_RANGE;
    memset(scene_noise_classes.style,255,sizeof(scene_noise_classes.style));
    text=malloc(entry->size);if(!text)return RF_IO;
    status=rf_vpp_read(tables,entry,0,text,entry->size);if(status)goto done;
    while(!(status=scene_guard_retreat_token(text,entry->size,&at,token,&quoted))){
        const char *name=token;if(quoted)continue;
        if(rf_emitter_name_lookup(&name,1,"$Name:")==0){
            status=scene_guard_retreat_token(text,entry->size,&at,token,&quoted);if(status||!quoted){status=RF_FORMAT;goto done;}
            selected=UINT32_MAX;seen=0;
            for(uint32_t i=0;i<campaign_seeds.class_count;i++){
                const char *cls;status=scene_turret_class_name(&campaign_seeds,i,&cls);if(status)goto done;
                if(rf_emitter_name_lookup(&cls,1,token)==0){selected=i;break;}
            }
        }else if(selected!=UINT32_MAX&&rf_emitter_name_lookup(&name,1,"$Attack")==0){
            status=scene_guard_retreat_token(text,entry->size,&at,token,&quoted);if(status||quoted){status=RF_FORMAT;goto done;}
            if(rf_emitter_name_lookup(&name,1,"Style:")!=0)continue;
            if(seen++){status=RF_FORMAT;goto done;}
            status=scene_guard_retreat_token(text,entry->size,&at,token,&quoted);if(status||!quoted){status=RF_FORMAT;goto done;}
            /* All measured Waiting source families use Direct3. Explicit
             * entity overrides are resolved separately through59c028. */
            scene_noise_classes.style[selected]=rf_emitter_name_lookup(&name,1,"Direct")==0?3:255;
        }
    }
    if(status==RF_NOT_FOUND){scene_noise_classes.count=campaign_seeds.class_count;scene_noise_classes.valid=1;status=scene_blind_classes_read(text,entry->size);}
 done:free(text);if(status)memset(&scene_noise_classes,0,sizeof(scene_noise_classes));return status;
}
#endif
