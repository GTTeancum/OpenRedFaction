/* Original43d0a9 action2/type0 consumes a press, not held51f220. This one
 * raw-sample owner precedes modal filters and all entity/trigger consumers. */
typedef struct scene_use_edge_state {
    uint32_t raw,held,edge,filtered,consumed,selected,kind,player,prepared,claimed;
} scene_use_edge_state;
static scene_use_edge_state scene_use_edge={0,0,0,0,0,UINT32_MAX,0,UINT32_MAX,0,0};
static void scene_use_sample(uint32_t held)
{
    scene_use_edge.raw=!!held;scene_use_edge.edge=held&&!scene_use_edge.held;
    scene_use_edge.held=!!held;scene_use_edge.filtered=0;
    if(!held)scene_use_edge.claimed=0;
    scene_use_edge.consumed=scene_use_edge.claimed;scene_use_edge.prepared=0;
    scene_use_edge.selected=UINT32_MAX;scene_use_edge.kind=0;
}
static void scene_use_filtered(uint32_t held)
{scene_use_edge.filtered=!!held;}
static uint32_t scene_use_owner_input(uint32_t handle)
{
    if(!scene_use_edge.prepared)return scene_use_edge.filtered; /* Legacy noncampaign testbed. */
    return scene_use_edge.edge&&scene_use_edge.selected==handle;
}
static void scene_use_claim(void)
{
    if(!scene_use_edge.prepared)return;
    scene_use_edge.claimed=scene_use_edge.raw;scene_use_edge.consumed=1;
}
static uint32_t scene_use_trigger_input(void)
{
    /*4a29f0 returns after consumed4a1660 before4a1970 trigger fallback.
     * Automatic trigger contacts are still visited with Use==0. */
    return scene_use_edge.filtered&&(!scene_use_edge.prepared||!scene_use_edge.consumed);
}
static uint32_t scene_medic_handoff_wait(void);

static float scene_use_eye[3],scene_use_basis[3][3];
static uint32_t scene_use_pose_frame=UINT32_MAX;
