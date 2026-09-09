#include "rf/entity_assets.h"
#include "rf/model.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
int rf_entity_assets_load(const char *path,const char *class_name,const char *skin,
    rf_entity_assets *assets,uint32_t budget)
{
    rf_vpp archive;rf_vpp_entry entry;void *text=NULL;int status;
    if(!path || !class_name || !skin || !assets)return RF_RANGE;
    status=rf_vpp_open(&archive,path);if(status)return status;
    status=rf_vpp_find(&archive,"entity.tbl",&entry);
    if(!status && (!entry.size || entry.size>budget))status=RF_RANGE;
    if(!status) {text=malloc(entry.size);if(!text)status=RF_RANGE;}
    if(!status)status=rf_vpp_read(&archive,&entry,0,text,entry.size);
    if(!status)status=rf_entity_assets_read(text,entry.size,class_name,skin,assets);
    free(text);rf_vpp_close(&archive);return status;
}
int rf_entity_skeletal_filename(const char *authored,char compiled[64])
{
    uint32_t length=0,stem=0;int dot=0;
    if(!authored || !compiled)return RF_RANGE;
    while(length<64 && authored[length]) {
        if(authored[length]=='.') {stem=length;dot=1;}
        ++length;
    }
    if(length==64)return RF_RANGE;
    if(!dot)stem=length;
    if(stem>59)return RF_RANGE;
    memmove(compiled,authored,stem);memcpy(compiled+stem,".v3c",5);
    return RF_OK;
}
int rf_entity_state_motion_open(const char *tables_path,const char *class_name,
    const char *weapon,const char *state,rf_vpp *motions,uint32_t budget,rf_motion_file *file)
{
    char authored[64],compiled[64];rf_motion_file value;int status;
    if(!motions || !file)return RF_RANGE;
    status=rf_entity_state_motion_load(tables_path,class_name,weapon,state,authored,budget);
    if(status)return status;
    if(!*authored)return RF_NOT_FOUND;
    status=rf_motion_compiled_filename(authored,compiled);if(status)return status;
    status=rf_motion_file_open(&value,motions,compiled);if(status)return status;
    *file=value;return RF_OK;
}
typedef struct lexer {const unsigned char *text;uint32_t size,at;} lexer;
static int same(const char *a,const char *b)
{
    while(*a && *b) {unsigned x=(unsigned char)*a++,y=(unsigned char)*b++;
        if(x>='A' && x<='Z')x+=32;if(y>='A' && y<='Z')y+=32;if(x!=y)return 0;}
    return *a==*b;
}
int rf_level_actor_assets_load(const rf_level *level,int32_t uid,const char *tables_path,
    rf_vpp *meshes,uint32_t table_budget,rf_level_actor_assets *result)
{
    rf_level_actor_assets value;char compiled[64];const char *extension;int status;
    if(!level || !tables_path || !meshes || !result)return RF_RANGE;
    status=rf_level_entity_find(level,uid,&value.entity);if(status)return status;
    status=rf_entity_assets_load(tables_path,value.entity.class_name,value.entity.skin,&value.assets,table_budget);
    if(status)return status;
    extension=strrchr(value.assets.model,'.');
    if(!extension || !same(extension,".vcm"))return RF_FORMAT;
    status=rf_entity_skeletal_filename(value.assets.model,compiled);if(status)return status;
    status=rf_vpp_find(meshes,compiled,&value.mesh);if(status)return status;
    *result=value;return RF_OK;
}
static int token(lexer *l,char out[256],int *quoted)
{
    uint32_t n=0;unsigned c;
    for(;;) {
        while(l->at<l->size && l->text[l->at]<=32) {if(!l->text[l->at])return RF_FORMAT;++l->at;}
        if(l->at==l->size)return RF_NOT_FOUND;
        if(l->at+1<l->size && l->text[l->at]=='/' && l->text[l->at+1]=='/') {
            while(l->at<l->size && l->text[l->at]!='\n')++l->at;
        } else break;
    }
    *quoted=l->text[l->at]=='"';
    if(*quoted) {
        ++l->at;
        while(l->at<l->size && l->text[l->at]!='"') {
            c=l->text[l->at++];if(!c || c=='\r' || c=='\n')return RF_FORMAT;
            if(n==255)return RF_RANGE;out[n++]=(char)c;
        }
        if(l->at==l->size)return RF_FORMAT;++l->at;
    } else if(strchr("(){}",l->text[l->at]))out[n++]=(char)l->text[l->at++];
    else while(l->at<l->size && l->text[l->at]>32 && !strchr("(){}\"",l->text[l->at])) {
        if(l->at+1<l->size && l->text[l->at]=='/' && l->text[l->at+1]=='/')break;
        if(n==255)return RF_RANGE;out[n++]=(char)l->text[l->at++];
    }
    out[n]=0;return RF_OK;
}
static int asset(char destination[64],const char *source)
{size_t n=strlen(source);if(!n || n>=64)return RF_RANGE;memcpy(destination,source,n+1);return RF_OK;}
static int sphere_number(lexer *l,float *result)
{
    char t[256],*end;int quoted,status;double value;
    status=token(l,t,&quoted);if(status || quoted)return RF_FORMAT;
    value=strtod(t,&end);if(end==t || *end || !isfinite(value) || fabs(value)>FLT_MAX)return RF_FORMAT;
    *result=(float)value;return RF_OK;
}
int rf_entity_material_read(const void *text,uint32_t bytes,const char *name,
    rf_entity_material *result)
{
    static const char *names[]={"Default","Rock","Metal","Flesh","Water","Lava","Solid","Sand","Ice","Glass"};
    static const char *tags[]={"$elasticity:","$friction:","$density:","$bouyancy:","$traction:"};
    lexer l={(const unsigned char*)text,bytes,0};rf_entity_material value={0};
    float coefficients[5]={0};char t[256];uint32_t i,mask=0;int status,quoted,found=0,in_section=0;
    if(!text || !name || !result)return RF_RANGE;
    for(i=0;i<10;++i)if(same(name,names[i])) {value.index=i;break;}
    while((status=token(&l,t,&quoted))==RF_OK) {
        if(quoted)continue;
        if(same(t,"#Materials")) {in_section=1;continue;}
        if(!in_section)continue;
        if(same(t,"#End"))break;
        if(same(t,"$name:")) {
            if(found)break;
            if(token(&l,t,&quoted) || !quoted)return RF_FORMAT;
            found=same(t,names[value.index]);
        } else if(found) {
            for(i=0;i<5;++i)if(same(t,tags[i])) {
                if(mask&(1u<<i))return RF_FORMAT;
                if(sphere_number(&l,coefficients+i))return RF_FORMAT;
                mask|=1u<<i;break;
            }
        }
    }
    if(status!=RF_OK && status!=RF_NOT_FOUND)return status;
    if(!found)return RF_NOT_FOUND;
    if(mask!=31)return RF_FORMAT;
    value.elasticity=coefficients[0];value.friction=coefficients[1];value.density=coefficients[2];
    value.buoyancy=coefficients[3];value.traction=coefficients[4];*result=value;return RF_OK;
}
int rf_entity_sphere_declarations_read(const void *text,uint32_t bytes,const char *class_name,
    rf_entity_sphere_declarations *result)
{
    lexer l={(const unsigned char*)text,bytes,0};rf_entity_sphere_declarations value={0};char t[256];
    int status,quoted,selected=0,found=0;
    if(!text || !class_name || !*class_name || !result)return RF_RANGE;
    while((status=token(&l,t,&quoted))==RF_OK) {
        if(quoted)continue;
        if(same(t,"$Name:")) {
            if(found)break;
            if(token(&l,t,&quoted) || !quoted)return RF_FORMAT;
            selected=same(t,class_name);found=selected;
        } else if(selected && same(t,"$Collision")) {
            rf_entity_sphere_override *o;lexer saved;
            if(token(&l,t,&quoted) || quoted)return RF_FORMAT;
            if(!same(t,"Sphere:"))continue;
            if(value.count==8)return RF_RANGE;o=value.items+value.count++;
            if(token(&l,t,&quoted) || !quoted || strlen(t)>=24)return RF_FORMAT;
            memcpy(o->name,t,strlen(t)+1);o->radius=-1;o->parameter_10=-1;
            if(sphere_number(&l,&o->scalar_sp) || sphere_number(&l,&o->scalar_mp))return RF_FORMAT;
            saved=l;status=token(&l,t,&quoted);
            if(!status && !quoted && same(t,"+radius:")) {if(sphere_number(&l,&o->radius))return RF_FORMAT;}
            else l=saved;
            saved=l;status=token(&l,t,&quoted);
            if(!status && !quoted && same(t,"+spring")) {
                float length;
                if(token(&l,t,&quoted) || quoted || !same(t,"constant:") || sphere_number(&l,&o->parameter_10))return RF_FORMAT;
                if(token(&l,t,&quoted) || quoted || !same(t,"+spring"))return RF_FORMAT;
                if(token(&l,t,&quoted) || quoted || !same(t,"length:") || sphere_number(&l,&length))return RF_FORMAT;
                memcpy(&o->opaque_14,&length,4);
            } else l=saved;
        }
    }
    if(status!=RF_OK && status!=RF_NOT_FOUND)return status;
    if(!found)return RF_NOT_FOUND;*result=value;return RF_OK;
}
static int state_group_exists(const void *text,uint32_t bytes,const char *class_name,const char *weapon)
{
    lexer l={(const unsigned char*)text,bytes,0};char t[256];int status,quoted,selected=0;
    while((status=token(&l,t,&quoted))==RF_OK) {
        if(quoted)continue;
        if(same(t,"$Name:")) {
            if(selected)return RF_NOT_FOUND;
            status=token(&l,t,&quoted);if(status || !quoted)return RF_FORMAT;
            selected=same(t,class_name);if(selected && !*weapon)return RF_OK;
        } else if(selected && same(t,"+Weapon")) {
            status=token(&l,t,&quoted);if(status || quoted || !same(t,"Specific:"))return RF_FORMAT;
            status=token(&l,t,&quoted);if(status || !quoted)return RF_FORMAT;
            if(same(t,weapon))return RF_OK;
        }
    }
    return status;
}
int rf_entity_state_set_open(const char *path,const char *class_name,const char *weapon,
    rf_vpp *motions,uint32_t budget,rf_entity_state_set *result)
{
    static const char *names[23]={"stand","attack_stand","walk","attack_walk","run","attack_run",
        "flee_run","flail_run","crouch","attack_crouch","attack_crouch_walk","attack_lean_left",
        "attack_lean_right","cower","freefall","on_turret","corpse_carry_stand","corpse_carry_walk",
        "swim_stand","swim_walk","jeep_drive","jeep_gun","custom"};
    rf_vpp archive;rf_vpp_entry entry;rf_entity_state_set *value=NULL;char *text,authored[64],compiled[64];
    uint32_t identities[23]={0},i,identity;uint8_t flags[23]={0};int status,added;int32_t index;
    rf_model_motion_registry registry={identities,flags,0,23};
    if(!path || !class_name || !*class_name || !weapon || !motions || !result)return RF_RANGE;
    status=rf_vpp_open(&archive,path);if(status)return status;
    status=rf_vpp_find(&archive,"entity.tbl",&entry);if(status)goto done;
    if(!entry.size || (uint64_t)entry.size+sizeof(*value)>budget) {status=RF_RANGE;goto done;}
    value=calloc(1,sizeof(*value)+entry.size);if(!value) {status=RF_IO;goto done;}
    text=(char*)(value+1);status=rf_vpp_read(&archive,&entry,0,text,entry.size);if(status)goto done;
    status=state_group_exists(text,entry.size,class_name,weapon);if(status)goto done;
    for(i=0;i<23;++i) {
        value->states[i]=-1;
        status=rf_entity_state_motion_read(text,entry.size,class_name,weapon,names[i],authored);
        if(status==RF_NOT_FOUND) {status=RF_OK;continue;}
        if(status)goto done;
        if(!*authored)continue;
        status=rf_motion_cache_acquire(value->cache,23,authored,&identity);if(status)goto done;
        status=rf_model_register_motion(&registry,identity+1,1,&index,&added);if(status)goto done;
        if(added) {
            status=rf_motion_compiled_filename((const char*)value->cache[identity].bytes,compiled);if(status)goto done;
            status=rf_motion_file_open(value->files+index,motions,compiled);if(status)goto done;
        }
        value->states[i]=index;
    }
    value->count=registry.count;*result=*value;
done:
    free(value);rf_vpp_close(&archive);return status;
}
int rf_entity_state_motion_read(const void *text,uint32_t bytes,const char *class_name,
    const char *weapon,const char *state,char motion[64])
{
    lexer l={(const unsigned char*)text,bytes,0};char t[256],value[64]={0};
    int quoted,status,selected=0,found=0,group,matched=0;
    if(!text || !class_name || !*class_name || !weapon || !state || !*state || !motion)return RF_RANGE;
    group=!*weapon;
    while((status=token(&l,t,&quoted))==RF_OK) {
        if(quoted)continue;
        if(same(t,"$Name:")) {
            if(found)break;
            status=token(&l,t,&quoted);if(status || !quoted)return RF_FORMAT;
            selected=same(t,class_name);found=selected;
        } else if(selected && same(t,"+Weapon")) {
            status=token(&l,t,&quoted);if(status || quoted || !same(t,"Specific:"))return RF_FORMAT;
            status=token(&l,t,&quoted);if(status || !quoted)return RF_FORMAT;
            group=*weapon && same(t,weapon);
        } else if(selected && same(t,"+State:")) {
            int use;
            status=token(&l,t,&quoted);if(status || !quoted)return RF_FORMAT;
            use=group && same(t,state);
            status=token(&l,t,&quoted);if(status || !quoted)return RF_FORMAT;
            if(use) {
                if(matched)return RF_FORMAT;
                if(*t) {status=asset(value,t);if(status)return status;}
                matched=1;
            }
        }
    }
    if(status!=RF_OK && status!=RF_NOT_FOUND)return status;
    if(!found || !matched)return RF_NOT_FOUND;
    memcpy(motion,value,64);return RF_OK;
}
int rf_entity_state_motion_load(const char *path,const char *class_name,
    const char *weapon,const char *state,char motion[64],uint32_t budget)
{
    rf_vpp archive;rf_vpp_entry entry;void *text=NULL;int status;
    if(!path || !class_name || !weapon || !state || !motion)return RF_RANGE;
    status=rf_vpp_open(&archive,path);if(status)return status;
    status=rf_vpp_find(&archive,"entity.tbl",&entry);
    if(!status && (!entry.size || entry.size>budget))status=RF_RANGE;
    if(!status) {text=malloc(entry.size);if(!text)status=RF_RANGE;}
    if(!status)status=rf_vpp_read(&archive,&entry,0,text,entry.size);
    if(!status)status=rf_entity_state_motion_read(text,entry.size,class_name,weapon,state,motion);
    free(text);rf_vpp_close(&archive);return status;
}
int rf_entity_assets_read(const void *text,uint32_t bytes,const char *class_name,const char *skin,rf_entity_assets *assets)
{
    lexer l={(const unsigned char*)text,bytes,0};rf_entity_assets value={0};char t[256];
    int quoted,status,selected=0,found=0,skin_found;
    if(!text || !class_name || !*class_name || !skin || !assets)return RF_RANGE;
    skin_found=!*skin;
    while((status=token(&l,t,&quoted))==RF_OK) {
        if(quoted)continue;
        if(same(t,"$Name:")) {
            if(found)break;
            status=token(&l,t,&quoted);if(status || !quoted)return RF_FORMAT;
            selected=same(t,class_name);found=selected;
        } else if(selected && same(t,"$V3D")) {
            status=token(&l,t,&quoted);if(status || quoted || !same(t,"Filename:"))return RF_FORMAT;
            status=token(&l,t,&quoted);if(status || !quoted)return RF_FORMAT;
            if(*t) {status=asset(value.model,t);if(status)return status;}else value.model[0]=0;
        } else if(selected && same(t,"$Skin:")) {
            int use;
            status=token(&l,t,&quoted);if(status || !quoted)return RF_FORMAT;use=*skin && same(t,skin);
            status=token(&l,t,&quoted);if(status || quoted || strcmp(t,"("))return RF_FORMAT;
            if(use && skin_found)return RF_FORMAT;
            for(;;) {
                status=token(&l,t,&quoted);if(status)return RF_FORMAT;
                if(!quoted && !strcmp(t,")"))break;
                if(!quoted)return RF_FORMAT;
                if(use) {if(value.texture_count==64)return RF_RANGE;status=asset(value.textures[value.texture_count++],t);if(status)return status;}
            }
            if(use)skin_found=1;
        }
    }
    if(status!=RF_OK && status!=RF_NOT_FOUND)return status;
    if(!found || !skin_found)return RF_NOT_FOUND;
    *assets=value;return RF_OK;
}
