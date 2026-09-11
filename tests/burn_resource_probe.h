static uint32_t brr_voice,brr_owner;
static void brr_stop(void *context,uint32_t voice){(void)context;brr_voice=voice;}
static void brr_clear(void *context,uint32_t token){(void)context;brr_owner=token;}
static int burn_resource_probe(void)
{
    static rf_particle records[1600];static rf_particle_list lists[133];static rf_emitter_slot slots[128];
    rf_particle_pool particles;rf_emitter_pool emitters;rf_burn_pool burns;
    rf_burn_release_owner_backend backend={brr_stop,brr_clear,NULL};uint32_t wire[4],i,j,out[4];int status;
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(wire,sizeof(wire),1,stdin)==1) {
        rf_particle_pool_init(&particles,records,lists,133);rf_emitter_pool_init(&emitters,slots,&particles);
        emitters.lists[0].next=4;slots[4].previous=128;emitters.lists[1]=(rf_particle_list){0,3};emitters.live=4;
        for(i=0;i<4;++i) {
            slots[i].active=1;slots[i].runtime.enabled=0xa5a50001u+i;
            slots[i].next=i==3?129:i+1;slots[i].previous=i?i-1:129;
            lists[5+i]=(rf_particle_list){500+i*2,501+i*2};
            for(j=0;j<2;++j) {
                rf_particle *p=records+500+i*2+j;p->next=j?1605+i:501+i*2;
                p->previous=j?500+i*2:1605+i;p->flags=1;p->owner=UINT32_MAX;p->room=1;p->emitter=i+1;p->pool=1;
            }
        }
        lists[4]=(rf_particle_list){508,508};records[508].next=records[508].previous=1604;
        records[508].flags=1;records[508].owner=UINT32_MAX;records[508].room=1;records[508].pool=1;
        lists[1].next=509;records[509].previous=1601;particles.live[1]=9;
        memset(&burns,0,sizeof(burns));burns.free_head=2;burns.active_head=1;burns.spread_deadline=-1;
        for(i=0;i<8;++i) {burns.records[i].voice=UINT32_MAX;burns.records[i].next=i==7?2:i+2;burns.records[i].previous=i==1?8:i;}
        burns.records[0].next=burns.records[0].previous=1;burns.records[0].voice=wire[3];
        burns.records[0].target=123;burns.records[0].volume=.75f;burns.records[0].elapsed=7;
        for(i=0;i<4;++i)burns.records[0].emitters[i]=(wire[0]&(1u<<i))?1+(i+wire[1])%4:0;
        brr_voice=brr_owner=UINT32_MAX;
        status=rf_burn_release_resolved(&burns,1,wire[2],&emitters,&backend);
        fwrite(&status,4,1,stdout);fwrite(&burns,sizeof(burns),1,stdout);
        for(i=0;i<128;++i) {out[0]=slots[i].next;out[1]=slots[i].previous;out[2]=slots[i].active;out[3]=slots[i].runtime.enabled;fwrite(out,4,4,stdout);}
        fwrite(emitters.lists,8,2,stdout);fwrite(&emitters.live,4,1,stdout);
        fwrite(records+500,120,9,stdout);fwrite(lists+4,8,5,stdout);fwrite(particles.live,4,2,stdout);
        fwrite(&brr_voice,4,1,stdout);fwrite(&brr_owner,4,1,stdout);
    }
    return ferror(stdin)?2:0;
}
