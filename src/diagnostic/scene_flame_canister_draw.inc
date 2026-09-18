#ifndef RF_SCENE_FLAME_CANISTER_DRAW_INC
#define RF_SCENE_FLAME_CANISTER_DRAW_INC
/* Include after scene_weapon_submit and scene_flame_canister.inc. */
static int scene_flame_canister_draw(scene_stream *stream,uint32_t model)
{
    scene_weapon_context c={0};uint32_t i,state[20]={0};int status;
    if(!rf_scene_flame_canister[3])return RF_OK;if(!model)return RF_NOT_FOUND;
    c.stream=stream;c.pickup=1;state[0]=0x80;
    c.buffers.cache=stream->npc_memory;c.buffers.clip=(float(*)[3])((uint8_t*)stream->npc_memory+4096*32);
    c.buffers.vertices=(uint8_t(*)[40])((uint8_t*)stream->npc_memory+4096*56);c.buffers.capacity=4096;
    c.attributes=(rf_model_render_output){1,{255,255,255},255,1,1};c.lights.ambient[0]=40;c.lights.ambient[1]=50;c.lights.ambient[2]=60;
    c.planes.near_depth=.1f;c.planes.far_depth=1000;c.projection.scale[0]=320;c.projection.scale[1]=240;c.projection.clamp=1;
    for(i=0;i<SCENE_FLAME_CANISTER_CAPACITY;i++)if(scene_flame_canisters[i].flight.lifecycle.active){
        rf_weapon_hand_placement pose={0};memcpy(pose.position,scene_flame_canisters[i].flight.position,12);
        pose.basis[0]=pose.basis[4]=pose.basis[8]=1;
        status=scene_weapon_submit(&c,model,&pose,state);if(status)return status;
    }
    return RF_OK;
}
#endif
