typedef struct direct_wire {
 uint32_t count,state,kind,target;float start[3],end[3],position[3],radius,height,target_radius,target_height;
 uint32_t order[4];rf_entity_navigation_candidate nodes[4];uint32_t neighbor_count[4],neighbors[4][4];
} direct_wire;
static int ai_direct_probe(void)
{
 direct_wire wire;_Static_assert(sizeof(wire)==436,"direct route wire");
 _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
 while(fread(&wire,sizeof(wire),1,stdin)==1) {
  rf_entity_ai_destination_actor a={0},target={0};rf_entity_navigation_reference refs[4];uint32_t i,out[4]={0,99,11,22};
  if(wire.count>4)return 3;a.state_554=(int32_t)wire.state;a.movement_kind=(int32_t)wire.kind;a.radius_7c0=wire.radius;a.height_7c4=wire.height;a.token_69c=11;a.token_6a0=22;memcpy(a.position_3c,wire.position,12);target.radius_7c0=wire.target_radius;target.height_7c4=wire.target_height;
  for(i=0;i<4;++i){refs[i].candidate=wire.nodes+i;refs[i].order_key=0x30001000+wire.order[i]*0x80;refs[i].neighbors=wire.neighbors[i];refs[i].neighbor_count=wire.neighbor_count[i];if(wire.neighbor_count[i]>4)return 3;}
  out[0]=(uint32_t)rf_entity_ai_direct_route(&a,wire.target?&target:NULL,wire.start,wire.end,refs,wire.count,out+1);out[2]=a.token_69c;out[3]=a.token_6a0;
  if(fwrite(out,sizeof(out),1,stdout)!=1 || fwrite(wire.nodes,sizeof(wire.nodes),1,stdout)!=1)return 3;
 }
 return ferror(stdin)?3:0;
}
