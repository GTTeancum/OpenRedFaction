typedef struct npc_reset_fixture {campaign_npc_body *owner;uint32_t calls,fail;} npc_reset_fixture;
static int npc_reset_stop_sound(void *context,int32_t handle)
{
 npc_reset_fixture *f=context;++f->calls;
 if(handle!=17 || f->owner->firing.active[2]!=1 || f->owner->view.flags_7d0!=0x2000)return RF_FORMAT;
 f->owner->view.flags_810=1;return f->fail==f->calls?RF_IO:RF_OK;
}
static int npc_reset_release_sound(void *context,int32_t cls,int32_t *handle)
{
 npc_reset_fixture *f=context;++f->calls;
 if(cls!=9 || f->owner->firing.sound_81c!=-1 || f->owner->view.flags_810!=1)return RF_FORMAT;
 f->owner->view.flags_7d0=0x2004;*handle=31;return f->fail==f->calls?RF_IO:RF_OK;
}
static int npc_reset_stop_effect(void *context,int32_t handle)
{
 npc_reset_fixture *f=context;++f->calls;
 if(handle!=23 || f->owner->view.flags_7d0!=4 || f->owner->firing.active[2] || f->owner->firing.sound_820!=31)return RF_FORMAT;
 f->owner->view.flags_810=0x80;return f->fail==f->calls?RF_IO:RF_OK;
}
static int npc_weapon_reset_binding_check(campaign_npc_body *owner)
{
 campaign_npc_body kept=*owner;rf_weapon_descriptor defs[64]={{0}};
 uint32_t saved_classes=campaign_seeds.class_count,saved_mappings=campaign_motion_catalog.mapping_count;
 campaign_seeds.class_count=campaign_motion_catalog.mapping_count=1;
 rf_weapon_reset_context context={0,64};uint32_t handle=owner->registration.handle,fail;
 rf_weapon_reset_ops ops={npc_reset_stop_sound,npc_reset_release_sound,npc_reset_stop_effect,NULL};
 npc_reset_fixture fixture={owner,0,0};rf_weapon_reset_state before;
 defs[2]=(rf_weapon_descriptor){6,64,9};
 memset(&owner->firing,0,sizeof(owner->firing));
 owner->firing.sound_81c=owner->firing.sound_820=owner->firing.effect_13d4=-1;
 owner->view.flags_7d0=0x2004;owner->view.flags_810=0;
 CHECK(rf_scene_npc_weapon_reset(handle^0x10000,2,defs,&context,NULL,NULL)==RF_NOT_FOUND && owner->view.flags_7d0==0x2004);
 CHECK(rf_scene_npc_weapon_reset(handle,2,defs,&context,NULL,NULL)==RF_OK && owner->view.flags_7d0==4);
 owner->firing.active[2]=1;owner->firing.sound_81c=17;owner->view.flags_7d0=0x2000;before=owner->firing;
 CHECK(rf_scene_npc_weapon_reset(handle,2,defs,&context,NULL,NULL)==RF_NOT_FOUND);
 CHECK(owner->firing.active[2]==before.active[2] && owner->firing.sound_81c==17 && owner->view.flags_7d0==0x2000);
 for(fail=0;fail<=3;++fail) {
  owner->firing.active[2]=1;owner->firing.sound_81c=17;owner->firing.sound_820=-1;owner->firing.effect_13d4=23;
  owner->view.flags_7d0=0x2000;owner->view.flags_810=0;fixture.calls=0;fixture.fail=fail;
  CHECK(rf_scene_npc_weapon_reset(handle,2,defs,&context,&ops,&fixture)==(fail?RF_IO:RF_OK));
  CHECK(fixture.calls==(fail?fail:3));
  CHECK(owner->firing.sound_81c==(fail==1?17:-1));
  CHECK(owner->firing.sound_820==((fail==1 || fail==2)?-1:31));
  CHECK(owner->firing.active[2]==(fail==1 || fail==2));
  CHECK(owner->view.flags_7d0==(fail==1?0x2000:fail==2?0x2004:4));
  CHECK(owner->view.flags_810==owner->damage.effects.flags_810 && owner->view.flags_810==(fail==1 || fail==2?1:0x80));
  CHECK(owner->firing.effect_13d4==23); /* Original does not clear the effect token. */
 }
 campaign_seeds.class_count=saved_classes;campaign_motion_catalog.mapping_count=saved_mappings;
 *owner=kept;return 0;
}
