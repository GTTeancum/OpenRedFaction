static int death_entry_binding_check(void)
{
 campaign_npc_body owner={0},before,want;rf_entity_seed seed={0};rf_entity_seed_class cls={0};
 rf_entity_seeds saved=campaign_seeds;uint32_t mode,kind,material,dying,entered,handle;
 rf_movement_descriptor saved_mode=campaign_modes[0];
 campaign_npc_bodies=&owner;campaign_npc_body_count=1;
 campaign_seeds.items=&seed;campaign_seeds.classes=&cls;campaign_seeds.class_count=campaign_seeds.records.count=1;
 rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
 CHECK(rf_entity_view_register(&campaign_registry,&campaign_entities,&owner.view,&owner.registration)==RF_OK);handle=owner.registration.handle;
 for(mode=0;mode<16;++mode)for(kind=0;kind<3;++kind)for(material=0;material<2;++material)for(dying=0;dying<2;++dying) {
  campaign_modes[0].index=mode;cls.physics.use_kind=kind;owner.support.material=material?-1:0;
  owner.view.flags_810=0xabcdef00u|dying;owner.damage.effects.flags_810=0x12345678u;
  owner.body.state.flags=0x8765ffffu;
  for(uint32_t k=0;k<3;++k){owner.command_714[k]=(float)(k+1);owner.body.state.velocity[k]=(float)(k+4);owner.body.state.vector_c8[k]=(float)(k+7);}
  before=owner;want=owner;entered=99;
  if(!dying) {
   want.view.flags_810|=1;want.damage.effects.flags_810=want.view.flags_810;want.body.state.flags&=~0x8000u;
   memset(want.command_714,0,12);memset(want.body.state.vector_c8,0,12);
   if(!(mode==3 || mode==8 || (kind==1 && material)))memset(want.body.state.velocity,0,12);
  }
  CHECK(rf_scene_npc_death_entry(handle^0x10000,&entered)==RF_NOT_FOUND && entered==99 && !memcmp(&before,&owner,sizeof(owner)));
  CHECK(rf_scene_npc_death_entry(handle,NULL)==RF_RANGE && !memcmp(&before,&owner,sizeof(owner)));
  CHECK(rf_scene_npc_death_entry(handle,&entered)==RF_OK && entered==!dying && !memcmp(&want,&owner,sizeof(owner)));
  before=owner;CHECK(rf_scene_npc_death_entry(handle,&entered)==RF_OK && !entered && !memcmp(&before,&owner,sizeof(owner)));
 }
 owner.view.flags_810=0;owner.movement_slot=16;before=owner;entered=99;
 CHECK(rf_scene_npc_death_entry(handle,&entered)==RF_RANGE && entered==99 && !memcmp(&before,&owner,sizeof(owner)));
 owner.movement_slot=0;seed.class_index=1;before=owner;
 CHECK(rf_scene_npc_death_entry(handle,&entered)==RF_RANGE && entered==99 && !memcmp(&before,&owner,sizeof(owner)));
 CHECK(rf_entity_view_unregister(&campaign_registry,&campaign_entities,&owner.registration)==RF_OK);
 CHECK(rf_scene_npc_death_entry(handle,&entered)==RF_NOT_FOUND && entered==99);
 campaign_seeds=saved;campaign_modes[0]=saved_mode;campaign_npc_bodies=NULL;campaign_npc_body_count=0;return 0;
}
