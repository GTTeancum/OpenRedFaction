typedef struct cdp_fixture {rf_entity_contact_dispatch_state state;uint32_t flags,present,fail,trace;rf_entity_contact_object_view object;} cdp_fixture;
static int cdp_timed(void *c){cdp_fixture *f=c;f->trace=1;return f->fail?RF_IO:RF_OK;}
static int cdp_surface(void *c,uint32_t route){cdp_fixture *f=c;f->trace=route+1;return f->fail?RF_IO:RF_OK;}
static int cdp_lookup(void *c,uint32_t target,const rf_entity_contact_object_view **object){cdp_fixture *f=c;f->trace=5;f->object.handle=target;f->object.type=2;*object=f->present?&f->object:NULL;return f->fail?RF_IO:RF_OK;}
static int contact_dispatch_probe(void)
{cdp_fixture f;uint32_t out[3];_setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
 while(fread(&f,76,1,stdin)==1){rf_entity_view source={0};source.flags_7c=f.flags;rf_entity_contact_object_backend object={&f,cdp_lookup,0,0,0};rf_entity_contact_dispatch_backend b={&f,cdp_timed,cdp_surface,&object};f.trace=0;out[1]=0xa5a5a5a5u;out[0]=(uint32_t)rf_entity_contact_dispatch(&f.state,&source,&b,out+1);out[2]=f.trace;if(fwrite(out,12,1,stdout)!=1)return 2;}return ferror(stdin)?1:0;}
