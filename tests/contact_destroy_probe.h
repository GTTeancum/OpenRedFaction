typedef struct contact_destroy_fixture {rf_entity_contact_destroy_actor actor;uint32_t mutation,fail,hash,count;} contact_destroy_fixture;
static void cd_word(contact_destroy_fixture *f,uint32_t v){f->hash=(f->hash^v)*16777619u;}
static int cd_damage(void *context,rf_entity_contact_destroy_actor *a,const rf_damage_request *r)
{
    contact_destroy_fixture *f=context;uint32_t amount;memcpy(&amount,&r->amount,4);++f->count;cd_word(f,1);
    cd_word(f,a->handle);cd_word(f,amount);cd_word(f,r->source);cd_word(f,UINT32_MAX);cd_word(f,(uint32_t)r->kind);
    cd_word(f,r->argument6);cd_word(f,r->auxiliary_uid);cd_word(f,r->force);
    if(f->mutation){a->sound=17;a->position[0]=9;}
    return f->fail==1?RF_IO:RF_OK;
}
static int cd_select(void *context,int32_t group,uint32_t *sample)
{
    contact_destroy_fixture *f=context;++f->count;cd_word(f,2);cd_word(f,(uint32_t)group);
    *sample=0xa5000000u^(uint32_t)group;if(f->mutation)f->actor.position[1]=-3;
    return f->fail==2?RF_IO:RF_OK;
}
static int cd_play(void *context,uint32_t sample,const float *position)
{
    contact_destroy_fixture *f=context;uint32_t words[3];memcpy(words,position,12);++f->count;cd_word(f,3);cd_word(f,sample);
    for(uint32_t i=0;i<3;++i)cd_word(f,words[i]);cd_word(f,0x3f800000);cd_word(f,0);
    return f->fail==3?RF_IO:RF_OK;
}
static int contact_destroy_probe(void)
{
    contact_destroy_fixture f;_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(&f,28,1,stdin)==1){
        rf_entity_contact_destroy_backend b={&f,cd_damage,cd_select,cd_play};int32_t status;
        f.hash=2166136261u;f.count=0;status=rf_entity_contact_destroy(&f.actor,&b);
        if(fwrite(&status,4,1,stdout)!=1 || fwrite(&f.actor,20,1,stdout)!=1 || fwrite(&f.hash,8,1,stdout)!=1)return 2;
    }
    return ferror(stdin)?1:0;
}
