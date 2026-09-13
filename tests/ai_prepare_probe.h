typedef struct prepare_fixture {uint32_t wire[13];rf_entity_ai_destination_actor *actor;} prepare_fixture;
static int prepare_weapon(void *context,rf_entity_ai_destination_actor *a,uint32_t *weapon)
{
 prepare_fixture *c=context;if(a!=c->actor)return RF_FORMAT;
 a->movement_kind=(int32_t)c->wire[7];memcpy(a->position_3c,c->wire+8,12);memcpy(&a->radius_7c0,c->wire+11,8);
 a->token_69c^=0x55555555u;a->token_6a0^=0xaaaaaaaau;*weapon=c->wire[6];return RF_OK;
}
static int ai_prepare_probe(void)
{
 uint32_t wire[13];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
 while(fread(wire,sizeof(wire),1,stdin)==1) {
  rf_entity_ai_destination_actor a;rf_entity_ai_destination_query q,kept;prepare_fixture c;uint32_t out[19]={0};
  memset(&a,0x5a,sizeof(a));memset(&q,0x5a,sizeof(q));kept=q;memcpy(c.wire,wire,sizeof(wire));c.actor=&a;
  memcpy(&a.radius_7c0,wire,8);a.movement_kind=(int32_t)wire[2];a.token_69c=wire[3];a.token_6a0=wire[4];
  out[0]=(uint32_t)rf_entity_ai_destination_prepare(&a,&q,wire[5],prepare_weapon,&c);
  if(q.destination_owner!=kept.destination_owner || q.first!=kept.first || q.second!=kept.second || q.search_63a!=kept.search_63a || q.count_64c!=kept.count_64c)return 4;
  out[1]=a.word_5e4;memcpy(out+2,a.begin_5a4,12);memcpy(out+5,a.next_5b0,12);memcpy(out+8,&q.radius_630,4);memcpy(out+9,&q.offset_634,4);
  out[10]=q.weapon_639;out[11]=q.mode_638;out[12]=q.start_618==a.begin_5a4?0x300005a4:0;out[13]=q.route_slot_650==&a.first_58c?0x3000058c:0;
  out[14]=q.token_61c;out[15]=q.token_620;memcpy(out+16,&q.limit_640,4);out[17]=q.word_644;out[18]=q.world_63c;
  if(fwrite(out,sizeof(out),1,stdout)!=1)return 3;
 }
 return ferror(stdin)?3:0;
}
