#ifndef RF_SCENE_TURRET_GENERATED_ATTACK_FIXTURE_INCLUDED
#define RF_SCENE_TURRET_GENERATED_ATTACK_FIXTURE_INCLUDED
/* Diagnostic source/observe modes. Source only stages one real Attack request;
 * load observe mode never reapplies an order or adjusts a restored deadline.
 * Pistol, AI aim, cover, collision and actual shot damage remain normal. */
uint32_t rf_scene_turret_generated_attack_config[3]; /* baseUID,actorUID,source1/observe2 */
/* requests,actorUID,baseUID,actorhandle,headhandle,scripted,alert,due,burst,
 * headhealth,basehealth,frame,status,headdead,actorhealth */
uint32_t rf_scene_turret_generated_attack_test[15];
static int scene_turret_generated_attack_fixture(uint32_t frame)
{
    uint32_t *config=rf_scene_turret_generated_attack_config,*p=rf_scene_turret_generated_attack_test;
    scene_turret_generated_key key={config[0],SCENE_TURRET_GENERATED_ROLE_AUTO_HEAD};
    scene_turret_generated_binding *binding;scene_turret_owner *head;campaign_npc_body *base,*actor=NULL;
    uint32_t handle,i;int status;
    if(!config[0])return RF_OK;
    if(!frame)memset(p,0,sizeof(rf_scene_turret_generated_attack_test));
    if(config[2]<1||config[2]>2){status=RF_RANGE;goto fail;}
    status=scene_turret_generated_resolve(key,&handle);if(status)goto fail;
    binding=scene_turret_generated_find(handle);head=scene_turret_lookup(handle);
    base=binding?scene_turret_generated_base(binding):NULL;
    for(i=0;i<campaign_npc_body_count;i++)if((uint32_t)campaign_seeds.records.items[i].record.uid==config[1]){actor=campaign_npc_bodies+i;break;}
    if(!head||!base||!actor||!actor->registration.view){status=RF_NOT_FOUND;goto fail;}
    if(config[2]==1&&frame==1&&!p[0]) {
        rf_level_event event={0};rf_level_link_target link={handle,1,0};
        if(actor->view.weapons[0]!=campaign_pistol_id||actor->damage.effects.health<=0){status=RF_FORMAT;goto fail;}
        /* Use the normal Set_AI_Mode backend to suspend head auto-fire. Its
         * saved action must survive load; the head stays live/damageable and
         * the guard still uses ordinary aiming, cover and weapon damage. */
        status=scene_turret_set_ai_mode(handle,1);if(status)goto fail;
        event.uid=0x41555441u;event.words[0]=config[1];event.link_count=1;
        status=campaign_script_attack(NULL,&event,&link,1);if(status)goto fail;
        /* Deliberate finite source deadline: save precedes the first shot so
         * the fresh process must preserve the actual target before damaging it.
         * Normal loaded AI owns aiming/cadence; no damage or motion is injected. */
        actor->combat_due=frame+105;actor->combat_navigation_due=UINT32_MAX;++p[0];
    }
    p[1]=config[1];p[2]=config[0];p[3]=actor->registration.handle;p[4]=handle;
    p[5]=actor->combat_scripted;p[6]=actor->combat_alert;p[7]=actor->combat_due;p[8]=actor->combat_burst_remaining;
    memcpy(p+9,&head->damage.effects.health,4);memcpy(p+10,&base->damage.effects.health,4);
    p[11]=frame;p[13]=head->dead;memcpy(p+14,&actor->damage.effects.health,4);return RF_OK;
fail:p[12]=(uint32_t)status;return status;
}
#endif
