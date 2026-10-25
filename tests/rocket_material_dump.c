/* Prints each DrillMissile01.vfx material: type word, blend word, authored
 * tint bytes, material colour and bound texture, per mesh. Read-only. */
#include "rf/effect.h"
#include "rf/material.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"rocket material line %d\n",__LINE__);return 1;}}while(0)
int main(int argc,char **argv)
{
    const char *names[5]={"maps1.vpp","maps2.vpp","maps3.vpp","maps4.vpp","maps_en.vpp"};
    rf_vpp archive={0},maps[5]={{0}};rf_vfx_geometry_asset *a=NULL;rf_vfx_asset_materials *m=NULL;char path[1024];uint32_t i,k;
    CHECK(argc==3);snprintf(path,sizeof(path),"%s/meshes.vpp",argv[1]);CHECK(!rf_vpp_open(&archive,path));
    CHECK(!rf_vfx_geometry_asset_open(&archive,argv[2],128*1024,&a));
    for(i=0;i<5;i++){snprintf(path,sizeof(path),"%s/%s",argv[1],names[i]);CHECK(!rf_vpp_open(maps+i,path));}
    CHECK(!rf_vfx_asset_materials_open(a,maps,5,1024*1024,&m));
    for(i=0;i<a->count;i++)printf("mesh %u %s materials %u..%u flags %#x\n",i,a->meshes[i]->prefix.name,m->first[i],m->first[i+1],a->meshes[i]->edges.flags);
    for(i=0;i<m->count;i++) {
        const unsigned char *tint=(const unsigned char *)m->views[i].words+9;uint32_t slot=m->textures.bindings[i][0];
        printf("material %u type %u word2 %#x tint %u %u %u %u colour %#010x texture %s words",i,m->views[i].words[0],m->views[i].words[2],
            tint[0],tint[1],tint[2],tint[3],m->colors[i],slot==UINT32_MAX?"-":m->textures.textures[slot].name);
        for(k=0;k<14;k++)printf(" %#x",m->views[i].words[k]);
        printf("\n");
    }
    return 0;
}
