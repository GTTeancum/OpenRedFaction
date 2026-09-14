#include "rf/level.h"
#include "rf/event.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#define CHECK(x) do{if(!(x)){fprintf(stderr,"message line %u\n",(unsigned)__LINE__);return 1;}}while(0)
static uint32_t calls,last_on,last_uid;static int32_t last_clock;static int message_status;
static int receive_message(void *context,const rf_level_event *event,int32_t now,uint32_t on)
{
    rf_level_message value;
    ++calls;last_on=on;last_uid=event->uid;last_clock=now;
    if(message_status)return message_status;
    return on?rf_level_message_read(context,event->words[0],&value):RF_OK;
}
static int dispatch_check(rf_level *level)
{
    rf_object_registry registry;rf_runtime_events events={0};rf_runtime_triggers triggers={0};
    rf_physics_gravity gravity={0};rf_startup_events_report report;rf_runtime_event *message=NULL;
    uint32_t i,pending;rf_runtime_trigger speaker={0};
    rf_object_registry_init(&registry);triggers.registry=&registry;
    CHECK(rf_runtime_events_open(level,&registry,1024*1024,&events)==RF_OK);
    for(i=0;i<events.count;i++)if(events.items[i].authored->record.uid==8356)message=events.items+i;
    CHECK(message && message->state.type==15 && message->authored->record.words[0]==0);
    /* Resolve the authored speaker link to a contained trigger: a message must
     * never enable it as if it were a downstream event action. */
    message->state.delay=0; /* Isolate immediate dispatch before the delayed case. */
    speaker.object_kind=5;speaker.state.flags=16;
    CHECK(rf_object_registry_insert(&registry,&speaker,&speaker.handle)==RF_OK);
    CHECK(message->authored->record.link_count==1);
    message->links[0].kind=1;message->links[0].value=speaker.handle;
    CHECK(rf_runtime_event_fire(&triggers,message->handle,7,9,100,&gravity,0,0,&report)==RF_OK);
    CHECK(report.unsupported_actions==1 && (speaker.state.flags&16));
    triggers.show_message=receive_message;triggers.message_context=level;
    CHECK(rf_runtime_event_fire(&triggers,message->handle,7,9,200,&gravity,0,0,&report)==RF_OK);
    CHECK(calls==1 && last_on==1 && last_uid==8356 && last_clock==200 && !report.unsupported_actions);
    CHECK(speaker.state.flags&16);
    message->state.delay=.25f;
    CHECK(rf_runtime_event_fire(&triggers,message->handle,7,9,300,&gravity,0,0,&report)==RF_OK && calls==1);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,549,0,0,&report,&pending)==RF_OK && calls==1);
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,550,0,0,&report,&pending)==RF_OK && calls==2 && last_clock==550 && !pending);
    message->state.deadline=600;message->state.mode=0;
    CHECK(rf_runtime_events_tick(&events,&triggers,&gravity,600,0,0,&report,&pending)==RF_OK && calls==3 && !last_on);
    message->state.delay=0;message_status=RF_NOT_FOUND;
    CHECK(rf_runtime_event_fire(&triggers,message->handle,7,9,700,&gravity,0,0,&report)==RF_OK && report.other_targets==1);
    message_status=RF_FORMAT;
    CHECK(rf_runtime_event_fire(&triggers,message->handle,7,9,800,&gravity,0,0,&report)==RF_FORMAT);
    rf_runtime_events_close(&events);return 0;
}
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
    CHECK(dispatch_check(&level)==0);
    rf_vpp_close(&archive);puts("PASS bounded mission dialogue reader");return 0;
}
