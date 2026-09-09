#include "rf/entity_assets.h"
#include "rf/model.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
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
