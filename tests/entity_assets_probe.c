#include "rf/entity_assets.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
int main(int argc,char **argv)
{
    rf_vpp archive;rf_vpp_entry entry;rf_entity_assets assets;char *text;int status;uint32_t i;
    if(argc==4 && !strcmp(argv[1],"--class-physics")) {
        rf_entity_class_physics value,before;
        memset(&value,0xa5,sizeof(value));before=value;
        if(rf_vpp_open(&archive,argv[2]) || rf_vpp_find(&archive,"entity.tbl",&entry) || entry.size>512*1024)return 2;
        text=malloc(entry.size);if(!text)return 2;
        status=rf_vpp_read(&archive,&entry,0,text,entry.size);
        if(!status)status=rf_entity_class_physics_read(text,entry.size,argv[3],&value);
        free(text);rf_vpp_close(&archive);
        if(status && memcmp(&value,&before,sizeof(value)))return 4;if(status)return 3;
        for(i=0;i<sizeof(value);++i)printf("%02x",((const unsigned char*)&value)[i]);
        printf("\n");return 0;
    }
    if(argc==4 && !strcmp(argv[1],"--material")) {
        rf_entity_material material,before;
        memset(&material,0xa5,sizeof(material));before=material;
        if(rf_vpp_open(&archive,argv[2]) || rf_vpp_find(&archive,"materials.tbl",&entry) || entry.size>512*1024)return 2;
        text=malloc(entry.size);if(!text)return 2;
        status=rf_vpp_read(&archive,&entry,0,text,entry.size);
        if(!status)status=rf_entity_material_read(text,entry.size,argv[3],&material);
        free(text);rf_vpp_close(&archive);
        if(status && memcmp(&material,&before,sizeof(material)))return 4;
        if(status)return 3;
        for(i=0;i<sizeof(material);++i)printf("%02x",((const unsigned char*)&material)[i]);
        printf("\n");return 0;
    }
    if(argc==4 && !strcmp(argv[1],"--sphere-declarations")) {
        rf_entity_sphere_declarations declarations,before;
        memset(&declarations,0xa5,sizeof(declarations));before=declarations;
        if(rf_vpp_open(&archive,argv[2]) || rf_vpp_find(&archive,"entity.tbl",&entry) || entry.size>512*1024)return 2;
        text=malloc(entry.size);if(!text)return 2;
        status=rf_vpp_read(&archive,&entry,0,text,entry.size);
        if(!status)status=rf_entity_sphere_declarations_read(text,entry.size,argv[3],&declarations);
        free(text);rf_vpp_close(&archive);
        if(status && memcmp(&declarations,&before,sizeof(declarations)))return 4;
        if(status)return 3;
        printf("%u\n",declarations.count);
        for(i=0;i<declarations.count;++i) {
            uint32_t j;const unsigned char *p=(const unsigned char*)(declarations.items+i);
            for(j=0;j<sizeof(*declarations.items);++j)printf("%02x",p[j]);printf("\n");
        }
        return 0;
    }
    if(argc==6 && !strcmp(argv[1],"--state-set")) {
        rf_entity_state_set *set=malloc(sizeof(*set)),*before=malloc(sizeof(*set));
        if(!set || !before || rf_vpp_open(&archive,argv[3]))return 2;
        memset(set,0xa5,sizeof(*set));*before=*set;
        status=rf_entity_state_set_open(argv[2],argv[4],argv[5],&archive,512*1024,set);
        if(status && memcmp(set,before,sizeof(*set)))return 4;
        printf("%d\n",status);
        if(!status) {
            printf("%u\n",set->count);
            for(i=0;i<23;++i)printf("%d%s",set->states[i],i==22?"\n":" ");
            for(i=0;i<set->count;++i) {
                uint32_t refs;memcpy(&refs,set->cache[i].bytes+0x70,4);
                printf("%s %u\n",set->files[i].entry.name,refs);
            }
        }
        *before=*set;
        if(rf_entity_state_set_open(argv[2],argv[4],argv[5],&archive,1,set)!=RF_RANGE || memcmp(set,before,sizeof(*set)))return 4;
        free(set);free(before);rf_vpp_close(&archive);return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--motion-cache")) {
        struct {uint32_t capacity;char name[64];rf_motion_cache_record records[8];} data;
        struct {int32_t status;uint32_t index;rf_motion_cache_record records[8];} result;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&data,sizeof(data),1,stdin)==1) {
            if(data.capacity>8)return 2;
            result.index=0xa5a5a5a5;
            result.status=rf_motion_cache_acquire(data.records,data.capacity,data.name,&result.index);
            memcpy(result.records,data.records,sizeof(data.records));
            if(fwrite(&result,sizeof(result),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==7 && !strcmp(argv[1],"--state-open")) {
        rf_motion_file file,before;
        if(rf_vpp_open(&archive,argv[3]))return 2;
        memset(&file,0xa5,sizeof(file));before=file;
        status=rf_entity_state_motion_open(argv[2],argv[4],argv[5],argv[6],&archive,512*1024,&file);
        if(status && memcmp(&file,&before,sizeof(file)))return 4;
        printf("%d\n",status);if(!status)printf("%s\n%u %u\n",file.entry.name,file.header[1],file.header[6]);
        rf_vpp_close(&archive);return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--motion-name")) {
        char input[64],output[64];
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(input,1,64,stdin)==64) {
            memset(output,0xa5,64);status=rf_motion_compiled_filename(input,output);
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(output,1,64,stdout)!=64)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==6 && !strcmp(argv[1],"--state")) {
        char motion[64],before[64];memset(motion,0xa5,64);memcpy(before,motion,64);
        status=rf_entity_state_motion_load(argv[2],argv[3],argv[4],argv[5],motion,512*1024);
        if(status && memcmp(motion,before,64))return 4;
        printf("%d\n",status);if(!status)puts(motion);return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--state-guards")) {
        char compiled[64],saved[64];
        strcpy(compiled,"two.dots.mvf");
        if(rf_motion_compiled_filename(compiled,compiled) || strcmp(compiled,"two.rfa"))return 3;
        memset(compiled,'x',64);memcpy(saved,compiled,64);
        if(rf_motion_compiled_filename(compiled,compiled)!=RF_RANGE || memcmp(compiled,saved,64))return 3;
        if(rf_motion_compiled_filename(NULL,compiled)!=RF_RANGE || memcmp(compiled,saved,64))return 3;
        const char good[]="$Name: \"Actor\" +State: \"stand\" \"base.mvf\" +State: \"custom\" \"\" "
            "+Weapon Specific: \"gun\" +State: \"stand\" \"gun.mvf\" $Name: \"other\" +State: \"stand\" \"other.mvf\"";
        const char duplicate[]="$Name: \"Actor\" +State: \"stand\" \"one.mvf\" +State: \"stand\" \"two.mvf\"";
        const char bad[]="$Name: \"Actor\" +State: \"stand\" \"unterminated";
        char motion[64],before[64];
        if(rf_entity_state_motion_read(good,sizeof(good)-1,"ACTOR","","STAND",motion) || strcmp(motion,"base.mvf"))return 3;
        if(rf_entity_state_motion_read(good,sizeof(good)-1,"actor","GUN","stand",motion) || strcmp(motion,"gun.mvf"))return 3;
        if(rf_entity_state_motion_read(good,sizeof(good)-1,"actor","","custom",motion) || *motion)return 3;
        memset(motion,0xa5,64);memcpy(before,motion,64);
        if(rf_entity_state_motion_read(good,sizeof(good)-1,"actor","absent","stand",motion)!=RF_NOT_FOUND || memcmp(motion,before,64))return 3;
        if(rf_entity_state_motion_read(good,sizeof(good)-1,"absent","","stand",motion)!=RF_NOT_FOUND || memcmp(motion,before,64))return 3;
        if(rf_entity_state_motion_read(duplicate,sizeof(duplicate)-1,"actor","","stand",motion)!=RF_FORMAT || memcmp(motion,before,64))return 3;
        if(rf_entity_state_motion_read(bad,sizeof(bad)-1,"actor","","stand",motion)!=RF_FORMAT || memcmp(motion,before,64))return 3;
        if(rf_entity_state_motion_load("Installed_Game/tables.vpp","miner1","","stand",motion,1)!=RF_RANGE || memcmp(motion,before,64))return 3;
        puts("PASS: base/weapon isolation, empty motion, absent keys, duplicate/malformed rejection, table budget, output preservation");return 0;
    }
    if(argc==7 && !strcmp(argv[1],"--level")) {
        rf_vpp meshes;rf_level level;rf_level_actor_assets actor,before;
        char *end;long uid=strtol(argv[6],&end,10);if(!*argv[6] || *end)return 2;
        if(rf_vpp_open(&archive,argv[2]))return 2;
        if(rf_vpp_open(&meshes,argv[5])) {rf_vpp_close(&archive);return 2;}
        status=rf_level_open(&level,&archive,argv[3]);memset(&actor,0xa5,sizeof(actor));before=actor;
        if(!status)status=rf_level_actor_assets_load(&level,(int32_t)uid,argv[4],&meshes,512*1024,&actor);
        if(!status) {
            printf("%d %s %s %s\n",actor.entity.uid,actor.entity.class_name,actor.entity.skin,actor.mesh.name);
            printf("%.9g %.9g %.9g\n",actor.entity.position[0],actor.entity.position[1],actor.entity.position[2]);
            for(i=0;i<9;++i)printf("%.9g%s",actor.entity.orientation[i/3][i%3],i==8?"\n":" ");
            for(i=0;i<actor.assets.texture_count;++i)puts(actor.assets.textures[i]);
        } else {if(memcmp(&actor,&before,sizeof(actor)))return 4;printf("%d\n",status);}
        rf_vpp_close(&meshes);rf_vpp_close(&archive);return status?3:0;
    }
    if(argc==2 && !strcmp(argv[1],"--compiled-name")) {
        char input[64],output[64];
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(input,1,64,stdin)==64) {
            memset(output,0xa5,64);status=rf_entity_skeletal_filename(input,output);
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(output,1,64,stdout)!=64)return 2;
        }
        return ferror(stdin)?2:0;
    }
    if(argc==2 && !strcmp(argv[1],"--self-test")) {
        const char good[]="// $Name: \"ignored\"\n$Name: \"Actor\" $V3D Filename: \"model.vcm\" $Skin: \"b\" (\"one.tga\" \"two.tga\")\n$Name: \"other\"";
        const char bad[]="$Name: \"actor\" $V3D Filename: \"unterminated";
        rf_entity_assets before;char many[2048];uint32_t used;
        char filename[64],preserved[64];
        strcpy(filename,"miner.vcm");
        if(rf_entity_skeletal_filename(filename,filename) || strcmp(filename,"miner.v3c"))return 1;
        memset(filename,'x',64);memcpy(preserved,filename,64);
        if(rf_entity_skeletal_filename(filename,filename)!=RF_RANGE || memcmp(filename,preserved,64))return 1;
        filename[63]=0;memcpy(preserved,filename,64);
        if(rf_entity_skeletal_filename(filename,filename)!=RF_RANGE || memcmp(filename,preserved,64))return 1;
        if(rf_entity_skeletal_filename(NULL,filename)!=RF_RANGE || memcmp(filename,preserved,64))return 1;
        if(rf_entity_assets_read(good,sizeof(good)-1,"ACTOR","B",&assets) || strcmp(assets.model,"model.vcm") || assets.texture_count!=2 || strcmp(assets.textures[1],"two.tga"))return 1;
        memset(&assets,0xa5,sizeof(assets));before=assets;
        if(rf_entity_assets_read(good,sizeof(good)-1,"ignored","",&assets)!=RF_NOT_FOUND || memcmp(&assets,&before,sizeof(assets)))return 1;
        if(rf_entity_assets_read(good,sizeof(good)-1,"actor","absent",&assets)!=RF_NOT_FOUND || memcmp(&assets,&before,sizeof(assets)))return 1;
        if(rf_entity_assets_read(bad,sizeof(bad)-1,"actor","",&assets)==RF_OK || memcmp(&assets,&before,sizeof(assets)))return 1;
        strcpy(many,"$Name: \"actor\" $Skin: \"b\" (");used=(uint32_t)strlen(many);
        for(i=0;i<65;++i) {memcpy(many+used,"\"x.tga\" ",8);used+=8;}
        many[used++]=')';
        if(rf_entity_assets_read(many,used,"actor","b",&assets)!=RF_RANGE || memcmp(&assets,&before,sizeof(assets)))return 1;
        puts("PASS: comments, case, missing class/skin, malformed quote, replacement capacity, output preservation");return 0;
    }
    if(argc!=4 || rf_vpp_open(&archive,argv[1]))return 2;
    if(rf_vpp_find(&archive,"entity.tbl",&entry) || entry.size>512*1024)return 2;
    text=malloc(entry.size);if(!text)return 2;
    status=rf_vpp_read(&archive,&entry,0,text,entry.size);
    if(!status)status=rf_entity_assets_read(text,entry.size,argv[2],argv[3],&assets);
    if(!status) {
        rf_entity_assets loaded,before;
        if(rf_entity_assets_load(argv[1],argv[2],argv[3],&loaded,entry.size) || memcmp(&loaded,&assets,sizeof(assets)))return 4;
        memset(&loaded,0xa5,sizeof(loaded));before=loaded;
        if(rf_entity_assets_load(argv[1],argv[2],argv[3],&loaded,entry.size-1)!=RF_RANGE || memcmp(&loaded,&before,sizeof(loaded)))return 4;
        if(rf_entity_assets_load(argv[1],argv[2],"missing-skin",&loaded,entry.size)!=RF_NOT_FOUND || memcmp(&loaded,&before,sizeof(loaded)))return 4;
    }
    if(!status) {printf("%s\n",assets.model);for(i=0;i<assets.texture_count;++i)printf("%s\n",assets.textures[i]);}
    free(text);rf_vpp_close(&archive);return status?3:0;
}
