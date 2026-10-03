#ifndef RF_SCENE_SCRIPTED_IGNITE_INCLUDED
#define RF_SCENE_SCRIPTED_IGNITE_INCLUDED
/* Original4b9cf0: explicit ignition creates an unowned burn, without a hit.
 * Existing burn lifetime/damage/extinguish remain the common gameplay policy. */
uint32_t rf_scene_script_ignite[8]; /* calls,created,already burning,rejected,UID,handle,burn,status */
/* first valid,UID,target,burn,source,health before/after create,current health,
 * remaining ticks,phase,flags814,last sampled frame; floats are bitcasts. */
uint32_t rf_scene_script_ignite_probe[12];
static void scene_script_ignite_reset(void)
{memset(rf_scene_script_ignite,0,sizeof(rf_scene_script_ignite));memset(rf_scene_script_ignite_probe,0,sizeof(rf_scene_script_ignite_probe));}
static void scene_script_ignite_sample(uint32_t frame)
{
    uint32_t *p=rf_scene_script_ignite_probe;
    campaign_npc_body *o=p[0]?scene_burning_owner(p[2],NULL):NULL;
    if(!o)return;memcpy(p+7,&o->damage.effects.health,4);p[10]=o->damage.effects.flags_814;p[11]=frame;
    if(p[3]&&p[3]<=SCENE_BURNING_CAPACITY){const scene_burning_record *r=scene_burning_records+p[3]-1;
        p[8]=r->target==p[2]?r->left:0;p[9]=r->target==p[2]?r->phase:0;}
}
static uint32_t scene_script_ignite_required(void)
{
    for(uint32_t i=0;i<campaign_events.count;i++)if(campaign_events.items[i].state.type==82)return 1;
    return 0;
}
static int campaign_ignite_npc(void *context,uint32_t handle)
{
    float health;uint32_t slot,burn;campaign_npc_body *owner=scene_burning_owner(handle,&slot);(void)context;
    ++rf_scene_script_ignite[0];rf_scene_script_ignite[5]=handle;
    if(!owner||owner->damage.effects.health<=0||(owner->view.flags_810&1u)){
        ++rf_scene_script_ignite[3];rf_scene_script_ignite[7]=RF_NOT_FOUND;return RF_NOT_FOUND;
    }
    rf_scene_script_ignite[4]=(uint32_t)campaign_seeds.records.items[slot].record.uid;
    if(owner->damage.effects.burn){++rf_scene_script_ignite[2];rf_scene_script_ignite[6]=owner->damage.effects.burn;rf_scene_script_ignite[7]=0;return RF_OK;}
    health=owner->damage.effects.health;burn=scene_burning_create(handle,UINT32_MAX);
    if(!burn){++rf_scene_script_ignite[3];rf_scene_script_ignite[7]=RF_NOT_FOUND;return RF_NOT_FOUND;}
    owner->damage.effects.burn=burn;owner->damage.burn_source=UINT32_MAX;
    if(!rf_scene_script_ignite_probe[0]){uint32_t *p=rf_scene_script_ignite_probe;
        p[0]=1;p[1]=rf_scene_script_ignite[4];p[2]=handle;p[3]=burn;p[4]=UINT32_MAX;
        memcpy(p+5,&health,4);memcpy(p+6,&owner->damage.effects.health,4);}
    ++rf_scene_script_ignite[1];rf_scene_script_ignite[6]=burn;rf_scene_script_ignite[7]=0;
    return RF_OK;
}
#endif
