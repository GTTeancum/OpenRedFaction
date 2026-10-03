#ifndef RF_SCENE_ITEM_PICKUP_STATE_INCLUDED
#define RF_SCENE_ITEM_PICKUP_STATE_INCLUDED
/* Item_Pickup_State54: original4b9290 clears item+2bc bit0 on ON;
 * original4ba090 sets it on OFF. Placed items use authored UID identity,
 * not actor registry handles. Override only this item, never its shared class.
 * 0 inherits class defaults,1 enables collection,2 disables collection.
 * Visibility and retirement remain owned by their ordinary paths. */
uint32_t rf_scene_item_pickup_state[8]; /* calls,enabled,disabled,missing,UID,on,override,taken */
uint32_t rf_scene_item_pickup_gate[8]; /* nearby blocked,UID,frame,override,taken,collected,rounds,reserved */
static void scene_item_pickup_state_reset(void)
{memset(rf_scene_item_pickup_state,0,sizeof(rf_scene_item_pickup_state));memset(rf_scene_item_pickup_gate,0,sizeof(rf_scene_item_pickup_gate));}
static int campaign_set_item_pickup_state(void *context,uint32_t uid,uint32_t enabled)
{
    scene_stream *s=context;uint32_t i;
    if(!s || enabled>1)return RF_RANGE;
    ++rf_scene_item_pickup_state[0];rf_scene_item_pickup_state[4]=uid;rf_scene_item_pickup_state[5]=enabled;
    rf_scene_item_pickup_state[6]=rf_scene_item_pickup_state[7]=0;
    if(s->pickup_state && s->pickup_taken)for(i=0;i<s->pickups.count;i++)if(s->pickups.items[i].uid==uid && !s->pickup_taken[i]){
        s->pickup_state[i]=(uint8_t)(enabled?1:2);
        ++rf_scene_item_pickup_state[enabled?1:2];
        rf_scene_item_pickup_state[6]=s->pickup_state[i];rf_scene_item_pickup_state[7]=s->pickup_taken[i];
        return RF_OK;
    }
    ++rf_scene_item_pickup_state[3];return RF_NOT_FOUND;
}
static uint32_t scene_item_pickup_enabled(const scene_stream *s,uint32_t i,const rf_item_definition *definition)
{
    uint32_t state=s->pickup_state?s->pickup_state[i]:0;
    return state?state==1:!(definition->flags&1u);
}
static void scene_item_pickup_blocked(const scene_stream *s,uint32_t i)
{
    ++rf_scene_item_pickup_gate[0];rf_scene_item_pickup_gate[1]=s->pickups.items[i].uid;
    rf_scene_item_pickup_gate[2]=combat_frame;rf_scene_item_pickup_gate[3]=s->pickup_state?s->pickup_state[i]:0;
    rf_scene_item_pickup_gate[4]=s->pickup_taken[i];rf_scene_item_pickup_gate[5]=rf_scene_pickups[3];
    rf_scene_item_pickup_gate[6]=rf_scene_pickups[4];
}
#endif
