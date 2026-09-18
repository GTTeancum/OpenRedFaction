#ifndef RF_SCENE_SCANNER_SELECT_INC
#define RF_SCENE_SCANNER_SELECT_INC
#include "rf/weapon_scanner.h"
/* Main-thread scene adapter, non-reentrant. Two fixed32-marker buffers replace
 * an NPC-count-sized candidate array. World AABB centers are first-pass marker
 * anchors; no occlusion query or room-visibility filter (scanner sees walls). */
static int scene_scanner_collect(scene_stream *stream,rf_weapon_scanner_result *out)
{
    static rf_weapon_scanner_result result,one;
    uint32_t i,k;int status;
    if(!stream || !out || (campaign_npc_body_count && !campaign_npc_bodies))return RF_RANGE;
    memset(&result,0,sizeof(result));
    /* Validate even when there are no eligible NPCs. */
    status=rf_weapon_scanner_select(&campaign_registry,NULL,0,&stream->npc_view,
        RF_WEAPON_SCANNER_DEFAULT_RANGE,640,480,&one);if(status)return status;
    for(i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *owner=campaign_npc_bodies+i;
        rf_weapon_scanner_candidate candidate={0};rf_weapon_scanner_marker marker;uint32_t pos,j;
        if(!owner->registration.view || !owner->body.allocated_bytes ||
           (owner->object_flags&(2|0x4000)) || (owner->view.flags_810&1) || owner->damage.effects.health<=0 ||
           rf_object_registry_lookup(&campaign_registry,owner->registration.handle)!=&owner->registration)continue;
        candidate.handle=owner->registration.handle;candidate.identity=&owner->registration;
        candidate.eligible=1;candidate.health=owner->damage.effects.health;
        for(k=0;k<3;k++){
            float low=owner->body.state.bounds.minimum[k],high=owner->body.state.bounds.maximum[k];
            if(!isfinite(low) || !isfinite(high) || low>high)return RF_RANGE;
            candidate.center[k]=(float)(((double)low+high)*.5);
        }
        status=rf_weapon_scanner_select(&campaign_registry,&candidate,1,&stream->npc_view,
            RF_WEAPON_SCANNER_DEFAULT_RANGE,640,480,&one);if(status)return status;if(!one.count)continue;
        marker=one.markers[0];marker.index=i;
        /* Registry identity admits one owner per handle, so actors cannot repeat
         * a handle here; retain stable scene order at exact distance ties. */
        for(pos=0;pos<result.count && result.markers[pos].distance<=marker.distance;pos++){}
        if(result.count==RF_WEAPON_SCANNER_CAPACITY){result.truncated=1;if(pos==result.count)continue;}
        else ++result.count;
        for(j=result.count-1;j>pos;j--)result.markers[j]=result.markers[j-1];result.markers[pos]=marker;
    }
    *out=result;return RF_OK;
}
#endif
