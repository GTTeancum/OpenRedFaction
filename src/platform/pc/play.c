#include "rf/event.h"
#include "rf/resource_budget.h"
/* Port-owned Windows frontend. Reads only messages addressed to its window.
 * The headless replay calls the shared provider directly; no OS input injection. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "rf/scene_preview.h"
#include "rf/physics.h"
#include "rf/frame_clock.h"
#include "pc_raster.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char rf_material_failure_name[61];
extern uint32_t rf_material_failure[3];
extern uint32_t rf_scene_actor_live_enabled;
extern uint32_t rf_scene_actor_follow_summary[5];
extern uint32_t rf_scene_player_input_frames[64][7];
extern uint32_t rf_preview_failure[8],rf_animation_progress[4];
extern rf_physics_body scene_actor_body;
extern rf_startup_events_report rf_scene_startup_events;
extern uint32_t rf_scene_startup_gravity[4];
extern uint32_t rf_scene_campaign_events[3],rf_scene_campaign_triggers[2],rf_scene_campaign_links[4];
extern uint32_t rf_scene_campaign_event_links[4];
extern uint32_t rf_scene_actor_initial_animation[12],rf_scene_player_climb[8],rf_scene_player_climb_frames[128][9];
extern float rf_scene_actor_initial_eye_offsets[6];
extern rf_physics_stance_cache rf_scene_actor_stance_cache;
extern uint32_t rf_scene_actor_selector_frames[64][8],rf_scene_actor_locomotion_frames[64][12];

typedef struct player {
    HWND window;
    unsigned char keys[256];
    unsigned char *dib;
    BITMAPINFO bitmap;
    rf_pc_raster raster;
    rf_lightmaps lightmaps;
    rf_frame_clock clock;
    LARGE_INTEGER frequency;
    uint32_t frames,headless;
    rf_scene_input *replay;uint32_t replay_count;
    int quit;
} player;

static uint32_t milliseconds(const player *p)
{
    LARGE_INTEGER now;uint64_t ticks,hz;
    QueryPerformanceCounter(&now);ticks=(uint64_t)now.QuadPart;hz=(uint64_t)p->frequency.QuadPart;
    return (uint32_t)((ticks/hz)*1000+(ticks%hz)*1000/hz);
}

static void paint(player *p,HDC dc)
{
    RECT rect;int w,h,x,y;
    GetClientRect(p->window,&rect);
    FillRect(dc,&rect,(HBRUSH)GetStockObject(BLACK_BRUSH));
    w=rect.right;h=rect.bottom;
    if(w<=0 || h<=0)return;
    if((int64_t)w*3>(int64_t)h*4)w=h*4/3;else h=w*3/4;
    x=(rect.right-w)/2;y=(rect.bottom-h)/2;
    StretchDIBits(dc,x,y,w,h,0,0,640,480,p->dib,&p->bitmap,DIB_RGB_COLORS,SRCCOPY);
}

static LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM key,LPARAM data)
{
    player *p=(player*)GetWindowLongPtrW(window,GWLP_USERDATA);
    if(message==WM_NCCREATE) {
        p=(player*)((CREATESTRUCTW*)data)->lpCreateParams;
        p->window=window;SetWindowLongPtrW(window,GWLP_USERDATA,(LONG_PTR)p);
    }
    if(p)switch(message) {
    case WM_KEYDOWN:
        if(key<256)p->keys[key]=1;
        if(key==VK_ESCAPE)p->quit=1;
        return 0;
    case WM_KEYUP:if(key<256)p->keys[key]=0;return 0;
    case WM_KILLFOCUS:memset(p->keys,0,sizeof(p->keys));return 0;
    case WM_CLOSE:p->quit=1;return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;HDC dc=BeginPaint(window,&ps);paint(p,dc);EndPaint(window,&ps);return 0;
    }
    case WM_ERASEBKGND:return 1;
    }
    return DefWindowProcW(window,message,key,data);
}

static int input(void *context,uint32_t frame,rf_scene_input *out)
{
    player *p=context;MSG message;uint32_t wait;
    memset(out,0,sizeof(*out));
    if(p->headless) {
        if(p->replay){if(frame>=p->replay_count)return RF_RANGE;*out=p->replay[frame];return RF_OK;}
        /* Same deterministic route as rf_scene_check --input. */
        if(frame>=24 && frame<48)out->move[0]=.25f;
        if(frame>=63)out->move[0]=1;
        out->look[0]=frame?((frame%180)<90?.25f:-.25f):0;
        out->look[1]=frame?((frame%240)<120?.2f:-.2f):0;
        out->crouch=frame>=32 && frame<56;return RF_OK;
    }
    /* Continue pumping our own messages while waiting for a simulation tick. */
    for(;;) {
        while(PeekMessageW(&message,p->window,0,0,PM_REMOVE)) {
            TranslateMessage(&message);DispatchMessageW(&message);
        }
        if(p->quit)return RF_NOT_FOUND;
        wait=rf_frame_clock_step(&p->clock,milliseconds(p));
        if(!wait)break;
        Sleep(wait>2?2:wait);
    }
    out->move[0]=(float)p->keys['D']-(float)p->keys['A'];
    out->move[2]=(float)p->keys['W']-(float)p->keys['S'];
    if(out->move[0] && out->move[2]) {out->move[0]*=.7071067811865475f;out->move[2]*=.7071067811865475f;}
    out->look[0]=(float)p->keys[VK_UP]-(float)p->keys[VK_DOWN];
    out->look[1]=(float)p->keys[VK_RIGHT]-(float)p->keys[VK_LEFT];
    out->jump=p->keys[VK_SPACE];out->crouch=p->keys[VK_CONTROL];return RF_OK;
}

static int present(void *context,uint32_t frame,const rf_preview_mesh *mesh,
    const rf_materials *materials,uint32_t world)
{
    player *p=context;uint32_t i;int status;
    if(frame!=p->frames || mesh->bytes>RF_SCENE_FOLLOW_CAPACITY)return RF_RANGE;
    /* Recorded-input diagnosis projects every tick, rasterizes only the last. */
    if(p->replay && frame+1<p->replay_count){++p->frames;return RF_OK;}
    if(!p->headless && !rf_frame_clock_present(&p->clock,milliseconds(p))){++p->frames;return RF_OK;}
    status=rf_pc_raster_frame(&p->raster,mesh,materials,&p->lightmaps,world);
    if(status)return status;
    ++p->frames;
    if(!p->headless) {
        for(i=0;i<p->raster.pixels;++i) {
            p->dib[i*4]=p->raster.rgb[i*3+2];
            p->dib[i*4+1]=p->raster.rgb[i*3+1];
            p->dib[i*4+2]=p->raster.rgb[i*3];
        }
        InvalidateRect(p->window,NULL,FALSE);UpdateWindow(p->window);
    }
    return RF_OK;
}

static int path_join(char *out,size_t size,const char *directory,const char *name)
{
    int n=snprintf(out,size,"%s/%s",directory,name);
    return n<0 || (size_t)n>=size?RF_RANGE:RF_OK;
}

int main(int argc,char **argv)
{
    const char *map_names[]={"maps1.vpp","maps2.vpp","maps3.vpp","maps4.vpp","maps_en.vpp"};
    const char *directory;char path[4096],meshes[4096],motions[4096],tables[4096];
    player p={0};rf_vpp archive={0},maps[5]={{0}};rf_level level;
    rf_geometry geometry={0};rf_geometry_collision_world collision={0};
    rf_scene_world_geometry retained={0};rf_preview_mesh mesh={0};rf_materials materials={0};
    uint32_t opened=0,i,limit=0;int status=RF_OK,spawn_profile=0;WNDCLASSW wc={0};
    if(argc==5 && !strcmp(argv[1],"--headless")) {
        char *end;unsigned long value=strtoul(argv[3],&end,10);
        if(!argv[3][0] || *end || value<1 || value>60000)return 2;
        p.headless=1;limit=(uint32_t)value;directory=argv[2];
    } else if(argc==5 && (!strcmp(argv[1],"--replay") || !strcmp(argv[1],"--spawn-replay"))) {
        spawn_profile=!strcmp(argv[1],"--spawn-replay");
        FILE *file=fopen(argv[3],"rb");uint32_t count,size;int read_failed=0;
        if(!file)return 2;
        if(rf_scene_replay_header(file,&count,&size)){fclose(file);return 2;}
        p.replay=calloc(count,sizeof(*p.replay));if(!p.replay){fclose(file);return 1;}
        for(i=0;i<count;++i)if(fread(p.replay+i,size,1,file)!=1){read_failed=1;break;}
        if(fclose(file) || read_failed){free(p.replay);return 2;}
        p.headless=1;limit=p.replay_count=count;directory=argv[2];
    } else if(argc==3 && !strcmp(argv[1],"--campaign")){spawn_profile=1;directory=argv[2];}
    else if(argc==2)directory=argv[1];
    else {fprintf(stderr,"Usage: rf_pc_play <Installed_Game>\n       rf_pc_play --campaign <Installed_Game>\n       rf_pc_play --headless <Installed_Game> <frames 1..60000> <output.ppm>\n       rf_pc_play --replay <Installed_Game> <inputs.bin> <output.ppm>\n       rf_pc_play --spawn-replay <Installed_Game> <inputs.bin> <output.ppm>\n");return 2;}
#define CHECK(call) do {status=(call);if(status){fprintf(stderr,"%s failed (%d)\n",#call,status);goto cleanup;}} while(0)
    CHECK(path_join(path,sizeof(path),directory,spawn_profile && p.headless && getenv("RF_REPLAY_ARCHIVE")?getenv("RF_REPLAY_ARCHIVE"):"levels1.vpp"));
    CHECK(rf_vpp_open(&archive,path));CHECK(rf_level_open(&level,&archive,spawn_profile && p.headless && getenv("RF_REPLAY_LEVEL")?getenv("RF_REPLAY_LEVEL"):"L1S1.rfl"));
    CHECK(path_join(meshes,sizeof(meshes),directory,"meshes.vpp"));
    CHECK(path_join(motions,sizeof(motions),directory,"motions.vpp"));
    CHECK(path_join(tables,sizeof(tables),directory,"tables.vpp"));
    if(spawn_profile && p.headless && getenv("RF_REPLAY_REGION_START")) {
        CHECK(rf_scene_stage_climb(&level,!strcmp(getenv("RF_REPLAY_REGION_START"),"2")?2:1));
    }
    if(spawn_profile)CHECK(rf_scene_set_campaign_spawn(&level));
    else CHECK(rf_scene_preview_route_camera(&level,9858));
    CHECK(rf_geometry_open(&geometry,&level,8*1024*1024));
    CHECK(rf_geometry_collision_world_open(&geometry,8*1024*1024,&collision));
    for(i=0;i<5;++i) {
        CHECK(path_join(path,sizeof(path),directory,map_names[i]));
        CHECK(rf_vpp_open(maps+i,path));++opened;
    }
    CHECK(rf_scene_world_open_retained(&level,&geometry,maps,opened,&mesh,&materials,
        8*1024*1024,RF_CAMPAIGN_MATERIAL_BUDGET,&retained));
    CHECK(rf_lightmaps_open(&p.lightmaps,&level,RF_CAMPAIGN_LIGHTMAP_BUDGET));
    CHECK(rf_pc_raster_open(&p.raster,1));
    if(!p.headless) {
        RECT rect={0,0,960,720};
        p.dib=calloc(640*480,4);if(!p.dib){status=RF_RANGE;goto cleanup;}
        p.bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        p.bitmap.bmiHeader.biWidth=640;p.bitmap.bmiHeader.biHeight=-480;
        p.bitmap.bmiHeader.biPlanes=1;p.bitmap.bmiHeader.biBitCount=32;
        p.bitmap.bmiHeader.biCompression=BI_RGB;
        wc.lpfnWndProc=window_proc;wc.hInstance=GetModuleHandleW(NULL);
        wc.lpszClassName=L"RFReconstructionPlayer";wc.hCursor=LoadCursor(NULL,IDC_ARROW);
        if(!RegisterClassW(&wc) || !QueryPerformanceFrequency(&p.frequency)){status=RF_IO;goto cleanup;}
        AdjustWindowRect(&rect,WS_OVERLAPPEDWINDOW,FALSE);
        p.window=CreateWindowExW(0,wc.lpszClassName,L"Red Faction reconstruction - PC input prototype",
            WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,rect.right-rect.left,rect.bottom-rect.top,
            NULL,NULL,wc.hInstance,&p);
        if(!p.window){status=RF_IO;goto cleanup;}
        ShowWindow(p.window,SW_SHOW);
        puts(spawn_profile?"WASD move | arrows look | Ctrl crouch | Space jump | Escape exit":"WASD move | arrows look | Ctrl crouch | Escape exit");
    }
    rf_scene_actor_live_enabled=1;rf_scene_actor_eye_enabled=1;
    rf_scene_actor_look_enabled=1;rf_scene_actor_turn_enabled=1;
    rf_scene_actor_drive(1);rf_scene_actor_follow(&retained);rf_scene_set_input(input,&p,limit);
    CHECK(rf_scene_stream_miner_body(&level,9858,meshes,motions,tables,maps,opened,&mesh,&materials,
        8*1024*1024,RF_CAMPAIGN_MATERIAL_BUDGET,present,&p,&collision,&geometry));
    if(p.headless) {
        if(p.frames!=limit){status=RF_FORMAT;goto cleanup;}
        if(spawn_profile){
            {uint32_t words[13],k;memcpy(words,&rf_scene_startup_events,sizeof(rf_scene_startup_events));
             memcpy(words+9,rf_scene_startup_gravity,16);printf("CAMPAIGN_STARTUP");
             for(k=0;k<13;++k)printf(" %u",words[k]);puts("");}
            printf("CAMPAIGN_LINKS %u %u %u %u\n",rf_scene_campaign_links[0],rf_scene_campaign_links[1],rf_scene_campaign_links[2],rf_scene_campaign_links[3]);
            printf("CAMPAIGN_EVENT_LINKS %u %u %u %u\n",rf_scene_campaign_event_links[0],rf_scene_campaign_event_links[1],rf_scene_campaign_event_links[2],rf_scene_campaign_event_links[3]);
            printf("CAMPAIGN_TRIGGERS %u %u\n",rf_scene_campaign_triggers[0],rf_scene_campaign_triggers[1]);
            printf("CAMPAIGN_EVENTS %u %u %u\n",rf_scene_campaign_events[0],rf_scene_campaign_events[1],rf_scene_campaign_events[2]);
            const void *records[7]={rf_scene_actor_initial_animation,rf_scene_actor_initial_eye_offsets,&rf_scene_actor_stance_cache,rf_scene_actor_selector_frames,rf_scene_actor_locomotion_frames,rf_scene_player_climb,rf_scene_player_climb_frames};
            const char *labels[7]={"PLAYER_INITIAL_ANIMATION","PLAYER_CLASS_EYE","PLAYER_CLASS_STANCE","PLAYER_STANCE_FRAMES","PLAYER_MOTION_FRAMES","PLAYER_CLIMB","PLAYER_CLIMB_FRAMES"};
            const uint32_t sizes[7]={12,6,sizeof(rf_scene_actor_stance_cache)/4,512,768,8,1152};uint32_t j;
            printf("PLAYER_SPAWN");for(i=0;i<19;++i)printf(" %u",rf_scene_player_spawn_diagnostic[i]);puts("");
            for(j=0;j<7;++j){printf("%s",labels[j]);for(i=0;i<sizes[j];++i){uint32_t word;memcpy(&word,(const char*)records[j]+i*4,4);printf(" %u",word);}puts("");}
        }
        CHECK(rf_pc_raster_save(&p.raster,argv[4]));
        printf("ACTOR_FOLLOW_SUMMARY");for(i=0;i<5;++i)printf(" %u",rf_scene_actor_follow_summary[i]);puts("");
        printf("PLAYER_JUMP");for(i=0;i<4;++i)printf(" %u",rf_scene_player_jump[i]);puts("");
        printf("PLAYER_JUMP_FRAMES");for(i=0;i<1024;++i)printf(" %u",((uint32_t*)rf_scene_player_jump_frames)[i]);puts("");
        printf("ACTOR_PLAYER_INPUT");for(i=0;i<64*7;++i)printf(" %u",((uint32_t*)rf_scene_player_input_frames)[i]);puts("");
        printf("PC_PLAY_BODY");for(i=0;i<sizeof(scene_actor_body.state)/4;++i) {
            uint32_t word;memcpy(&word,(const unsigned char*)&scene_actor_body.state+i*4,4);printf(" %u",word);
        }puts("");
    }
    printf("Completed %u frames, 640x480 raster, %u byte mesh cap.\n",p.frames,RF_SCENE_FOLLOW_CAPACITY);
cleanup:
    if(status) {
        fprintf(stderr,"MATERIAL_FAILURE %s %u %u %u\n",rf_material_failure_name,rf_material_failure[0],rf_material_failure[1],rf_material_failure[2]);
        fprintf(stderr,"ANIMATION_PROGRESS");for(i=0;i<4;++i)fprintf(stderr," %u",rf_animation_progress[i]);
        fprintf(stderr,"\nSCENE_STAGE %u %u\nPREVIEW_FAILURE",rf_scene_profile_stage[0],rf_scene_profile_stage[1]);
        for(i=0;i<8;++i)fprintf(stderr," %u",rf_preview_failure[i]);fprintf(stderr,"\n");
    }
    free(p.replay);
    rf_scene_set_input(NULL,NULL,0);rf_scene_actor_follow(NULL);
    if(p.window)DestroyWindow(p.window);
    if(wc.lpszClassName)UnregisterClassW(wc.lpszClassName,wc.hInstance);
    free(p.dib);rf_pc_raster_close(&p.raster);rf_lightmaps_close(&p.lightmaps);
    rf_materials_close(&materials);rf_preview_close(&mesh);rf_scene_world_geometry_close(&retained);
    rf_geometry_collision_world_close(&collision);rf_geometry_close(&geometry);
    while(opened)rf_vpp_close(maps+--opened);rf_vpp_close(&archive);
    return status?1:0;
}
