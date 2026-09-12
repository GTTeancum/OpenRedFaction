typedef struct wr_input {rf_weapon_inventory inventory;int32_t weapon,count,special;uint32_t players[4],mutation;} wr_input;
typedef struct wr_fixture {wr_input input;uint32_t calls,trace[16];} wr_fixture;
static rf_weapon_inventory *wr_inventory(void *context,uint32_t player)
{wr_fixture *f=context;f->trace[f->calls*2]=0;f->trace[f->calls++*2+1]=player;
 if(f->input.mutation==1)f->input.special=f->input.weapon;
 if(f->input.mutation==3 && f->calls==1)f->input.players[0]=99;
 return !(player&1)?&f->input.inventory:NULL;}
static void wr_notify(void *context,uint32_t player)
{wr_fixture *f=context;f->trace[f->calls*2]=1;f->trace[f->calls++*2+1]=player;if(f->input.mutation==2)f->input.count=0;}
static int weapon_remove_probe(void)
{
 wr_fixture f;int status;rf_weapon_remove_backend b={f.input.players,&f.input.count,&f.input.special,4,wr_inventory,wr_notify,&f};
 _Static_assert(sizeof(wr_input)==480,"remove fixture ABI");
 while(fread(&f.input,sizeof(f.input),1,stdin)==1){f.calls=0;memset(f.trace,0,sizeof(f.trace));status=rf_weapon_remove_owned(&f.input.inventory,f.input.weapon,&b);
 if(fwrite(&status,4,1,stdout)!=1 || fwrite(&f.input.inventory,448,1,stdout)!=1 || fwrite(&f.input.count,24,1,stdout)!=1 || fwrite(&f.calls,68,1,stdout)!=1)return 3;}
 return 0;
}

typedef struct ws_fixture {uint32_t slots[25],mutation,calls,trace[25];} ws_fixture;
static void ws_release(void *context,uint32_t token)
{
 ws_fixture *f=context;f->trace[f->calls++]=token;
 if(f->mutation==1)f->slots[24]=0x12345678;
 if(f->mutation==2)f->slots[24]=0;
 if(f->mutation==3)f->slots[0]=0xabcdef;
}
static int weapon_slots_probe(void)
{
 ws_fixture f;int status;
 while(fread(f.slots,104,1,stdin)==1){f.calls=0;memset(f.trace,0,sizeof(f.trace));
 status=rf_weapon_release_player_slots(f.slots,ws_release,&f);
 if(fwrite(&status,4,1,stdout)!=1 || fwrite(f.slots,100,1,stdout)!=1 || fwrite(&f.calls,104,1,stdout)!=1)return 3;}
 return 0;
}
