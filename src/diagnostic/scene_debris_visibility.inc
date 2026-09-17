/* Original room-filtered draw route: reverse prepared room order, authored
 * room bounds, rectangle-adjusted world planes. Chunk room never follows its
 * current position. No allocation and no collision-overlay bounds. */
static int scene_debris_visible_order(scene_stream *s,uint32_t order[80],uint32_t *count)
{
    scene_debris_pool *p=s->debris;unsigned char selected[80]={0};
    float depths[80];uint32_t n=0,r,i,j,k;int status;
    rf_scene_debris_visibility[2]=rf_scene_debris_visibility[3]=0;
    rf_scene_debris_visibility[4]=2166136261u;
    if(!s->geometry || !s->visibility.storage)return RF_FORMAT;
    for(r=s->visibility.state.visible_count;r>0;r--) {
        uint32_t room=s->visibility.state.order[r-1],begin=n,has_chunks=0,admitted;
        rf_visibility_frustum active=s->particle_camera.frustum;float minimum[3],maximum[3];
        const float *rect;
        if(room>=s->geometry->rooms || room>=s->visibility.state.count)return RF_FORMAT;
        for(i=0;i<80;i++)if(p->chunks[i].active && p->chunks[i].room==room)has_chunks=1;
        if(!has_chunks)continue;
        rect=s->visibility.state.rooms[room].rectangle;
        /* Empty traversal rectangles submit no room. */
        if(rect[0]>=rect[2] || rect[1]>=rect[3])continue;
        status=rf_debris_room_rectangle(&s->particle_camera.view,640,480,rect,&active);if(status)return status;
        memcpy(minimum,s->geometry->data+s->geometry->room_offsets[room]+4,12);
        memcpy(maximum,s->geometry->data+s->geometry->room_offsets[room]+16,12);
        status=rf_debris_room_render_admit(active.planes,active.count,minimum,maximum,1,&admitted);if(status)return status;
        ++rf_scene_debris_visibility[6];if(!admitted){++rf_scene_debris_visibility[7];continue;}
        for(i=0;i<80;i++)if(p->chunks[i].active && p->chunks[i].room==room) {
            float depth=0;
            for(k=0;k<3;k++)depth+=(p->chunks[i].position[k]-s->rocket_camera.player_position[k])*s->rocket_camera.player_orientation[2][k];
            j=n;while(j>begin && depths[j-1]<depth){depths[j]=depths[j-1];order[j]=order[j-1];--j;}
            depths[j]=depth;order[j]=i;selected[i]=1;++n;
        }
    }
    for(i=0;i<80;i++)if(p->chunks[i].active && !selected[i]) {
        ++rf_scene_debris_visibility[3];
        rf_scene_debris_visibility[4]=npc_hash_bytes(rf_scene_debris_visibility[4],&p->chunks[i].age,4);
    }
    rf_scene_debris_visibility[0]+=rf_scene_debris_visibility[3];
    rf_scene_debris_visibility[1]+=n;rf_scene_debris_visibility[2]=n;
    *count=n;return RF_OK;
}
