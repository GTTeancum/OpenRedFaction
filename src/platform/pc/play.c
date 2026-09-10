/* Port-owned Windows frontend. Reads only messages addressed to its window.
 * The headless replay calls the shared provider directly; no OS input injection. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "rf/scene_preview.h"
#include "rf/physics.h"
#include "pc_raster.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern uint32_t rf_scene_actor_live_enabled;
extern uint32_t rf_scene_actor_follow_summary[5];
extern uint32_t rf_scene_player_input_frames[64][7];
extern rf_physics_body scene_actor_body;

typedef struct player {
    HWND window;
    unsigned char keys[256];
    unsigned char *dib;
    BITMAPINFO bitmap;
    rf_pc_raster raster;
    rf_lightmaps lightmaps;
    LARGE_INTEGER frequency,deadline;
    uint32_t frames,headless;
    int quit;
} player;

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
    player *p=context;MSG message;LARGE_INTEGER now;
    memset(out,0,sizeof(*out));
    if(p->headless) {
        /* Same deterministic route as rf_scene_check --input. */
        if(frame>=24 && frame<48)out->move[0]=.25f;
        if(frame>=63)out->move[0]=1;
        out->look[0]=frame?((frame%180)<90?.25f:-.25f):0;
        out->look[1]=frame?((frame%240)<120?.2f:-.2f):0;
        out->crouch=frame>=32 && frame<56;return RF_OK;
    }
    /* Cap production at 60 Hz; do not create a backlog after a slow frame.
     * Simulation remains 1/60 per produced frame, without catch-up yet. */
    for(;;) {
        while(PeekMessageW(&message,p->window,0,0,PM_REMOVE)) {
            TranslateMessage(&message);DispatchMessageW(&message);
        }
        if(p->quit)return RF_NOT_FOUND;
        QueryPerformanceCounter(&now);
        if(now.QuadPart>=p->deadline.QuadPart)break;
        Sleep(1);
    }
    p->deadline.QuadPart=now.QuadPart+p->frequency.QuadPart/60;
    out->move[0]=(float)p->keys['D']-(float)p->keys['A'];
    out->move[2]=(float)p->keys['W']-(float)p->keys['S'];
    if(out->move[0] && out->move[2]) {out->move[0]*=.7071067811865475f;out->move[2]*=.7071067811865475f;}
    out->look[0]=(float)p->keys[VK_UP]-(float)p->keys[VK_DOWN];
    out->look[1]=(float)p->keys[VK_RIGHT]-(float)p->keys[VK_LEFT];
    out->crouch=p->keys[VK_CONTROL];return RF_OK;
}

static int present(void *context,uint32_t frame,const rf_preview_mesh *mesh,
    const rf_materials *materials,uint32_t world)
{
    player *p=context;uint32_t i;int status;
    if(frame!=p->frames || mesh->bytes>RF_SCENE_FOLLOW_CAPACITY)return RF_RANGE;
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
    uint32_t opened=0,i,limit=0;int status=RF_OK;WNDCLASSW wc={0};
    if(argc==5 && !strcmp(argv[1],"--headless")) {
        char *end;unsigned long value=strtoul(argv[3],&end,10);
        if(!argv[3][0] || *end || value<1 || value>60000)return 2;
        p.headless=1;limit=(uint32_t)value;directory=argv[2];
    } else if(argc==2)directory=argv[1];
    else {fprintf(stderr,"Usage: rf_pc_play <Installed_Game>\n       rf_pc_play --headless <Installed_Game> <frames 1..60000> <output.ppm>\n");return 2;}
#define CHECK(call) do {status=(call);if(status){fprintf(stderr,"%s failed (%d)\n",#call,status);goto cleanup;}} while(0)
    CHECK(path_join(path,sizeof(path),directory,"levels1.vpp"));
    CHECK(rf_vpp_open(&archive,path));CHECK(rf_level_open(&level,&archive,"L1S1.rfl"));
    CHECK(path_join(meshes,sizeof(meshes),directory,"meshes.vpp"));
    CHECK(path_join(motions,sizeof(motions),directory,"motions.vpp"));
    CHECK(path_join(tables,sizeof(tables),directory,"tables.vpp"));
    CHECK(rf_scene_preview_route_camera(&level,9858));
    CHECK(rf_geometry_open(&geometry,&level,8*1024*1024));
    CHECK(rf_geometry_collision_world_open(&geometry,8*1024*1024,&collision));
    for(i=0;i<5;++i) {
        CHECK(path_join(path,sizeof(path),directory,map_names[i]));
        CHECK(rf_vpp_open(maps+i,path));++opened;
    }
    CHECK(rf_scene_world_open_retained(&level,&geometry,maps,opened,&mesh,&materials,
        8*1024*1024,4*1024*1024,&retained));
    CHECK(rf_lightmaps_open(&p.lightmaps,&level,4*1024*1024));
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
        puts("WASD move | arrows look | Ctrl crouch | Escape exit");
    }
    rf_scene_actor_live_enabled=1;rf_scene_actor_eye_enabled=1;
    rf_scene_actor_look_enabled=1;rf_scene_actor_turn_enabled=1;
    rf_scene_actor_drive(1);rf_scene_actor_follow(&retained);rf_scene_set_input(input,&p,limit);
    CHECK(rf_scene_stream_miner_body(&level,9858,meshes,motions,tables,maps,opened,&mesh,&materials,
        8*1024*1024,4*1024*1024,present,&p,&collision,&geometry));
    if(p.headless) {
        if(p.frames!=limit){status=RF_FORMAT;goto cleanup;}
        CHECK(rf_pc_raster_save(&p.raster,argv[4]));
        printf("ACTOR_FOLLOW_SUMMARY");for(i=0;i<5;++i)printf(" %u",rf_scene_actor_follow_summary[i]);puts("");
        printf("ACTOR_PLAYER_INPUT");for(i=0;i<64*7;++i)printf(" %u",((uint32_t*)rf_scene_player_input_frames)[i]);puts("");
        printf("PC_PLAY_BODY");for(i=0;i<sizeof(scene_actor_body.state)/4;++i) {
            uint32_t word;memcpy(&word,(const unsigned char*)&scene_actor_body.state+i*4,4);printf(" %u",word);
        }puts("");
    }
    printf("Completed %u frames, 640x480 raster, %u byte mesh cap.\n",p.frames,RF_SCENE_FOLLOW_CAPACITY);
cleanup:
    rf_scene_set_input(NULL,NULL,0);rf_scene_actor_follow(NULL);
    if(p.window)DestroyWindow(p.window);
    if(wc.lpszClassName)UnregisterClassW(wc.lpszClassName,wc.hInstance);
    free(p.dib);rf_pc_raster_close(&p.raster);rf_lightmaps_close(&p.lightmaps);
    rf_materials_close(&materials);rf_preview_close(&mesh);rf_scene_world_geometry_close(&retained);
    rf_geometry_collision_world_close(&collision);rf_geometry_close(&geometry);
    while(opened)rf_vpp_close(maps+--opened);rf_vpp_close(&archive);
    return status?1:0;
}
