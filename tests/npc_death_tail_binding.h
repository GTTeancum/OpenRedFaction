typedef struct death_tail_binding_fixture {campaign_npc_body *owner;uint32_t calls,errors;} death_tail_binding_fixture;
static void death_tail_binding_call(void *context,uint32_t op,uint32_t arg)
{
 death_tail_binding_fixture *f=context;campaign_npc_body *o=f->owner;
 if(op!=f->calls++)++f->errors;
 if(op==0){if(arg || o->death.deadline_4b8!=17)++f->errors;o->damage.effects.class_flags_728=32;o->model_radius_78=7;}
 if(op==1){if(arg || o->death.deadline_4b8!=1000)++f->errors;o->view.flags_810=0x400000;}
 if(op==2){if(arg!=1234 || o->damage.effects.flags_810!=0x400000)++f->errors;o->death.model_148c=77;}
 if(op==3){if(arg!=77 || o->death.model_148c!=77)++f->errors;o->death.model_148c=99;}
}
static int death_tail_binding_check(void)
{
 campaign_npc_body owner={0};death_tail_binding_fixture f={&owner,0,0};int32_t now=RF_TIMER_PERIOD-1000;uint32_t handle;
 rf_entity_death_tail_backend backend={death_tail_binding_call,&f,&now};
 campaign_npc_bodies=&owner;campaign_npc_body_count=1;rf_object_registry_init(&campaign_registry);memset(&campaign_entities,0,sizeof(campaign_entities));
 CHECK(rf_entity_view_register(&campaign_registry,&campaign_entities,&owner.view,&owner.registration)==RF_OK);handle=owner.registration.handle;
 owner.view.action_520=13;owner.death.deadline_4b8=17;owner.death.model_148c=55;owner.death.item_82c=6;owner.death.action_824=8;owner.death.requested_83c=9;owner.death.linked_146c=10;
 CHECK(rf_scene_npc_death_tail(handle^0x10000,1234,&backend)==RF_NOT_FOUND && !f.calls);
 CHECK(rf_scene_npc_death_tail(handle,1234,&backend)==RF_OK && f.calls==4 && !f.errors);
 CHECK(owner.death.deadline_4b8==1000 && !owner.death.model_148c && owner.damage.effects.flags_810==owner.view.flags_810);
 CHECK(owner.death.item_82c==6 && owner.death.action_824==8 && owner.death.requested_83c==9 && owner.death.linked_146c==10);
 f.calls=0;now=-1;owner.death.deadline_4b8=17;owner.death.model_148c=55;
 CHECK(rf_scene_npc_death_tail(handle,1234,&backend)==RF_RANGE && f.calls==1 && !f.errors);
 CHECK(owner.death.deadline_4b8==17 && owner.death.model_148c==55);
 CHECK(rf_entity_view_unregister(&campaign_registry,&campaign_entities,&owner.registration)==RF_OK);
 CHECK(rf_scene_npc_death_tail(handle,1234,&backend)==RF_NOT_FOUND && f.calls==1);
 campaign_npc_bodies=NULL;campaign_npc_body_count=0;return 0;
}
