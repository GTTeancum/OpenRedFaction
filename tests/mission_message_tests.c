#include "rf/level.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#define CHECK(x) do{if(!(x)){fprintf(stderr,"message line %u\n",(unsigned)__LINE__);return 1;}}while(0)
int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--parse")) {
        uint32_t header[2];
#ifdef _WIN32
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
#endif
        while(fread(header,sizeof(header),1,stdin)==1) {
            rf_level_message message={0};void *data;int32_t status;
            if(header[0]>65536)return 2;data=malloc(header[0]?header[0]:1);if(!data)return 3;
            if(fread(data,1,header[0],stdin)!=header[0]){free(data);return 4;}
            status=rf_level_message_parse(data,header[0],header[1],&message);free(data);
            if(fwrite(&status,4,1,stdout)!=1 || fwrite(&message,sizeof(message),1,stdout)!=1)return 5;
        }
        return ferror(stdin)?6:0;
    }
    const char data[]="0 \"zero.wav\"\r\r\nEn: \"Hello\\nParker\"\nGr: \"skip\"\n1 \"one.wav\"\nFr: \"bonjour\"\nEn: \"Second\"";
    rf_level_message result={0},saved;rf_vpp archive={0};rf_level level;
    CHECK(rf_level_message_parse(data,sizeof(data)-1,0,&result)==RF_OK && !strcmp(result.text,"Hello\nParker"));
    CHECK(!strcmp(result.voice,"zero.wav") && result.id==0);saved=result;
    CHECK(rf_level_message_parse(data,sizeof(data)-1,2,&result)==RF_NOT_FOUND && !memcmp(&saved,&result,sizeof(result)));
    CHECK(rf_level_message_parse(data,sizeof(data)-2,1,&result)==RF_FORMAT && !memcmp(&saved,&result,sizeof(result)));
    {const char duplicate[]="0 \"a\" En: \"one\" 0 \"b\" En: \"two\"";
     CHECK(rf_level_message_parse(duplicate,sizeof(duplicate)-1,0,&result)==RF_FORMAT);}
    {char huge[700];memset(huge,'a',sizeof(huge));memcpy(huge,"0 \"a\" En: \"",11);huge[698]='"';huge[699]=0;
     CHECK(rf_level_message_parse(huge,699,0,&result)==RF_RANGE);}
    CHECK(argc==2);CHECK(rf_level_campaign_open(&level,&archive,argv[1],"L1S1.rfl")==RF_OK);
    CHECK(rf_level_message_read(&level,0,&result)==RF_OK && !strcmp(result.voice,"L1S1_GRD_01.wav"));
    CHECK(!strcmp(result.text,"Hey, where do you think you're goin'?"));
    rf_vpp_close(&archive);puts("PASS bounded mission dialogue reader");return 0;
}
