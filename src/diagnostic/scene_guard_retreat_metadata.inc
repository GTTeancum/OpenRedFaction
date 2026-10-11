#ifndef RF_SCENE_GUARD_RETREAT_METADATA_INCLUDED
#define RF_SCENE_GUARD_RETREAT_METADATA_INCLUDED
/* The seed ABI does not retain class720. Read only the actual selected class
 * from the existing constructor archive, within its existing table-scratch
 * charge. No hardcoded level/UID, default style, or later asset scan. */
static struct {uint32_t valid,cls;int32_t style;} scene_guard_retreat_class;
static int scene_guard_retreat_token(const unsigned char *text,uint32_t bytes,
    uint32_t *at,char out[256],uint32_t *quoted)
{
    uint32_t n=0;unsigned char c;
    for(;;){
        while(*at<bytes&&(text[*at]==' '||text[*at]=='\t'||text[*at]=='\r'||text[*at]=='\n'))++*at;
        if(*at>=bytes)return RF_NOT_FOUND;
        if(bytes-*at>=2&&text[*at]=='/'&&text[*at+1]=='/'){
            while(*at<bytes&&text[*at]!='\n')++*at;continue;
        }
        if(bytes-*at>=2&&text[*at]=='/'&&text[*at+1]=='*'){
            *at+=2;while(bytes-*at>=2&&(text[*at]!='*'||text[*at+1]!='/'))++*at;
            if(bytes-*at<2)return RF_FORMAT;*at+=2;continue;
        }
        break;
    }
    *quoted=text[*at]=='"';if(*quoted)++*at;
    while(*at<bytes){
        c=text[*at];
        if(*quoted){if(c=='"'){++*at;out[n]=0;return RF_OK;}if(c=='\n'||c=='\r'||!c)return RF_FORMAT;}
        else if(c==' '||c=='\t'||c=='\r'||c=='\n'||c=='"')break;
        if(n==255)return RF_RANGE;out[n++]=(char)c;++*at;
    }
    if(*quoted||!n)return RF_FORMAT;out[n]=0;return RF_OK;
}
static int scene_guard_retreat_class_read(const void *data,uint32_t bytes,int32_t *style)
{
    const unsigned char *text=data;uint32_t at=0,quoted,selected=0,found=0,seen=0;
    char token[256];int status;int32_t result=-1;
    if(!data||!bytes||!style)return RF_RANGE;
    while(!(status=scene_guard_retreat_token(text,bytes,&at,token,&quoted))){
        const char *name=token;
        if(quoted)continue;
        if(rf_emitter_name_lookup(&name,1,"$Name:")==0){
            if(selected)break;
            status=scene_guard_retreat_token(text,bytes,&at,token,&quoted);if(status||!quoted)return RF_FORMAT;
            selected=rf_emitter_name_lookup(&name,1,"guard1")==0;found|=selected;
        }else if(selected&&rf_emitter_name_lookup(&name,1,"$Attack")==0){
            status=scene_guard_retreat_token(text,bytes,&at,token,&quoted);if(status||quoted)return RF_FORMAT;
            if(rf_emitter_name_lookup(&name,1,"Style:")!=0)continue;
            if(seen++)return RF_FORMAT;
            status=scene_guard_retreat_token(text,bytes,&at,token,&quoted);if(status||!quoted)return RF_FORMAT;
            /* Only the proven Direct3 profile is admitted. Another actual
             * class style is a supported absence, not a guessed default. */
            result=rf_emitter_name_lookup(&name,1,"Direct")==0?3:-1;
        }
    }
    if(status!=RF_OK&&status!=RF_NOT_FOUND)return status;
    if(!found||!seen)return RF_FORMAT;*style=result;return RF_OK;
}
static int scene_guard_retreat_class_load(rf_vpp *tables,const rf_vpp_entry *entry,uint32_t budget)
{
    uint32_t cls=UINT32_MAX;unsigned char *text;int32_t style;int status;
    memset(&scene_guard_retreat_class,0,sizeof(scene_guard_retreat_class));
    for(uint32_t i=0;i<campaign_seeds.class_count;i++){
        const char *name;status=scene_turret_class_name(&campaign_seeds,i,&name);if(status)return status;
        if(rf_emitter_name_lookup(&name,1,"guard1")==0){if(cls!=UINT32_MAX)return RF_FORMAT;cls=i;}
    }
    if(cls==UINT32_MAX)return RF_OK;
    if(!entry||!entry->size||entry->size>budget)return RF_RANGE;
    text=malloc(entry->size);if(!text)return RF_IO;
    status=rf_vpp_read(tables,entry,0,text,entry->size);
    if(!status)status=scene_guard_retreat_class_read(text,entry->size,&style);
    free(text);if(status)return status;
    scene_guard_retreat_class.cls=cls;scene_guard_retreat_class.style=style;scene_guard_retreat_class.valid=1;
    return RF_OK;
}
#endif
