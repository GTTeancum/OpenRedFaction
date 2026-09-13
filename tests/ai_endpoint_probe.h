static int endpoint_nearest(void *ctx,const float point[3],uint32_t alternate,uint32_t *token)
{uint32_t *c=ctx;if(point[0]!=1 || point[1]!=2 || point[2]!=3 || alternate!=77)return RF_FORMAT;++c[1];*token=c[0]?0x30000400:0;return RF_OK;}
static int ai_endpoint_probe(void)
{
 uint32_t in[9];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
 while(fread(in,sizeof(in),1,stdin)==1) {
  uint32_t i,j,data[4][8],out[39]={0},context[2]={in[4],0};float point[3]={1,2,3};rf_entity_navigation_token_list lists[4];
  for(i=0;i<4;++i){if(in[5+i]>4)return 3;for(j=0;j<8;++j)data[i][j]=100+i*16+j;lists[i]=(rf_entity_navigation_token_list){data[i],in[5+i],8};}
  out[1]=99;
  if(!in[0])out[0]=(uint32_t)rf_entity_navigation_connect_start(lists,in[1]?0x30000100:0,in[2]?(in[3]?0x30000100:0x30000200):0,point,77,endpoint_nearest,context,out+1);
  else out[0]=(uint32_t)rf_entity_navigation_connect_goal(lists+3,in[1]?lists+1:NULL,in[2]?(in[3]?lists+1:lists+2):NULL,0x30000300,out+1);
  if(in[0]==2 && !out[0] && out[1])out[0]=(uint32_t)rf_entity_navigation_disconnect_goal(lists+3,lists+1,in[2]?(in[3]?lists+1:lists+2):NULL,0x30000300);
  out[2]=context[1];for(i=0;i<4;++i)out[3+i]=lists[i].count;memcpy(out+7,data,sizeof(data));if(fwrite(out,sizeof(out),1,stdout)!=1)return 3;
 }
 return ferror(stdin)?3:0;
}
