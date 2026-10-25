/* Lists every level room whose liquid has a texture mover pan (both authored
 * speeds nonzero, as original 4f0c00 requires). Read-only scan of the
 * installed level archives; reports level, room and pan U/V. */
#include "rf/material.h"
#include <stdio.h>
#include <string.h>
static char level_names[256][61];static uint32_t level_count;
static int collect(const rf_vpp_entry *entry,void *context)
{(void)context;if(strstr(entry->name,".rfl") && level_count<256)memcpy(level_names[level_count++],entry->name,61);return RF_OK;}
int main(int argc,char **argv)
{
    const char *archives[4]={"levels1.vpp","levels2.vpp","levels3.vpp","levelsm.vpp"};
    const char *names[6]={"maps1.vpp","maps2.vpp","maps3.vpp","maps4.vpp","maps_en.vpp","ui.vpp"};
    rf_vpp maps[6]={{0}};char path[512];uint32_t a,i,found=0;
    const char *root=argc>1?argv[1]:"Installed_Game";
    for(i=0;i<6;i++){snprintf(path,sizeof(path),"%s/%s",root,names[i]);if(rf_vpp_open(maps+i,path))return 1;}
    for(a=0;a<4;a++){
        rf_vpp levels={0};uint32_t e;
        snprintf(path,sizeof(path),"%s/%s",root,archives[a]);if(rf_vpp_open(&levels,path))continue;
        level_count=0;rf_vpp_visit(&levels,collect,NULL);
        for(e=0;e<level_count;e++){
            rf_level level={0};const char *name=level_names[e];rf_geometry geometry={0};rf_geometry_materials m={0};const rf_geometry *g=&geometry;uint32_t r;
            if(rf_level_open(&level,&levels,name) || rf_geometry_open(&geometry,&level,32*1024*1024))continue;
            if(argc>2)for(r=0;r<geometry.rooms;r++){ /* raw record dump: liquid rooms only */
                const unsigned char *q=geometry.data+geometry.room_offsets[r];uint32_t n=(uint32_t)q[40]|((uint32_t)q[41]<<8),t;float f[3];uint32_t w;
                if(!q[32])continue;q+=42+n;t=(uint32_t)q[8]|((uint32_t)q[9]<<8);
                memcpy(f,q+10+t,4);memcpy(&w,q+35+t,4);memcpy(f+1,q+39+t,8);
                printf("RAW %s room %u tex %.*s vis %g waveform %#x pan %g %g\n",name,r,(int)t,(const char*)q+10,f[0],w,f[1],f[2]);}
            if(geometry.scrolls){uint32_t k,id,faces=0,liquid=0;float pan[2];
                for(k=0;k<geometry.scrolls;k++){uint32_t f;if(rf_geometry_scroll(&geometry,k,&id,pan))break;
                    for(f=0;f<geometry.faces;f++){rf_geometry_face face;if(rf_geometry_get_face(&geometry,f,&face))break;
                        if(face.source_word==id){faces++;liquid+=(face.flags&4u)!=0;}}}
                printf("MOVERS %s %u movers, %u faces (%u liquid), first pan %g %g\n",name,geometry.scrolls,faces,liquid,pan[0],pan[1]);}
            if(!rf_geometry_materials_open_limit(&m,&g,1,maps,6,64*1024*1024,0) && m.liquid_pan)
                for(r=0;r<m.liquid_rooms;r++)if(m.liquid_pan[r][0]!=0){
                    printf("%s room %u pan %g %g slot %#x\n",name,r,m.liquid_pan[r][0],m.liquid_pan[r][1],m.liquid_slots[r]);found++;}
            rf_geometry_materials_close(&m);rf_geometry_close(&geometry);
        }
        rf_vpp_close(&levels);
    }
    printf("PANNED_ROOMS %u\n",found);
    for(i=0;i<6;i++)rf_vpp_close(maps+i);
    return 0;
}
