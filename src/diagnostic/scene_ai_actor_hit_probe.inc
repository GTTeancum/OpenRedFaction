/* Bounded damage evidence; never selects targets or mutates gameplay. */
uint32_t rf_scene_enemy_actor_hits[113]; /* count +16 rows: sourceUID,intendedUID,actualUID,frame,amount,actual health,intended health */
static uint32_t scene_ai_actor_probe_uid(uint32_t handle,float *health)
{
    uint32_t i;*health=0;
    if(handle==UINT32_MAX)return UINT32_MAX;
    if(handle==campaign_player_object.handle){*health=campaign_player_damage.state.effects.health;return 0;}
    for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].registration.handle==handle){
        *health=campaign_npc_bodies[i].damage.effects.health;return campaign_seeds.records.items[i].record.uid;}
    return UINT32_MAX;
}
static void scene_ai_actor_hit_probe(const campaign_npc_body *owner,uint32_t intended,uint32_t actual,uint32_t frame,float amount)
{
    uint32_t index,*row;float health,unused;
    if(amount<=0)return;
    index=rf_scene_enemy_actor_hits[0]++;if(index>=16)return;
    row=rf_scene_enemy_actor_hits+1+index*7;
    row[0]=scene_ai_actor_probe_uid(owner->registration.handle,&unused);
    row[1]=scene_ai_actor_probe_uid(intended,&health);memcpy(row+6,&health,4);
    row[2]=scene_ai_actor_probe_uid(actual,&health);memcpy(row+5,&health,4);
    row[3]=frame;memcpy(row+4,&amount,4);
}
