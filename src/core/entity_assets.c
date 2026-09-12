#include "rf/entity_assets.h"
#include "rf/clutter.h"
#include <limits.h>
#include "rf/model.h"
#include "rf/model_file.h"
#include "rf/effect.h"
#include "rf/audio.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
int rf_entity_vitals_config_load(rf_vpp *tables,const char *class_name,uint32_t budget,
    rf_entity_creation_vitals_class *result)
{
    rf_vpp_entry entry;void *text;int status;
    if(!tables || !class_name || !*class_name || !result)return RF_RANGE;
    status=rf_vpp_find(tables,"entity.tbl",&entry);if(status)return status;
    if(!entry.size || entry.size>budget)return RF_RANGE;
    text=malloc(entry.size);if(!text)return RF_RANGE;
    status=rf_vpp_read(tables,&entry,0,text,entry.size);
    if(!status)status=rf_entity_vitals_config_read(text,entry.size,class_name,result);
    free(text);return status;
}
int rf_entity_physics_config_load(rf_vpp *tables,const char *class_name,
    uint32_t scratch_budget,rf_entity_physics_config *result)
{
    rf_vpp_entry entity,material;rf_entity_physics_config value={0};
    uint32_t size;void *scratch;int status;
    if(!tables || !class_name || !*class_name || !result)return RF_RANGE;
    status=rf_vpp_find(tables,"entity.tbl",&entity);if(status)return status;
    status=rf_vpp_find(tables,"materials.tbl",&material);if(status)return status;
    size=entity.size>material.size?entity.size:material.size;
    if(!entity.size || !material.size || size>scratch_budget)return RF_RANGE;
    scratch=malloc(size);if(!scratch)return RF_RANGE;
    status=rf_vpp_read(tables,&entity,0,scratch,entity.size);
    if(!status)status=rf_entity_class_physics_read(scratch,entity.size,class_name,&value.authored);
    if(!status)status=rf_entity_sphere_declarations_read(scratch,entity.size,class_name,&value.spheres);
    if(!status)status=rf_vpp_read(tables,&material,0,scratch,material.size);
    if(!status)status=rf_entity_material_read(scratch,material.size,value.authored.material,&value.material);
    free(scratch);if(!status)*result=value;return status;
}
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
int rf_entity_class_spheres_build(const rf_model_file *model,const float (*matrices)[12],uint32_t bone_count,
    const rf_entity_physics_config *config,rf_physics_sphere spheres[8],uint32_t *sphere_count)
{
    rf_entity_class_sphere resolved[8]={0};uint32_t n=0,j;int status;
    if(!model || !config || !spheres || !sphere_count || (bone_count && !matrices))return RF_RANGE;
    for(j=0;j<8;++j) {
        rf_model_collision_sphere sphere;float posed[4];
        status=rf_model_file_collision_sphere(model,j,&sphere);
        if(status==RF_NOT_FOUND)break;if(status)return status;
        status=rf_model_collision_sphere_pose(&sphere,matrices,bone_count,posed);if(status)return status;
        if(strlen(sphere.name)>=24)return RF_RANGE;
        strcpy(resolved[j].name,sphere.name);memcpy(resolved[j].center,posed,12);
        if(config->authored.flags&0x24000)resolved[j].center[0]=resolved[j].center[2]=0;
        resolved[j].radius=posed[3];resolved[j].selected_scalar=1;resolved[j].parameter_10=-1;resolved[j].model_index=j;++n;
    }
    status=rf_entity_sphere_overrides(resolved,n,config->spheres.items,config->spheres.count,0);if(status)return status;
    for(j=0;j<n;++j) {
        memcpy(spheres[j].center,resolved[j].center,12);spheres[j].radius=resolved[j].radius;
        spheres[j].parameter_10=resolved[j].parameter_10;spheres[j].opaque_14=resolved[j].opaque_14;
    }
    *sphere_count=n;return RF_OK;
}
int rf_entity_class_stance_build(const rf_model_file *model,const rf_model_bone *bones,uint32_t bone_count,
    const rf_motion_playback_state *initial,const rf_motion_file *const *handles,
    const rf_motion_playback_resource *resources,uint32_t resource_count,int32_t crouch,
    const rf_entity_physics_config *config,const rf_physics_sphere *standing,uint32_t sphere_count,rf_physics_stance_cache *result,
    const rf_model_attachment *eye,const float eye_transform[12],float *eye_offsets,uint32_t scratch_budget)
{
    rf_motion_playback_state state;rf_motion_playback_resource *copied;
    uint64_t scratch=(uint64_t)resource_count*sizeof(*copied)+(uint64_t)bone_count*48;
    rf_physics_stance_cache value={0};uint16_t generations[256]={0};float displacement[3]={0};
    float (*matrices)[12],offsets[6],crouch_eye[12];uint32_t i;int status;
    if(!model || !initial || !config || !result || !bones || !bone_count || bone_count>256 ||
       sphere_count>8 || (sphere_count && !standing) || (resource_count && (!resources || !handles)) ||
       scratch>scratch_budget || (eye_offsets && (!eye || !eye_transform || (uint32_t)eye->parent>=bone_count)))return RF_RANGE;
    state=*initial;
    copied=resource_count?malloc(resource_count*sizeof(*copied)):NULL;
    if(resource_count && !copied)return RF_IO;
    if(resource_count)memcpy(copied,resources,resource_count*sizeof(*copied));
    matrices=malloc(bone_count*48);if(!matrices){free(copied);return RF_IO;}
    status=RF_OK;
    if(eye_offsets) {
        /* Class offsets use identity/zero placement, not the diagnostic's
         * root displacement. Re-evaluate the initial pose in this workspace. */
        for(i=0;i<bone_count;++i)generations[i]=(uint16_t)(state.generation-1);
        status=rf_model_evaluate_playback(bones,bone_count,&state,handles,copied,resource_count,displacement,matrices,generations,bone_count);
        if(!status)status=rf_model_compose_transform(eye_transform,matrices[eye->parent],crouch_eye);
        if(!status)memcpy(offsets,crouch_eye+9,12);
    }
    if(!status)status=rf_motion_stop_looping(&state,copied,resource_count);
    if(!status)status=rf_motion_set_weight(&state,copied,resource_count,crouch,1);
    if(!status)status=rf_motion_update(&state,copied,resource_count,.2f);
    for(i=0;i<bone_count;++i)generations[i]=(uint16_t)(state.generation-1);
    if(!status)status=rf_model_evaluate_playback(bones,bone_count,&state,handles,copied,resource_count,displacement,matrices,generations,bone_count);
    if(!status && eye_offsets) {
        status=rf_model_compose_transform(eye_transform,matrices[eye->parent],crouch_eye);
        if(!status) {
            memcpy(offsets+3,crouch_eye+9,12);
            /* 423bd0 / 40a150: class flag 20000 keeps only eye height. */
            if(config->authored.flags&0x20000u)offsets[0]=offsets[2]=offsets[3]=offsets[5]=0;
        }
    }
    value.count=sphere_count;
    for(i=0;!status && i<value.count;++i) {
        rf_model_collision_sphere sphere;float posed[4],difference;
        status=rf_model_file_collision_sphere(model,i,&sphere);if(status)break;
        status=rf_model_collision_sphere_pose(&sphere,matrices,bone_count,posed);if(status)break;
        memcpy(value.centers[0][i],standing[i].center,12);
        memcpy(value.centers[1][i],posed,12);
        if(config->authored.flags&0x24000)value.centers[1][i][0]=value.centers[1][i][2]=0;
        difference=(float)((double)value.centers[0][i][1]-value.centers[1][i][1]);
        value.height_difference=fmaxf(value.height_difference,difference);
    }
    free(matrices);free(copied);if(!status) {*result=value;if(eye_offsets)memcpy(eye_offsets,offsets,sizeof(offsets));}return status;
}
int rf_entity_body_open(const rf_entity_physics_config *config,const rf_physics_sphere *spheres,
    uint32_t count,const float position[3],const float orientation[9],uint32_t creation_flags,
    uint32_t budget,rf_physics_body *result)
{
    rf_physics_body body={0};rf_physics_body_parameters parameters={0};int status;
    if(!config || !position || !orientation || !result || count>8 || (count && !spheres) ||
       result->allocated_bytes || result->spheres.items || result->spheres.count || result->spheres.allocated_bytes)return RF_RANGE;
    if(!(config->authored.mass>0))return RF_FORMAT; /* Generated mass remains separate. */
    parameters.mass=config->authored.mass;parameters.coefficients[0]=config->material.elasticity;
    parameters.coefficients[1]=10;parameters.coefficients[2]=config->material.friction;
    memcpy(parameters.position,position,12);memcpy(parameters.orientation,orientation,36);
    /* 42256b zero tensor;49ec90's empty-sphere fallback installs identity. */
    if(count==0)parameters.local_tensor[0]=parameters.local_tensor[4]=parameters.local_tensor[8]=1;
    parameters.flags=rf_entity_creation_physics_flags(creation_flags,config->authored.flags,
        config->authored.flags2,config->authored.use_kind,0);
    status=rf_physics_body_open(&parameters,NULL,0,budget,&body);
    if(!status)status=rf_physics_body_replace_spheres(&body,spheres,count,budget);
    if(status){rf_physics_body_close(&body);return status;}
    *result=body;return RF_OK;
}
typedef struct lexer {const unsigned char *text;uint32_t size,at;} lexer;
static int same(const char *a,const char *b)
{
    while(*a && *b) {unsigned x=(unsigned char)*a++,y=(unsigned char)*b++;
        if(x>='A' && x<='Z')x+=32;if(y>='A' && y<='Z')y+=32;if(x!=y)return 0;}
    return *a==*b;
}
/* Writes only caller-local working records; public wrappers commit together. */
static int skeletal_assets_load(const char *tables_path,const char *class_name,const char *skin,
    rf_vpp *meshes,uint32_t table_budget,rf_entity_assets *assets,rf_vpp_entry *mesh)
{
    char compiled[64];const char *extension;int status;
    status=rf_entity_assets_load(tables_path,class_name,skin,assets,table_budget);
    if(status)return status;
    extension=strrchr(assets->model,'.');
    if(!extension || !same(extension,".vcm"))return RF_FORMAT;
    status=rf_entity_skeletal_filename(assets->model,compiled);if(status)return status;
    return rf_vpp_find(meshes,compiled,mesh);
}
int rf_entity_skeletal_assets_load(const char *tables_path,const char *class_name,const char *skin,
    rf_vpp *meshes,uint32_t table_budget,rf_entity_skeletal_assets *result)
{
    rf_entity_skeletal_assets value;int status;
    if(!tables_path || !class_name || !skin || !meshes || !result)return RF_RANGE;
    status=skeletal_assets_load(tables_path,class_name,skin,meshes,table_budget,&value.assets,&value.mesh);
    if(status)return status;
    *result=value;return RF_OK;
}
int rf_level_actor_assets_load(const rf_level *level,int32_t uid,const char *tables_path,
    rf_vpp *meshes,uint32_t table_budget,rf_level_actor_assets *result)
{
    rf_level_actor_assets value;int status;
    if(!level || !tables_path || !meshes || !result)return RF_RANGE;
    status=rf_level_entity_find(level,uid,&value.entity);if(status)return status;
    status=skeletal_assets_load(tables_path,value.entity.class_name,value.entity.skin,
        meshes,table_budget,&value.assets,&value.mesh);if(status)return status;
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
int rf_movement_descriptor_load(rf_vpp *tables,uint32_t index,uint32_t budget,rf_movement_descriptor *result)
{
    static const char *names[]={"none","run","climb","fall","swim","apc","apc fall","sub","sub fall","fighter","turret","robot fly","hover","freelookcam","deadcam","john's descent flying mode"};
    static const char *refs[2][4]={{"none","eye","body","parent"},{"none","eye-obj","body-obj","body-world"}};
    rf_vpp_entry entry;rf_movement_descriptor value={0};lexer l;char t[256];void *text;
    uint32_t mask=0,axis,kind,i;int status,quoted,found=0;
    if(!tables || !result || index>=16)return RF_RANGE;
    status=rf_vpp_find(tables,"movemodes.tbl",&entry);if(status)return status;
    if(!entry.size || entry.size>budget)return RF_RANGE;
    text=malloc(entry.size);if(!text)return RF_RANGE;
    status=rf_vpp_read(tables,&entry,0,text,entry.size);if(status)goto done;
    l.text=text;l.size=entry.size;l.at=0;
    while((status=token(&l,t,&quoted))==RF_OK) {
        if(quoted)continue;
        if(same(t,"$name:")) {
            if(found)break;
            if(token(&l,t,&quoted) || !quoted) {status=RF_FORMAT;goto done;}
            found=same(t,names[index]);
        } else if(found && (same(t,"$move") || same(t,"$rot"))) {
            kind=same(t,"$rot");
            if(token(&l,t,&quoted) || quoted) {status=RF_FORMAT;goto done;}
            axis=same(t,"x")?0:same(t,"y")?1:same(t,"z")?2:3;
            if(axis==3 || (mask&(1u<<(kind*3+axis))) || token(&l,t,&quoted) || quoted || !same(t,"ref:") ||
               token(&l,t,&quoted) || !quoted) {status=RF_FORMAT;goto done;}
            for(i=0;i<4;++i)if(same(t,refs[kind][i]))break;
            (kind?value.rotation:value.translation)[axis]=i==4?0:i;
            mask|=1u<<(kind*3+axis);
        }
    }
    if(status==RF_NOT_FOUND || status==RF_OK) {
        status=!found?RF_NOT_FOUND:mask!=63?RF_FORMAT:RF_OK;
        if(!status) {value.enabled=1;value.index=index;*result=value;}
    }
done:
    free(text);return status;
}
static int asset(char destination[64],const char *source)
{size_t n=strlen(source);if(!n || n>=64)return RF_RANGE;memcpy(destination,source,n+1);return RF_OK;}
int32_t rf_weapon_name_find(const rf_weapon_names *table,const char *name)
{
    uint32_t i;if(!name)name="";
    for(i=0;i<table->count;++i)if(same(table->names[i],name))return (int32_t)i;
    return -1;
}
int rf_weapon_names_read(const void *text,uint32_t bytes,rf_weapon_names *result)
{
    rf_weapon_names v={0};lexer l={(const unsigned char*)text,bytes,0};char t[256];int q,status,section=0;
    if(!text || !result)return RF_RANGE;
    while((status=token(&l,t,&q))==RF_OK) {
        if(q)continue;
        if(same(t,"#Primary") || same(t,"#Secondary")) {
            int next=same(t,"#Primary")?1:3;
            if((next==1 && section!=0) || (next==3 && section!=2))return RF_FORMAT;
            if(token(&l,t,&q) || q || !same(t,"Weapons"))return RF_FORMAT;
            section=next;
        } else if(same(t,"#End")) {
            if(section!=1 && section!=3)return RF_FORMAT;
            if(section==1)v.primary_count=v.count;
            ++section;
        } else if(same(t,"$Name:")) {
            if(section!=1 && section!=3)return RF_FORMAT;
            if(v.count==64)return RF_RANGE;
            if(token(&l,t,&q) || !q)return RF_FORMAT;
            if(*t){status=asset(v.names[v.count],t);if(status)return status;}
            ++v.count;
        }
    }
    if(status!=RF_NOT_FOUND)return status;
    if(section!=4)return RF_FORMAT;
    *result=v;return RF_OK;
}
int rf_weapon_names_load(rf_vpp *tables,uint32_t budget,rf_weapon_names *result)
{
    rf_vpp_entry entry;void *text;int status;
    if(!tables || !result)return RF_RANGE;
    status=rf_vpp_find(tables,"weapons.tbl",&entry);if(status)return status;
    if(!entry.size || entry.size>budget)return RF_RANGE;
    text=malloc(entry.size);if(!text)return RF_RANGE;
    status=rf_vpp_read(tables,&entry,0,text,entry.size);
    if(!status)status=rf_weapon_names_read(text,entry.size,result);
    free(text);return status;
}
int rf_entity_weapon_groups_read(const void *text,uint32_t bytes,const char *class_name,
    const rf_weapon_names *weapons,uint32_t groups[2])
{
    lexer l={(const unsigned char*)text,bytes,0};char t[256];uint32_t masks[2]={0};int status,q,selected=0,found=0;
    if(!text || !class_name || !*class_name || !weapons || weapons->count>64 || !groups)return RF_RANGE;
    while((status=token(&l,t,&q))==RF_OK) {
        if(q)continue;
        if(same(t,"$Name:")) {
            if(found)break;
            if(token(&l,t,&q) || !q)return RF_FORMAT;
            selected=same(t,class_name);found=selected;
        } else if(selected && same(t,"+Weapon")) {
            int32_t id;uint32_t bit;
            if(token(&l,t,&q) || q || !same(t,"Specific:"))return RF_FORMAT;
            if(token(&l,t,&q) || !q)return RF_FORMAT;
            id=rf_weapon_name_find(weapons,t);if(id<0)return RF_NOT_FOUND;
            bit=1u<<((uint32_t)id&31u);if(masks[(uint32_t)id>>5]&bit)return RF_FORMAT;
            masks[(uint32_t)id>>5]|=bit;
        }
    }
    if(status!=RF_NOT_FOUND && status!=RF_OK)return status;
    if(!found)return RF_NOT_FOUND;
    memcpy(groups,masks,sizeof(masks));return RF_OK;
}
int rf_entity_default_weapons_read(const void *text,uint32_t bytes,const char *class_name,
    const rf_weapon_names *weapons,rf_entity_default_weapons *result)
{
    lexer l={(const unsigned char*)text,bytes,0};char t[256];
    rf_entity_default_weapons v={-1,-1};uint32_t mask=0;int status,q,selected=0,found=0;
    if(!text || !class_name || !*class_name || !weapons || weapons->count>64 || !result)return RF_RANGE;
    while((status=token(&l,t,&q))==RF_OK) {
        if(q)continue;
        if(same(t,"$Name:")) {
            if(found)break;
            if(token(&l,t,&q) || !q)return RF_FORMAT;
            selected=same(t,class_name);found=selected;
        } else if(selected && same(t,"$Default")) {
            uint32_t bit;int32_t id;
            if(token(&l,t,&q) || q)return RF_FORMAT;
            bit=same(t,"Primary:")?1:same(t,"Secondary:")?2:0;if(!bit)continue;
            if(mask&bit || token(&l,t,&q) || !q)return RF_FORMAT;
            id=*t?rf_weapon_name_find(weapons,t):-1;
            if(bit==1)v.primary=id;else v.secondary=id;mask|=bit;
        }
    }
    if(status!=RF_NOT_FOUND && status!=RF_OK)return status;
    if(!found)return RF_NOT_FOUND;if(mask!=3)return RF_FORMAT;
    *result=v;return RF_OK;
}
static int sphere_number(lexer *l,float *result)
{
    char t[256];uint32_t at=0,digits=0;int quoted,status,negative=0,fraction=0,exponent=0,exp_negative=0;double value=0;
    status=token(l,t,&quoted);if(status || quoted)return RF_FORMAT;
    /* NXDK strtod/strtof are assertion stubs. Authored decimal syntax only. */
    if(t[at]=='+' || t[at]=='-')negative=t[at++]=='-';
    while(t[at]>='0' && t[at]<='9') {value=value*10+(t[at++]-'0');++digits;}
    if(t[at]=='.') {
        ++at;while(t[at]>='0' && t[at]<='9') {value=value*10+(t[at++]-'0');++digits;++fraction;}
    }
    if(!digits)return RF_FORMAT;
    if(t[at]=='e' || t[at]=='E') {
        ++at;if(t[at]=='+' || t[at]=='-')exp_negative=t[at++]=='-';
        digits=0;while(t[at]>='0' && t[at]<='9') {if(exponent<10000)exponent=exponent*10+t[at]-'0';++at;++digits;}
        if(!digits)return RF_FORMAT;
    }
    if(t[at])return RF_FORMAT;
    exponent=(exp_negative?-exponent:exponent)-fraction;
    if(value!=0) {
        if(exponent>308)return RF_FORMAT;
        if(exponent< -600)value=0;
        else {
            while(exponent>0) {value*=10;--exponent;}
            while(exponent<0) {value/=10;++exponent;}
        }
    }
    if(!isfinite(value) || value>FLT_MAX)return RF_FORMAT;
    *result=(float)(negative?-value:value);return RF_OK;
}
static int metadata_tag(lexer *l,const char *tag)
{
    lexer probe=*l;char expected[64],actual[256];uint32_t n;int quoted;
    while(*tag) {
        n=0;while(*tag && *tag!=' ')expected[n++]=*tag++;
        expected[n]=0;while(*tag==' ')++tag;
        if(token(&probe,actual,&quoted) || quoted || !same(actual,expected))return 0;
    }
    *l=probe;return 1;
}
static int metadata_string(lexer *l,char *destination,uint32_t capacity)
{
    char value[256];int quoted;size_t length;
    if(token(l,value,&quoted) || !quoted)return RF_FORMAT;
    length=strlen(value);if(length>=capacity)return RF_FORMAT;
    if(destination)memcpy(destination,value,length+1);
    return RF_OK;
}
static int metadata_integer(lexer *l,uint32_t *result)
{
    char value[256];uint32_t at=0,base=10,digit,digits=0;uint64_t number=0;int quoted,negative=0;
    if(token(l,value,&quoted) || quoted)return RF_FORMAT;
    if(value[at]=='-' || value[at]=='+')negative=value[at++]=='-';
    if(value[at]=='0' && (value[at+1]=='x' || value[at+1]=='X')) {at+=2;base=16;}
    for(;value[at];++at) {
        char c=value[at];digit=c>='0' && c<='9'?(uint32_t)(c-'0'):
            c>='a' && c<='f'?(uint32_t)(c-'a'+10):c>='A' && c<='F'?(uint32_t)(c-'A'+10):16;
        if(digit>=base)return RF_FORMAT;
        number=number*base+digit;if(number>(negative?2147483648ull:4294967295ull))return RF_FORMAT;++digits;
    }
    if(!digits)return RF_FORMAT;*result=negative?0u-(uint32_t)number:(uint32_t)number;return RF_OK;
}
static int metadata_pass(const void *text,uint32_t bytes,rf_sound_metadata *rows,uint32_t *count)
{
    lexer l={text,bytes,0};char trailing[256];int quoted;uint32_t n=0,value;
    if(!metadata_tag(&l,"$Sound Root:") || metadata_string(&l,NULL,219) ||
       !metadata_tag(&l,"$PS2 Sound Root:") || metadata_string(&l,NULL,219))return RF_FORMAT;
    metadata_tag(&l,"+Use Flat Directory Layout for PS2 Sounds");
    while(metadata_tag(&l,"$Folder:"))if(metadata_string(&l,NULL,256))return RF_FORMAT;
    while(metadata_tag(&l,"$Sound:")) {
        rf_sound_metadata row={0};
        if(n==RF_SOUND_METADATA_CAPACITY || metadata_string(&l,row.name,80) ||
           !metadata_tag(&l,"$Folder:") || metadata_string(&l,NULL,40))return RF_FORMAT;
        if(metadata_tag(&l,"+Time:") && metadata_integer(&l,&value))return RF_FORMAT;
        if(metadata_tag(&l,"+Music Track"))row.keyoff_flags|=0x20000000u;
        else {
            if(metadata_tag(&l,"+Ambient Sound"))row.loop_flags|=0x80000000u;
            if(!metadata_tag(&l,"$Envelope:") || metadata_integer(&l,&value) ||
               !metadata_tag(&l,"$Keyoff Time:") || metadata_integer(&l,&value))return RF_FORMAT;
            row.keyoff_flags|=value&0x0fffffffu;
        }
        if(metadata_tag(&l,"+Looping Sound")) {
            if(!metadata_tag(&l,"+Loop Start:") || metadata_integer(&l,&value))return RF_FORMAT;
            row.loop_flags|=0x40000000u|(value&0x07ffffffu);
        }
        if(metadata_tag(&l,"+Preload"))row.keyoff_flags|=0x80000000u;
        metadata_tag(&l,"+Incidental");
        if(metadata_tag(&l,"+Preserve Low Frequencies"))row.loop_flags|=0x10000000u;
        else if(metadata_tag(&l,"+Preserve Medium Frequencies"))row.loop_flags|=0x08000000u;
        row.keyoff_flags|=0x10000000u;
        if(rows)rows[n]=row;++n;
    }
    if(token(&l,trailing,&quoted)!=RF_NOT_FOUND)return RF_FORMAT;
    *count=n;return RF_OK;
}
int rf_sound_metadata_read(const void *text,uint32_t bytes,rf_sound_metadata *rows,uint32_t capacity,uint32_t *count)
{
    uint32_t i,n;int status;
    if(!text || !bytes || !count || (!rows && capacity))return RF_RANGE;
    for(i=0;i<bytes;++i)if(!((const unsigned char *)text)[i] || ((const unsigned char *)text)[i]>127)return RF_FORMAT;
    status=metadata_pass(text,bytes,NULL,&n);if(status)return status;
    if(rows && n>capacity)return RF_RANGE;
    if(rows) {status=metadata_pass(text,bytes,rows,&n);if(status)return status;}
    *count=n;return RF_OK;
}
int rf_sound_metadata_open(const void *text,uint32_t bytes,uint32_t budget,rf_sound_metadata_owner *result)
{
    rf_sound_metadata_owner value={0};uint32_t size;int status;
    if(!result || result->rows || result->order || result->count || result->allocated_bytes)return RF_RANGE;
    status=rf_sound_metadata_read(text,bytes,NULL,0,&value.count);if(status)return status;
    size=value.count*sizeof(*value.rows)+RF_SOUND_METADATA_CAPACITY*sizeof(*value.order);
    if(size+sizeof(value)>budget)return RF_RANGE;
    value.rows=malloc(size);if(!value.rows)return RF_RANGE;
    value.order=(uint16_t *)(value.rows+value.count);value.allocated_bytes=size+sizeof(value);
    status=rf_sound_metadata_read(text,bytes,value.rows,value.count,&value.count);
    if(!status)status=rf_sound_metadata_order(value.rows,value.count,value.order);
    if(status) {free(value.rows);return status;}
    *result=value;return RF_OK;
}
void rf_sound_metadata_close(rf_sound_metadata_owner *owner)
{ if(owner) {free(owner->rows);memset(owner,0,sizeof(*owner));} }
static int sound_table_pass(const void *text,uint32_t bytes,rf_audio_declaration *rows,uint32_t *count)
{
    lexer l={text,bytes,0};char t[256];int quoted;uint32_t n=0;
    if(token(&l,t,&quoted) || quoted || !same(t,"#Sounds") ||
       token(&l,t,&quoted) || quoted || !same(t,"Start"))return RF_FORMAT;
    for(;;) {
        rf_audio_declaration row={0};size_t length;
        if(token(&l,t,&quoted))return RF_FORMAT;
        if(!quoted && same(t,"#Sounds")) {
            if(token(&l,t,&quoted) || quoted || !same(t,"End") || token(&l,t,&quoted)!=RF_NOT_FOUND)return RF_FORMAT;
            *count=n;return RF_OK;
        }
        length=strlen(t);if(!quoted || !length || length>60 || n==2048)return RF_FORMAT;
        memcpy(row.name,t,length+1);
        if(sphere_number(&l,&row.near_distance) || sphere_number(&l,&row.volume) ||
           sphere_number(&l,&row.rolloff) || row.volume<0 || row.rolloff<=0)return RF_FORMAT;
        if(rows)rows[n]=row;++n;
    }
}
int rf_sound_table_read(const void *text,uint32_t bytes,rf_audio_declaration *rows,uint32_t capacity,uint32_t *count)
{
    uint32_t n;int status;
    if(!text || !bytes || !count || (!rows && capacity))return RF_RANGE;
    status=sound_table_pass(text,bytes,NULL,&n);if(status)return status;
    if(rows && n>capacity)return RF_RANGE;
    if(rows){status=sound_table_pass(text,bytes,rows,&n);if(status)return status;}
    *count=n;return RF_OK;
}
int rf_sound_table_load(rf_vpp *tables,uint32_t scratch_budget,
    rf_audio_declaration *rows,uint32_t capacity,uint32_t *count)
{
    rf_vpp_entry entry;void *text;int status;
    if(!tables || !count || (!rows && capacity))return RF_RANGE;
    status=rf_vpp_find(tables,"sounds.tbl",&entry);if(status)return status;
    if(!entry.size || entry.size>scratch_budget)return RF_RANGE;
    text=malloc(entry.size);if(!text)return RF_RANGE;
    status=rf_vpp_read(tables,&entry,0,text,entry.size);
    if(!status)status=rf_sound_table_read(text,entry.size,rows,capacity,count);
    free(text);return status;
}
/* Unlike token(), original forward search skips comments only at its initial
 * cursor, then performs a literal substring search across remaining bytes. */
static int foley_skip(lexer *l)
{
    for(;;) {
        while(l->at<l->size && (l->text[l->at]==' ' ||
              (l->text[l->at]>=9 && l->text[l->at]<=13)))++l->at;
        if(l->at+1>=l->size || l->text[l->at]!='/')return RF_OK;
        if(l->text[l->at+1]=='/') {
            l->at+=2;while(l->at<l->size && l->text[l->at]!='\r')++l->at;
        } else if(l->text[l->at+1]=='*') {
            l->at+=2;
            while(l->at+1<l->size && !(l->text[l->at]=='*' && l->text[l->at+1]=='/'))++l->at;
            if(l->at+1>=l->size)return RF_FORMAT;l->at+=2;
        } else return RF_OK;
    }
}
static int foley_search(lexer *l,const char *word)
{
    uint32_t at,n=(uint32_t)strlen(word);int status=foley_skip(l);if(status)return status;
    for(at=l->at;at<=l->size && n<=l->size-at;++at)
        if(!memcmp(l->text+at,word,n)) {l->at=at;return RF_OK;}
    return RF_NOT_FOUND;
}
static int foley_pass(const void *text,uint32_t bytes,rf_foley_group *groups,
    rf_audio_declaration *samples,uint32_t *group_count,uint32_t *sample_count)
{
    static const char *materials[]={"default","rock","metal","flesh","water","laval",
        "solid","sand","ice","glass","ladder","chain fence"};
    lexer l={text,bytes,0};uint32_t ng=0,ns=0,i,j;int status;
    if(!metadata_tag(&l,"#Entity Sounds"))return RF_FORMAT;
    for(;;) {
        rf_foley_group group={0};group.count=1;group.first=ns;
        status=foley_search(&l,"$Name:");
        if(status==RF_NOT_FOUND) {
            if(foley_search(&l,"#End"))return RF_FORMAT;
            *group_count=ng;*sample_count=ns;return RF_OK;
        }
        if(status || ng==640 || !metadata_tag(&l,"$Name:") ||
           metadata_string(&l,group.name,sizeof(group.name)))return RF_FORMAT;
        if(metadata_tag(&l,"$Sounds:") && metadata_integer(&l,&group.count))return RF_FORMAT;
        if(group.count>4096-ns)return RF_RANGE;
        if(metadata_tag(&l,"$Material:")) {
            char material[32];if(metadata_string(&l,material,sizeof(material)))return RF_FORMAT;
            for(j=0;j<12;++j)if(same(material,materials[j]))break;
            if(j==12)return RF_FORMAT;group.material=j==10?1:j==11?2:j;
        }
        for(i=0;i<group.count;++i) {
            rf_audio_declaration sample={0};sample.rolloff=1;
            if(!metadata_tag(&l,"$Sound:") || metadata_string(&l,sample.name,sizeof(sample.name)))return RF_FORMAT;
            if(sample.name[0] && (sphere_number(&l,&sample.near_distance) ||
               sphere_number(&l,&sample.volume) || sample.volume<0))return RF_FORMAT;
            if(samples)samples[ns]=sample;++ns;
        }
        if(groups)groups[ng]=group;++ng;
    }
}
int rf_foley_table_read(const void *text,uint32_t bytes,
    rf_foley_group *groups,uint32_t group_capacity,
    rf_audio_declaration *samples,uint32_t sample_capacity,
    uint32_t *group_count,uint32_t *sample_count)
{
    uint32_t ng,ns,i;int status;
    if(!text || !bytes || !group_count || !sample_count ||
       (!groups && group_capacity) || (!samples && sample_capacity))return RF_RANGE;
    for(i=0;i<bytes;++i)if(!((const unsigned char *)text)[i])return RF_FORMAT;
    status=foley_pass(text,bytes,NULL,NULL,&ng,&ns);if(status)return status;
    if((groups && ng>group_capacity) || (samples && ns>sample_capacity))return RF_RANGE;
    if(groups || samples) {status=foley_pass(text,bytes,groups,samples,&ng,&ns);if(status)return status;}
    *group_count=ng;*sample_count=ns;return RF_OK;
}
int rf_foley_open(const void *text,uint32_t bytes,uint32_t budget,
    rf_ambient_register registration,void *context,rf_foley_owner *result)
{
    rf_foley_owner value={0};rf_audio_declaration *declarations=NULL;
    uint32_t storage,scratch,i;int status;
    if(!registration || !result || result->groups || result->samples ||
       result->group_count || result->sample_count || result->resident_bytes || result->peak_bytes)return RF_RANGE;
    status=rf_foley_table_read(text,bytes,NULL,0,NULL,0,&value.group_count,&value.sample_count);
    if(status)return status;
    storage=value.group_count*sizeof(*value.groups)+value.sample_count*sizeof(*value.samples);
    scratch=value.sample_count*sizeof(*declarations);
    value.resident_bytes=sizeof(value)+storage;value.peak_bytes=value.resident_bytes+scratch;
    if(value.peak_bytes>budget)return RF_RANGE;
    if(storage) {
        value.groups=malloc(storage);if(!value.groups)return RF_RANGE;
        value.samples=(int32_t *)(value.groups+value.group_count);
    }
    if(scratch) {declarations=malloc(scratch);if(!declarations){free(value.groups);return RF_RANGE;}}
    status=rf_foley_table_read(text,bytes,value.groups,value.group_count,declarations,
        value.sample_count,&value.group_count,&value.sample_count);
    if(status) {free(declarations);free(value.groups);return status;}
    for(i=0;i<value.sample_count;++i) {
        const rf_audio_declaration *sample=declarations+i;
        value.samples[i]=sample->name[0]?registration(context,sample->name,
            sample->near_distance,sample->volume,sample->rolloff):-1;
    }
    free(declarations);*result=value;return RF_OK;
}
void rf_foley_close(rf_foley_owner *owner)
{if(owner){free(owner->groups);memset(owner,0,sizeof(*owner));}}
int rf_foley_find(const rf_foley_owner *owner,const char *name,int32_t *group)
{
    uint32_t i;int32_t value=-1;
    if(!owner || !name || !group || owner->group_count>640 || (owner->group_count && !owner->groups))return RF_RANGE;
    for(i=0;i<owner->group_count;++i)if(!memchr(owner->groups[i].name,0,32))return RF_FORMAT;
    if(*name)for(i=0;i<owner->group_count;++i)if(same(owner->groups[i].name,name)){value=(int32_t)i;break;}
    *group=value;return RF_OK;
}
static int entity_damage_sound_groups_read(const void *text,uint32_t bytes,const char *class_name,
    const rf_foley_owner *owner,int32_t *groups,uint32_t count)
{
    lexer l={text,bytes,0};char t[256],name[64];int status,quoted,selected=0;
    uint32_t seen=0,index;int32_t value[3]={-1,-1,-1};
    if(!text || !bytes || !class_name || !*class_name || !groups)return RF_RANGE;
    status=rf_foley_find(owner,"",&value[0]);if(status)return status;
    for(;;) {
        status=token(&l,t,&quoted);if(status==RF_NOT_FOUND)break;if(status)return status;
        if(quoted)continue;
        if(same(t,"$Name:")) {
            if(selected)break;
            if(token(&l,t,&quoted) || !quoted)return RF_FORMAT;
            selected=same(t,class_name);
        } else if(same(t,"#End"))break;
        else if(selected && (same(t,"$Low_Pain") || same(t,"$Med_Pain") || (count==3 && same(t,"$DeathSnd:")))) {
            index=same(t,"$Low_Pain")?0:same(t,"$Med_Pain")?1:2;
            if(seen&(1u<<index))return RF_FORMAT;seen|=1u<<index;
            if(index<2 && (token(&l,t,&quoted) || quoted || !same(t,"Sounds:")))return RF_FORMAT;
            if(metadata_string(&l,name,sizeof(name)))return RF_FORMAT;
            status=rf_foley_find(owner,name,value+index);if(status)return status;
        }
    }
    if(!selected)return RF_NOT_FOUND;memcpy(groups,value,count*sizeof(*value));return RF_OK;
}
int rf_entity_impact_sound_group_read(const void *text,uint32_t bytes,const char *class_name,
    const rf_foley_owner *owner,int32_t *group)
{
    lexer l={text,bytes,0};char t[256],name[64];int status,quoted,selected=0,seen=0;int32_t value;
    if(!text || !bytes || !class_name || !*class_name || !group)return RF_RANGE;
    value=*group;
    for(;;) {
        status=token(&l,t,&quoted);if(status==RF_NOT_FOUND)break;if(status)return status;
        if(quoted)continue;
        if(same(t,"$Name:")) {
            if(selected)break;
            if(token(&l,t,&quoted) || !quoted)return RF_FORMAT;selected=same(t,class_name);
        } else if(same(t,"#End"))break;
        else if(selected && same(t,"$Impact")) {
            if(seen++)return RF_FORMAT;
            if(token(&l,t,&quoted) || quoted || !same(t,"Death") || token(&l,t,&quoted) || quoted || !same(t,"Sound:"))return RF_FORMAT;
            if(metadata_string(&l,name,sizeof(name)))return RF_FORMAT;
            status=rf_foley_find(owner,name,&value);if(status)return status;
        }
    }
    if(!selected)return RF_NOT_FOUND;*group=value;return RF_OK;
}
int rf_entity_pain_groups_read(const void *text,uint32_t bytes,const char *class_name,
    const rf_foley_owner *owner,int32_t groups[2])
{return entity_damage_sound_groups_read(text,bytes,class_name,owner,groups,2);}
int rf_entity_damage_sound_groups_read(const void *text,uint32_t bytes,const char *class_name,
    const rf_foley_owner *owner,int32_t groups[3])
{return entity_damage_sound_groups_read(text,bytes,class_name,owner,groups,3);}
int rf_foley_bind_materials(const rf_foley_owner *owner,const char (*names)[32],
    uint32_t count,int32_t slots[10])
{
    int32_t value[10];uint32_t i,j;
    if(!owner || !slots || (count && !names) || owner->group_count>640 ||
       (owner->group_count && !owner->groups))return RF_RANGE;
    for(i=0;i<owner->group_count;++i)
        if(!memchr(owner->groups[i].name,0,32) || owner->groups[i].material>=10)return RF_FORMAT;
    for(i=0;i<10;++i)value[i]=-1;
    for(i=0;i<count;++i) {
        if(!memchr(names[i],0,32))return RF_FORMAT;
        if(!names[i][0])return RF_NOT_FOUND;
        for(j=0;j<owner->group_count;++j)if(same(owner->groups[j].name,names[i]))break;
        if(j==owner->group_count)return RF_NOT_FOUND;
        value[owner->groups[j].material]=(int32_t)j;
    }
    memcpy(slots,value,sizeof(value));return RF_OK;
}
int rf_entity_footstep_groups_read(const void *text,uint32_t bytes,const char *class_name,
    const rf_foley_owner *owner,int32_t slots[10])
{
    lexer l={text,bytes,0};char t[256],name[1][32];int32_t value[10],binding[10];
    int status,quoted,selected=0;uint32_t i;
    if(!text || !bytes || !class_name || !*class_name || !slots)return RF_RANGE;
    status=rf_foley_bind_materials(owner,NULL,0,value);if(status)return status;
    for(;;) {
        status=token(&l,t,&quoted);
        if(status==RF_NOT_FOUND)break;if(status)return status;
        if(quoted)continue;
        if(same(t,"$Name:")) {
            if(selected)break;
            if(token(&l,t,&quoted) || !quoted)return RF_FORMAT;
            selected=same(t,class_name);
        } else if(same(t,"#End"))break;
        else if(selected && same(t,"$Footstep")) {
            if(token(&l,t,&quoted) || quoted || !same(t,"Sound:") ||
               metadata_string(&l,name[0],sizeof(name[0])))return RF_FORMAT;
            status=rf_foley_bind_materials(owner,name,1,binding);if(status)return status;
            for(i=0;i<10;++i)if(binding[i]>=0)value[i]=binding[i];
        }
    }
    if(!selected)return RF_NOT_FOUND;memcpy(slots,value,sizeof(value));return RF_OK;
}
int rf_entity_lod_distances_read(const void *text,uint32_t bytes,const char *name,
    rf_entity_lod_distances *result)
{
    lexer l={text,bytes,0};char t[256];rf_entity_lod_distances value={0};
    int status,quoted,selected=0,found=0;
    if(!text || !bytes || !name || !*name || !result)return RF_RANGE;
    for(;;) {
        status=token(&l,t,&quoted);if(status==RF_NOT_FOUND)break;if(status)return status;
        if(quoted)continue;
        if(same(t,"$Name:")) {
            if(selected)break;
            if(token(&l,t,&quoted) || !quoted)return RF_FORMAT;
            selected=same(t,name);
        } else if(same(t,"#End"))break;
        else if(selected && same(t,"$LOD")) {
            if(found++ || token(&l,t,&quoted) || quoted || !same(t,"Distances:") ||
               token(&l,t,&quoted) || quoted || strcmp(t,"{"))return RF_FORMAT;
            for(;;) {
                lexer saved=l;float number;
                if(token(&l,t,&quoted))return RF_FORMAT;
                if(!quoted && !strcmp(t,"}"))break;
                l=saved;status=sphere_number(&l,&number);if(status)return status;
                if(value.count<4)value.distances[value.count++]=number;
            }
        }
    }
    if(!selected)return RF_NOT_FOUND;*result=value;return RF_OK;
}
int rf_game_jump_height_read(const void *text,uint32_t bytes,float *height)
{
    static const char *words[]={"$Max","Entity","Jump","Height:"};
    lexer l;char t[256];float value=0;uint32_t match=0;int found=0,status,quoted;
    if(!text || !bytes || !height)return RF_RANGE;
    l.text=text;l.size=bytes;l.at=0;
    while((status=token(&l,t,&quoted))==RF_OK) {
        if(quoted){match=0;continue;}
        if(same(t,words[match]))++match;
        else match=same(t,words[0])?1:0;
        if(match==4) {
            if(found)return RF_FORMAT;
            status=sphere_number(&l,&value);if(status)return status;
            if(value<0)return RF_FORMAT;
            found=1;match=0;
        }
    }
    if(status!=RF_NOT_FOUND)return status;
    if(!found)return RF_NOT_FOUND;
    *height=value;return RF_OK;
}
int rf_game_jump_height_load(rf_vpp *tables,uint32_t budget,float *height)
{
    rf_vpp_entry entry;void *text;int status;
    if(!tables || !height)return RF_RANGE;
    status=rf_vpp_find(tables,"game.tbl",&entry);if(status)return status;
    if(!entry.size || entry.size>budget)return RF_RANGE;
    text=malloc(entry.size);if(!text)return RF_RANGE;
    status=rf_vpp_read(tables,&entry,0,text,entry.size);
    if(!status)status=rf_game_jump_height_read(text,entry.size,height);
    free(text);return status;
}
int rf_entity_movement_load(rf_vpp *tables,const char *name,uint32_t budget,rf_entity_movement_values *result)
{
    rf_vpp_entry entry;rf_entity_movement_values value={0,1,1,0};void *text;lexer l;
    char t[256];int status,quoted,found=0;uint32_t mask=0;
    if(!tables || !name || !*name || !result)return RF_RANGE;
    status=rf_vpp_find(tables,"entity.tbl",&entry);if(status)return status;
    if(!entry.size || entry.size>budget)return RF_RANGE;text=malloc(entry.size);if(!text)return RF_RANGE;
    status=rf_vpp_read(tables,&entry,0,text,entry.size);if(status)goto done;
    l.text=text;l.size=entry.size;l.at=0;
    while((status=token(&l,t,&quoted))==RF_OK) {
        if(quoted)continue;
        if(same(t,"$name:")) {
            if(found)break;
            if(token(&l,t,&quoted) || !quoted) {status=RF_FORMAT;goto done;}
            found=same(t,name);
        } else if(found && same(t,"$Max")) {
            lexer saved;
            if(token(&l,t,&quoted)) {status=RF_FORMAT;goto done;}
            if(quoted || !same(t,"Vel:"))continue;
            if(mask&1 || sphere_number(&l,&value.speed)) {status=RF_FORMAT;goto done;}mask|=1;
            saved=l;
            if(!token(&l,t,&quoted) && !quoted && same(t,"+slow")) {
                if(token(&l,t,&quoted) || quoted || !same(t,"factor:") || sphere_number(&l,&value.slow_factor)) {status=RF_FORMAT;goto done;}
            } else l=saved;
            saved=l;
            if(!token(&l,t,&quoted) && !quoted && same(t,"+fast")) {
                if(token(&l,t,&quoted) || quoted || !same(t,"factor:") || sphere_number(&l,&value.fast_factor)) {status=RF_FORMAT;goto done;}
            } else l=saved;
        } else if(found && same(t,"$Acceleration:")) {
            if(mask&2 || sphere_number(&l,&value.acceleration)) {status=RF_FORMAT;goto done;}mask|=2;
        }
    }
    if(status==RF_OK || status==RF_NOT_FOUND) {
        status=!found?RF_NOT_FOUND:mask!=3 || value.speed<0 || value.acceleration<0 || value.slow_factor<0 || value.fast_factor<0?RF_FORMAT:RF_OK;
        if(!status)*result=value;
    }
done:
    free(text);return status;
}
int rf_entity_damage_factors_read(const void *text,uint32_t bytes,const char *name,float factors[11])
{
    static const char *names[]={"bash","bullet","armor piercing bullet","explosive","fire","energy","electrical","acid","scalding"};
    lexer l={(const unsigned char*)text,bytes,0};float value[11];
    char t[256];int status,quoted,found=0;uint32_t i;
    if(!text || !name || !*name || !factors)return RF_RANGE;
    for(i=0;i<11;++i)value[i]=1;
    while((status=token(&l,t,&quoted))==RF_OK) {
        if(quoted)continue;
        if(same(t,"$Name:")) {
            if(found)break;
            if(token(&l,t,&quoted) || !quoted)return RF_FORMAT;
            found=same(t,name);
        } else if(found && same(t,"$Damage")) {
            if(token(&l,t,&quoted) || quoted || !same(t,"Type"))return RF_FORMAT;
            if(token(&l,t,&quoted) || quoted || !same(t,"Factor:"))return RF_FORMAT;
            if(token(&l,t,&quoted) || !quoted)return RF_FORMAT;
            for(i=0;i<9;++i)if(same(t,names[i]))break;
            if(i==9 || sphere_number(&l,&value[i]))return RF_FORMAT;
        }
    }
    if(status!=RF_OK && status!=RF_NOT_FOUND)return status;
    if(!found)return RF_NOT_FOUND;memcpy(factors,value,sizeof(value));return RF_OK;
}
static int eye_limit_vector(lexer *l,float result[3])
{
    uint32_t i,start;char t[256];int quoted;float degrees;lexer number;
    while(l->at<l->size && l->text[l->at]<=32)++l->at;
    if(l->at==l->size || l->text[l->at++]!='<')return RF_FORMAT;
    for(i=0;i<3;i++) {
        start=l->at;while(l->at<l->size && l->text[l->at]!=(i==2?'>':','))++l->at;
        if(l->at==l->size)return RF_FORMAT;
        number=(lexer){l->text+start,l->at-start,0};
        if(sphere_number(&number,&degrees) || token(&number,t,&quoted)!=RF_NOT_FOUND)return RF_FORMAT;
        result[i]=(float)((double)degrees*(double)0.01745329238474369f);++l->at;
    }
    return RF_OK;
}
int rf_entity_eye_limits_read(const void *text,uint32_t bytes,const char *name,rf_entity_eye_limits *result)
{
    lexer l={(const unsigned char*)text,bytes,0};rf_entity_eye_limits value={{-1.5707963705062866f,0,0},{1.5707963705062866f,0,0}};
    char t[256];int status,quoted,found=0,seen=0;
    if(!text || !name || !*name || !result)return RF_RANGE;
    while((status=token(&l,t,&quoted))==RF_OK) {
        if(quoted)continue;
        if(same(t,"$Name:")) {
            if(found)break;
            if(token(&l,t,&quoted) || !quoted)return RF_FORMAT;
            found=same(t,name);
        } else if(found && same(t,"$Min")) {
            if(!metadata_tag(&l,"Relative Eye PHB:"))continue;
            if(seen++)return RF_FORMAT;
            if(eye_limit_vector(&l,value.minimum) || !metadata_tag(&l,"$Max Relative Eye PHB:") ||
               eye_limit_vector(&l,value.maximum))return RF_FORMAT;
        } else if(found && same(t,"$Max") && metadata_tag(&l,"Relative Eye PHB:"))return RF_FORMAT;
    }
    if(status!=RF_OK && status!=RF_NOT_FOUND)return status;
    if(!found)return RF_NOT_FOUND;*result=value;return RF_OK;
}
int rf_entity_vitals_config_read(const void *text,uint32_t bytes,const char *name,
    rf_entity_creation_vitals_class *result)
{
    lexer l={(const unsigned char*)text,bytes,0};rf_entity_creation_vitals_class value={0};
    char t[256];uint32_t mask=0,bit;int status,quoted,found=0;float fov=0,cosine;
    if(!text || !name || !*name || !result)return RF_RANGE;
    while((status=token(&l,t,&quoted))==RF_OK) {
        if(quoted)continue;
        if(same(t,"$Name:")) {
            if(found)break;
            if(token(&l,t,&quoted) || !quoted)return RF_FORMAT;
            found=same(t,name);
        } else if(found) {
            bit=same(t,"$Life:")?1:same(t,"$Envirosuit:")?2:same(t,"$FOV:")?4:0;
            if(!bit)continue;if(mask&bit)return RF_FORMAT;mask|=bit;
            if(sphere_number(&l,bit==1?&value.health:bit==2?&value.armor:&fov))return RF_FORMAT;
        }
    }
    if(status!=RF_OK && status!=RF_NOT_FOUND)return status;
    if(!found)return RF_NOT_FOUND;if(mask!=7 || fov<0 || fov>360)return RF_FORMAT;
    cosine=(float)cos(((double)fov*(double)0.01745329238474369f)*0.5);
    memcpy(&value.field_764,&cosine,4);*result=value;return RF_OK;
}
static int class_flags(lexer *l,uint32_t secondary,uint32_t *result)
{
    /* Original pointer tables 594598 (28) and 594608 (8), bit = name index. */
    static const char *primary[]={"walk","fly","climb","holds_weapons","sentient","alt_fire","water_only","linked_primaries","swim","apc","sub","fighter","driller","turret","mouselook","crusher","medic","humanoid","no_collide","collide_corpse","is_camera","custom_corpse","jeep","ambient","envirosuit","nano_shield","slippery","fire_outside_range"};
    static const char *second[]={"collide_player","collide_entity","merc","mutant","ignore_fire","drools slime","linked_eye","tankbot"};
    const char *const *names=secondary?second:primary;uint32_t count=secondary?8:28,i,value=0;
    char t[256];int quoted,status;
    if(token(l,t,&quoted) || quoted || strcmp(t,"("))return RF_FORMAT;
    while((status=token(l,t,&quoted))==RF_OK) {
        if(!quoted && !strcmp(t,")")) {*result=value;return RF_OK;}
        if(!quoted)return RF_FORMAT;
        for(i=0;i<count;++i)if(same(t,names[i]))break;
        if(i==count)return RF_FORMAT;value|=1u<<i;
    }
    return RF_FORMAT;
}
int rf_entity_class_physics_read(const void *text,uint32_t bytes,const char *name,
    rf_entity_class_physics *result)
{
    static const char *movement[]={"none","run","climb","fall","swim","apc","apc fall","sub","sub fall","fighter","turret","robot fly","hover","freelookcam","deadcam","john's descent flying mode"};
    static const char *uses[]={"vehicle","switch","command","turret","monitor","medic","ai response","play_sound"};
    static const uint32_t kinds[]={1,2,3,4,5,6,9,10};
    lexer l={(const unsigned char*)text,bytes,0};rf_entity_class_physics value={0};
    char t[256];uint32_t mask=0,bit,i;int status,quoted,found=0;
    if(!text || !name || !*name || !result)return RF_RANGE;
    while((status=token(&l,t,&quoted))==RF_OK) {
        if(quoted)continue;
        if(same(t,"$Name:")) {
            if(found)break;
            if(token(&l,t,&quoted) || !quoted)return RF_FORMAT;
            found=same(t,name);
        } else if(found) {
            bit=same(t,"$Mass:")?1:same(t,"$Material:")?2:same(t,"$Flags:")?4:same(t,"$Flags2:")?8:same(t,"$Movemode:")?16:same(t,"$Use:")?32:0;
            if(!bit)continue;if(mask&bit)return RF_FORMAT;mask|=bit;
            if(bit==1) {if(sphere_number(&l,&value.mass))return RF_FORMAT;}
            else if(bit==2) {if(token(&l,t,&quoted) || !quoted || asset(value.material,t))return RF_FORMAT;}
            else if(bit==16) {
                if(token(&l,t,&quoted) || !quoted)return RF_FORMAT;
                for(i=0;i<16;++i)if(same(t,movement[i]))break;
                if(i==16)return RF_FORMAT;value.movement_index=i;
            } else if(bit==32) {
                if(token(&l,t,&quoted) || !quoted)return RF_FORMAT;
                for(i=0;i<8;++i)if(same(t,uses[i]))break;
                if(i<8) {
                    value.use_kind=kinds[i];
                    if(token(&l,t,&quoted) || quoted || !same(t,"+radius:") || sphere_number(&l,&value.use_radius))return RF_FORMAT;
                }
            } else if(class_flags(&l,bit==8,bit==8?&value.flags2:&value.flags))return RF_FORMAT;
        }
    }
    if(status!=RF_OK && status!=RF_NOT_FOUND)return status;
    if(!found)return RF_NOT_FOUND;if((mask&23)!=23)return RF_FORMAT;
    *result=value;return RF_OK;
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
int rf_surface_materials_read(const void *text,uint32_t bytes,rf_surface_materials *result)
{
    static const char *names[]={"Default","Rock","Metal","Flesh","Water","Lava","Solid","Sand","Ice","Glass"};
    rf_surface_materials value={0};lexer l={(const unsigned char*)text,bytes,0};
    char t[256];uint32_t i,current=0;int status,quoted,section=0,selected=0,ended=0;
    if(!text || !result)return RF_RANGE;
    for(i=0;i<10;++i) {
        status=rf_entity_material_read(text,bytes,names[i],value.materials+i);if(status)return status;
    }
    while((status=token(&l,t,&quoted))==RF_OK) {
        if(quoted)continue;
        if(same(t,"#Materials")) {section=1;continue;}
        if(!section)continue;
        if(same(t,"#End")) {ended=1;break;}
        if(same(t,"$name:")) {
            if(token(&l,t,&quoted) || !quoted)return RF_FORMAT;
            current=0;selected=1;
            for(i=0;i<10;++i)if(same(t,names[i])) {current=i;break;}
        } else if(same(t,"$bitmap")) {
            if(token(&l,t,&quoted) || quoted || !same(t,"prefix:"))return RF_FORMAT;
            if(!selected || token(&l,t,&quoted) || !quoted)return RF_FORMAT;
            if(value.count==64 || strlen(t)>=32)return RF_RANGE;
            strcpy(value.prefixes[value.count].name,t);
            value.prefixes[value.count++].material=current;
        }
    }
    if(status!=RF_OK && status!=RF_NOT_FOUND)return status;
    if(!ended)return RF_FORMAT;
    *result=value;return RF_OK;
}
uint32_t rf_surface_material_lookup(const rf_surface_materials *table,const char *texture)
{
    const char *end;char prefix[32];uint32_t i;size_t length;
    if(!table || !texture || table->count>64)return 0;
    end=strchr(texture,'_');if(!end)return 0;
    length=(size_t)(end-texture);if(length>=sizeof(prefix))return 0;
    memcpy(prefix,texture,length);prefix[length]=0;
    for(i=0;i<table->count;++i)if(same(prefix,table->prefixes[i].name))
        return table->prefixes[i].material<10?table->prefixes[i].material:0;
    return 0;
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
static int state_set_read(const void *text,uint32_t size,const char *class_name,const char *weapon,
    rf_vpp *motions,rf_entity_state_set *value)
{
    static const char *names[23]={"stand","attack_stand","walk","attack_walk","run","attack_run",
        "flee_run","flail_run","crouch","attack_crouch","attack_crouch_walk","attack_lean_left",
        "attack_lean_right","cower","freefall","on_turret","corpse_carry_stand","corpse_carry_walk",
        "swim_stand","swim_walk","jeep_drive","jeep_gun","custom"};
    rf_entity_state_declaration declaration;char compiled[64];uint32_t identities[23]={0},i,identity;
    uint8_t flags[23]={0};int status,added;int32_t index;
    rf_model_motion_registry registry={identities,flags,0,23};
    status=state_group_exists(text,size,class_name,weapon);if(status)return status;
    for(i=0;i<45;++i)value->actions[i]=-1;
    for(i=0;i<23;++i) {
        value->states[i]=-1;
        status=rf_entity_state_declaration_read(text,size,class_name,weapon,names[i],&declaration);
        if(status==RF_NOT_FOUND) {status=RF_OK;continue;}
        if(status)return status;
        if(!*declaration.motion)continue;
        value->marker_counts[i]=declaration.marker_count;memcpy(value->marker_frames[i],declaration.marker_frames,sizeof(declaration.marker_frames));
        status=rf_motion_cache_acquire(value->cache,23,declaration.motion,&identity);if(status)return status;
        status=rf_model_register_motion(&registry,identity+1,1,&index,&added);if(status)return status;
        if(added) {
            value->cache_indices[index]=identity;value->looping[index]=1;
            status=rf_motion_compiled_filename((const char*)value->cache[identity].bytes,compiled);if(status)return status;
            status=rf_motion_file_open(value->files+index,motions,compiled);if(status)return status;
        }
        value->states[i]=index;
    }
    value->count=registry.count;return RF_OK;
}
int rf_entity_state_set_open(const char *path,const char *class_name,const char *weapon,
    rf_vpp *motions,uint32_t budget,rf_entity_state_set *result)
{
    rf_vpp archive;rf_vpp_entry entry;rf_entity_state_set *value=NULL;char *text;int status;
    if(!path || !class_name || !*class_name || !weapon || !motions || !result)return RF_RANGE;
    status=rf_vpp_open(&archive,path);if(status)return status;
    status=rf_vpp_find(&archive,"entity.tbl",&entry);if(status)goto done;
    if(!entry.size || (uint64_t)entry.size+sizeof(*value)>budget){status=RF_RANGE;goto done;}
    value=calloc(1,sizeof(*value)+entry.size);if(!value){status=RF_IO;goto done;}
    text=(char*)(value+1);status=rf_vpp_read(&archive,&entry,0,text,entry.size);
    if(!status)status=state_set_read(text,entry.size,class_name,weapon,motions,value);
    if(!status)*result=*value;
done:
    free(value);rf_vpp_close(&archive);return status;
}
static const char *const entity_action_names[45]={"corpse_drop","corpse_carry","fire_stand","alt_fire_stand","fire_crouch","death_generic","death_blast_forward","death_blast_backward","death_head_forward","death_head_backward","death_head_neutral","death_chest_forward","death_chest_backward","death_chest_neutral","death_leg_left","death_leg_right","death_crouch","sidestep_left","sidestep_right","roll_left","roll_right","land","flinch_stand","flinch_attack_stand","flinch_chest","flinch_back","flinch_leg_left","flinch_leg_right","idle_to_ready","ready_to_idle","idle_1","idle_2","idle_3","idle_4","rock_drop","rock_pickup","death_still_1","death_still_2","death_still_3","reload","unholster","speak","speak_short","heal_light_1","hit_alarm"};
int32_t rf_entity_declared_action_lookup(const uint32_t declarations[2],uint32_t model,uint32_t model_kind,const char *name)
{
    const char *names[45];uint32_t i;if(!declarations)return -1;
    for(i=0;i<45;++i)names[i]=(declarations[i/32]&(1u<<(i%32)))?entity_action_names[i]:NULL;
    return rf_entity_action_name_lookup(model,model_kind,names,name);
}
int rf_entity_action_declarations_read(const void *text,uint32_t bytes,const char *class_name,const char *weapon,uint32_t result[2])
{
    uint32_t bits[2]={0},i;int status;rf_entity_action_declaration action;
    if(!text || !class_name || !*class_name || !weapon || !result)return RF_RANGE;
    for(i=0;i<45;++i) {
        status=rf_entity_action_read(text,bytes,class_name,weapon,entity_action_names[i],&action);
        if(status==RF_NOT_FOUND)continue;if(status)return status;bits[i/32]|=1u<<(i%32);
    }
    memcpy(result,bits,sizeof(bits));return RF_OK;
}
static int action_set_extend(const void *text,uint32_t size,const char *name,const char *weapon,rf_vpp *motions,rf_entity_state_set *v)
{
    const char *const *names=entity_action_names;

    uint32_t identities[68]={0},i,identity;uint8_t flags[68]={0};int status,added;int32_t index;char compiled[64];
    rf_model_motion_registry registry={identities,flags,0,68};
    /* Reconstruct registration keys from retained state cache identities. */
    if(v->count>23)return RF_RANGE;
    registry.count=v->count;
    for(i=0;i<v->count;++i){identities[i]=v->cache_indices[i]+1;flags[i]=v->looping[i];}
    for(i=0;i<45;++i) {
        rf_entity_action_declaration action;
        status=rf_entity_action_read(text,size,name,weapon,names[i],&action);
        if(status==RF_NOT_FOUND)continue;if(status)return status;
        v->action_declarations[i/32]|=1u<<(i%32);
        memcpy(v->action_sounds[i],action.sound,64);
        if(!*action.motion)continue;
        status=rf_motion_cache_acquire(v->cache,68,action.motion,&identity);if(status)return status;
        status=rf_model_register_motion(&registry,identity+1,0,&index,&added);if(status)return status;
        if(added) {
            status=rf_motion_compiled_filename((const char*)v->cache[identity].bytes,compiled);if(status)return status;
            status=rf_motion_file_open(v->files+index,motions,compiled);if(status)return status;
            v->cache_indices[index]=identity;v->looping[index]=0;
        }
        v->actions[i]=index;
    }
    v->count=registry.count;return RF_OK;
}
void rf_entity_base_motions_close(rf_entity_base_motions *m)
{
    uint32_t i;if(!m)return;
    for(i=0;i<m->group_count;++i)free(m->groups[i].files);
    free(m->groups);free(m->classes);memset(m,0,sizeof(*m));
}
const rf_entity_weapon_motion_group *rf_entity_weapon_motion_find(const rf_entity_base_motions *m,uint32_t class_index,int32_t weapon)
{
    uint32_t i;if(!m || class_index>=m->class_count || weapon<0)return NULL;
    for(i=0;i<m->group_count;++i)if(m->groups[i].class_index==class_index && m->groups[i].weapon==(uint32_t)weapon)return m->groups+i;
    return NULL;
}
int rf_entity_base_motions_open(const rf_entity_seeds *seeds,rf_vpp *tables,rf_vpp *motions,
    uint32_t budget,rf_entity_base_motions *result)
{
    rf_entity_base_motions v={0};rf_vpp_entry entry,weapon_entry;void *text=NULL;
    uint64_t bytes;uint32_t i,j,group_count=0,at=0;int status;rf_entity_state_set *working=NULL;
    if(!seeds || !tables || !motions || !result || result->classes || result->class_count ||
       result->resident_bytes || result->peak_bytes || result->weapons.count || result->weapons.primary_count || result->groups || result->group_count ||
       (seeds->class_count && !seeds->classes))return RF_RANGE;
    v.class_count=seeds->class_count;bytes=sizeof(v)+(uint64_t)v.class_count*sizeof(*v.classes);
    if(bytes>budget)return RF_RANGE;
    v.resident_bytes=v.peak_bytes=(uint32_t)bytes;
    if(!v.class_count){*result=v;return RF_OK;}
    status=rf_vpp_find(tables,"weapons.tbl",&weapon_entry);if(status)return status;
    if(!weapon_entry.size || bytes+weapon_entry.size>budget)return RF_RANGE;
    status=rf_weapon_names_load(tables,budget-(uint32_t)bytes,&v.weapons);if(status)return status;
    status=rf_vpp_find(tables,"entity.tbl",&entry);if(status)return status;
    if(!entry.size || bytes+entry.size>budget)return RF_RANGE;
    v.peak_bytes+=entry.size>weapon_entry.size?entry.size:weapon_entry.size;
    v.classes=calloc(v.class_count,sizeof(*v.classes));text=malloc(entry.size);
    if(!v.classes || !text){status=RF_RANGE;goto done;}
    status=rf_vpp_read(tables,&entry,0,text,entry.size);if(status)goto done;
    for(i=0;i<v.class_count;++i) {
        const rf_entity_seed_class *c=seeds->classes+i;
        for(j=0;j<23;++j)v.classes[i].states[j]=-1;
        for(j=0;j<45;++j)v.classes[i].actions[j]=-1;
        if(c->record_index>=seeds->records.count || !seeds->records.items){status=RF_RANGE;goto done;}
        status=rf_entity_weapon_groups_read(text,entry.size,seeds->records.items[c->record_index].record.class_name,&v.weapons,v.classes[i].weapon_groups);
        if(status)goto done;
        status=rf_entity_default_weapons_read(text,entry.size,seeds->records.items[c->record_index].record.class_name,&v.weapons,&v.classes[i].default_weapons);
        if(status)goto done;
        if(c->model_kind!=2)continue;
        status=state_set_read(text,entry.size,seeds->records.items[c->record_index].record.class_name,"",motions,v.classes+i);
        if(status)goto done;
        status=action_set_extend(text,entry.size,seeds->records.items[c->record_index].record.class_name,"",motions,v.classes+i);
        if(status)goto done;
        for(j=0;j<v.weapons.count;++j)if(v.classes[i].weapon_groups[j>>5]&(1u<<(j&31)))++group_count;
    }
    if(group_count) {
        bytes+=(uint64_t)group_count*sizeof(*v.groups);
        if(bytes+entry.size+sizeof(*working)>budget){status=RF_RANGE;goto done;}
        v.groups=calloc(group_count,sizeof(*v.groups));working=calloc(1,sizeof(*working));
        if(!v.groups || !working){status=RF_RANGE;goto done;}
        v.group_count=group_count;v.resident_bytes=(uint32_t)bytes;
        for(i=0;i<v.class_count;++i)if(seeds->classes[i].model_kind==2)for(j=0;j<v.weapons.count;++j) {
            rf_entity_weapon_motion_group *g;uint64_t resource_bytes,peak;const char *name;
            if(!(v.classes[i].weapon_groups[j>>5]&(1u<<(j&31))))continue;
            name=seeds->records.items[seeds->classes[i].record_index].record.class_name;
            memset(working,0,sizeof(*working));
            status=state_set_read(text,entry.size,name,v.weapons.names[j],motions,working);if(status)goto done;
            status=action_set_extend(text,entry.size,name,v.weapons.names[j],motions,working);if(status)goto done;
            resource_bytes=(uint64_t)working->count*(sizeof(*g->files)+sizeof(*g->looping)+sizeof(*g->identities));
            peak=bytes+resource_bytes+entry.size+sizeof(*working);if(peak>budget){status=RF_RANGE;goto done;}
            if(peak>v.peak_bytes)v.peak_bytes=(uint32_t)peak;
            g=v.groups+at++;g->class_index=i;g->weapon=j;g->count=working->count;
            memcpy(g->states,working->states,sizeof(g->states));memcpy(g->actions,working->actions,sizeof(g->actions));
            memcpy(g->action_sounds,working->action_sounds,sizeof(g->action_sounds));
            memcpy(g->action_declarations,working->action_declarations,sizeof(g->action_declarations));
            if(resource_bytes) {
                g->files=malloc((size_t)resource_bytes);if(!g->files){status=RF_RANGE;goto done;}
                g->looping=(uint8_t*)(g->files+g->count);
                g->identities=(char(*)[64])(g->looping+g->count);
                memcpy(g->files,working->files,g->count*sizeof(*g->files));memcpy(g->looping,working->looping,g->count);
                {uint32_t k;for(k=0;k<g->count;++k)memcpy(g->identities[k],working->cache[working->cache_indices[k]].bytes,64);}
            }
            bytes+=resource_bytes;v.resident_bytes=(uint32_t)bytes;
        }
    }
    free(working);free(text);*result=v;return RF_OK;
done:
    free(working);free(text);rf_entity_base_motions_close(&v);return status;
}

/* Temporary per-model registry; it is discarded after copying exact resources. */
typedef struct catalog_work {
    rf_motion_cache_record *cache;uint32_t cache_capacity;
    rf_model_motion_registry registry;rf_entity_model_motion *resources;
} catalog_work;
static int catalog_bind(catalog_work *w,const rf_entity_state_set *base,
    const rf_entity_weapon_motion_group *group,rf_entity_motion_mapping *out)
{
    uint32_t count=base?base->count:group->count,i,identity;int status,added;
    int32_t remap[68],index;const int32_t *states=base?base->states:group->states;
    const int32_t *actions=base?base->actions:group->actions;
    if(count>68)return RF_RANGE;
    for(i=0;i<count;++i) {
        const char *name;uint8_t loop=base?base->looping[i]:group->looping[i];
        if(base) {
            if(base->cache_indices[i]>=68)return RF_RANGE;
            name=(const char*)base->cache[base->cache_indices[i]].bytes;
        } else name=group->identities[i];
        if(!*name)return RF_FORMAT;
        status=rf_motion_cache_acquire(w->cache,w->cache_capacity,name,&identity);if(status)return status;
        status=rf_model_register_motion(&w->registry,identity+1,loop,&index,&added);if(status)return status;
        if(added) {
            rf_entity_model_motion *r=w->resources+index;
            r->file=base?base->files[i]:group->files[i];r->looping=loop;
            {rf_motion_track track;status=rf_motion_file_track(&r->file,0,&track);if(status)return status;r->comparison=track.envelope;}
            memcpy(r->identity,w->cache[identity].bytes,64);
        }
        remap[i]=index;
    }
    for(i=0;i<68;++i) {
        int32_t source=i<23?states[i]:actions[i-23];
        if(source<-1 || (source>=0 && (uint32_t)source>=count))return RF_RANGE;
        if(i<23)out->states[i]=source<0?-1:remap[source];
        else out->actions[i-23]=source<0?-1:remap[source];
    }
    return RF_OK;
}
void rf_entity_motion_catalog_close(rf_entity_motion_catalog *v)
{
    uint32_t i;if(!v)return;
    for(i=0;i<v->model_count;++i)free(v->models[i].items);
    free(v->models);free(v->mappings);memset(v,0,sizeof(*v));
}
int rf_entity_motion_mapping_overlay(const rf_entity_motion_mapping *base,
    const rf_entity_motion_mapping *weapon,rf_entity_motion_mapping *result)
{
    rf_motion_bindings a={0},b={0};rf_entity_motion_mapping v;uint32_t i;
    if(!base || !result || base->weapon!=-1 || (weapon && (weapon->weapon<0 ||
       weapon->class_index!=base->class_index || weapon->skeleton!=base->skeleton)))return RF_RANGE;
    for(i=0;i<68;++i) {
        int32_t x=i<23?base->states[i]:base->actions[i-23];
        int32_t y=!weapon?-1:i<23?weapon->states[i]:weapon->actions[i-23];
        if(x<-1 || y<-1)return RF_RANGE;
        if(i<23){a.states[i].motion=x;b.states[i].motion=y;}
        else {a.actions[i-23].motion=x;b.actions[i-23].motion=y;}
    }
    rf_motion_overlay_bindings(&a,&a,b.states,b.actions);v=*base;
    if(weapon)v.weapon=weapon->weapon;
    for(i=0;i<23;++i)v.states[i]=a.states[i].motion;
    for(i=0;i<45;++i)v.actions[i]=a.actions[i].motion;
    *result=v;return RF_OK;
}
int rf_entity_motion_selection_base(const rf_entity_motion_catalog *c,
    const rf_entity_base_motions *b,uint32_t class_index,rf_entity_motion_selection *result)
{
    rf_entity_motion_selection v;uint32_t i;
    if(!c || !b || !result || c->class_count!=b->class_count || class_index>=c->class_count ||
       !c->mappings || !b->classes || c->mapping_count<c->class_count)return RF_RANGE;
    v.mapping=c->mappings[class_index];
    if(v.mapping.class_index!=class_index || v.mapping.weapon!=-1)return RF_RANGE;
    if(v.mapping.skeleton==UINT32_MAX)return RF_NOT_FOUND;
    if(v.mapping.skeleton>=c->model_count)return RF_RANGE;
    for(i=0;i<45;++i)v.action_sounds[i]=b->classes[class_index].action_sounds[i];
    *result=v;return RF_OK;
}
int rf_entity_motion_selection_weapon(const rf_entity_motion_catalog *c,
    const rf_entity_base_motions *b,uint32_t class_index,int32_t weapon,rf_entity_motion_selection *result)
{
    rf_entity_motion_selection v;const rf_entity_motion_mapping *map=NULL;
    const rf_entity_weapon_motion_group *group=NULL;uint32_t i,j;int status;int32_t alias;
    if(!result)return RF_RANGE;
    if(weapon<0)return RF_OK;
    status=rf_entity_motion_selection_base(c,b,class_index,&v);if(status)return status;
    if((uint32_t)weapon>=b->weapons.count || b->weapons.count>64 ||
       (uint64_t)c->class_count+b->group_count!=c->mapping_count || (b->group_count && !b->groups))return RF_RANGE;
    alias=rf_weapon_name_find(&b->weapons,"machine pistol special");
    if(weapon==alias) {
        weapon=rf_weapon_name_find(&b->weapons,"machine pistol");if(weapon<0)return RF_NOT_FOUND;
    }
    for(i=0;i<b->group_count;++i)if(b->groups[i].class_index==class_index && b->groups[i].weapon==(uint32_t)weapon) {
        group=b->groups+i;map=c->mappings+c->class_count+i;
        if(map->weapon!=weapon)return RF_RANGE;break;
    }
    status=rf_entity_motion_mapping_overlay(c->mappings+class_index,map,&v.mapping);if(status)return status;
    v.mapping.weapon=weapon;
    if(group)for(j=0;j<45;++j)if(map->actions[j]!=-1)v.action_sounds[j]=group->action_sounds[j];
    *result=v;return RF_OK;
}
static int catalog_markers(const rf_entity_skeletons *s,const rf_entity_base_motions *b,
    rf_entity_motion_catalog *v,uint32_t budget)
{
    rf_motion_cache_record *cache;uint64_t total=0,peak;uint32_t capacity,i,j,k,identity;int status=RF_OK;
    static const char *names[2]={"footstep_left","footstep_right"};
    for(i=0;i<v->model_count;++i)total+=v->models[i].count;
    if(!total)return RF_OK;
    capacity=(uint32_t)(total<800?total:800);peak=(uint64_t)v->resident_bytes+capacity*sizeof(*cache);
    if(peak>budget)return RF_RANGE;
    if(peak>v->peak_bytes)v->peak_bytes=(uint32_t)peak;
    cache=calloc(capacity,sizeof(*cache));if(!cache)return RF_IO;
    /* Markers belong to the motion cache, across models and loop registrations. */
    for(i=0;i<b->class_count;++i)if(s->class_indices[i]!=UINT32_MAX)for(j=0;j<23;++j) {
        const rf_entity_state_set *base=b->classes+i;int32_t index=base->states[j];
        if(index<0 || !base->marker_counts[j])continue;
        if(base->marker_counts[j]!=2 || (uint32_t)index>=base->count || base->cache_indices[index]>=68){status=RF_RANGE;goto done;}
        status=rf_motion_cache_acquire(cache,capacity,(const char*)base->cache[base->cache_indices[index]].bytes,&identity);if(status)goto done;
        for(k=0;k<2;++k){status=rf_motion_marker_register(cache+identity,names[k],base->marker_frames[j][k]);if(status)goto done;}
    }
    for(i=0;i<v->model_count;++i)for(j=0;j<v->models[i].count;++j) {
        rf_entity_model_motion *r=v->models[i].items+j;
        status=rf_motion_cache_acquire(cache,capacity,r->identity,&identity);if(status)goto done;
        for(k=0;k<2;++k) {
            memcpy(r->markers+k,cache[identity].bytes+0x50+k*20,4);
            memcpy(v->models[i].marker_names[j].names[k],cache[identity].bytes+0x40+k*20,16);
            if(cache[identity].bytes[0x40+k*20])r->marker_mask|=1u<<k;
        }
    }
done:
    free(cache);return status;
}
int rf_entity_motion_catalog_open(const rf_entity_skeletons *s,const rf_entity_base_motions *b,
    uint32_t budget,rf_entity_motion_catalog *result)
{
    rf_entity_motion_catalog v={0};catalog_work w={0};
    uint64_t bytes,capacity,scratch,peak;uint32_t i,j,k;int status=RF_RANGE;
    if(!s || !b || !result || result->models || result->mappings || result->model_count ||
       result->mapping_count || result->class_count || result->resident_bytes || result->peak_bytes ||
       s->class_count!=b->class_count || (s->class_count && (!s->class_indices || !b->classes)) ||
       (b->group_count && !b->groups) || (s->count && !s->items))return RF_RANGE;
    if((uint64_t)b->class_count+b->group_count>UINT32_MAX)return RF_RANGE;
    v.model_count=s->count;v.class_count=b->class_count;v.mapping_count=b->class_count+b->group_count;
    bytes=sizeof(v)+(uint64_t)v.model_count*sizeof(*v.models)+(uint64_t)v.mapping_count*sizeof(*v.mappings);
    if(bytes>budget)return RF_RANGE;
    v.resident_bytes=v.peak_bytes=(uint32_t)bytes;
    if(v.model_count){v.models=calloc(v.model_count,sizeof(*v.models));if(!v.models)return RF_IO;}
    if(v.mapping_count){v.mappings=calloc(v.mapping_count,sizeof(*v.mappings));if(!v.mappings){status=RF_IO;goto done;}}
    for(i=0;i<v.mapping_count;++i) {
        rf_entity_motion_mapping *m=v.mappings+i;
        m->class_index=i<v.class_count?i:b->groups[i-v.class_count].class_index;
        if(m->class_index>=v.class_count)goto done;
        m->skeleton=s->class_indices[m->class_index];
        if(m->skeleton!=UINT32_MAX && m->skeleton>=v.model_count)goto done;
        m->weapon=i<v.class_count?-1:(int32_t)b->groups[i-v.class_count].weapon;
        for(j=0;j<23;++j)m->states[j]=-1;for(j=0;j<45;++j)m->actions[j]=-1;
    }
    for(i=0;i<v.model_count;++i) {
        capacity=0;
        for(j=0;j<v.mapping_count;++j)if(v.mappings[j].skeleton==i)
            capacity+=j<v.class_count?b->classes[j].count:b->groups[j-v.class_count].count;
        if(!capacity)continue;
        if(capacity>INT32_MAX)goto done;
        w.cache_capacity=(uint32_t)(capacity<800?capacity:800);
        scratch=(uint64_t)w.cache_capacity*sizeof(*w.cache)+capacity*(sizeof(*w.registry.identities)+sizeof(*w.registry.flags)+sizeof(*w.resources));
        peak=bytes+scratch;if(peak>budget)goto done;
        if(peak>v.peak_bytes)v.peak_bytes=(uint32_t)peak;
        w.cache=calloc(w.cache_capacity,sizeof(*w.cache));
        w.registry.identities=calloc((size_t)capacity,sizeof(*w.registry.identities));
        w.registry.flags=calloc((size_t)capacity,sizeof(*w.registry.flags));
        w.resources=calloc((size_t)capacity,sizeof(*w.resources));
        if(!w.cache || !w.registry.identities || !w.registry.flags || !w.resources){status=RF_IO;goto done;}
        w.registry.count=0;w.registry.capacity=(uint32_t)capacity;
        for(j=0;j<v.class_count;++j)if(s->class_indices[j]==i) {
            for(k=0;k<b->group_count;++k)if(b->groups[k].class_index==j) {
                status=catalog_bind(&w,NULL,b->groups+k,v.mappings+v.class_count+k);if(status)goto done;
            }
            status=catalog_bind(&w,b->classes+j,NULL,v.mappings+j);if(status)goto done;
        }
        bytes+=(uint64_t)w.registry.count*(sizeof(*v.models[i].items)+sizeof(*v.models[i].marker_names));peak=bytes+scratch;
        status=RF_RANGE;if(peak>budget)goto done;
        if(peak>v.peak_bytes)v.peak_bytes=(uint32_t)peak;
        v.models[i].items=malloc(w.registry.count*(sizeof(*v.models[i].items)+sizeof(*v.models[i].marker_names)));
        if(!v.models[i].items){status=RF_IO;goto done;}
        v.models[i].count=w.registry.count;
        v.models[i].marker_names=(rf_motion_marker_names *)(v.models[i].items+w.registry.count);
        memset(v.models[i].marker_names,0,w.registry.count*sizeof(*v.models[i].marker_names));
        memcpy(v.models[i].items,w.resources,w.registry.count*sizeof(*w.resources));
        v.resident_bytes=(uint32_t)bytes;
        free(w.cache);free(w.registry.identities);free(w.registry.flags);free(w.resources);memset(&w,0,sizeof(w));
    }
    status=catalog_markers(s,b,&v,budget);if(status)goto done;
    *result=v;return RF_OK;
done:
    free(w.cache);free(w.registry.identities);free(w.registry.flags);free(w.resources);
    rf_entity_motion_catalog_close(&v);return status;
}
int rf_entity_state_declaration_read(const void *text,uint32_t bytes,const char *class_name,
    const char *weapon,const char *state,rf_entity_state_declaration *result)
{
    lexer l={(const unsigned char*)text,bytes,0};char t[256];rf_entity_state_declaration value={0};
    int quoted,status,selected=0,found=0,group,matched=0;
    if(!text || !class_name || !*class_name || !weapon || !state || !*state || !result)return RF_RANGE;
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
                if(*t) {status=asset(value.motion,t);if(status)return status;}
                matched=1;
                while(metadata_tag(&l,"+Footstep Trigger:")) {
                    if(value.marker_count || sphere_number(&l,&value.marker_frames[0]) ||
                       sphere_number(&l,&value.marker_frames[1]))return RF_FORMAT;
                    value.marker_count=2;
                }
            }
        }
    }
    if(status!=RF_OK && status!=RF_NOT_FOUND)return status;
    if(!found || !matched)return RF_NOT_FOUND;
    *result=value;return RF_OK;
}
int rf_entity_state_motion_read(const void *text,uint32_t bytes,const char *class_name,
    const char *weapon,const char *state,char motion[64])
{
    rf_entity_state_declaration value;int status;if(!motion)return RF_RANGE;
    status=rf_entity_state_declaration_read(text,bytes,class_name,weapon,state,&value);
    if(!status)memcpy(motion,value.motion,64);return status;
}
int rf_entity_action_read(const void *text,uint32_t bytes,const char *class_name,
    const char *weapon,const char *action,rf_entity_action_declaration *result)
{
    lexer l={(const unsigned char*)text,bytes,0};char t[256];rf_entity_action_declaration value={0};
    int quoted,status,selected=0,found=0,group,matched=0;
    if(!text || !class_name || !*class_name || !weapon || !action || !*action || !result)return RF_RANGE;
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
        } else if(selected && same(t,"+Action:")) {
            int use;
            status=token(&l,t,&quoted);if(status || !quoted)return RF_FORMAT;use=group && same(t,action);
            status=token(&l,t,&quoted);if(status || !quoted)return RF_FORMAT;
            if(use){if(matched)return RF_FORMAT;if(*t){status=asset(value.motion,t);if(status)return status;}}
            status=token(&l,t,&quoted);if(status || !quoted)return RF_FORMAT;
            if(use){if(*t){status=asset(value.sound,t);if(status)return status;}matched=1;}
        }
    }
    if(status!=RF_OK && status!=RF_NOT_FOUND)return status;
    if(!found || !matched)return RF_NOT_FOUND;
    *result=value;return RF_OK;
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
static int class_assets_read(const void *text,uint32_t bytes,const char *class_name,const char *skin,rf_entity_assets *assets,int clutter)
{
    lexer l={(const unsigned char*)text,bytes,0};rf_entity_assets value={0};char t[256];
    int quoted,status,selected=0,found=0,skin_found;
    if(!text || !class_name || !*class_name || !skin || !assets)return RF_RANGE;
    skin_found=!*skin;
    while((status=token(&l,t,&quoted))==RF_OK) {
        if(quoted)continue;
        if((!clutter && same(t,"$Name:")) || (clutter && same(t,"$Class") && metadata_tag(&l,"Name:"))) {
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
            if(use && skin_found) {if(!clutter)return RF_FORMAT;use=0;}
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
int rf_clutter_flags_read(const void *text,uint32_t bytes,uint32_t *flags,uint32_t *consumed)
{
    static const char *const names[]={"collectable","collide_weapon","collide_object","is_screen",
        "shatters","has_alpha","is_switch","can_carry","is_clock"};
    lexer l={(const unsigned char *)text,bytes,0};char t[256];int status,quoted;uint32_t value=0,i;
    if(!text || !flags || !consumed)return RF_RANGE;
    status=token(&l,t,&quoted);if(status || quoted || strcmp(t,"("))return RF_FORMAT;
    for(;;) {
        status=token(&l,t,&quoted);if(status)return RF_FORMAT;
        if(!quoted && !strcmp(t,")"))break;
        if(!quoted)return RF_FORMAT;
        for(i=0;i<9;++i)if(same(t,names[i]))break;
        if(i==9)return RF_FORMAT;value|=1u<<i;
    }
    *flags=value;*consumed=l.at;return RF_OK;
}
int rf_entity_assets_read(const void *text,uint32_t bytes,const char *class_name,const char *skin,rf_entity_assets *assets)
{return class_assets_read(text,bytes,class_name,skin,assets,0);}
int rf_clutter_assets_read(const void *text,uint32_t bytes,const char *class_name,const char *skin,rf_entity_assets *assets)
{return class_assets_read(text,bytes,class_name,skin,assets,1);}
int rf_clutter_definition_read(const void *text,uint32_t bytes,const char *name,rf_clutter_definition *result)
{
    lexer l={(const unsigned char *)text,bytes,0};rf_clutter_definition v={0};
    char t[256],*destination;const char *extension;uint32_t mask=0,bit,used;
    int status,quoted,found=0;
    if(!text || !name || !*name || !result)return RF_RANGE;
    v.emitter_lifetime=v.radius=-1;v.screen_width=v.screen_height=64;
    while((status=token(&l,t,&quoted))==RF_OK) {
        if(quoted)continue;
        if(same(t,"$Class") && metadata_tag(&l,"Name:")) {
            if(found)break;
            if(token(&l,t,&quoted) || !quoted)return RF_FORMAT;
            found=same(t,name);if(found){if(strlen(t)>=64)return RF_RANGE;strcpy(v.name,t);}continue;
        }
        if(!found)continue;if(same(t,"$Skin:"))break;
        destination=NULL;bit=0;
        if(same(t,"$V3D") && metadata_tag(&l,"Filename:")){destination=v.model;bit=1;}
        else if(same(t,"$Material:")){destination=v.material;bit=2;}
        else if(same(t,"$Corpse") && metadata_tag(&l,"Class Name:")){destination=v.corpse;bit=16;}
        else if(same(t,"$Sound:")){destination=v.sound;bit=32;}
        else if(same(t,"$Explode") && metadata_tag(&l,"Anim:")){destination=v.explosion;bit=64;}
        else if(same(t,"$Glare:")){destination=v.glare;bit=128;}
        else if(same(t,"$Rod") && metadata_tag(&l,"Glare:")){destination=v.rod;bit=256;}
        else if(same(t,"$Emitter:")) {
            if(v.emitter_count==16)return RF_RANGE;
            if(metadata_string(&l,v.emitters[v.emitter_count++],64))return RF_FORMAT;continue;
        } else if(same(t,"$Life:")) {
            bit=4;if(sphere_number(&l,&v.life))return RF_FORMAT;
        } else if(same(t,"$Flags:")) {
            bit=8;status=rf_clutter_flags_read(l.text+l.at,l.size-l.at,&v.flags,&used);
            if(status)return status;l.at+=used;
        } else if(same(t,"$Emitter") && metadata_tag(&l,"Life:")) {
            bit=512;if(sphere_number(&l,&v.emitter_lifetime))return RF_FORMAT;
        } else if(same(t,"$Radius:")) {
            bit=1024;if(sphere_number(&l,&v.radius))return RF_FORMAT;
        } else if(same(t,"$Screen")) {
            if(metadata_tag(&l,"Width:")){bit=2048;if(metadata_integer(&l,&v.screen_width))return RF_FORMAT;}
            else if(metadata_tag(&l,"Height:")){bit=4096;if(metadata_integer(&l,&v.screen_height))return RF_FORMAT;}
        }
        if(bit && (mask&bit))return RF_FORMAT;mask|=bit;
        if(destination && metadata_string(&l,destination,64))return RF_FORMAT;
    }
    if(status!=RF_OK && status!=RF_NOT_FOUND)return status;
    if(!found)return RF_NOT_FOUND;if((mask&15)!=15)return RF_FORMAT;
    extension=strrchr(v.model,'.');v.model_kind=extension && same(extension,".vfx")?3:1;
    v.resource_fields=(mask>>5)&15;
    *result=v;return RF_OK;
}
int rf_clutter_definition_bind(const rf_clutter_definition *d,const rf_clutter_resource_names *names,
    int32_t *emitter_ids,uint32_t capacity,rf_clutter_class_binding *binding)
{
    rf_clutter_class_binding v={0,-1,-1,-1,-1,NULL};int32_t ids[16];uint32_t i;int status;
    if(!d || !names || !binding || d->emitter_count>16 || d->emitter_count>capacity ||
       (d->emitter_count && !emitter_ids) || (d->resource_fields&~15u) ||
       names->emitter_count>INT_MAX || names->glare_count>INT_MAX ||
       (names->emitter_count && !names->emitters) || (names->glare_count && !names->glares))return RF_RANGE;
    if(!memchr(d->material,0,64))return RF_FORMAT;
    for(i=0;i<d->emitter_count;++i)if(!memchr(d->emitters[i],0,64))return RF_FORMAT;
    for(i=0;i<4;++i)if(d->resource_fields&(1u<<i)) {
        const char *field=i==0?d->sound:i==1?d->explosion:i==2?d->glare:d->rod;
        if(!memchr(field,0,64))return RF_FORMAT;
    }
    if((d->resource_fields&RF_CLUTTER_HAS_SOUND) && !names->sounds)return RF_RANGE;
    if((d->resource_fields&RF_CLUTTER_HAS_EXPLOSION) && !names->vclips)return RF_RANGE;
    for(i=0;i<d->emitter_count;++i)ids[i]=rf_emitter_name_lookup(names->emitters,names->emitter_count,d->emitters[i]);
    v.material=rf_clutter_material_index(d->material);
    if(d->resource_fields&RF_CLUTTER_HAS_SOUND){status=rf_foley_find(names->sounds,d->sound,&v.sound);if(status)return status;}
    if(d->resource_fields&RF_CLUTTER_HAS_EXPLOSION)v.explosion=rf_vclip_name_lookup(names->vclips,d->explosion);
    if(d->resource_fields&RF_CLUTTER_HAS_GLARE)v.glare=rf_glare_name_lookup(names->glares,names->glare_count,d->glare);
    if(d->resource_fields&RF_CLUTTER_HAS_ROD)v.rod=rf_glare_name_lookup(names->glares,names->glare_count,d->rod);
    if(d->emitter_count){memcpy(emitter_ids,ids,d->emitter_count*4);v.emitters=emitter_ids;}
    *binding=v;return RF_OK;
}
static int clutter_catalog_scan(const void *text,uint32_t bytes,uint32_t kind,
    const char **names,uint32_t capacity,char **storage,uint32_t *remaining,
    uint32_t *count,uint32_t *string_bytes)
{
    lexer l={text,bytes,0};char t[256];uint32_t n=0,strings=0,length;int status,quoted,inside=0;
    while((status=token(&l,t,&quoted))==RF_OK) {
        if(quoted)continue;
        if(!inside) {
            if((kind==0 && same(t,"#particle") && metadata_tag(&l,"emitter types")) ||
               (kind==1 && same(t,"#Glares")) || (kind==2 && same(t,"#Vclips")))inside=1;
            continue;
        }
        if(same(t,"#End")){*count=n;*string_bytes=strings;return RF_OK;}
        if(!same(t,"$Name:"))continue;
        if(token(&l,t,&quoted) || !quoted)return RF_FORMAT;
        length=(uint32_t)strlen(t)+1;if(length>64 || n==64)return RF_RANGE;
        if(names) {
            if(n>=capacity || length>*remaining)return RF_RANGE;
            names[n]=*storage;memcpy(*storage,t,length);*storage+=length;*remaining-=length;
        }
        ++n;strings+=length;
    }
    if(status!=RF_NOT_FOUND)return status;
    return inside?RF_FORMAT:RF_NOT_FOUND;
}
int rf_clutter_catalogs_open(rf_vpp *tables,const rf_foley_owner *sounds,uint32_t budget,rf_clutter_catalogs *owner)
{
    static const char *const files[]={"emitters.tbl","effects.tbl","vclip.tbl"};
    rf_clutter_catalogs v={0};rf_vpp_entry entries[3];void *scratch=NULL;const char **pointers;
    char *strings;uint32_t counts[3],sizes[3],i,scratch_bytes=0,string_bytes=0,remaining,count,size;
    uint64_t bytes;int status;
    if(!tables || !owner || owner->storage || owner->names.emitters || owner->names.emitter_count ||
       owner->names.glares || owner->names.glare_count || owner->names.vclips || owner->names.sounds ||
       owner->allocated_bytes || owner->peak_bytes)return RF_RANGE;
    for(i=0;i<3;++i){status=rf_vpp_find(tables,files[i],entries+i);if(status)return status;
        if(entries[i].size>scratch_bytes)scratch_bytes=entries[i].size;}
    if(!scratch_bytes || (uint64_t)sizeof(v)+scratch_bytes>budget)return RF_RANGE;
    scratch=malloc(scratch_bytes);if(!scratch)return RF_IO;
    for(i=0;i<3;++i) {
        status=rf_vpp_read(tables,entries+i,0,scratch,entries[i].size);if(status)goto done;
        status=clutter_catalog_scan(scratch,entries[i].size,i,NULL,0,NULL,NULL,counts+i,sizes+i);if(status)goto done;
        string_bytes+=sizes[i];
    }
    bytes=(uint64_t)sizeof(v)+(counts[0]+counts[1]+64)*sizeof(*pointers)+string_bytes;
    if(bytes+scratch_bytes>budget || bytes+scratch_bytes>UINT32_MAX){status=RF_RANGE;goto done;}
    v.storage=malloc((size_t)bytes-sizeof(v));if(!v.storage){status=RF_IO;goto done;}
    v.allocated_bytes=(uint32_t)bytes;v.peak_bytes=(uint32_t)bytes+scratch_bytes;
    memset(v.storage,0,(size_t)bytes-sizeof(v));pointers=v.storage;
    v.names.emitters=counts[0]?pointers:NULL;v.names.emitter_count=counts[0];
    v.names.glares=counts[1]?pointers+counts[0]:NULL;v.names.glare_count=counts[1];
    v.names.vclips=pointers+counts[0]+counts[1];v.names.sounds=sounds;
    strings=(char *)(pointers+counts[0]+counts[1]+64);remaining=string_bytes;
    for(i=0;i<3;++i) {
        status=rf_vpp_read(tables,entries+i,0,scratch,entries[i].size);if(status)goto done;
        status=clutter_catalog_scan(scratch,entries[i].size,i,pointers,counts[i],&strings,&remaining,&count,&size);
        if(status)goto done;
        if(count!=counts[i] || size!=sizes[i]){status=RF_FORMAT;goto done;}pointers+=counts[i];
    }
    *owner=v;v.storage=NULL;status=RF_OK;
done:
    free(v.storage);free(scratch);return status;
}
void rf_clutter_catalogs_close(rf_clutter_catalogs *owner)
{if(owner){free(owner->storage);memset(owner,0,sizeof(*owner));}}

static int named_effect_block(const void *text,uint32_t bytes,const char *name,
    uint32_t *start,uint32_t *length,char authored_name[64])
{
    lexer l;char t[256];uint32_t body=0;int selected=0,status,quoted;
    if(!text || !bytes || !name || !*name || !start || !length || !authored_name)return RF_RANGE;
    l.text=text;l.size=bytes;l.at=0;
    for(;;) {
        uint32_t boundary=l.at;
        status=token(&l,t,&quoted);
        if(status==RF_NOT_FOUND || (!status && !quoted && (same(t,"$Name:") || same(t,"#End")))) {
            if(selected){*start=body;*length=boundary-body;return RF_OK;}
            if(status==RF_NOT_FOUND || same(t,"#End"))return RF_NOT_FOUND;
            status=token(&l,t,&quoted);if(status || !quoted)return RF_FORMAT;
            selected=same(t,name);body=l.at;
            if(selected){if(strlen(t)>=64)return RF_RANGE;strcpy(authored_name,t);}
        } else if(status)return status;
    }
}
int rf_entity_corpse_config_read(const void *text,uint32_t bytes,const char *name,
    rf_entity_corpse_config *result)
{
    rf_entity_corpse_config value={0};uint32_t start,length,seen=0;
    char authored[64],t[256];lexer l;int status,quoted;
    if(!result)return RF_RANGE;
    status=named_effect_block(text,bytes,name,&start,&length,authored);if(status)return status;
    l=(lexer){(const unsigned char *)text+start,length,0};value.emitter_lifetime=-1.0f;
    while((status=token(&l,t,&quoted))==RF_OK) {
        if(quoted)continue;
        if(same(t,"$Body") && metadata_tag(&l,"Temperature ( F ) :")) {
            if((seen&8) || sphere_number(&l,&value.body_temperature))return RF_FORMAT;seen|=8;continue;
        }
        if(!same(t,"$Corpse"))continue;
        if(metadata_tag(&l,"V3D Filename:")) {
            if(seen&7)return RF_FORMAT;
            if(metadata_string(&l,value.model,sizeof(value.model)))return RF_FORMAT;seen|=1;
        } else if(metadata_tag(&l,"Emitter:")) {
            if(seen&6)return RF_FORMAT;
            if(metadata_string(&l,value.emitter,sizeof(value.emitter)))return RF_FORMAT;seen|=2;
        } else if(metadata_tag(&l,"Emitter Lifetime:")) {
            if(!(seen&2) || (seen&4) || sphere_number(&l,&value.emitter_lifetime))return RF_FORMAT;seen|=4;
        } else return RF_FORMAT;
    }
    if(status!=RF_NOT_FOUND)return status;*result=value;return RF_OK;
}

int rf_emitter_definition_read(const void *text,uint32_t bytes,const char *name,
    rf_particle_definition *result)
{
    uint32_t start,length;char authored[64];int status;
    if(!result)return RF_RANGE;
    status=named_effect_block(text,bytes,name,&start,&length,authored);if(status)return status;
    return rf_particle_definition_read((const unsigned char *)text+start,length,result);
}
int rf_emitter_definition_load(rf_vpp *tables,const char *name,uint32_t scratch_budget,
    rf_particle_definition *result)
{
    rf_vpp_entry entry;void *text;int status;
    if(!tables || !name || !*name || !result)return RF_RANGE;
    status=rf_vpp_find(tables,"emitters.tbl",&entry);if(status)return status;
    if(!entry.size || entry.size>scratch_budget)return RF_RANGE;
    text=malloc(entry.size);if(!text)return RF_RANGE;
    status=rf_vpp_read(tables,&entry,0,text,entry.size);
    if(!status)status=rf_emitter_definition_read(text,entry.size,name,result);
    free(text);return status;
}

/* Bounded particle metadata; runtime resources and direction normalization are
 * deliberately separate from authored storage. Fields follow 497590. */
int rf_particle_definition_read(const void *text,uint32_t bytes,rf_particle_definition *result)
{
    static const char *names[]={"pos","dir","dirrand","minvel","maxvel","spawnradius",
        "minspawndelay","maxspawndelay","emitterflags","initiallyon","alternatestates",
        "ontime","ontimevariance","offtime","offtimevariance","minlifesecs","maxlifesecs",
        "minpradius","maxpradius","growthrate","acceleration","gravityscale","bitmap",
        "particlecolor","particlecolordest","particleflags","bounciness","stickiness",
        "swirliness","damagefactor","agepcttofinishvbm"};
    rf_particle_definition v={0};const unsigned char *s=text;
    float *numbers[31]={v.position,v.direction,&v.direction_random,&v.min_velocity,&v.max_velocity,
        &v.spawn_radius,&v.min_spawn_delay,&v.max_spawn_delay,NULL,NULL,NULL,
        &v.cycle.on_time,&v.cycle.on_variance,&v.cycle.off_time,&v.cycle.off_variance,
        &v.min_life,&v.max_life,&v.min_radius,&v.max_radius,&v.growth,&v.acceleration,&v.gravity_scale,
        NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,&v.age_to_finish_vbm};
    char emitter[256]={0},particle[256]={0};unsigned seen=0,present=0,initial=0,alternate=0;
    int packed[4]={0};uint32_t at=0;
    if(!text || !bytes || !result)return RF_RANGE;
    while(at<bytes) {
        char label[64],value[256],t[256];uint32_t n=0,k=0;unsigned field,i;int quote=0,q,status;
        lexer l;
        while(at<bytes && s[at]<=32) {if(!s[at])return RF_FORMAT;++at;}
        if(at==bytes)break;
        if(at+1<bytes && s[at]=='/' && s[at+1]=='/') {
            while(at<bytes && s[at]!='\n')++at;continue;
        }
        if(s[at++]!='$')return RF_FORMAT;
        while(at<bytes && s[at]!=':') {
            unsigned c=s[at++];if(!c || c=='\r' || c=='\n')return RF_FORMAT;
            if(c<=32 || c=='_')continue;if(c>='A' && c<='Z')c+=32;
            if(n>=sizeof(label)-1)return RF_RANGE;label[n++]=(char)c;
        }
        if(at==bytes)return RF_FORMAT;++at;label[n]=0;
        for(field=0;field<31 && strcmp(label,names[field]);++field){}
        if(field==31 || (seen&(1u<<field)))return RF_FORMAT;
        seen|=1u<<field;
        while(at<bytes && s[at]!='\r' && s[at]!='\n') {
            unsigned c=s[at++];if(!c)return RF_FORMAT;
            if(!quote && c=='/' && at<bytes && s[at]=='/') {
                while(at<bytes && s[at]!='\n')++at;break;
            }
            if(c=='"')quote=!quote;
            if(!quote && strchr("<>{},",c))c=' ';
            if(k==255)return RF_RANGE;value[k++]=(char)c;
        }
        if(quote)return RF_FORMAT;value[k]=0;l.text=(const unsigned char *)value;l.size=k;l.at=0;
        if(numbers[field]) {
            unsigned count=field<2?3:1;
            for(i=0;i<count;++i)if(sphere_number(&l,numbers[field]+i))return RF_FORMAT;
        } else if(field==8 || field==22 || field==25) {
            status=token(&l,t,&q);if(status || !q)return RF_FORMAT;
            if(field==22) {if(strlen(t)>=sizeof(v.bitmap))return RF_RANGE;strcpy(v.bitmap,t);}
            else strcpy(field==8?emitter:particle,t);
        } else if(field==9 || field==10) {
            unsigned b;status=token(&l,t,&q);if(status || q)return RF_FORMAT;
            if(same(t,"yes") || same(t,"true") || same(t,"1"))b=1;
            else if(same(t,"no") || same(t,"false") || same(t,"0"))b=0;
            else return RF_FORMAT;
            if(field==9)initial=b;else alternate=b;
        } else {
            unsigned count=field==23 || field==24?4:1;
            for(i=0;i<count;++i) {
                uint32_t mag=0,j=0,limit;int negative=0,integer;
                status=token(&l,t,&q);if(status || q)return RF_FORMAT;
                if(t[j]=='+' || t[j]=='-')negative=t[j++]=='-';
                if(!t[j])return RF_FORMAT;limit=negative?0x80000000u:0x7fffffffu;
                for(;t[j];++j) {
                    unsigned digit=(unsigned char)t[j]-'0';
                    if(digit>9 || mag>(limit-digit)/10)return RF_FORMAT;mag=mag*10+digit;
                }
                integer=negative?(mag==0x80000000u?(-2147483647-1):-(int)mag):(int)mag;
                if(count==4) {if(integer<0 || integer>255)return RF_FORMAT;
                    (field==23?v.color:v.color_destination)[i]=(uint8_t)integer;}
                else {packed[field-26]=integer;present|=1u<<(field-26);}
            }
        }
        if(token(&l,t,&q)!=RF_NOT_FOUND)return RF_FORMAT;
    }
    /* Required fields: all except spawn delays, cycle times, destination,
     * numeric nibble options and the optional animation completion fraction. */
    {
        const unsigned optional=(3u<<6)|(15u<<11)|(1u<<24)|(31u<<26);
        const unsigned required=0x7fffffffu & ~optional;
        if((seen&required)!=required)return RF_FORMAT;
    }
    if(((seen>>6)&3u)!=0 && ((seen>>6)&3u)!=3)return RF_FORMAT;
    if(alternate && (seen&(15u<<11))!=(15u<<11))return RF_FORMAT;
    if(!alternate && (seen&(15u<<11)))return RF_FORMAT;
    if(!(seen&(1u<<24)))memcpy(v.color_destination,v.color,4);
    v.has_age_to_finish_vbm=(seen>>30)&1u;
    rf_particle_flags_read(emitter,particle,&v.flags);
    rf_particle_flags_pack(&v.flags,present,packed);
    rf_particle_cycle_read(&v.flags,initial,alternate,&v.cycle,&v.cycle);
    *result=v;return RF_OK;
}

int rf_vclip_definition_read(const void *text,uint32_t bytes,const char *name,rf_vclip_definition *result)
{
    rf_vclip_definition v={0};uint32_t start,length,seen=0;lexer l;char t[256];int status,q;
    static const char *labels[]={"$flags:","$damage:","$vbmfilename:","$vbmglow:","$explosionname:","$vfxfilename:","$vfxradius:","$foleysound:","$particlecount:"};
    if(!result)return RF_RANGE;
    status=named_effect_block(text,bytes,name,&start,&length,v.name);if(status)return status;
    v.vfx_radius=20;l.text=(const unsigned char *)text+start;l.size=length;l.at=0;
    while((status=token(&l,t,&q))==RF_OK) {
        char label[64]={0};uint32_t n=0,field;
        if(q || t[0]!='$')return RF_FORMAT;
        for(;;) {
            unsigned i;
            for(i=0;t[i];++i) {unsigned c=(unsigned char)t[i];if(c=='_')continue;if(c>='A' && c<='Z')c+=32;
                if(n==63)return RF_RANGE;label[n++]=(char)c;}
            if(n && label[n-1]==':')break;
            if(token(&l,t,&q) || q)return RF_FORMAT;
        }
        for(field=0;field<9 && strcmp(label,labels[field]);++field){}
        if(field==9 || (seen&(1u<<field)))return RF_FORMAT;seen|=1u<<field;
        if(field==0) {
            static const char *flags[]={"liquid_surface","radius_in_multiples","no_z_check","code_explode"};
            if(token(&l,t,&q) || q || strcmp(t,"("))return RF_FORMAT;
            for(;;) {
                unsigned bit;if(token(&l,t,&q))return RF_FORMAT;
                if(!q && !strcmp(t,")"))break;
                if(!q && !strcmp(t,","))continue;
                if(!q)return RF_FORMAT;
                for(bit=0;bit<4 && !same(t,flags[bit]);++bit){}
                if(bit==4)return RF_FORMAT;v.flags|=1u<<bit;
            }
        } else if(field==1 || field==6) {
            if(sphere_number(&l,field==1?&v.damage:&v.vfx_radius))return RF_FORMAT;
        } else if(field==3) {
            if(!(seen&(1u<<2)) || token(&l,t,&q) || q)return RF_FORMAT;
            if(same(t,"yes") || same(t,"true") || !strcmp(t,"1"))v.glow=1;
            else if(!same(t,"no") && !same(t,"false") && strcmp(t,"0"))return RF_FORMAT;
        } else if(field==8) {
            unsigned at=0,negative=0;uint32_t mag=0,limit;
            if(token(&l,t,&q) || q)return RF_FORMAT;
            if(t[at]=='+' || t[at]=='-')negative=t[at++]=='-';
            if(!t[at])return RF_FORMAT;limit=negative?0x80000000u:0x7fffffffu;
            for(;t[at];++at) {unsigned digit=(unsigned char)t[at]-'0';
                if(digit>9 || mag>(limit-digit)/10)return RF_FORMAT;mag=mag*10+digit;}
            v.particle_count=negative?(mag==0x80000000u?(-2147483647-1):-(int32_t)mag):(int32_t)mag;
            v.has_particle=1;
            status=rf_particle_definition_read(l.text+l.at,l.size-l.at,&v.particle);if(status)return status;
            l.at=l.size;
        } else {
            char *dest=field==2?v.vbm:field==4?v.explosion:field==5?v.vfx:v.foley;
            unsigned capacity=field==4?32:64;
            if(token(&l,t,&q) || !q)return RF_FORMAT;
            if(strlen(t)>=capacity)return RF_RANGE;strcpy(dest,t);if(field==7)v.has_foley=1;
        }
    }
    if(status!=RF_NOT_FOUND)return status;
    *result=v;return RF_OK;
}
int rf_vclip_definition_load(rf_vpp *tables,const char *name,uint32_t scratch_budget,rf_vclip_definition *result)
{
    rf_vpp_entry entry;void *text;int status;
    if(!tables || !name || !*name || !result)return RF_RANGE;
    status=rf_vpp_find(tables,"vclip.tbl",&entry);if(status)return status;
    if(!entry.size || entry.size>scratch_budget)return RF_RANGE;
    text=malloc(entry.size);if(!text)return RF_RANGE;
    status=rf_vpp_read(tables,&entry,0,text,entry.size);
    if(!status)status=rf_vclip_definition_read(text,entry.size,name,result);
    free(text);return status;
}

static int effect_label(lexer *l,const char *label,int required)
{
    lexer saved=*l;char t[256];int q,status=token(l,t,&q);
    if(!status && !q) {
        char *colon=strchr(t,':');
        if(colon){l->at-=(uint32_t)strlen(colon+1);colon[1]=0;}
    }
    if(!status && !q && same(t,label))return 1;
    *l=saved;
    if(status && status!=RF_NOT_FOUND)return status;
    return required?RF_FORMAT:0;
}
static int effect_string(lexer *l,char *out,unsigned capacity)
{
    char t[256];int q,status=token(l,t,&q);
    if(status || !q)return RF_FORMAT;if(strlen(t)>=capacity)return RF_RANGE;
    strcpy(out,t);return RF_OK;
}
static int effect_optional_float(lexer *l,const char *label,float fallback,float *out)
{
    int status=effect_label(l,label,0);if(status<0)return status;
    if(status)return sphere_number(l,out);*out=fallback;return RF_OK;
}
int rf_explosion_recipe_read(const void *text,uint32_t bytes,const char *name,rf_explosion_recipe *result)
{
    rf_explosion_recipe v={0};uint32_t start,length;char authored[64],t[256];lexer l;int q,status;
    if(!result)return RF_RANGE;
    status=named_effect_block(text,bytes,name,&start,&length,authored);if(status)return status;
    if(strlen(authored)>=sizeof(v.name))return RF_RANGE;strcpy(v.name,authored);
    l.text=(const unsigned char *)text+start;l.size=length;l.at=0;
    if(effect_label(&l,"$Flags:",1)!=1 || token(&l,t,&q) || q || strcmp(t,"("))return RF_FORMAT;
    for(;;) {
        if(token(&l,t,&q))return RF_FORMAT;
        if(!q && !strcmp(t,")"))break;
        if(!q || !same(t,"no_trails"))return RF_FORMAT;v.flags|=1;
    }
    if(effect_label(&l,"$Explosion_Play_Time:",1)!=1 || sphere_number(&l,&v.play_time))return RF_FORMAT;
    while((status=effect_label(&l,"+Central_Emitter:",0))==1) {
        rf_explosion_central *c;
        if(v.central_count==6)return RF_RANGE;c=v.central+v.central_count;
        status=effect_string(&l,c->emitter,sizeof(c->emitter));if(status)return status;
        if(effect_label(&l,"+process_per_frame:",1)!=1 || token(&l,t,&q) || q)return RF_FORMAT;
        if(same(t,"yes") || same(t,"true") || !strcmp(t,"1"))c->process_per_frame=1;
        else if(!same(t,"no") && !same(t,"false") && strcmp(t,"0"))return RF_FORMAT;
        status=effect_optional_float(&l,"+min_size_before_use:",0,&c->min_size);if(status)return status;
        status=effect_optional_float(&l,"+play_time_factor:",FLT_MAX,&c->play_factor);if(status)return status;
        status=effect_optional_float(&l,"+rand_pos_factor:",0,&v.central_random);if(status)return status;
        ++v.central_count;
    }
    if(status<0)return status;
    status=effect_label(&l,"$Sparks_Emitter:",0);if(status<0)return status;
    if(status) {
        uint32_t mag=0,limit;unsigned at=0,negative=0;v.present|=1;
        status=effect_string(&l,v.sparks,sizeof(v.sparks));if(status)return status;
        if(effect_label(&l,"+number:",1)!=1 || token(&l,t,&q) || q)return RF_FORMAT;
        if(t[at]=='+' || t[at]=='-')negative=t[at++]=='-';
        if(!t[at])return RF_FORMAT;limit=negative?0x80000000u:0x7fffffffu;
        for(;t[at];++at) {unsigned digit=(unsigned char)t[at]-'0';if(digit>9 || mag>(limit-digit)/10)return RF_FORMAT;mag=mag*10+digit;}
        v.sparks_count=negative?(mag==0x80000000u?(-2147483647-1):-(int32_t)mag):(int32_t)mag;
    }
    status=effect_label(&l,"$Trail_Head_Emitter:",0);if(status<0)return status;
    if(status) {
        v.present|=2;status=effect_string(&l,v.head,sizeof(v.head));if(status)return status;
        if(effect_label(&l,"+time_to_emit_head_parts:",1)!=1 || sphere_number(&l,&v.head_time))return RF_FORMAT;
        status=effect_optional_float(&l,"+rand_pos_factor:",0,&v.head_random);if(status)return status;
    }
    status=effect_label(&l,"$Trail_Tail_Emitter:",0);if(status<0)return status;
    if(status) {v.present|=4;status=effect_string(&l,v.tail,sizeof(v.tail));if(status)return status;}
    if(token(&l,t,&q)!=RF_NOT_FOUND)return RF_FORMAT;
    *result=v;return RF_OK;
}
int rf_explosion_recipe_load(rf_vpp *tables,const char *name,uint32_t scratch_budget,rf_explosion_recipe *result)
{
    rf_vpp_entry entry;void *text;int status;
    if(!tables || !name || !*name || !result)return RF_RANGE;
    status=rf_vpp_find(tables,"explosion.tbl",&entry);if(status)return status;
    if(!entry.size || entry.size>scratch_budget)return RF_RANGE;
    text=malloc(entry.size);if(!text)return RF_RANGE;
    status=rf_vpp_read(tables,&entry,0,text,entry.size);
    if(!status)status=rf_explosion_recipe_read(text,entry.size,name,result);
    free(text);return status;
}

int rf_explosion_definition_resolve(const rf_explosion_recipe *recipe,const void *emitters,
    uint32_t bytes,rf_explosion_definition *result)
{
    rf_explosion_definition v={0};uint32_t slot;int status;
    if(!recipe || !emitters || !bytes || !result || recipe->central_count>6 || (recipe->present&~7u))return RF_RANGE;
    v.recipe=*recipe;v.resident_bytes=v.peak_bytes=sizeof(v);
    for(slot=0;slot<9;++slot) {
        const char *name;
        if(slot<6) {if(slot>=recipe->central_count)continue;name=recipe->central[slot].emitter;}
        else {if(!(recipe->present&(1u<<(slot-6))))continue;name=slot==6?recipe->sparks:slot==7?recipe->head:recipe->tail;}
        if(!memchr(name,0,64))return RF_RANGE;
        status=*name?rf_emitter_definition_read(emitters,bytes,name,v.emitters+slot):RF_NOT_FOUND;
        if(status==RF_NOT_FOUND && slot>=6)continue;if(status)return status;
        status=rf_particle_definition_prepare(v.emitters+slot,v.emitters+slot);if(status)return status;
        v.resolved|=1u<<slot;
    }
    *result=v;return RF_OK;
}
int rf_explosion_definition_load(rf_vpp *tables,const char *name,uint32_t budget,rf_explosion_definition *result)
{
    rf_vpp_entry recipes,emitters;rf_explosion_definition v={0};rf_explosion_recipe recipe={0};
    uint32_t scratch_size;void *scratch;int status;
    if(!tables || !name || !*name || !result || budget<sizeof(v))return RF_RANGE;
    status=rf_vpp_find(tables,"explosion.tbl",&recipes);if(status)return status;
    status=rf_vpp_find(tables,"emitters.tbl",&emitters);if(status)return status;
    scratch_size=recipes.size>emitters.size?recipes.size:emitters.size;
    if(!recipes.size || !emitters.size || scratch_size>budget-sizeof(v))return RF_RANGE;
    scratch=malloc(scratch_size);if(!scratch)return RF_RANGE;
    status=rf_vpp_read(tables,&recipes,0,scratch,recipes.size);
    if(!status)status=rf_explosion_recipe_read(scratch,recipes.size,name,&recipe);
    if(!status)status=rf_vpp_read(tables,&emitters,0,scratch,emitters.size);
    if(!status)status=rf_explosion_definition_resolve(&recipe,scratch,emitters.size,&v);
    free(scratch);if(status)return status;
    v.peak_bytes=v.resident_bytes+scratch_size;*result=v;return RF_OK;
}

void rf_entity_seeds_close(rf_entity_seeds *seeds)
{
    if(!seeds)return;
    rf_level_owned_entities_close(&seeds->records);
    free(seeds->items);free(seeds->classes);memset(seeds,0,sizeof(*seeds));
}
int rf_entity_class_death_bones(const rf_entity_seeds *seeds,const rf_entity_skeletons *skeletons,
    uint32_t cls,int32_t out[3])
{
    const rf_entity_seed_class *definition;uint32_t index;
    if(!seeds || !out || !seeds->classes || cls>=seeds->class_count)return RF_RANGE;
    definition=seeds->classes+cls;
    if(definition->model_kind!=2 || !(definition->physics.flags&0x20000u)) {
        out[0]=out[1]=out[2]=-1;return RF_OK;
    }
    if(!skeletons || !skeletons->class_indices || !skeletons->items || cls>=skeletons->class_count)return RF_RANGE;
    index=skeletons->class_indices[cls];if(index>=skeletons->count)return RF_RANGE;
    return rf_model_death_bones(skeletons->items[index].bones,skeletons->items[index].count,out);
}
int rf_entity_model_kind(const char *model,uint32_t *kind)
{
    const char *extension=NULL;uint32_t i;
    if(!model || !kind)return RF_RANGE;
    for(i=0;i<64 && model[i];++i)if(model[i]=='.')extension=model+i;
    if(i==64)return RF_RANGE;
    *kind=extension && same(extension,".vfx")?3u:extension && same(extension,".vcm")?2u:1u;
    return RF_OK;
}
void rf_entity_skeletons_close(rf_entity_skeletons *s)
{
    uint32_t i;if(!s)return;
    for(i=0;i<s->count;++i)free(s->items[i].bones);
    free(s->items);free(s->class_indices);memset(s,0,sizeof(*s));
}
int rf_entity_skeletons_open(const rf_entity_seeds *seeds,rf_vpp *meshes,uint32_t budget,rf_entity_skeletons *result)
{
    rf_entity_skeletons v={0};rf_model_file *model=NULL;void *payload=NULL;
    uint64_t bytes,peak;uint32_t i,j,k,total;int status=RF_OK;char compiled[64];
    if(!seeds || !meshes || !result || result->items || result->class_indices || result->count ||
       result->class_count || result->resident_bytes || result->peak_bytes ||
       (seeds->class_count && !seeds->classes))return RF_RANGE;
    v.class_count=seeds->class_count;
    bytes=sizeof(v)+(uint64_t)v.class_count*(sizeof(*v.items)+sizeof(*v.class_indices));
    if(bytes>budget)return RF_RANGE;
    v.resident_bytes=v.peak_bytes=(uint32_t)bytes;
    if(v.class_count) {
        v.items=calloc(v.class_count,sizeof(*v.items));v.class_indices=malloc(v.class_count*sizeof(*v.class_indices));
        if(!v.items || !v.class_indices){status=RF_RANGE;goto done;}
    }
    for(i=0;i<v.class_count;++i) {
        const rf_model_section *section=NULL;rf_entity_skeleton *item;
        v.class_indices[i]=UINT32_MAX;
        if(seeds->classes[i].model_kind!=2)continue;
        status=rf_entity_skeletal_filename(seeds->classes[i].model,compiled);if(status)goto done;
        for(j=0;j<v.count;++j)if(same(v.items[j].model,compiled))break;
        v.class_indices[i]=j;if(j<v.count)continue;
        if(bytes+sizeof(*model)>budget){status=RF_RANGE;goto done;}
        model=malloc(sizeof(*model));if(!model){status=RF_RANGE;goto done;}
        status=rf_model_file_open(model,meshes,compiled);if(status)goto done;
        for(k=0;k<model->section_count;++k)if(model->sections[k].type==0x424f4e45) {
            if(section){status=RF_FORMAT;goto done;}section=model->sections+k;
        }
        if(!section || section->size<4 || (section->size-4)%56){status=RF_FORMAT;goto done;}
        total=(section->size-4)/56;if(!total || total>256){status=RF_RANGE;goto done;}
        peak=bytes+(uint64_t)total*sizeof(rf_model_bone)+sizeof(*model)+section->size;
        if(peak>budget){status=RF_RANGE;goto done;}
        if(peak>v.peak_bytes)v.peak_bytes=(uint32_t)peak;
        item=v.items+v.count++;memcpy(item->model,compiled,strlen(compiled)+1);
        item->bones=calloc(total,sizeof(*item->bones));payload=malloc(section->size);
        if(!item->bones || !payload){status=RF_RANGE;goto done;}
        status=rf_vpp_read(meshes,&model->entry,section->offset,payload,section->size);
        if(!status)status=rf_model_decode_bones(payload,section->size,item->bones,total,&item->count);
        if(status)goto done;
        bytes+=(uint64_t)total*sizeof(*item->bones);v.resident_bytes=(uint32_t)bytes;
        free(payload);payload=NULL;free(model);model=NULL;
    }
    *result=v;return RF_OK;
done:
    free(payload);free(model);rf_entity_skeletons_close(&v);return status;
}
void rf_entity_playback_resources_close(rf_entity_playback_resources *r)
{
    if(!r)return;free(r->models);free(r->resources);free(r->cache_ids);memset(r,0,sizeof(*r));
}
int rf_entity_playback_resources_open(const rf_entity_motion_catalog *catalog,uint32_t budget,rf_entity_playback_resources *result)
{
    rf_entity_playback_resources v={0};rf_motion_cache_record *cache=NULL;
    uint64_t total=0,bytes,peak;uint32_t i,j,at=0,capacity;int status=RF_RANGE;
    if(!catalog || !result || result->models || result->resources || result->cache_ids || result->model_count ||
       result->resource_count || result->cache_count || result->resident_bytes || result->peak_bytes ||
       (catalog->model_count && !catalog->models))return RF_RANGE;
    for(i=0;i<catalog->model_count;++i) {
        if(catalog->models[i].count && !catalog->models[i].items)return RF_RANGE;
        total+=catalog->models[i].count;
    }
    if(total>UINT32_MAX)return RF_RANGE;capacity=total>800?800:(uint32_t)total;
    bytes=sizeof(v)+(uint64_t)catalog->model_count*sizeof(*v.models)+total*(sizeof(*v.resources)+sizeof(*v.cache_ids));
    peak=bytes+(uint64_t)capacity*sizeof(*cache);if(peak>budget)return RF_RANGE;
    v.model_count=catalog->model_count;v.resource_count=(uint32_t)total;v.resident_bytes=(uint32_t)bytes;v.peak_bytes=(uint32_t)peak;
    if(v.model_count)v.models=calloc(v.model_count,sizeof(*v.models));
    if(total){v.resources=calloc((size_t)total,sizeof(*v.resources));v.cache_ids=calloc((size_t)total,sizeof(*v.cache_ids));cache=calloc(capacity,sizeof(*cache));}
    if((v.model_count && !v.models) || (total && (!v.resources || !v.cache_ids || !cache)))goto done;
    for(i=0;i<v.model_count;++i) {
        rf_entity_playback_model *m=v.models+i;m->count=catalog->models[i].count;
        if(m->count){m->resources=v.resources+at;m->cache_ids=v.cache_ids+at;}
        for(j=0;j<m->count;++j,++at) {
            const rf_entity_model_motion *source=catalog->models[i].items+j;uint32_t id;
            status=rf_motion_cache_acquire(cache,capacity,source->identity,&id);if(status)goto done;
            v.cache_ids[at]=id;if(id>=v.cache_count)v.cache_count=id+1;
            v.resources[at].comparison=source->comparison;v.resources[at].looping=source->looping;
            memcpy(v.resources[at].markers,source->markers,sizeof(source->markers));
        }
    }
    free(cache);*result=v;return RF_OK;
done:
    free(cache);rf_entity_playback_resources_close(&v);return status;
}
int rf_entity_playback_cache_references(const rf_entity_playback_resources *r,uint32_t cache_id,uint32_t *references)
{
    uint64_t total=0;uint32_t i;
    if(!r || !references || cache_id>=r->cache_count || (r->resource_count && (!r->resources || !r->cache_ids)))return RF_RANGE;
    for(i=0;i<r->resource_count;++i)if(r->cache_ids[i]==cache_id) {
        if(r->resources[i].references<0)return RF_RANGE;
        total+=(uint32_t)r->resources[i].references;if(total>UINT32_MAX)return RF_RANGE;
    }
    *references=(uint32_t)total;return RF_OK;
}
void rf_entity_appearances_close(rf_entity_appearances *a)
{
    uint32_t i;if(!a)return;if(a->items)for(i=0;i<a->count;++i)free(a->items[i].textures);
    free(a->items);free(a->actor_indices);memset(a,0,sizeof(*a));
}
int rf_entity_appearances_open(const rf_entity_seeds *seeds,const rf_entity_skeletons *skeletons,
    rf_vpp *tables,uint32_t budget,rf_entity_appearances *result)
{
    rf_entity_appearances v={0};rf_vpp_entry entry;void *text=NULL;uint64_t bytes;
    uint32_t i,j,k;int status;rf_entity_assets selected;char model[64];
    if(!seeds || !skeletons || !tables || !result || result->items || result->actor_indices || result->count ||
       result->actor_count || result->resident_bytes || result->peak_bytes || seeds->class_count!=skeletons->class_count ||
       (seeds->records.count && (!seeds->items || !seeds->records.items)) || (skeletons->class_count && !skeletons->class_indices))return RF_RANGE;
    status=rf_vpp_find(tables,"entity.tbl",&entry);if(status)return status;
    v.actor_count=seeds->records.count;
    bytes=sizeof(v)+(uint64_t)v.actor_count*(sizeof(*v.items)+sizeof(*v.actor_indices));
    if(bytes+entry.size>budget)return RF_RANGE;
    if(v.actor_count){v.items=calloc(v.actor_count,sizeof(*v.items));v.actor_indices=calloc(v.actor_count,sizeof(*v.actor_indices));}
    text=malloc(entry.size);if(!text || (v.actor_count && (!v.items || !v.actor_indices))){status=RF_RANGE;goto fail;}
    status=rf_vpp_read(tables,&entry,0,text,entry.size);if(status)goto fail;
    for(i=0;i<v.actor_count;++i) {
        uint32_t c=seeds->items[i].class_index,index;const rf_level_entity *record=&seeds->records.items[i].record;
        if(c>=skeletons->class_count){status=RF_RANGE;goto fail;}index=skeletons->class_indices[c];v.actor_indices[i]=UINT32_MAX;
        if(index==UINT32_MAX)continue;
        if(index>=skeletons->count || !skeletons->items){status=RF_RANGE;goto fail;}
        status=rf_entity_assets_read(text,entry.size,record->class_name,record->skin,&selected);if(status)goto fail;
        status=rf_entity_skeletal_filename(selected.model,model);if(status)goto fail;
        if(!same(model,skeletons->items[index].model)){status=RF_FORMAT;goto fail;}
        for(j=0;j<v.count;++j) {
            if(v.items[j].skeleton!=index || v.items[j].texture_count!=selected.texture_count)continue;
            for(k=0;k<selected.texture_count && same(v.items[j].textures[k],selected.textures[k]);++k){}
            if(k==selected.texture_count)break;
        }
        if(j==v.count) {
            rf_entity_appearance *a=v.items+v.count;uint32_t names=selected.texture_count*64;
            if(bytes+entry.size+names>budget){status=RF_RANGE;goto fail;}
            a->skeleton=index;a->texture_count=selected.texture_count;
            if(names){a->textures=malloc(names);if(!a->textures){status=RF_RANGE;goto fail;}memcpy(a->textures,selected.textures,names);}
            bytes+=names;++v.count;
        }
        v.actor_indices[i]=j;
    }
    v.resident_bytes=(uint32_t)bytes;v.peak_bytes=(uint32_t)(bytes+entry.size);free(text);*result=v;return RF_OK;
fail:
    free(text);rf_entity_appearances_close(&v);return status;
}
void rf_entity_render_models_close(rf_entity_render_models *models)
{
    uint32_t i,j;if(!models)return;
    if(models->items)for(i=0;i<models->count;++i) {
        rf_entity_render_model *m=models->items+i;
        if(m->lods)for(j=0;j<m->file.lod_count;++j)rf_model_geometry_close(m->lods+j);
        free(m->lods);free(m->stored);
    }
    free(models->items);memset(models,0,sizeof(*models));
}
int rf_entity_render_models_open(const rf_entity_skeletons *skeletons,rf_vpp *meshes,uint32_t budget,rf_entity_render_models *result)
{
    rf_entity_render_models v={0};uint64_t bytes;uint32_t i,j;int status=RF_RANGE;
    if(!skeletons || !meshes || !result || result->items || result->count || result->resident_bytes ||
       (skeletons->count && !skeletons->items))return RF_RANGE;
    bytes=sizeof(v)+(uint64_t)skeletons->count*sizeof(*v.items);if(bytes>budget)return RF_RANGE;
    v.count=skeletons->count;if(v.count){v.items=calloc(v.count,sizeof(*v.items));if(!v.items)return RF_RANGE;}
    for(i=0;i<v.count;++i) {
        rf_entity_render_model *m=v.items+i;const rf_entity_skeleton *s=skeletons->items+i;
        if(!s->bones || !s->count || s->count>50){status=RF_RANGE;goto fail;}
        status=rf_model_file_open(&m->file,meshes,s->model);if(status)goto fail;
        for(j=0;j<=8;++j) {
            rf_model_collision_sphere sphere;
            status=rf_model_file_collision_sphere(&m->file,j,&sphere);
            if(status==RF_NOT_FOUND){status=RF_OK;break;}if(status)goto fail;
            if(j==8){status=RF_RANGE;goto fail;}
            m->collision_spheres[m->collision_sphere_count++]=sphere;
        }
        m->bone_count=s->count;
        bytes+=(uint64_t)s->count*sizeof(*m->stored)+(uint64_t)m->file.lod_count*sizeof(*m->lods);
        if(bytes>budget){status=RF_RANGE;goto fail;}
        m->stored=calloc(s->count,sizeof(*m->stored));
        if(m->file.lod_count)m->lods=calloc(m->file.lod_count,sizeof(*m->lods));
        if(!m->stored || (m->file.lod_count && !m->lods)){status=RF_RANGE;goto fail;}
        for(j=0;j<s->count;++j){status=rf_model_bone_transform(s->bones[j].rotation,s->bones[j].position,m->stored[j]);if(status)goto fail;}
        for(j=0;j<m->file.lod_count;++j) {
            uint64_t available=(uint64_t)budget-bytes+sizeof(*m->lods);
            status=rf_model_geometry_open(m->lods+j,&m->file,j,available>UINT32_MAX?UINT32_MAX:(uint32_t)available);if(status)goto fail;
            bytes+=m->lods[j].accounted_bytes-sizeof(*m->lods);
        }
    }
    v.resident_bytes=(uint32_t)bytes;*result=v;return RF_OK;
fail:
    rf_entity_render_models_close(&v);return status;
}
void rf_entity_collision_models_close(rf_entity_collision_models *models)
{
    uint32_t i;if(!models)return;
    if(models->items)for(i=0;i<models->count;++i)rf_model_skin_geometry_close(models->items+i);
    free(models->items);free(models->lod_indices);memset(models,0,sizeof(*models));
}
int rf_entity_collision_models_open(const rf_entity_render_models *models,uint32_t budget,rf_entity_collision_models *result)
{
    rf_entity_collision_models value={0};uint64_t bytes;uint32_t i;int status;
    if(!models || !result || (models->count && !models->items) || result->items || result->lod_indices ||
        result->count || result->resident_bytes || result->max_vertices)return RF_RANGE;
    bytes=sizeof(value)+(uint64_t)models->count*(sizeof(*value.items)+sizeof(*value.lod_indices));
    if(bytes>budget)return RF_RANGE;
    value.count=models->count;
    if(value.count){value.items=calloc(value.count,sizeof(*value.items));value.lod_indices=calloc(value.count,sizeof(*value.lod_indices));}
    if(value.count && (!value.items || !value.lod_indices)){status=RF_IO;goto fail;}
    for(i=0;i<value.count;++i) {
        rf_model_part_metadata metadata;const rf_entity_render_model *model=models->items+i;
        if(!model->bone_count || model->bone_count>50){status=RF_RANGE;goto fail;}
        status=rf_model_file_part_metadata(&model->file,0,&metadata);if(status)goto fail;
        value.lod_indices[i]=metadata.first_lod+metadata.lod_count-1;
        status=rf_model_skin_geometry_open(value.items+i,&model->file,value.lod_indices[i],model->bone_count,
            (uint32_t)((uint64_t)budget-bytes+sizeof(*value.items)));if(status)goto fail;
        bytes+=value.items[i].accounted_bytes-sizeof(*value.items);
        if(value.items[i].max_vertices>value.max_vertices)value.max_vertices=value.items[i].max_vertices;
    }
    value.resident_bytes=(uint32_t)bytes;*result=value;return RF_OK;
 fail:
    rf_entity_collision_models_close(&value);return status;
}

int rf_entity_poses_start_initial(const rf_entity_seeds *seeds,const rf_entity_skeletons *skeletons,
    const rf_entity_motion_catalog *catalog,rf_entity_playback_resources *resources,rf_entity_poses *poses,const rf_movement_descriptor descriptors[16],float elapsed)
{
    uint32_t i;int status,handled;
    if(!seeds || !skeletons || !catalog || !resources || !poses || !descriptors || !isfinite(elapsed) || elapsed<0 ||
       poses->count!=seeds->records.count || catalog->class_count!=seeds->class_count ||
       (poses->count && (!poses->items || !seeds->items)) || (seeds->class_count && (!seeds->classes || !catalog->mappings)))return RF_RANGE;
    for(i=0;i<poses->count;++i)if(poses->items[i].skeleton!=UINT32_MAX) {
        rf_motion_playback_state initial;rf_motion_playback_initialize(&initial);
        if(memcmp(&poses->items[i].playback,&initial,sizeof(initial)))return RF_RANGE;
    }
    for(i=0;i<poses->count;++i) {
        rf_entity_pose *pose=poses->items+i;const rf_entity_seed_class *cls;const rf_entity_motion_mapping *map;
        rf_motion_priority priority={0};rf_motion_movement movement={0};rf_entity_playback_model *model;float displacement[3]={0};
        uint32_t class_index=seeds->items[i].class_index;
        if(pose->skeleton==UINT32_MAX)continue;
        if(class_index>=seeds->class_count || pose->skeleton>=resources->model_count || !resources->models)return RF_RANGE;
        cls=seeds->classes+class_index;map=catalog->mappings+class_index;model=resources->models+pose->skeleton;
        if(map->weapon!=-1 || map->skeleton!=pose->skeleton)return RF_RANGE;
        pose->controller=(rf_motion_controller){0,-1,0,0,0,0};
        priority.forced_state=-1;
        priority.physics_flags=rf_entity_creation_physics_flags(seeds->items[i].spawn.creation_flags,cls->physics.flags,cls->physics.flags2,cls->physics.use_kind,0);
        {
            uint32_t selected=rf_movement_start(descriptors,(int32_t)cls->physics.movement_index,&priority.physics_flags);
            priority.mode=(int32_t)descriptors[selected].index;
        }
        priority.linked_occupant_handle=-1;priority.entity_handle=-1;
        status=rf_motion_select_priority(&pose->controller,map->states,&priority,&handled);if(status)return status;
        if(!handled) {
            movement.mode=priority.mode;movement.idle_state=0;movement.move_state=2;movement.alternate_state=4;
            status=rf_motion_select_movement(&pose->controller,map->states,&movement);if(status)return status;
        }
        status=rf_motion_apply_controller(&pose->controller,map->states,elapsed,&pose->playback,model->resources,model->count);if(status)return status;
        status=rf_entity_pose_advance(pose,skeletons,catalog,resources,elapsed,displacement);if(status)return status;
    }
    return RF_OK;
}
static int entity_pose_reference_check(const rf_entity_pose *pose,const rf_entity_playback_resources *resources)
{
    const rf_entity_playback_model *model;const rf_motion_slot_state *active;uint32_t i,j;
    if(!pose || !resources || !resources->models || pose->skeleton>=resources->model_count ||
       !pose->generations || !pose->bone_count || pose->bone_count>50)return RF_RANGE;
    model=resources->models+pose->skeleton;active=&pose->playback.completion.active;
    if(active->count>16 || (active->count && !model->resources))return RF_RANGE;
    for(i=0;i<active->count;++i) {
        int32_t id=active->slots[i].motion;
        if(id<0 || (uint32_t)id>=model->count || model->resources[id].references<1)return RF_RANGE;
        for(j=0;j<i;++j)if(active->slots[j].motion==id)return RF_RANGE;
    }
    return RF_OK;
}
int rf_entity_pose_release(rf_entity_pose *pose,rf_entity_playback_resources *resources)
{
    rf_entity_playback_model *model;rf_motion_slot_state *active;uint32_t i;int status;
    status=entity_pose_reference_check(pose,resources);if(status)return status;
    model=resources->models+pose->skeleton;active=&pose->playback.completion.active;
    for(i=0;i<active->count;++i)--model->resources[active->slots[i].motion].references;
    rf_motion_playback_initialize(&pose->playback);memset(pose->generations,0,pose->bone_count*sizeof(*pose->generations));return RF_OK;
}
int rf_entity_pose_take(rf_entity_pose *source,const rf_entity_playback_resources *resources,
    uint32_t budget,rf_entity_owned_pose *result)
{
    rf_entity_owned_pose value={0};uint32_t bytes,matrix_bytes,override_bytes;int status;
    if(!source || !result || source==&result->pose || !source->matrices || result->storage || result->allocated_bytes ||
       result->pose.matrices || result->pose.generations || result->pose.overrides || result->pose.bone_count)return RF_RANGE;
    status=entity_pose_reference_check(source,resources);if(status)return status;
    matrix_bytes=source->bone_count*sizeof(*source->matrices);
    override_bytes=source->overrides?source->bone_count*sizeof(*source->overrides):0;
    bytes=matrix_bytes+override_bytes+source->bone_count*sizeof(*source->generations);
    if((uint64_t)sizeof(value)+bytes>budget)return RF_RANGE;
    value.storage=malloc(bytes);if(!value.storage)return RF_RANGE;
    value.pose=*source;value.pose.matrices=value.storage;
    value.pose.generations=(uint16_t *)((unsigned char *)value.storage+matrix_bytes+override_bytes);
    value.pose.overrides=override_bytes?(rf_model_bone_override *)((unsigned char *)value.storage+matrix_bytes):NULL;
    if(override_bytes)memcpy(value.pose.overrides,source->overrides,override_bytes);
    memcpy(value.pose.matrices,source->matrices,matrix_bytes);
    memcpy(value.pose.generations,source->generations,source->bone_count*sizeof(*source->generations));
    value.allocated_bytes=sizeof(value)+bytes;
    rf_motion_playback_initialize(&source->playback);memset(source->generations,0,source->bone_count*sizeof(*source->generations));
    if(override_bytes)memset(source->overrides,0,override_bytes);
    source->skeleton=UINT32_MAX;*result=value;return RF_OK;
}
int rf_entity_registered_pose_take(rf_model_skeletal_registration *registration,
    rf_entity_pose **published,const rf_entity_playback_resources *resources,
    uint32_t budget,rf_entity_owned_pose *result)
{
    int status;
    if(!registration || !published || !*published || !registration->loaded ||
       registration->active!=&(*published)->playback.completion.active ||
       !registration->next || !registration->previous ||
       registration->next->previous!=registration || registration->previous->next!=registration)return RF_RANGE;
    status=rf_entity_pose_take(*published,resources,budget,result);if(status)return status;
    *published=&result->pose;registration->active=&result->pose.playback.completion.active;
    return RF_OK;
}
int rf_entity_owned_pose_close(rf_entity_owned_pose *pose,rf_entity_playback_resources *resources)
{
    int status;if(!pose)return RF_RANGE;
    if(!pose->storage && !pose->allocated_bytes)return RF_OK;
    if(!pose->storage || !pose->allocated_bytes)return RF_RANGE;
    status=rf_entity_pose_release(&pose->pose,resources);if(status)return status;
    free(pose->storage);memset(pose,0,sizeof(*pose));return RF_OK;
}
int rf_entity_pose_advance(rf_entity_pose *pose,const rf_entity_skeletons *skeletons,
    const rf_entity_motion_catalog *catalog,rf_entity_playback_resources *resources,float elapsed,float displacement[3])
{
    rf_entity_playback_model *model;int status;
    if(!pose || !skeletons || !catalog || !resources || !resources->models || !catalog->models || !displacement ||
       pose->skeleton>=resources->model_count || pose->skeleton>=catalog->model_count || pose->skeleton>=skeletons->count ||
       !skeletons->items || !pose->matrices || !pose->generations || pose->bone_count!=skeletons->items[pose->skeleton].count)return RF_RANGE;
    model=resources->models+pose->skeleton;
    if(model->count!=catalog->models[pose->skeleton].count)return RF_RANGE;
    status=rf_motion_update(&pose->playback,model->resources,model->count,elapsed);if(status)return status;
    return rf_entity_pose_evaluate(pose,skeletons,catalog,displacement);
}
int rf_entity_pose_evaluate(rf_entity_pose *pose,const rf_entity_skeletons *skeletons,
    const rf_entity_motion_catalog *catalog,float pending_displacement[3])
{
    rf_motion_playback_state compact;rf_motion_playback_resource resources[16]={0};
    const rf_motion_file *files[16];const rf_entity_model_motions *model;
    const rf_entity_skeleton *skeleton;uint32_t i,count;
    if(!pose || !skeletons || !catalog || !pending_displacement || !skeletons->items || !catalog->models ||
       pose->skeleton>=skeletons->count || pose->skeleton>=catalog->model_count || !pose->matrices || !pose->generations)return RF_RANGE;
    skeleton=skeletons->items+pose->skeleton;model=catalog->models+pose->skeleton;
    if(!skeleton->bones || !skeleton->count || skeleton->count>50 || pose->bone_count!=skeleton->count)return RF_RANGE;
    compact=pose->playback;count=compact.completion.active.count;
    if(count>16 || (count && !model->items))return RF_RANGE;
    for(i=0;i<count;++i) {
        int32_t id=compact.completion.active.slots[i].motion;const rf_entity_model_motion *motion;
        if(id<0 || (uint32_t)id>=model->count)return RF_RANGE;
        motion=model->items+id;files[i]=&motion->file;resources[i].comparison=motion->comparison;
        resources[i].looping=motion->looping;resources[i].markers[0]=motion->markers[0];resources[i].markers[1]=motion->markers[1];
        compact.completion.active.slots[i].motion=(int32_t)i;
    }
    return rf_model_evaluate_overrides(skeleton->bones,skeleton->count,&compact,files,resources,count,
        pending_displacement,pose->matrices,pose->generations,pose->bone_count,pose->overrides);
}
int rf_entity_collision_cache_open(const rf_entity_pose *pose,uint32_t budget,rf_entity_collision_cache *cache)
{
    rf_entity_collision_cache value={0};uint32_t bytes,i;
    if(!pose || !cache || !pose->bone_count || pose->bone_count>50 || pose->skeleton==UINT32_MAX ||
        !pose->matrices || !pose->generations || pose->playback.generation>65535 ||
        cache->matrices || cache->generations || cache->bone_count || cache->skeleton || cache->allocated_bytes)return RF_RANGE;
    bytes=pose->bone_count*50;if((uint64_t)sizeof(value)+bytes>budget)return RF_RANGE;
    value.matrices=calloc(1,bytes);if(!value.matrices)return RF_IO;
    value.generations=(uint16_t *)((unsigned char *)value.matrices+pose->bone_count*48);
    value.bone_count=pose->bone_count;value.skeleton=pose->skeleton;value.allocated_bytes=(uint32_t)sizeof(value)+bytes;
    for(i=0;i<value.bone_count;++i)value.generations[i]=(uint16_t)(pose->playback.generation-1u);
    *cache=value;return RF_OK;
}
void rf_entity_collision_cache_close(rf_entity_collision_cache *cache)
{
    if(!cache)return;free(cache->matrices);memset(cache,0,sizeof(*cache));
}
int rf_entity_collision_cache_view(rf_entity_collision_cache *cache,const rf_entity_pose *pose,
    const float (*stored)[12],rf_collision_model_skin_pose *view)
{
    rf_collision_model_skin_pose value={0};uint32_t i;
    if(!cache || !pose || !stored || !view || !cache->matrices || !cache->generations ||
        !pose->matrices || !pose->generations || !pose->bone_count || pose->bone_count>50 ||
        cache->bone_count!=pose->bone_count || cache->skeleton!=pose->skeleton || pose->playback.generation>65535 ||
        cache->allocated_bytes!=sizeof(*cache)+pose->bone_count*50 ||
        (void *)cache->generations!=(unsigned char *)cache->matrices+pose->bone_count*48)return RF_RANGE;
    for(i=0;i<pose->bone_count;++i)if(pose->generations[i]!=(uint16_t)pose->playback.generation)return RF_RANGE;
    value.stored=stored;value.evaluated=pose->matrices;value.prepared=cache->matrices;value.generations=cache->generations;
    value.bone_count=value.capacity=pose->bone_count;value.generation=(uint16_t)pose->playback.generation;
    *view=value;return RF_OK;
}

void rf_entity_poses_close(rf_entity_poses *p)
{
    if(!p)return;free(p->items);free(p->matrices);free(p->generations);free(p->overrides);memset(p,0,sizeof(*p));
}
int rf_entity_poses_open(const rf_entity_seeds *seeds,const rf_entity_skeletons *s,uint32_t budget,rf_entity_poses *result)
{
    rf_entity_poses v={0};uint64_t bones=0,bytes;uint32_t i,at=0;
    if(!seeds || !s || !result || result->items || result->matrices || result->generations || result->overrides ||
       result->count || result->bone_count || result->resident_bytes || s->class_count!=seeds->class_count ||
       (seeds->records.count && !seeds->items) || (s->class_count && !s->class_indices) || (s->count && !s->items))return RF_RANGE;
    v.count=seeds->records.count;
    for(i=0;i<v.count;++i) {
        uint32_t c=seeds->items[i].class_index,index;if(c>=s->class_count)return RF_RANGE;
        index=s->class_indices[c];if(index==UINT32_MAX)continue;
        if(index>=s->count || !s->items[index].count || s->items[index].count>50)return RF_RANGE;
        bones+=s->items[index].count;
    }
    bytes=sizeof(v)+(uint64_t)v.count*sizeof(*v.items)+bones*(sizeof(*v.matrices)+sizeof(*v.generations)+sizeof(*v.overrides));
    if(bytes>budget || bones>UINT32_MAX)return RF_RANGE;
    v.bone_count=(uint32_t)bones;v.resident_bytes=(uint32_t)bytes;
    if(v.count)v.items=calloc(v.count,sizeof(*v.items));
    if(bones){v.matrices=calloc((size_t)bones,sizeof(*v.matrices));v.generations=calloc((size_t)bones,sizeof(*v.generations));v.overrides=calloc((size_t)bones,sizeof(*v.overrides));}
    if((v.count && !v.items) || (bones && (!v.matrices || !v.generations || !v.overrides))){rf_entity_poses_close(&v);return RF_RANGE;}
    for(i=0;i<v.count;++i) {
        rf_entity_pose *p=v.items+i;p->skeleton=s->class_indices[seeds->items[i].class_index];
        if(p->skeleton==UINT32_MAX)continue;
        p->bone_count=s->items[p->skeleton].count;p->matrices=v.matrices+at;p->generations=v.generations+at;p->overrides=v.overrides+at;
        rf_motion_playback_initialize(&p->playback);at+=p->bone_count;
    }
    *result=v;return RF_OK;
}
int rf_entity_seeds_open(const rf_level *level,rf_vpp *tables,uint32_t budget,rf_entity_seeds *result)
{
    rf_entity_seeds v={0};rf_vpp_entry entry;void *text=NULL;
    uint64_t bytes;uint32_t i,j;int status;
    if(!level || !tables || !result || result->records.storage || result->records.items ||
       result->records.count || result->records.allocated_bytes || result->items || result->classes ||
       result->class_count || result->resident_bytes || result->peak_bytes || budget<sizeof(v))return RF_RANGE;
    status=rf_level_owned_entities_open(level,budget-(uint32_t)(sizeof(v)-sizeof(v.records)),&v.records);
    if(status)return status;
    bytes=sizeof(v)+(uint64_t)v.records.allocated_bytes-sizeof(v.records);
    bytes+=(uint64_t)v.records.count*sizeof(*v.items);
    if(bytes>budget){status=RF_RANGE;goto done;}
    if(v.records.count) {
        v.items=calloc(v.records.count,sizeof(*v.items));if(!v.items){status=RF_RANGE;goto done;}
    }
    for(i=0;i<v.records.count;++i) {
        status=rf_level_entity_spawn_read(v.records.items+i,&v.items[i].spawn);if(status)goto done;
        for(j=0;j<i;++j)if(same(v.records.items[j].record.class_name,v.records.items[i].record.class_name))break;
        v.items[i].class_index=j<i?v.items[j].class_index:v.class_count++;
    }
    bytes+=(uint64_t)v.class_count*sizeof(*v.classes);
    if(bytes>budget){status=RF_RANGE;goto done;}
    v.resident_bytes=(uint32_t)bytes;v.peak_bytes=v.resident_bytes;
    if(v.class_count) {
        status=rf_vpp_find(tables,"entity.tbl",&entry);if(status)goto done;
        if(!entry.size || entry.size>budget-bytes){status=RF_RANGE;goto done;}
        v.peak_bytes+=(uint32_t)entry.size;
        v.classes=calloc(v.class_count,sizeof(*v.classes));text=malloc(entry.size);
        if(!v.classes || !text){status=RF_RANGE;goto done;}
        status=rf_vpp_read(tables,&entry,0,text,entry.size);if(status)goto done;
        for(i=0,j=0;i<v.records.count;++i)if(v.items[i].class_index==j) {
            rf_entity_assets assets;
            const char *name=v.records.items[i].record.class_name;
            v.classes[j].record_index=i;
            status=rf_entity_vitals_config_read(text,entry.size,name,&v.classes[j].vitals);if(status)goto done;
            status=rf_entity_eye_limits_read(text,entry.size,name,&v.classes[j].eye_limits);if(status)goto done;
            status=rf_entity_corpse_config_read(text,entry.size,name,&v.classes[j].corpse);if(status)goto done;
            status=rf_entity_damage_factors_read(text,entry.size,name,v.classes[j].damage_factors);if(status)goto done;
            status=rf_entity_class_physics_read(text,entry.size,name,&v.classes[j].physics);if(status)goto done;
            status=rf_entity_lod_distances_read(text,entry.size,name,&v.classes[j].lod);if(status)goto done;
            status=rf_entity_assets_read(text,entry.size,name,"",&assets);if(status)goto done;
            memcpy(v.classes[j].model,assets.model,sizeof(assets.model));
            status=rf_entity_model_kind(assets.model,&v.classes[j].model_kind);if(status)goto done;
            ++j;
        }
    }
    free(text);*result=v;return RF_OK;
done:
    free(text);rf_entity_seeds_close(&v);return status;
}

int rf_explosion_central_prepare(const rf_explosion_definition *definition,uint32_t slot,float size,
    rf_particle_definition *particle,float *random_extent)
{
    rf_particle_definition value;
    if(!definition || !particle || !random_extent || slot>=6 || definition->recipe.central_count>6 || slot>=definition->recipe.central_count)return RF_RANGE;
    if(!(definition->resolved&(1u<<slot)) || !(size>=definition->recipe.central[slot].min_size))return RF_NOT_FOUND;
    value=definition->emitters[slot];
    value.min_velocity*=size;value.max_velocity*=size;
    value.min_radius*=size;value.max_radius*=size;
    value.min_life*=size;value.max_life*=size;
    *particle=value;*random_extent=size*definition->recipe.central_random;return RF_OK;
}

int rf_explosion_clock_tick(const rf_explosion_recipe *recipe,float dt,
    rf_explosion_clock *clock,rf_explosion_clock_actions *actions)
{
    rf_explosion_clock next;rf_explosion_clock_actions out={0};unsigned slot;
    if(!recipe || !clock || !actions || recipe->central_count>6 || clock->active>1 ||
       (clock->live&~((1u<<recipe->central_count)-1u)) || !isfinite(dt) || !isfinite(clock->elapsed) || !isfinite(clock->size))return RF_RANGE;
    next=*clock;
    if(next.active) {
        next.elapsed+=dt;if(!isfinite(next.elapsed))return RF_RANGE;
        for(slot=0;slot<recipe->central_count;++slot)
            if((next.live&(1u<<slot)) && (recipe->central[slot].process_per_frame&255u)==1u &&
               (double)next.elapsed<(double)recipe->central[slot].play_factor*next.size)out.process|=1u<<slot;
        if(next.elapsed>recipe->play_time){out.release=next.live;next.live=0;next.active=0;}
    }
    *clock=next;*actions=out;return RF_OK;
}
