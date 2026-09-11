#include "rf/entity_assets.h"
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
    char authored[64],compiled[64];uint32_t identities[23]={0},i,identity;
    uint8_t flags[23]={0};int status,added;int32_t index;
    rf_model_motion_registry registry={identities,flags,0,23};
    status=state_group_exists(text,size,class_name,weapon);if(status)return status;
    for(i=0;i<45;++i)value->actions[i]=-1;
    for(i=0;i<23;++i) {
        value->states[i]=-1;
        status=rf_entity_state_motion_read(text,size,class_name,weapon,names[i],authored);
        if(status==RF_NOT_FOUND) {status=RF_OK;continue;}
        if(status)return status;
        if(!*authored)continue;
        status=rf_motion_cache_acquire(value->cache,23,authored,&identity);if(status)return status;
        status=rf_model_register_motion(&registry,identity+1,1,&index,&added);if(status)return status;
        if(added) {
            value->looping[index]=1;
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
static int action_set_extend(const void *text,uint32_t size,const char *name,rf_vpp *motions,rf_entity_state_set *v)
{
    static const char *names[45]={"corpse_drop","corpse_carry","fire_stand","alt_fire_stand","fire_crouch","death_generic","death_blast_forward","death_blast_backward","death_head_forward","death_head_backward","death_head_neutral","death_chest_forward","death_chest_backward","death_chest_neutral","death_leg_left","death_leg_right","death_crouch","sidestep_left","sidestep_right","roll_left","roll_right","land","flinch_stand","flinch_attack_stand","flinch_chest","flinch_back","flinch_leg_left","flinch_leg_right","idle_to_ready","ready_to_idle","idle_1","idle_2","idle_3","idle_4","rock_drop","rock_pickup","death_still_1","death_still_2","death_still_3","reload","unholster","speak","speak_short","heal_light_1","hit_alarm"};
    uint32_t identities[68]={0},i,identity;uint8_t flags[68]={0};int status,added;int32_t index;char compiled[64];
    rf_model_motion_registry registry={identities,flags,0,68};
    /* Called immediately after base states: their cache and registry indices coincide. */
    if(v->count>23)return RF_RANGE;
    registry.count=v->count;
    for(i=0;i<v->count;++i){identities[i]=i+1;flags[i]=1;}
    for(i=0;i<45;++i) {
        rf_entity_action_declaration action;
        status=rf_entity_action_read(text,size,name,"",names[i],&action);
        if(status==RF_NOT_FOUND)continue;if(status)return status;
        memcpy(v->action_sounds[i],action.sound,64);
        if(!*action.motion)continue;
        status=rf_motion_cache_acquire(v->cache,68,action.motion,&identity);if(status)return status;
        status=rf_model_register_motion(&registry,identity+1,0,&index,&added);if(status)return status;
        if(added) {
            status=rf_motion_compiled_filename((const char*)v->cache[identity].bytes,compiled);if(status)return status;
            status=rf_motion_file_open(v->files+index,motions,compiled);if(status)return status;
            v->looping[index]=0;
        }
        v->actions[i]=index;
    }
    v->count=registry.count;return RF_OK;
}
void rf_entity_base_motions_close(rf_entity_base_motions *m)
{
    if(!m)return;free(m->classes);memset(m,0,sizeof(*m));
}
int rf_entity_base_motions_open(const rf_entity_seeds *seeds,rf_vpp *tables,rf_vpp *motions,
    uint32_t budget,rf_entity_base_motions *result)
{
    rf_entity_base_motions v={0};rf_vpp_entry entry,weapon_entry;void *text=NULL;
    uint64_t bytes;uint32_t i,j;int status;
    if(!seeds || !tables || !motions || !result || result->classes || result->class_count ||
       result->resident_bytes || result->peak_bytes || result->weapons.count || result->weapons.primary_count ||
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
        if(c->model_kind!=2)continue;
        status=state_set_read(text,entry.size,seeds->records.items[c->record_index].record.class_name,"",motions,v.classes+i);
        if(status)goto done;
        status=action_set_extend(text,entry.size,seeds->records.items[c->record_index].record.class_name,motions,v.classes+i);
        if(status)goto done;
    }
    free(text);*result=v;return RF_OK;
done:
    free(text);rf_entity_base_motions_close(&v);return status;
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
void rf_entity_poses_close(rf_entity_poses *p)
{
    if(!p)return;free(p->items);free(p->matrices);free(p->generations);memset(p,0,sizeof(*p));
}
int rf_entity_poses_open(const rf_entity_seeds *seeds,const rf_entity_skeletons *s,uint32_t budget,rf_entity_poses *result)
{
    rf_entity_poses v={0};uint64_t bones=0,bytes;uint32_t i,at=0;
    if(!seeds || !s || !result || result->items || result->matrices || result->generations ||
       result->count || result->bone_count || result->resident_bytes || s->class_count!=seeds->class_count ||
       (seeds->records.count && !seeds->items) || (s->class_count && !s->class_indices) || (s->count && !s->items))return RF_RANGE;
    v.count=seeds->records.count;
    for(i=0;i<v.count;++i) {
        uint32_t c=seeds->items[i].class_index,index;if(c>=s->class_count)return RF_RANGE;
        index=s->class_indices[c];if(index==UINT32_MAX)continue;
        if(index>=s->count || !s->items[index].count || s->items[index].count>50)return RF_RANGE;
        bones+=s->items[index].count;
    }
    bytes=sizeof(v)+(uint64_t)v.count*sizeof(*v.items)+bones*(sizeof(*v.matrices)+sizeof(*v.generations));
    if(bytes>budget || bones>UINT32_MAX)return RF_RANGE;
    v.bone_count=(uint32_t)bones;v.resident_bytes=(uint32_t)bytes;
    if(v.count)v.items=calloc(v.count,sizeof(*v.items));
    if(bones){v.matrices=calloc((size_t)bones,sizeof(*v.matrices));v.generations=calloc((size_t)bones,sizeof(*v.generations));}
    if((v.count && !v.items) || (bones && (!v.matrices || !v.generations))){rf_entity_poses_close(&v);return RF_RANGE;}
    for(i=0;i<v.count;++i) {
        rf_entity_pose *p=v.items+i;p->skeleton=s->class_indices[seeds->items[i].class_index];
        if(p->skeleton==UINT32_MAX)continue;
        p->bone_count=s->items[p->skeleton].count;p->matrices=v.matrices+at;p->generations=v.generations+at;
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
            status=rf_entity_class_physics_read(text,entry.size,name,&v.classes[j].physics);if(status)goto done;
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
