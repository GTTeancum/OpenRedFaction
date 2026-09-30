#ifndef SCENE_TURRET_DEATH_EFFECTS_INC
#define SCENE_TURRET_DEATH_EFFECTS_INC
/* Include after scene_script_explode_runtime.inc and turret owner/combat audio.
 * Presentation only: Explode Anim Radius never becomes blast damage/GeoMod.
 * Explicit live->dead notifications avoid replaying effects for loaded wrecks. */
typedef struct scene_turret_death_profile {
    uint32_t class_index;char sound[64],clip_name[64];float radius,offset[3];int32_t clip;
} scene_turret_death_profile;
typedef struct scene_turret_death_pending {
    uint32_t seen,pending,uid,profile;float position[3],origin[3];
} scene_turret_death_pending;
static scene_turret_death_profile scene_turret_death_profiles[SCENE_TURRET_CLASS_LIMIT];
static scene_turret_death_pending scene_turret_death_pending_rows[SCENE_TURRET_OWNER_LIMIT];
static uint32_t scene_turret_death_profile_count;
uint32_t rf_scene_turret_death_effects[8]; /* transitions,audio requests,visual starts,visual skips,unavailable,duplicates,last UID,errors */
static char *scene_turret_death_trim(char *p)
{
    char *end;while(*p==' '||*p=='\t')++p;end=p+strlen(p);
    while(end>p&&(end[-1]==' '||end[-1]=='\t'))*--end=0;return p;
}
static int scene_turret_death_string(char *p,char out[64])
{
    uint32_t n;p=scene_turret_death_trim(p);n=(uint32_t)strlen(p);
    if(n<2||n>65||p[0]!='"'||p[n-1]!='"')return RF_FORMAT;
    memcpy(out,p+1,n-2);out[n-2]=0;return RF_OK;
}
static int scene_turret_death_number(char *p,float *out)
{
    int sign=1,status;p=scene_turret_death_trim(p);
    if(*p=='-'||*p=='+'){if(*p=='-')sign=-1;++p;}
    status=scene_vehicle_primary_number(p,out);if(!status)*out*=sign;return status;
}
static int scene_turret_death_read(const void *bytes,uint32_t length,const char *name,scene_turret_death_profile *out)
{
    const unsigned char *text=bytes;scene_turret_death_profile v={0};uint32_t at=0,seen=0;int selected=0,found=0;
    if(!bytes||!name||!out)return RF_RANGE;v.clip=-1;
    while(at<length){char line[512],*p,*value;uint32_t n=0,bit=0;int quoted=0,status=RF_OK;
        while(at<length&&text[at]!='\r'&&text[at]!='\n'){
            if(n+1>=sizeof(line))return RF_RANGE;line[n++]=(char)text[at++];}
        while(at<length&&(text[at]=='\r'||text[at]=='\n'))++at;line[n]=0;
        for(p=line;*p;p++){if(*p=='"')quoted=!quoted;if(!quoted&&p[0]=='/'&&p[1]=='/'){*p=0;break;}}
        p=scene_turret_death_trim(line);
        if(!strncmp(p,"$Name:",6)){
            char label[64];if(selected)break;
            status=scene_turret_death_string(p+6,label);if(status)return status;
            if(!strcmp(label,name))selected=found=1;continue;
        }
        if(!selected)continue;if(!strcmp(p,"#End"))break;
        value=strchr(p,':');if(!value)continue;*value++=0;p=scene_turret_death_trim(p);
        if(!strcmp(p,"$DeathSnd")){bit=1;status=scene_turret_death_string(value,v.sound);}
        else if(!strcmp(p,"$Explode Anim")){bit=2;status=scene_turret_death_string(value,v.clip_name);}
        else if(!strcmp(p,"$Explode Anim Radius")){bit=4;status=scene_turret_death_number(value,&v.radius);}
        else if(!strcmp(p,"$Explode Offset")){
            char *next;bit=8;value=scene_turret_death_trim(value);n=(uint32_t)strlen(value);
            if(n<2||value[0]!='<'||value[n-1]!='>')return RF_FORMAT;
            value[n-1]=0;++value;
            for(uint32_t k=0;k<3;k++){
                next=strchr(value,',');if((k<2&&!next)||(k==2&&next))return RF_FORMAT;
                if(next)*next=0;status=scene_turret_death_number(value,v.offset+k);if(status)return status;
                if(next)value=next+1;
            }
        }
        if(status)return status;if(bit&seen)return RF_FORMAT;seen|=bit;
    }
    if(!found)return RF_NOT_FOUND;
    if(v.clip_name[0]&&(!(seen&4)||v.radius<=0))return RF_FORMAT;
    *out=v;return RF_OK;
}
/* Call after all component restore assignments too: a loaded dead model is
 * state, not a new death notification. Old queued presentation must not leak. */
static void scene_turret_death_effects_reset_pending(void)
{memset(scene_turret_death_pending_rows,0,sizeof(scene_turret_death_pending_rows));}
static void scene_turret_death_effects_close(void)
{
    scene_turret_death_effects_reset_pending();memset(scene_turret_death_profiles,0,sizeof(scene_turret_death_profiles));
    scene_turret_death_profile_count=0;
    /* Vclip resources belong to scene_script_explode_effects_close. */
}
static int scene_turret_death_effects_open(rf_vpp *tables,rf_vpp *maps,uint32_t map_count)
{
    rf_vpp_entry entry;void *text=NULL;uint32_t i;int status;
    scene_turret_death_effects_close();memset(rf_scene_turret_death_effects,0,sizeof(rf_scene_turret_death_effects));
    if(!scene_turret_count)return RF_OK;
    if(!tables||!maps||!map_count)return RF_RANGE;
    status=rf_vpp_find(tables,"entity.tbl",&entry);if(status)goto unavailable;
    if(!entry.size||entry.size>512*1024){status=RF_RANGE;goto unavailable;}
    text=malloc(entry.size);if(!text){status=RF_IO;goto unavailable;}
    status=rf_vpp_read(tables,&entry,0,text,entry.size);if(status)goto unavailable;
    for(i=0;i<scene_turret_model_pair_count;i++){
        uint32_t cls=scene_turret_model_pairs[i].class_index;scene_turret_death_profile p;
        const rf_entity_seed_class *definition;const char *name;
        if(cls>=campaign_seeds.class_count){status=RF_FORMAT;goto unavailable;}
        definition=campaign_seeds.classes+cls;
        if(definition->record_index>=campaign_seeds.records.count){status=RF_FORMAT;goto unavailable;}
        name=campaign_seeds.records.items[definition->record_index].record.class_name;
        status=scene_turret_death_read(text,entry.size,name,&p);
        if(status){++rf_scene_turret_death_effects[4];++rf_scene_turret_death_effects[7];continue;}
        p.class_index=cls;
        if(p.clip_name[0]){
            p.clip=rf_vclip_name_lookup(campaign_clutter_catalogs.names.vclips,p.clip_name);
            status=p.clip<0?RF_NOT_FOUND:scene_script_explode_effects_open(tables,maps,map_count,(uint32_t)p.clip,p.clip_name);
            if(status){p.clip=-1;++rf_scene_turret_death_effects[4];}
        }
        /* Corpse emitter lifetime is separate from the one-shot explosion.
         * No installed stationary class enables that optional emitter. */
        if(definition->corpse.emitter[0])++rf_scene_turret_death_effects[4];
        if(scene_turret_death_profile_count==SCENE_TURRET_CLASS_LIMIT){status=RF_RANGE;goto unavailable;}
        scene_turret_death_profiles[scene_turret_death_profile_count++]=p;
    }
    free(text);return RF_OK;
unavailable:
    free(text);++rf_scene_turret_death_effects[4];++rf_scene_turret_death_effects[7];
    return RF_OK; /* Cosmetic resource failures never undo an actual death. */
}
/* Notify exactly where a living owner transitions to dead, NOT from owner
 * opening, corpse model drawing, snapshot assignment or a polling edge test. */
static void scene_turret_death_effects_queue(scene_turret_owner *o)
{
    uint32_t slot,i;scene_turret_death_pending *pending;float offset[3];
    if(!o||!o->dead||o->damage.effects.health>0)return;
    for(slot=0;slot<scene_turret_count&&scene_turrets+slot!=o;slot++){}
    if(slot>=scene_turret_count||slot>=SCENE_TURRET_OWNER_LIMIT){++rf_scene_turret_death_effects[7];return;}
    pending=scene_turret_death_pending_rows+slot;
    if(pending->seen){++rf_scene_turret_death_effects[5];return;}pending->seen=1;
    ++rf_scene_turret_death_effects[0];rf_scene_turret_death_effects[6]=o->uid;
    for(i=0;i<scene_turret_death_profile_count;i++)if(scene_turret_death_profiles[i].class_index==o->class_index)break;
    if(i==scene_turret_death_profile_count){++rf_scene_turret_death_effects[4];return;}
    pending->uid=o->uid;pending->profile=i;pending->pending=1;
    memcpy(pending->origin,o->position,12);
    scene_turret_world_vector(o->basis,scene_turret_death_profiles[i].offset,offset);
    for(i=0;i<3;i++)pending->position[i]=o->position[i]+offset[i];
}
static int scene_turret_death_effects_tick(scene_stream *s,uint32_t frame)
{
    uint32_t i;int status;if(!s)return RF_RANGE;
    for(i=0;i<SCENE_TURRET_OWNER_LIMIT;i++)if(scene_turret_death_pending_rows[i].pending){
        scene_turret_death_pending *pending=scene_turret_death_pending_rows+i;
        const scene_turret_death_profile *p=scene_turret_death_profiles+pending->profile;
        uint32_t before=rf_scene_script_explode_visual[0];pending->pending=0;
        if(p->sound[0]){combat_sound(p->sound,pending->origin);++rf_scene_turret_death_effects[1];}
        if(p->clip<0){if(p->clip_name[0])++rf_scene_turret_death_effects[3];continue;}
        status=scene_script_explode_effects_start(s,pending->position,frame,p->radius,(uint32_t)p->clip);
        if(status){++rf_scene_turret_death_effects[7];++rf_scene_turret_death_effects[3];continue;}
        if(rf_scene_script_explode_visual[0]>before)++rf_scene_turret_death_effects[2];
        else ++rf_scene_turret_death_effects[3];
        if(scene_script_explode_effects[p->clip]&&scene_script_explode_effects[p->clip]->foley[0]){
            combat_sound(scene_script_explode_effects[p->clip]->foley,pending->position);++rf_scene_turret_death_effects[1];}
    }
    return RF_OK;
}
#endif
