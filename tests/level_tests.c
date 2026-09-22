#include "rf/level.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static unsigned char image[6144];
static void word(unsigned offset, uint32_t value)
{
    unsigned i;
    for (i = 0; i < 4; ++i) image[offset + i] = (unsigned char)(value >> (i * 8));
}
static int run(rf_level *level, rf_vpp *archive)
{
    FILE *file = fopen("level-fixture.tmp", "wb");
    int result;
    if (!file) return RF_IO;
    if (fwrite(image, 1, sizeof(image), file) != sizeof(image)) { fclose(file); return RF_IO; }
    if (fclose(file)) return RF_IO;
    result = rf_vpp_open(archive, "level-fixture.tmp");
    if (result != RF_OK) return result;
    return rf_level_open(level, archive, "test.rfl");
}
int main(void)
{
    rf_vpp archive;
    rf_level level;
    unsigned char byte;
    const rf_level_section *section;
    word(0, 0x51890ace); word(4, 1); word(8, 1); word(12, sizeof(image));
    memcpy(image + 2048, "test.rfl", 9); word(2108, 112);
    word(4096, 0xd4bada55); word(4100, 180); word(4108, 36); word(4112, 92); word(4116, 2);
    image[4124] = 4; memcpy(image + 4126, "Test", 4);
    word(4132, 0x70000); word(4136, 48);
    /* Position and deliberately distinct matrix rows test disk ordering. */
    word(4140, 0x3f800000); word(4152, 0x40000000); word(4164, 0x40400000); word(4176, 0x40800000);
    word(4188, 0x1000000); word(4192, 4);
    CHECK(run(&level, &archive) == RF_OK);
    CHECK(level.player_position[0] == 1 && level.player_orientation[0][0] == 3 && level.player_orientation[1][0] == 4 && level.player_orientation[2][0] == 2);
    section = rf_level_find(&level, 0x70000); CHECK(section != NULL);
    CHECK(rf_level_read(&level, section, 48, &byte, 1) == RF_RANGE);
    CHECK(rf_level_read(&level, section, UINT32_MAX, &byte, 1) == RF_RANGE);
    rf_vpp_close(&archive);
    word(4136, UINT32_MAX); CHECK(run(&level, &archive) != RF_OK && !level.archive); rf_vpp_close(&archive); word(4136, 48);
    word(4116, 129); CHECK(run(&level, &archive) == RF_RANGE && !level.archive); rf_vpp_close(&archive); word(4116, 2);
    word(4108, 37); CHECK(run(&level, &archive) == RF_FORMAT); rf_vpp_close(&archive); word(4108, 36);
    word(4188, 0x70000); CHECK(run(&level, &archive) == RF_FORMAT); rf_vpp_close(&archive); word(4188, 0x1000000);
    word(4140, 0x7fc00000); CHECK(run(&level, &archive) == RF_FORMAT); rf_vpp_close(&archive); word(4140, 0x3f800000);
    word(4200, 1); CHECK(run(&level, &archive) == RF_FORMAT); rf_vpp_close(&archive); word(4200, 0);
    word(2108, 111); CHECK(run(&level, &archive) != RF_OK); rf_vpp_close(&archive);
    /* One minimal variable-length entity after level info, before the end marker. */
    {
        unsigned char raw[156]={0};rf_level_owned_entity item={0};rf_level_entity_spawn fields,saved;
        unsigned cut,flag;raw[0]=123;raw[57]=9;raw[61]=255;raw[62]=255;raw[63]=255;raw[64]=255;
        raw[65]=0xef;raw[66]=0xbe;item.raw=raw;item.record.uid=123;item.record.bytes=155;
        for(flag=0;flag<4;++flag) {
            raw[135]=(unsigned char)(flag?flag==1?1:flag==2?2:255:0);raw[149]=raw[135];
            CHECK(rf_level_entity_spawn_read(&item,&fields)==RF_OK);
            CHECK(fields.relationship_51c==9 && fields.friendliness==0xffffffffu && fields.byte_28==0xef && fields.creation_flags==(flag?6u:0u));
        }
        memset(&fields,0xa5,sizeof(fields));saved=fields;
        for(cut=0;cut<155;++cut) {
            item.record.bytes=cut;CHECK(rf_level_entity_spawn_read(&item,&fields)!=RF_OK);
            CHECK(!memcmp(&fields,&saved,sizeof(fields)));
        }
        item.record.bytes=156;CHECK(rf_level_entity_spawn_read(&item,&fields)==RF_FORMAT && !memcmp(&fields,&saved,sizeof(fields)));
        item.record.bytes=155;raw[150]=2;
        CHECK(rf_level_entity_spawn_read(&item,&fields)==RF_FORMAT && !memcmp(&fields,&saved,sizeof(fields)));
    }
    word(2108,279);word(4116,3);word(4200,0x30000);word(4204,159);word(4208,1);word(4212,123);
    word(4218,0x3f800000);word(4230,0x40000000);word(4242,0x40400000);word(4254,0x40800000);
    CHECK(run(&level,&archive)==RF_OK);
    {
        rf_level_entity_reader reader,before,start;rf_level_entity entity,saved;unsigned cut;
        CHECK(rf_level_entities_begin(&level,&reader)==RF_OK);start=reader;
        CHECK(rf_level_entity_next(&reader,&entity)==RF_OK);
        CHECK(entity.uid==123 && entity.position[0]==1 && entity.orientation[0][0]==3 && entity.orientation[1][0]==4 && entity.orientation[2][0]==2);
        CHECK(entity.offset==4 && entity.bytes==155);
        CHECK(rf_level_entity_find(&level,123,&entity)==RF_OK && entity.uid==123);
        CHECK(rf_level_entity_next(&reader,&entity)==RF_NOT_FOUND);
        memset(&entity,0xa5,sizeof(entity));saved=entity;
        CHECK(rf_level_entity_find(&level,124,&entity)==RF_NOT_FOUND && !memcmp(&entity,&saved,sizeof(entity)));
        for(cut=4;cut<159;++cut) {
            rf_level truncated=level;rf_level_owned_entities owned={0},empty={0};
            unsigned section_index=(unsigned)(rf_level_find(&level,0x30000)-level.sections);
            reader=start;reader.section.size=cut;before=reader;
            CHECK(rf_level_entity_next(&reader,&entity)!=RF_OK);
            CHECK(!memcmp(&reader,&before,sizeof(reader)) && !memcmp(&entity,&saved,sizeof(entity)));
            truncated.sections[section_index].size=cut;
            CHECK(rf_level_owned_entities_open(&truncated,1024*1024,&owned)!=RF_OK);
            CHECK(!memcmp(&owned,&empty,sizeof(owned)));
        }
    }
    rf_vpp_close(&archive);
    word(4218,0x7fc00000);CHECK(run(&level,&archive)==RF_OK);
    {rf_level_entity_reader r;rf_level_entity e;CHECK(rf_level_entities_begin(&level,&r)==RF_OK);CHECK(rf_level_entity_next(&r,&e)==RF_FORMAT);}
    rf_vpp_close(&archive);word(4218,0x3f800000);
    image[4217]=1;CHECK(run(&level,&archive)==RF_OK);
    {rf_level_entity_reader r;rf_level_entity e;CHECK(rf_level_entities_begin(&level,&r)==RF_OK);CHECK(rf_level_entity_next(&r,&e)==RF_RANGE);}
    rf_vpp_close(&archive);image[4217]=0;
    image[4362]=2;CHECK(run(&level,&archive)==RF_OK);
    {rf_level_entity_reader r;rf_level_entity e;CHECK(rf_level_entities_begin(&level,&r)==RF_OK);CHECK(rf_level_entity_next(&r,&e)==RF_FORMAT);}
    rf_vpp_close(&archive);
    image[4362]=0;memcpy(image+4367,image+4212,155);
    word(2108,434);word(4204,314);word(4208,2);
    CHECK(run(&level,&archive)==RF_OK);
    {rf_level_entity e,saved;memset(&e,0xa5,sizeof(e));saved=e;
     CHECK(rf_level_entity_find(&level,123,&e)==RF_FORMAT && !memcmp(&e,&saved,sizeof(e)));}
    rf_vpp_close(&archive);word(4367,124);word(4373,0x7fc00000);
    CHECK(run(&level,&archive)==RF_OK);
    {rf_level_entity e,saved;rf_level_owned_entities owned={0},empty={0};memset(&e,0xa5,sizeof(e));saved=e;
     CHECK(rf_level_entity_find(&level,123,&e)==RF_FORMAT && !memcmp(&e,&saved,sizeof(e)));
     CHECK(rf_level_owned_entities_open(&level,1024*1024,&owned)==RF_FORMAT && !memcmp(&owned,&empty,sizeof(owned)));}
    rf_vpp_close(&archive);
    /* Instance weapon names precede state/skin strings in the seven-name block. */
    {
        const char *primary[]={"Rocket Launcher","Grenade","","none"};
        const char *secondary[]={"none","Fighter Rocket","","none"};
        unsigned test;
        for(test=0;test<4;++test) {
            unsigned p=(unsigned)strlen(primary[test]),s=(unsigned)strlen(secondary[test]);
            unsigned raw_bytes=155+p+s,at=4212+102;
            rf_level_entity entity;rf_level_entity_reader reader,before;rf_level_entity saved;
            rf_level_owned_entities owned={0};
            memset(image+4200,0,sizeof(image)-4200);
            word(2108,124+raw_bytes);word(4200,0x30000);word(4204,4+raw_bytes);word(4208,1);word(4212,123);
            image[at]=(unsigned char)p;memcpy(image+at+2,primary[test],p);at+=2+p;
            image[at]=(unsigned char)s;memcpy(image+at+2,secondary[test],s);
            CHECK(run(&level,&archive)==RF_OK);
            CHECK(rf_level_entities_begin(&level,&reader)==RF_OK);
            CHECK(rf_level_entity_next(&reader,&entity)==RF_OK);
            CHECK(!strcmp(entity.primary_weapon,primary[test]) && !strcmp(entity.secondary_weapon,secondary[test]));
            CHECK(entity.bytes==raw_bytes && !entity.state_animation[0] && !entity.skin[0]);
            CHECK(rf_level_owned_entities_open(&level,65536,&owned)==RF_OK);
            rf_vpp_close(&archive);
            CHECK(!strcmp(owned.items[0].record.primary_weapon,primary[test]));
            CHECK(!strcmp(owned.items[0].record.secondary_weapon,secondary[test]));
            rf_level_owned_entities_close(&owned);
            /* A truncated named record cannot publish partial names or reader state. */
            CHECK(run(&level,&archive)==RF_OK);
            CHECK(rf_level_entities_begin(&level,&reader)==RF_OK);--reader.section.size;before=reader;
            memset(&entity,0xa5,sizeof(entity));saved=entity;
            CHECK(rf_level_entity_next(&reader,&entity)!=RF_OK);
            CHECK(!memcmp(&entity,&saved,sizeof(entity)) && !memcmp(&reader,&before,sizeof(reader)));
            rf_vpp_close(&archive);
        }
    }
    {
        unsigned char raw[159]={0};rf_level_owned_entity entity={0};
        rf_level_entity_vitals value,saved;uint32_t bits;unsigned i,j;
        const float cases[][2]={{-1,-1},{0,0},{-2,37.5f},{50,50},{10000,10000}};
        /* Three-byte class name moves vitals away from the minimal-record offsets. */
        raw[0]=123;raw[4]=3;memcpy(raw+6,"npc",3);
        entity.raw=raw;entity.record.bytes=158;entity.record.uid=0x70000001;
        for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++){
            memcpy(&bits,&cases[i][0],4);for(j=0;j<4;j++)raw[93+j]=(unsigned char)(bits>>(j*8));
            memcpy(&bits,&cases[i][1],4);for(j=0;j<4;j++)raw[97+j]=(unsigned char)(bits>>(j*8));
            CHECK(rf_level_entity_vitals_read(&entity,&value)==RF_OK);
            CHECK(value.health==cases[i][0] && value.armor==cases[i][1]);
        }
        memset(&value,0xa5,sizeof(value));saved=value;
        entity.record.bytes=157;CHECK(rf_level_entity_vitals_read(&entity,&value)!=RF_OK);
        CHECK(!memcmp(&value,&saved,sizeof(value)));
        entity.record.bytes=159;CHECK(rf_level_entity_vitals_read(&entity,&value)==RF_FORMAT);
        CHECK(!memcmp(&value,&saved,sizeof(value)));entity.record.bytes=158;
        for(i=0;i<2;i++){
            unsigned at=i?97:93;unsigned char old[4];memcpy(old,raw+at,4);
            bits=i?0x7f800000u:0x7fc00000u;for(j=0;j<4;j++)raw[at+j]=(unsigned char)(bits>>(j*8));
            CHECK(rf_level_entity_vitals_read(&entity,&value)==RF_FORMAT && !memcmp(&value,&saved,sizeof(value)));
            memcpy(raw+at,old,4);
        }
        raw[153]=2;CHECK(rf_level_entity_vitals_read(&entity,&value)==RF_FORMAT);
        CHECK(!memcmp(&value,&saved,sizeof(value)));raw[153]=0;
        CHECK(rf_level_entity_vitals_read(NULL,&value)==RF_RANGE);
        CHECK(rf_level_entity_vitals_read(&entity,NULL)==RF_RANGE);
        {rf_level_entity_spawn spawn;CHECK(rf_level_entity_spawn_read(&entity,&spawn)==RF_FORMAT);}
    }
    CHECK(remove("level-fixture.tmp") == 0);
    puts("Level bounds, invalid headers, duplicate sections, truncation and spawn ordering passed");
    return 0;
}
