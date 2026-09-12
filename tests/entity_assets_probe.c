#include "rf/entity_assets.h"
#include "rf/audio.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
#include "owned_pose_probe.h"
#include "clutter_create_probe.h"
int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--owned-pose"))return owned_pose_probe();
    if(argc==2 && !strcmp(argv[1],"--clutter-create"))return clutter_create_probe();
    rf_vpp archive;rf_vpp_entry entry;rf_entity_assets assets;char *text;int status;uint32_t i;
    if(argc==4 && !strcmp(argv[1],"--clutter-definition")) {
        FILE *f=fopen(argv[2],"rb");long size;rf_clutter_definition value,before;
        if(!f)return 2;fseek(f,0,SEEK_END);size=ftell(f);rewind(f);
        if(size<0 || size>1024*1024){fclose(f);return 2;}
        text=malloc((size_t)size+1);if(!text){fclose(f);return 2;}
        if(fread(text,1,(size_t)size,f)!=(size_t)size){free(text);fclose(f);return 2;}fclose(f);
        memset(&value,0xa5,sizeof(value));before=value;
        status=rf_clutter_definition_read(text,(uint32_t)size,argv[3],&value);free(text);
        if(status && memcmp(&value,&before,sizeof(value)))return 3;
        _setmode(_fileno(stdout),_O_BINARY);fwrite(&status,4,1,stdout);fwrite(&value,sizeof(value),1,stdout);return 0;
    }
    if(argc==3 && !strcmp(argv[1],"--clutter-flags")) {
        FILE *f=fopen(argv[2],"rb");long size;uint32_t flags=0xa5a5a5a5,consumed=0x5a5a5a5a;
        if(!f)return 2;fseek(f,0,SEEK_END);size=ftell(f);rewind(f);
        if(size<0 || size>1024*1024){fclose(f);return 2;}
        text=malloc((size_t)size+1);if(!text){fclose(f);return 2;}
        if(fread(text,1,(size_t)size,f)!=(size_t)size){free(text);fclose(f);return 2;}fclose(f);
        status=rf_clutter_flags_read(text,(uint32_t)size,&flags,&consumed);free(text);
        _setmode(_fileno(stdout),_O_BINARY);fwrite(&status,4,1,stdout);fwrite(&flags,4,1,stdout);fwrite(&consumed,4,1,stdout);return 0;
    }
    if(argc==5 && !strcmp(argv[1],"--clutter-assets")) {
        FILE *f=fopen(argv[2],"rb");long size;rf_entity_assets before;
        if(!f)return 2;fseek(f,0,SEEK_END);size=ftell(f);rewind(f);
        if(size<0 || size>1024*1024){fclose(f);return 2;}
        text=malloc((size_t)size+1);if(!text){fclose(f);return 2;}
        if(fread(text,1,(size_t)size,f)!=(size_t)size){free(text);fclose(f);return 2;}fclose(f);
        memset(&assets,0xa5,sizeof(assets));before=assets;
        status=rf_clutter_assets_read(text,(uint32_t)size,argv[3],argv[4],&assets);free(text);
        if(status && memcmp(&assets,&before,sizeof(assets)))return 3;
        _setmode(_fileno(stdout),_O_BINARY);
        fwrite(&status,4,1,stdout);fwrite(&assets,1,sizeof(assets),stdout);return 0;
    }
    if(argc==4 && !strcmp(argv[1],"--corpse-config")) {
        FILE *f=fopen(argv[2],"rb");long size;rf_entity_corpse_config value,before;
        if(!f)return 2;fseek(f,0,SEEK_END);size=ftell(f);rewind(f);
        if(size<0 || size>1024*1024){fclose(f);return 2;}
        text=malloc((size_t)size+1);if(!text){fclose(f);return 2;}
        if(fread(text,1,(size_t)size,f)!=(size_t)size){free(text);fclose(f);return 2;}fclose(f);
        memset(&value,0xa5,sizeof(value));before=value;
        status=rf_entity_corpse_config_read(text,(uint32_t)size,argv[3],&value);free(text);
        if(status && memcmp(&value,&before,sizeof(value)))return 3;
        _setmode(_fileno(stdout),_O_BINARY);
        fwrite(&status,4,1,stdout);fwrite(&value,1,sizeof(value),stdout);return 0;
    }
    if(argc==4 && (!strcmp(argv[1],"--pain-groups") || !strcmp(argv[1],"--damage-sound-groups") || !strcmp(argv[1],"--impact-sound-group"))) {
        uint32_t count=!strcmp(argv[1],"--impact-sound-group")?1:!strcmp(argv[1],"--pain-groups")?2:3;
        rf_foley_owner owner={0};int32_t groups[3]={123,456,789};FILE *f;long size;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        if(fread(&owner.group_count,4,1,stdin)!=1 || owner.group_count>640)return 2;
        owner.groups=calloc(owner.group_count?owner.group_count:1,sizeof(*owner.groups));if(!owner.groups)return 2;
        for(i=0;i<owner.group_count;++i)if(fread(owner.groups[i].name,32,1,stdin)!=1)return 2;
        f=fopen(argv[2],"rb");if(!f)return 2;fseek(f,0,SEEK_END);size=ftell(f);rewind(f);
        if(size<0 || size>1024*1024)return 2;text=malloc((size_t)size+1);if(!text)return 2;
        if(fread(text,1,(size_t)size,f)!=(size_t)size)return 2;fclose(f);
        status=count==1?rf_entity_impact_sound_group_read(text,(uint32_t)size,argv[3],&owner,groups):count==2?rf_entity_pain_groups_read(text,(uint32_t)size,argv[3],&owner,groups):
            rf_entity_damage_sound_groups_read(text,(uint32_t)size,argv[3],&owner,groups);
        fwrite(&status,4,1,stdout);fwrite(groups,4,count,stdout);free(text);free(owner.groups);return 0;
    }
    if(argc==4 && !strcmp(argv[1],"--eye-limits")) {
        FILE *f=fopen(argv[2],"rb");long size;rf_entity_eye_limits value;
        if(!f)return 2;fseek(f,0,SEEK_END);size=ftell(f);rewind(f);
        if(size<0 || size>1024*1024){fclose(f);return 2;}
        text=malloc((size_t)size+1);if(!text){fclose(f);return 2;}
        if(fread(text,1,(size_t)size,f)!=(size_t)size){fclose(f);free(text);return 2;}fclose(f);
        memset(&value,0xa5,sizeof(value));status=rf_entity_eye_limits_read(text,(uint32_t)size,argv[3],&value);
        free(text);_setmode(_fileno(stdout),_O_BINARY);fwrite(&status,4,1,stdout);fwrite(&value,sizeof(value),1,stdout);return 0;
    }
    if(argc==4 && !strcmp(argv[1],"--damage-factors")) {
        FILE *f=fopen(argv[2],"rb");long size;float factors[11];
        if(!f)return 2;fseek(f,0,SEEK_END);size=ftell(f);rewind(f);
        if(size<0 || size>1024*1024){fclose(f);return 2;}
        text=malloc((size_t)size+1);if(!text){fclose(f);return 2;}
        if(fread(text,1,(size_t)size,f)!=(size_t)size){fclose(f);free(text);return 2;}fclose(f);
        memset(factors,0xa5,sizeof(factors));status=rf_entity_damage_factors_read(text,(uint32_t)size,argv[3],factors);
        free(text);_setmode(_fileno(stdout),_O_BINARY);fwrite(&status,4,1,stdout);fwrite(factors,sizeof(factors),1,stdout);return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--body-owner-check")) {
        rf_entity_physics_config config={0};rf_physics_body body={0},guard={0};
        rf_physics_sphere spheres[2]={{{0,1,0},.5f,-1,0},{{0,2,0},.25f,-1,0}};
        const float position[3]={2,3,4},orientation[9]={1,0,0,0,1,0,0,0,1};
        uint32_t budget;rf_physics_sphere saved[2];
        config.authored.mass=80;config.material.elasticity=.5f;config.material.friction=.2f;
        if(rf_entity_body_open(&config,spheres,2,position,orientation,0,4096,&body))return 40;
        budget=body.allocated_bytes;memcpy(saved,spheres,sizeof(saved));
        if(rf_entity_body_open(&config,spheres,2,position,orientation,0,budget-1,&guard)!=RF_RANGE ||
           memcmp(&guard,&(rf_physics_body){0},sizeof(guard)))return 41;
        if(rf_entity_body_open(&config,spheres,2,position,orientation,0,budget,&guard))return 42;
        rf_physics_body_close(&guard);
        spheres[1].radius=-1;
        if(rf_entity_body_open(&config,spheres,2,position,orientation,0,4096,&guard)!=RF_RANGE ||
           memcmp(&guard,&(rf_physics_body){0},sizeof(guard)))return 43;
        memset(spheres,0xa5,sizeof(spheres));
        if(body.spheres.count!=2 || memcmp(body.spheres.items,saved,sizeof(saved)))return 44;
        rf_physics_body_close(&body);rf_physics_body_close(&body);
        config.authored.mass=0;
        if(rf_entity_body_open(&config,NULL,0,position,orientation,0,4096,&guard)!=RF_FORMAT)return 45;
        puts("BODY_OWNER PASS exact/short budgets, failed installation, copied spheres, repeat close, unsupported mass");return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--lod-distances")) {
        uint32_t size;char name[64];rf_entity_lod_distances value;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&size,4,1,stdin)==1) {
            if(!size || size>1024*1024 || fread(name,64,1,stdin)!=1 || !memchr(name,0,64))return 2;
            text=malloc(size);if(!text || fread(text,size,1,stdin)!=1)return 3;
            memset(&value,0xa5,sizeof(value));status=rf_entity_lod_distances_read(text,size,name,&value);
            free(text);fwrite(&status,4,1,stdout);fwrite(&value,sizeof(value),1,stdout);
        }
        return 0;
    }
    if(argc==4 && (!strcmp(argv[1],"--default-weapons") || !strcmp(argv[1],"--default-weapons-text"))) {
        rf_weapon_names names;rf_entity_default_weapons result;char raw[8192];uint32_t size;
        if(rf_vpp_open(&archive,argv[2]) || rf_weapon_names_load(&archive,256*1024,&names))return 2;
        if(!strcmp(argv[1],"--default-weapons-text")) {
            _setmode(_fileno(stdin),_O_BINARY);size=(uint32_t)fread(raw,1,sizeof(raw),stdin);text=raw;
        } else {
            if(rf_vpp_find(&archive,"entity.tbl",&entry))return 2;size=entry.size;text=malloc(size);
            if(!text || rf_vpp_read(&archive,&entry,0,text,size))return 3;
        }
        memset(&result,0xa5,sizeof(result));status=rf_entity_default_weapons_read(text,size,argv[3],&names,&result);
        _setmode(_fileno(stdout),_O_BINARY);fwrite(&status,4,1,stdout);fwrite(&result,sizeof(result),1,stdout);
        if(text!=raw)free(text);rf_vpp_close(&archive);return 0;
    }
    if(argc==5 && !strcmp(argv[1],"--state-text")) {
        char raw[8192];size_t size;rf_entity_state_declaration result;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        size=fread(raw,1,sizeof(raw),stdin);memset(&result,0xa5,sizeof(result));
        status=rf_entity_state_declaration_read(raw,(uint32_t)size,argv[2],argv[3],argv[4],&result);
        fwrite(&status,4,1,stdout);fwrite(&result,sizeof(result),1,stdout);return 0;
    }
    if(argc==3 && !strcmp(argv[1],"--catalog-fixture")) {
        rf_entity_state_set *base=calloc(2,sizeof(*base));rf_entity_skeleton skeleton={0};
        uint32_t models[2]={0,0};rf_entity_skeletons skeletons={0};
        rf_entity_weapon_motion_group group={0};rf_motion_file file={0};uint8_t loop=1;
        char identities[1][64]={"a.b.mvf"};rf_entity_base_motions bindings={0};
        rf_entity_motion_catalog catalog={0},guard={0};uint32_t j;
        if(!base)return 1;
        skeletons.items=&skeleton;skeletons.class_indices=models;skeletons.count=1;skeletons.class_count=2;
        bindings.classes=base;bindings.class_count=2;bindings.groups=&group;bindings.group_count=1;
        group.files=&file;group.looping=&loop;group.identities=identities;group.count=1;group.weapon=3;
        if(rf_vpp_open(&archive,argv[2]) || rf_motion_file_open(&file,&archive,"tech01_stand.rfa"))return 8;
        for(i=0;i<2;++i){for(j=0;j<23;++j)base[i].states[j]=-1;for(j=0;j<45;++j)base[i].actions[j]=-1;}
        for(j=0;j<23;++j)group.states[j]=-1;for(j=0;j<45;++j)group.actions[j]=-1;
        group.states[0]=0;base[0].count=2;base[1].count=1;
        strcpy((char*)base[0].cache[0].bytes,"a.c.mvf");strcpy((char*)base[0].cache[1].bytes,"a.b.mvf");
        strcpy((char*)base[1].cache[0].bytes,"A.B.other");
        base[0].cache_indices[1]=1;base[0].looping[0]=base[1].looping[0]=1;
        base[0].states[0]=base[1].states[0]=0;base[0].actions[0]=1;
        base[0].files[0]=base[0].files[1]=base[1].files[0]=file;
        base[0].marker_counts[0]=base[0].marker_counts[1]=base[1].marker_counts[0]=2;
        base[0].marker_frames[0][0]=5;base[0].marker_frames[0][1]=19;
        base[0].states[1]=0;base[0].marker_frames[1][0]=9;base[0].marker_frames[1][1]=30;
        base[1].marker_frames[0][0]=7;base[1].marker_frames[0][1]=20;
        if(rf_entity_motion_catalog_open(&skeletons,&bindings,128*1024,&catalog))return 2;
        if(catalog.models[0].count!=3 || catalog.mappings[2].states[0]!=0 || catalog.mappings[0].states[0]!=1 ||
           catalog.mappings[0].actions[0]!=2 || catalog.mappings[1].states[0]!=0 ||
           strcmp(catalog.models[0].items[0].identity,"a.b.mvf") ||
           strcmp(catalog.models[0].items[1].identity,"a.c.mvf") ||
           strcmp(catalog.models[0].items[2].identity,"a.b.mvf") || catalog.models[0].items[2].looping)return 3;
        if(catalog.models[0].items[1].marker_mask!=3 || catalog.models[0].items[1].markers[0]!=800 ||
           catalog.models[0].items[1].markers[1]!=3040 || catalog.models[0].items[0].markers[0]!=1120 ||
           catalog.models[0].items[2].markers[0]!=1120 || catalog.models[0].items[2].markers[1]!=3200)return 7;
        if(rf_entity_motion_catalog_open(&skeletons,&bindings,catalog.peak_bytes-1,&guard)!=RF_RANGE ||
           memcmp(&guard,&(rf_entity_motion_catalog){0},sizeof(guard)))return 4;
        if(rf_entity_motion_catalog_open(&skeletons,&bindings,catalog.peak_bytes,&guard))return 5;
        rf_entity_motion_catalog_close(&guard);
        base[1].states[0]=1;
        if(rf_entity_motion_catalog_open(&skeletons,&bindings,128*1024,&guard)!=RF_RANGE ||
           memcmp(&guard,&(rf_entity_motion_catalog){0},sizeof(guard)))return 6;
        rf_entity_motion_catalog_close(&catalog);rf_vpp_close(&archive);free(base);puts("CATALOG_FIXTURE PASS");return 0;
    }
    if(argc==3 && !strcmp(argv[1],"--weapon-names")) {
        rf_weapon_names value,before;memset(&value,0xa5,sizeof(value));before=value;
        if(rf_vpp_open(&archive,argv[2]) || rf_vpp_find(&archive,"weapons.tbl",&entry))return 2;
        if(rf_weapon_names_load(&archive,entry.size-1,&value)!=RF_RANGE || memcmp(&value,&before,sizeof(value)))return 3;
        status=rf_weapon_names_load(&archive,entry.size,&value);rf_vpp_close(&archive);
        _setmode(_fileno(stdout),_O_BINARY);fwrite(&status,4,1,stdout);fwrite(&value,sizeof(value),1,stdout);return 0;
    }
    if(argc==3 && !strcmp(argv[1],"--weapon-find")) {
        rf_weapon_names value;int32_t index;_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        if(fread(&value,sizeof(value),1,stdin)!=1 || value.count>64)return 2;
        index=rf_weapon_name_find(&value,argv[2]);fwrite(&index,4,1,stdout);return 0;
    }
    if(argc==5 && !strcmp(argv[1],"--action-text")) {
        char raw[8192];size_t size;rf_entity_action_declaration result;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        size=fread(raw,1,sizeof(raw),stdin);memset(&result,0xa5,sizeof(result));
        status=rf_entity_action_read(raw,(uint32_t)size,argv[2],argv[3],argv[4],&result);
        fwrite(&status,4,1,stdout);fwrite(&result,sizeof(result),1,stdout);return 0;
    }
    if(argc==6 && !strcmp(argv[1],"--action")) {
        rf_entity_action_declaration result;memset(&result,0xa5,sizeof(result));
        if(rf_vpp_open(&archive,argv[2]) || rf_vpp_find(&archive,"entity.tbl",&entry))return 2;
        text=malloc(entry.size);if(!text || rf_vpp_read(&archive,&entry,0,text,entry.size))return 3;
        status=rf_entity_action_read(text,entry.size,argv[3],argv[4],argv[5],&result);
        free(text);rf_vpp_close(&archive);_setmode(_fileno(stdout),_O_BINARY);
        fwrite(&status,4,1,stdout);fwrite(&result,sizeof(result),1,stdout);return 0;
    }
    if((argc==6 && !strcmp(argv[1],"--base-motions")) || (argc==7 && !strcmp(argv[1],"--catalog"))) {
        rf_vpp levels,tables,motions;rf_level level;rf_entity_seeds seeds={0};
        rf_entity_base_motions m={0},guard={0};uint32_t files=0,j;
        rf_entity_motion_catalog catalog={0},catalog_guard={0};
        rf_entity_state_set *expected=malloc(sizeof(*expected));if(!expected)return 1;
        if(rf_vpp_open(&levels,argv[2]) || rf_vpp_open(&tables,argv[3]) || rf_vpp_open(&motions,argv[4]) || rf_level_open(&level,&levels,argv[argc-1]))return 2;
        if(rf_entity_seeds_open(&level,&tables,4*1024*1024,&seeds))return 3;
        status=rf_entity_base_motions_open(&seeds,&tables,&motions,1024*1024,&m);
        if(status){fprintf(stderr,"base motions %d\n",status);return 4;}
        if(rf_entity_base_motions_open(&seeds,&tables,&motions,m.peak_bytes-1,&guard)!=RF_RANGE || memcmp(&guard,&(rf_entity_base_motions){0},sizeof(guard)))return 5;
        if(rf_entity_base_motions_open(&seeds,&tables,&motions,m.peak_bytes,&guard))return 6;
        rf_entity_base_motions_close(&guard);rf_entity_base_motions_close(&guard);
        for(i=0;i<m.class_count;++i)printf("ACTION_DECLARATIONS\t%s\t\t%u\t%u\n",seeds.records.items[seeds.classes[i].record_index].record.class_name,m.classes[i].action_declarations[0],m.classes[i].action_declarations[1]);
        for(i=0;i<m.group_count;++i)printf("ACTION_DECLARATIONS\t%s\t%s\t%u\t%u\n",seeds.records.items[seeds.classes[m.groups[i].class_index].record_index].record.class_name,m.weapons.names[m.groups[i].weapon],m.groups[i].action_declarations[0],m.groups[i].action_declarations[1]);
        for(i=0;i<m.class_count;++i)printf("WEAPON_GROUPS\t%s\t%u\t%u\n",
            seeds.records.items[seeds.classes[i].record_index].record.class_name,m.classes[i].weapon_groups[0],m.classes[i].weapon_groups[1]);
        for(i=0;i<m.class_count;++i)printf("DEFAULT_WEAPONS\t%s\t%d\t%d\n",
            seeds.records.items[seeds.classes[i].record_index].record.class_name,m.classes[i].default_weapons.primary,m.classes[i].default_weapons.secondary);
        for(i=0;i<m.class_count;++i)if(seeds.classes[i].model_kind==2) {
            status=rf_entity_state_set_open(argv[3],seeds.records.items[seeds.classes[i].record_index].record.class_name,"",&motions,512*1024,expected);
            if(status || memcmp(expected->states,m.classes[i].states,sizeof(expected->states)))return 7;
            for(j=0;j<expected->count;++j) {
                const rf_motion_file *a=expected->files+j,*b=m.classes[i].files+j;
                if(a->archive!=b->archive || strcmp(a->entry.name,b->entry.name) || a->entry.offset!=b->entry.offset ||
                   a->entry.size!=b->entry.size || memcmp(a->header,b->header,sizeof(a->header)))return 7;
            }
            for(j=0;j<m.classes[i].count;++j) {
                uint32_t identity=m.classes[i].cache_indices[j];
                if(identity>=68)return 15;
                printf("IDENTITY\t%s\t\t%u\t%u\t%s\n",seeds.records.items[seeds.classes[i].record_index].record.class_name,
                    j,m.classes[i].looping[j],(const char*)m.classes[i].cache[identity].bytes);
            }
            for(j=0;j<45;++j) {
                int32_t index=m.classes[i].actions[j];
                if(index<-1 || (index>=0 && ((uint32_t)index>=m.classes[i].count || m.classes[i].looping[index])))return 9;
                printf("ACTION\t%s\t%u\t%d\t%s\t%s\n",seeds.records.items[seeds.classes[i].record_index].record.class_name,j,index,
                    index<0?"":m.classes[i].files[index].entry.name,m.classes[i].action_sounds[j]);
            }
        }
        for(i=0;i<m.group_count;++i) {
            const rf_entity_weapon_motion_group *g=m.groups+i;
            const char *cls=seeds.records.items[seeds.classes[g->class_index].record_index].record.class_name;
            if(rf_entity_weapon_motion_find(&m,g->class_index,(int32_t)g->weapon)!=g)return 10;
            for(j=0;j<g->count;++j)printf("IDENTITY\t%s\t%s\t%u\t%u\t%s\n",cls,m.weapons.names[g->weapon],j,g->looping[j],g->identities[j]);
            for(j=0;j<23;++j) {
                int32_t index=g->states[j];if(index<-1 || (index>=0 && ((uint32_t)index>=g->count || g->looping[index]!=1)))return 11;
                printf("GROUP_STATE\t%s\t%s\t%u\t%d\t%s\n",cls,m.weapons.names[g->weapon],j,index,index<0?"":g->files[index].entry.name);
            }
            for(j=0;j<45;++j) {
                int32_t index=g->actions[j];if(index<-1 || (index>=0 && ((uint32_t)index>=g->count || g->looping[index]!=0)))return 12;
                printf("GROUP_ACTION\t%s\t%s\t%u\t%d\t%s\t%s\n",cls,m.weapons.names[g->weapon],j,index,index<0?"":g->files[index].entry.name,g->action_sounds[j]);
            }
        }
        if(rf_entity_weapon_motion_find(&m,0,-1) || rf_entity_weapon_motion_find(&m,m.class_count,0))return 13;
        if(argc==7) {
            rf_vpp meshes;rf_entity_skeletons skeletons={0};
            if(rf_vpp_open(&meshes,argv[5]) || rf_entity_skeletons_open(&seeds,&meshes,256*1024,&skeletons))return 16;
            status=rf_entity_motion_catalog_open(&skeletons,&m,512*1024,&catalog);
            if(status){fprintf(stderr,"catalog %d\n",status);return 17;}
            if(rf_entity_motion_catalog_open(&skeletons,&m,catalog.peak_bytes-1,&catalog_guard)!=RF_RANGE ||
                memcmp(&catalog_guard,&(rf_entity_motion_catalog){0},sizeof(catalog_guard)))return 18;
            if(rf_entity_motion_catalog_open(&skeletons,&m,catalog.peak_bytes,&catalog_guard))return 19;
            rf_entity_motion_catalog_close(&catalog_guard);rf_entity_motion_catalog_close(&catalog_guard);
            {
                rf_entity_playback_resources playback={0},guard={0};uint32_t at=0,id,total;
                if(rf_entity_playback_resources_open(&catalog,256*1024,&playback))return 30;
                if(rf_entity_playback_resources_open(&catalog,playback.peak_bytes-1,&guard)!=RF_RANGE || memcmp(&guard,&(rf_entity_playback_resources){0},sizeof(guard)))return 31;
                if(rf_entity_playback_resources_open(&catalog,playback.peak_bytes,&guard))return 32;
                rf_entity_playback_resources_close(&guard);rf_entity_playback_resources_close(&guard);
                for(i=0;i<playback.model_count;++i)for(j=0;j<playback.models[i].count;++j,++at) {
                    rf_motion_playback_resource *r=playback.models[i].resources+j;const rf_entity_model_motion *m=catalog.models[i].items+j;
                    if(r!=playback.resources+at || r->references || memcmp(&r->comparison,&m->comparison,sizeof(r->comparison)) ||
                       r->looping!=m->looping || memcmp(r->markers,m->markers,sizeof(r->markers)))return 33;
                    r->references=(int32_t)(at%3+1);
                    printf("PLAYBACK_ALIAS\t%u\t%u\t%u\t%d\n",i,j,playback.models[i].cache_ids[j],r->references);
                }
                for(id=0;id<playback.cache_count;++id) {
                    if(rf_entity_playback_cache_references(&playback,id,&total))return 34;
                    printf("PLAYBACK_REFERENCES\t%u\t%u\n",id,total);
                }
                total=123;if(rf_entity_playback_cache_references(&playback,playback.cache_count,&total)!=RF_RANGE || total!=123)return 35;
                if(playback.resource_count) {
                    playback.resources[0].references=-1;
                    if(rf_entity_playback_cache_references(&playback,playback.cache_ids[0],&total)!=RF_RANGE || total!=123)return 36;
                }
                printf("PLAYBACK_OWNER\t%u\t%u\t%u\t%u\n",playback.resource_count,playback.cache_count,playback.resident_bytes,playback.peak_bytes);
                {
                    rf_entity_poses poses={0};uint32_t actor,k;
                    rf_movement_descriptor descriptors[16];rf_vpp descriptor_archive;const char *fallback=getenv("RF_TEST_START_FALLBACK");
                    if(rf_vpp_open(&descriptor_archive,argv[3]))return 40;
                    for(k=0;k<16;++k)if(rf_movement_descriptor_load(&descriptor_archive,k,65536,descriptors+k))return 40;
                    rf_vpp_close(&descriptor_archive);
                    if(fallback) {
                        descriptors[0].enabled=1;descriptors[0].index=3;
                        if(!strcmp(fallback,"disabled"))for(k=1;k<16;++k)descriptors[k].enabled=256;
                        else if(!strcmp(fallback,"direct"))for(k=0;k<seeds.class_count;++k)seeds.classes[k].physics.movement_index=0;
                        else return 40;
                    }

                    for(k=0;k<playback.resource_count;++k)playback.resources[k].references=0;
                    if(rf_entity_poses_open(&seeds,&skeletons,1024*1024,&poses) ||
                       rf_entity_poses_start_initial(&seeds,&skeletons,&catalog,&playback,&poses,descriptors,1.0f/30.0f))return 37;
                    for(actor=0;actor<poses.count;++actor)if(poses.items[actor].skeleton!=UINT32_MAX) {
                        const rf_entity_pose *p=poses.items+actor;const unsigned char *bytes;
                        printf("STARTUP_POSE\t%s\t%u\t",seeds.records.items[actor].record.class_name,seeds.items[actor].spawn.creation_flags);
                        bytes=(const unsigned char*)&p->playback;for(k=0;k<sizeof(p->playback);++k)printf("%02x",bytes[k]);putchar('\t');
                        bytes=(const unsigned char*)p->matrices;for(k=0;k<p->bone_count*48;++k)printf("%02x",bytes[k]);
                        bytes=(const unsigned char*)p->generations;for(k=0;k<p->bone_count*2;++k)printf("%02x",bytes[k]);putchar('\n');
                    }
                    for(actor=0;actor<poses.count;++actor)if(poses.items[actor].skeleton!=UINT32_MAX && rf_entity_pose_release(poses.items+actor,&playback))return 38;
                    for(k=0;k<playback.resource_count;++k)if(playback.resources[k].references)return 39;
                    rf_entity_poses_close(&poses);
                }
                rf_entity_playback_resources_close(&playback);rf_entity_playback_resources_close(&playback);
            }
            for(i=0;i<catalog.mapping_count;++i) {
                const rf_entity_motion_mapping *map=catalog.mappings+i;
                const char *cls=seeds.records.items[seeds.classes[map->class_index].record_index].record.class_name;
                printf("CATALOG_MAP\t%s\t%s\t%u",cls,map->weapon<0?"":m.weapons.names[map->weapon],map->skeleton);
                for(j=0;j<23;++j)printf("\t%d",map->states[j]);for(j=0;j<45;++j)printf("\t%d",map->actions[j]);puts("");
            }
            for(i=catalog.class_count;i<catalog.mapping_count;++i) {
                rf_entity_motion_mapping effective=catalog.mappings[i],bad=catalog.mappings[i],before;
                const rf_entity_motion_mapping *base=catalog.mappings+effective.class_index;
                const char *cls=seeds.records.items[seeds.classes[effective.class_index].record_index].record.class_name;
                bad.skeleton=UINT32_MAX;before=effective;
                if(rf_entity_motion_mapping_overlay(base,&bad,&effective)!=RF_RANGE || memcmp(&effective,&before,sizeof(before)))return 21;
                if(rf_entity_motion_mapping_overlay(base,NULL,&effective) || memcmp(&effective,base,sizeof(effective)))return 22;
                effective=catalog.mappings[i];
                if(rf_entity_motion_mapping_overlay(base,&effective,&effective))return 23;
                printf("EFFECTIVE_MAP\t%s\t%s\t%u",cls,m.weapons.names[effective.weapon],effective.skeleton);
                for(j=0;j<23;++j)printf("\t%d",effective.states[j]);for(j=0;j<45;++j)printf("\t%d",effective.actions[j]);puts("");
            }
            for(i=0;i<catalog.class_count;++i) {
                rf_entity_motion_selection selected,before;uint32_t weapon,k;
                const char *cls=seeds.records.items[seeds.classes[i].record_index].record.class_name;
                memset(&selected,0xa5,sizeof(selected));before=selected;
                status=rf_entity_motion_selection_base(&catalog,&m,i,&selected);
                if(skeletons.class_indices[i]==UINT32_MAX) {
                    if(status!=RF_NOT_FOUND || memcmp(&selected,&before,sizeof(selected)))return 24;continue;
                }
                if(status || memcmp(&selected.mapping,catalog.mappings+i,sizeof(selected.mapping)))return 25;
                before=selected;
                if(rf_entity_motion_selection_weapon(&catalog,&m,i,-1,&selected) || memcmp(&selected,&before,sizeof(selected)))return 26;
                if(rf_entity_motion_selection_weapon(&catalog,&m,i,(int32_t)m.weapons.count,&selected)!=RF_RANGE || memcmp(&selected,&before,sizeof(selected)))return 27;
                for(weapon=0;weapon<m.weapons.count;++weapon) {
                    if(rf_entity_motion_selection_weapon(&catalog,&m,i,(int32_t)weapon,&selected))return 28;
                    printf("SELECTED_MAP\t%s\t%u\t%d\t%u",cls,weapon,selected.mapping.weapon,selected.mapping.skeleton);
                    for(k=0;k<23;++k)printf("\t%d",selected.mapping.states[k]);
                    for(k=0;k<45;++k)printf("\t%d",selected.mapping.actions[k]);
                    for(k=0;k<45;++k)printf("\t%s",selected.action_sounds[k]);puts("");
                }
            }
            rf_entity_skeletons_close(&skeletons);rf_vpp_close(&meshes);
        }

        rf_entity_seeds_close(&seeds);rf_vpp_close(&levels);rf_vpp_close(&tables);
        for(i=0;i<m.class_count;++i)for(j=0;j<m.classes[i].count;++j) {
            rf_motion_file *f=m.classes[i].files+j;rf_motion_track track;
            if(f->header[6] && rf_motion_file_track(f,0,&track))return 8;++files;
        }
        printf("BASE_MOTIONS %u %u %u %u\n",m.class_count,files,m.resident_bytes,m.peak_bytes);
        printf("BOUND_GROUPS %u\n",m.group_count);
        for(i=0;i<m.group_count;++i)for(j=0;j<m.groups[i].count;++j) {
            rf_motion_file *f=m.groups[i].files+j;rf_motion_track track;
            if(f->header[6] && rf_motion_file_track(f,0,&track))return 14;
        }
        rf_entity_base_motions_close(&m);
        for(i=0;i<catalog.model_count;++i)for(j=0;j<catalog.models[i].count;++j) {
            rf_entity_model_motion *r=catalog.models[i].items+j;rf_motion_track track;
            if(r->file.header[6] && rf_motion_file_track(&r->file,0,&track))return 20;
            printf("CATALOG_RESOURCE\t%u\t%u\t%u\t%s\t%s\n",i,j,r->looping,r->identity,r->file.entry.name);
            printf("CATALOG_MARKERS\t%u\t%u\t%u\t%d\t%d\n",i,j,r->marker_mask,r->markers[0],r->markers[1]);
            printf("CATALOG_MARKER_NAMES\t%u\t%u\t%s\t%s\n",i,j,catalog.models[i].marker_names[j].names[0],catalog.models[i].marker_names[j].names[1]);
            {rf_motion_playback_state playback={0};uint32_t fired;
             playback.completion.active.count=1;playback.completion.active.slots[0].motion=(int32_t)j;playback.completion.active.dominant_slot=0;playback.event_mask=3;
             if(rf_motion_consume_marker(&playback,catalog.models[i].marker_names,catalog.models[i].count,"footstep_left",&fired) || fired!=!!(r->marker_mask&1))return 122;
             if(rf_motion_consume_marker(&playback,catalog.models[i].marker_names,catalog.models[i].count,"footstep_right",&fired) || fired!=!!(r->marker_mask&2))return 123;
            }
            {uint32_t weight;memcpy(&weight,&r->comparison.weight,4);
             printf("CATALOG_ENVELOPE\t%u\t%u\t%u\t%d\t%d\t%d\t%d\n",i,j,weight,r->comparison.start_tick,r->comparison.end_tick,r->comparison.fade_in,r->comparison.fade_out);}

        }
        if(argc==7)printf("CATALOG %u %u %u %u\n",catalog.model_count,catalog.mapping_count,catalog.resident_bytes,catalog.peak_bytes);
        rf_entity_motion_catalog_close(&catalog);rf_entity_motion_catalog_close(&catalog);
        rf_vpp_close(&motions);free(expected);return 0;
    }
    if(argc==3 && !strcmp(argv[1],"--model-kind")) {
        uint32_t kind=0xa5a5a5a5;status=rf_entity_model_kind(argv[2],&kind);
        printf("%d %u\n",status,kind);return 0;
    }
    if(argc==6 && !strcmp(argv[1],"--skeletons")) {
        rf_vpp levels,tables,meshes;rf_level level;rf_entity_seeds seeds={0};rf_entity_skeletons s={0},guard={0};
        rf_entity_poses poses={0},pose_guard={0};
        if(rf_vpp_open(&levels,argv[2]) || rf_vpp_open(&tables,argv[3]) || rf_vpp_open(&meshes,argv[4]) || rf_level_open(&level,&levels,argv[5]))return 2;
        if(rf_entity_seeds_open(&level,&tables,4*1024*1024,&seeds))return 3;
        status=rf_entity_skeletons_open(&seeds,&meshes,1024*1024,&s);if(status){fprintf(stderr,"skeleton open %d\n",status);return 4;}
        if(rf_entity_skeletons_open(&seeds,&meshes,s.peak_bytes-1,&guard)!=RF_RANGE || guard.items || guard.class_indices)return 5;
        if(rf_entity_skeletons_open(&seeds,&meshes,s.peak_bytes,&guard))return 6;
        rf_entity_skeletons_close(&guard);rf_entity_skeletons_close(&guard);
        if(seeds.class_count) {
            rf_entity_seed_class *last=seeds.classes+seeds.class_count-1,saved=*last;
            strcpy(last->model,"__missing_fixture__.vcm");last->model_kind=2;
            status=rf_entity_skeletons_open(&seeds,&meshes,1024*1024,&guard);*last=saved;
            if(status!=RF_NOT_FOUND || memcmp(&guard,&(rf_entity_skeletons){0},sizeof(guard)))return 10;
        }
        for(i=0;i<s.class_count;++i)if(seeds.classes[i].model_kind==2) {
            char name[64];if(rf_entity_skeletal_filename(seeds.classes[i].model,name) || s.class_indices[i]>=s.count ||
                _stricmp(name,s.items[s.class_indices[i]].model))return 7;
        } else if(s.class_indices[i]!=UINT32_MAX)return 8;
        for(i=0;i<seeds.class_count;++i) {
            int32_t indices[3];rf_entity_seed_class saved=seeds.classes[i];
            const char *model=s.class_indices[i]==UINT32_MAX?"-":s.items[s.class_indices[i]].model;
            if(rf_entity_class_death_bones(&seeds,&s,i,indices))return 27;
            printf("CLASS_DEATH_BONES\t%u\t%u\t%s\t%d\t%d\t%d\n",saved.model_kind,saved.physics.flags,model,indices[0],indices[1],indices[2]);
            seeds.classes[i].physics.flags&=~0x20000u;
            if(rf_entity_class_death_bones(&seeds,NULL,i,indices) || indices[0]!=-1 || indices[1]!=-1 || indices[2]!=-1)return 28;
            seeds.classes[i]=saved;seeds.classes[i].model_kind=1;
            if(rf_entity_class_death_bones(&seeds,NULL,i,indices) || indices[0]!=-1 || indices[1]!=-1 || indices[2]!=-1)return 29;
            seeds.classes[i]=saved;
        }
        if(rf_entity_poses_open(&seeds,&s,1024*1024,&poses))return 11;
        if(rf_entity_poses_open(&seeds,&s,poses.resident_bytes-1,&pose_guard)!=RF_RANGE ||
           memcmp(&pose_guard,&(rf_entity_poses){0},sizeof(pose_guard)))return 12;
        if(rf_entity_poses_open(&seeds,&s,poses.resident_bytes,&pose_guard))return 13;
        rf_entity_poses_close(&pose_guard);rf_entity_poses_close(&pose_guard);
        {uint32_t at=0;rf_motion_playback_state expected;rf_motion_playback_initialize(&expected);
        for(i=0;i<poses.count;++i)if(poses.items[i].skeleton!=UINT32_MAX) {
            rf_entity_pose *p=poses.items+i;
            if(p->matrices!=poses.matrices+at || p->generations!=poses.generations+at || p->overrides!=poses.overrides+at ||
               memcmp(&p->playback,&expected,sizeof(expected)))return 14;
            at+=p->bone_count;
        }
        if(at!=poses.bone_count)return 15;
        for(i=0;i<poses.bone_count*sizeof(*poses.overrides);++i)if(((unsigned char *)poses.overrides)[i])return 25;
        if(poses.resident_bytes!=sizeof(poses)+poses.count*sizeof(*poses.items)+poses.bone_count*(48+2+sizeof(*poses.overrides)))return 26;
        printf("POSE_OVERRIDES %u %u %u\n",poses.bone_count,poses.bone_count*(unsigned)sizeof(*poses.overrides),poses.resident_bytes);
        }
        for(i=0;i<s.class_count;++i)if(s.class_indices[i]!=UINT32_MAX)
            printf("SKELETON_CLASS\t%s\t%s\n",seeds.records.items[seeds.classes[i].record_index].record.class_name,s.items[s.class_indices[i]].model);
        {
            rf_entity_render_models render={0},guard={0};uint32_t j,k;
            if(rf_entity_render_models_open(&s,&meshes,4*1024*1024,&render))return 16;
            if(rf_entity_render_models_open(&s,&meshes,render.resident_bytes-1,&guard)!=RF_RANGE || memcmp(&guard,&(rf_entity_render_models){0},sizeof(guard)))return 17;
            if(rf_entity_render_models_open(&s,&meshes,render.resident_bytes,&guard))return 18;
            rf_entity_render_models_close(&guard);rf_entity_render_models_close(&guard);
            for(j=0;j<render.count;++j)for(k=0;k<render.items[j].file.lod_count;++k) {
                rf_model_geometry *g=render.items[j].lods+k;
                printf("RENDER_LOD\t%s\t%u\t%u\t%u\t%u\n",s.items[j].model,k,g->batch_count,g->vertex_count,g->triangle_count);
            }
            for(j=0;j<render.count;++j) {
                rf_entity_render_model *model=render.items+j;rf_model_collision_sphere sphere;
                for(k=0;k<model->collision_sphere_count;++k) {
                    if(rf_model_file_collision_sphere(&model->file,k,&sphere) || memcmp(&sphere,model->collision_spheres+k,sizeof(sphere)))return 24;
                }
                if(rf_model_file_collision_sphere(&model->file,k,&sphere)!=RF_NOT_FOUND)return 25;
                printf("RENDER_SPHERES\t%s\t%u\n",s.items[j].model,model->collision_sphere_count);
            }
            printf("RENDER_MODELS %u %u\n",render.count,render.resident_bytes);
            rf_entity_render_models_close(&render);rf_entity_render_models_close(&render);
        }
        {
            rf_entity_appearances appearances={0},guard={0};uint32_t actor,k;
            status=rf_entity_appearances_open(&seeds,&s,&tables,1024*1024,&appearances);
            if(status){fprintf(stderr,"appearances %d\n",status);return 20;}
            if(rf_entity_appearances_open(&seeds,&s,&tables,appearances.peak_bytes-1,&guard)!=RF_RANGE || memcmp(&guard,&(rf_entity_appearances){0},sizeof(guard)))return 21;
            if(rf_entity_appearances_open(&seeds,&s,&tables,appearances.peak_bytes,&guard))return 22;
            rf_entity_appearances_close(&guard);rf_entity_appearances_close(&guard);
            for(actor=0;actor<appearances.actor_count;++actor)if(appearances.actor_indices[actor]!=UINT32_MAX) {
                const rf_entity_appearance *a=appearances.items+appearances.actor_indices[actor];const rf_level_entity *r=&seeds.records.items[actor].record;
                printf("APPEARANCE\t%s\t%s\t%u\t%s",r->class_name,r->skin,appearances.actor_indices[actor],s.items[a->skeleton].model);
                for(k=0;k<a->texture_count;++k)printf("\t%s",a->textures[k]);putchar('\n');
            }
            printf("APPEARANCES %u %u %u %u\n",appearances.actor_count,appearances.count,appearances.resident_bytes,appearances.peak_bytes);
            rf_entity_appearances_close(&appearances);rf_entity_appearances_close(&appearances);
        }
        printf("POSES %u %u %u\n",poses.count,poses.bone_count,poses.resident_bytes);
        rf_entity_poses_close(&poses);
        rf_entity_seeds_close(&seeds);rf_vpp_close(&levels);rf_vpp_close(&tables);rf_vpp_close(&meshes);
        for(i=0;i<s.count;++i){uint8_t order[256];if(rf_model_bone_order(s.items[i].bones,s.items[i].count,order,256))return 9;}
        printf("SKELETONS %u %u %u %u\n",s.class_count,s.count,s.resident_bytes,s.peak_bytes);
        rf_entity_skeletons_close(&s);return 0;
    }
    if(argc==5 && !strcmp(argv[1],"--seeds")) {
        rf_vpp levels,tables;rf_level level;rf_entity_seeds seeds={0},guard={0};uint32_t peak,count,classes;
        if(rf_vpp_open(&levels,argv[2]) || rf_vpp_open(&tables,argv[3]) || rf_level_open(&level,&levels,argv[4]))return 2;
        status=rf_entity_seeds_open(&level,&tables,4*1024*1024,&seeds);if(status){fprintf(stderr,"seed open %d\n",status);return 3;}
        peak=seeds.peak_bytes;count=seeds.records.count;classes=seeds.class_count;
        if(rf_entity_seeds_open(&level,&tables,peak-1,&guard)!=RF_RANGE || memcmp(&guard,&(rf_entity_seeds){0},sizeof(guard)))return 4;
        if(rf_entity_seeds_open(&level,&tables,peak,&guard))return 5;
        rf_entity_seeds_close(&guard);rf_entity_seeds_close(&guard);
        for(i=0;i<classes;++i) {
            rf_entity_creation_vitals_class expected;
            if(seeds.classes[i].record_index>=count ||
               rf_entity_vitals_config_load(&tables,seeds.records.items[seeds.classes[i].record_index].record.class_name,1024*1024,&expected) ||
               memcmp(&expected,&seeds.classes[i].vitals,sizeof(expected)))return 7;
        }
        rf_vpp_close(&levels);rf_vpp_close(&tables);
        for(i=0;i<count;++i) {
            rf_level_entity_spawn spawn;
            if(seeds.items[i].class_index>=classes || rf_level_entity_spawn_read(seeds.records.items+i,&spawn) ||
               memcmp(&spawn,&seeds.items[i].spawn,sizeof(spawn)) ||
               _stricmp(seeds.records.items[i].record.class_name,
                   seeds.records.items[seeds.classes[seeds.items[i].class_index].record_index].record.class_name))return 6;
        }
        for(i=0;i<count;++i)printf("SEED_FLAGS\t%s\t%u\n",seeds.records.items[i].record.class_name,seeds.items[i].spawn.creation_flags);
        for(i=0;i<classes;++i) {
            uint32_t eye_words[6];memcpy(eye_words,&seeds.classes[i].eye_limits,24);
            printf("SEED_EYE\t%s\t%u\t%u\t%u\t%u\t%u\t%u\n",
                seeds.records.items[seeds.classes[i].record_index].record.class_name,
                eye_words[0],eye_words[1],eye_words[2],eye_words[3],eye_words[4],eye_words[5]);
            printf("SEED_CORPSE\t%s\t%s\t%s\t%.9g\t%.9g\n",
                seeds.records.items[seeds.classes[i].record_index].record.class_name,
                seeds.classes[i].corpse.model,seeds.classes[i].corpse.emitter,seeds.classes[i].corpse.emitter_lifetime,seeds.classes[i].corpse.body_temperature);
            const rf_entity_lod_distances *lod=&seeds.classes[i].lod;
            printf("SEED_LOD\t%s\t%u\t%.9g\t%.9g\t%.9g\t%.9g\n",
                seeds.records.items[seeds.classes[i].record_index].record.class_name,
                lod->count,lod->distances[0],lod->distances[1],lod->distances[2],lod->distances[3]);
        }
        printf("SEEDS %u %u %u %u\n",count,classes,seeds.resident_bytes,peak);
        rf_entity_seeds_close(&seeds);return 0;
    }
    if(argc==4 && !strcmp(argv[1],"--vitals-config")) {
        rf_entity_creation_vitals_class value,before;
        memset(&value,0xa5,sizeof(value));before=value;
        if(rf_vpp_open(&archive,argv[2]) || rf_vpp_find(&archive,"entity.tbl",&entry))return 2;
        if(rf_entity_vitals_config_load(&archive,argv[3],entry.size-1,&value)!=RF_RANGE || memcmp(&value,&before,12))return 3;
        status=rf_entity_vitals_config_load(&archive,argv[3],entry.size,&value);rf_vpp_close(&archive);
        _setmode(_fileno(stdout),_O_BINARY);
        if(fwrite(&status,4,1,stdout)!=1 || fwrite(&value,12,1,stdout)!=1)return 4;
        return 0;
    }
    if(argc==3 && !strcmp(argv[1],"--vitals-text")) {
        char data[8192];size_t size;rf_entity_creation_vitals_class value;
        memset(&value,0xa5,sizeof(value));_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        size=fread(data,1,sizeof(data),stdin);status=rf_entity_vitals_config_read(data,(uint32_t)size,argv[2],&value);
        if(fwrite(&status,4,1,stdout)!=1 || fwrite(&value,12,1,stdout)!=1)return 4;
        return 0;
    }
    if(argc==3 && !strcmp(argv[1],"--jump-height")) {
        float value=123;uint32_t budget;
        if(rf_vpp_open(&archive,argv[2]) || rf_vpp_find(&archive,"game.tbl",&entry))return 2;
        budget=entry.size;
        if(rf_game_jump_height_load(&archive,budget-1,&value)!=RF_RANGE || value!=123)return 4;
        status=rf_game_jump_height_load(&archive,budget,&value);rf_vpp_close(&archive);
        if(status)return 3;_setmode(_fileno(stdout),_O_BINARY);
        return fwrite(&value,4,1,stdout)==1?0:1;
    }
    if(argc==2 && !strcmp(argv[1],"--jump-height-text")) {
        char data[65536];float value=123;size_t size;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        size=fread(data,1,sizeof(data),stdin);status=rf_game_jump_height_read(data,(uint32_t)size,&value);
        if(fwrite(&status,4,1,stdout)!=1 || fwrite(&value,4,1,stdout)!=1)return 1;
        return 0;
    }
    if(argc==4 && !strcmp(argv[1],"--physics-config")) {
        rf_entity_physics_config value,before;rf_vpp_entry material;uint32_t budget;
        memset(&value,0xa5,sizeof(value));before=value;
        if(rf_vpp_open(&archive,argv[2]) || rf_vpp_find(&archive,"entity.tbl",&entry) || rf_vpp_find(&archive,"materials.tbl",&material))return 2;
        budget=entry.size>material.size?entry.size:material.size;if(budget>512*1024 || !budget)return 2;
        if(rf_entity_physics_config_load(&archive,argv[3],budget-1,&value)!=RF_RANGE || memcmp(&value,&before,sizeof(value)))return 4;
        status=rf_entity_physics_config_load(&archive,argv[3],budget,&value);rf_vpp_close(&archive);
        memset(&archive,0xa5,sizeof(archive));
        if(status && memcmp(&value,&before,sizeof(value)))return 4;if(status)return 3;
        for(i=0;i<sizeof(value);++i)printf("%02x",((const unsigned char*)&value)[i]);
        printf("\n");return 0;
    }
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
    if(argc==4 && !strcmp(argv[1],"--surface")) {
        rf_surface_materials palette,before;uint32_t index;
        memset(&palette,0xa5,sizeof(palette));before=palette;
        if(rf_vpp_open(&archive,argv[2]) || rf_vpp_find(&archive,"materials.tbl",&entry) || entry.size>65536)return 2;
        text=malloc(entry.size);if(!text)return 2;
        status=rf_vpp_read(&archive,&entry,0,text,entry.size);
        if(!status)status=rf_surface_materials_read(text,entry.size,&palette);
        free(text);rf_vpp_close(&archive);
        if(status && memcmp(&palette,&before,sizeof(palette)))return 4;
        if(status)return 3;
        index=rf_surface_material_lookup(&palette,argv[3]);
        printf("%u %.9g\n",index,(double)palette.materials[index].traction);return 0;
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
    if(argc==6 && !strcmp(argv[1],"--class")) {
        rf_vpp meshes;rf_entity_skeletal_assets value,before;
        if(rf_vpp_open(&meshes,argv[3]))return 2;
        memset(&value,0xa5,sizeof(value));before=value;
        status=rf_entity_skeletal_assets_load(argv[2],argv[4],argv[5],&meshes,512*1024,&value);
        if(!status) {
            printf("%s %u %u\n",value.mesh.name,value.mesh.offset,value.mesh.size);
            puts(value.assets.model);
            for(i=0;i<value.assets.texture_count;++i)puts(value.assets.textures[i]);
        } else {if(memcmp(&value,&before,sizeof(value)))return 4;printf("%d\n",status);}
        rf_vpp_close(&meshes);return status?3:0;
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
