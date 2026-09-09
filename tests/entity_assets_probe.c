#include "rf/entity_assets.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
int main(int argc,char **argv)
{
    rf_vpp archive;rf_vpp_entry entry;rf_entity_assets assets;char *text;int status;uint32_t i;
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
    if(!status) {printf("%s\n",assets.model);for(i=0;i<assets.texture_count;++i)printf("%s\n",assets.textures[i]);}
    free(text);rf_vpp_close(&archive);return status?3:0;
}
