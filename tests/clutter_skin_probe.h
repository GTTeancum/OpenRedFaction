typedef struct clutter_skin_probe_variant {char name[16],textures[6][16];int32_t count,glare;} clutter_skin_probe_variant;
typedef struct clutter_skin_probe_input {
    uint32_t count;int32_t material_count;char name[16];clutter_skin_probe_variant variants[6];
    rf_model_material_record materials[6];rf_clutter_skin_glare glares[6];
} clutter_skin_probe_input;
typedef struct clutter_skin_probe_trace {uint32_t kind;char name[16];} clutter_skin_probe_trace;
typedef struct clutter_skin_probe_output {
    int32_t status,selected;uint32_t calls;clutter_skin_probe_trace trace[7];
    rf_model_material_record materials[6];rf_clutter_skin_glare glares[6];
} clutter_skin_probe_output;
typedef struct clutter_skin_probe_context {clutter_skin_probe_input *in;clutter_skin_probe_output *out;} clutter_skin_probe_context;
static int clutter_skin_probe_materials(void *context,uint32_t model,rf_model_material_record **records,int32_t *count)
{
    clutter_skin_probe_context *c=context;if(model!=0x12345678 || c->out->calls>=7)abort();
    c->out->trace[c->out->calls++].kind=1;*records=c->out->materials;*count=c->in->material_count;return RF_OK;
}
static int clutter_skin_probe_texture(void *context,const char *name,int32_t first,uint32_t second,int32_t *result)
{
    clutter_skin_probe_context *c=context;clutter_skin_probe_trace *trace;uint32_t value=0x900000;const unsigned char *p=(const unsigned char *)name;
    if(first!=-1 || second!=1 || c->out->calls>=7 || strlen(name)>=16)abort();
    trace=c->out->trace+c->out->calls++;trace->kind=2;strcpy(trace->name,name);
    while(*p)value+=*p++;
    *result=!strcmp(name,"missing")?-1:(int32_t)value;return RF_OK;
}
static int clutter_skin_probe(void)
{
    clutter_skin_probe_input input;clutter_skin_probe_output output;uint32_t i,j;
    _Static_assert(sizeof(input)==2016,"Clutter skin input");_Static_assert(sizeof(output)==1424,"Clutter skin output");
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(&input,sizeof(input),1,stdin)==1) {
        rf_clutter_skin_variant variants[6];const char *textures[6][6];const void *classes[5];
        clutter_skin_probe_context context={&input,&output};rf_clutter_skin_backend backend={clutter_skin_probe_materials,clutter_skin_probe_texture,&context};
        if(input.count>6)return 4;
        memset(&output,0,sizeof(output));output.selected=0x12345678;
        memcpy(output.materials,input.materials,sizeof(output.materials));memcpy(output.glares,input.glares,sizeof(output.glares));
        for(i=0;i<5;++i)classes[i]=(const void *)(uintptr_t)(0x5c9e98+i*52);
        for(i=0;i<input.count;++i) {
            if(input.variants[i].count>6)return 4;
            for(j=0;j<6;++j)textures[i][j]=input.variants[i].textures[j];
            variants[i].name=input.variants[i].name;variants[i].textures=textures[i];
            variants[i].texture_count=input.variants[i].count;variants[i].glare_class=input.variants[i].glare;
        }
        output.status=rf_clutter_skin_apply(variants,input.count,input.name,77,0x12345678,output.glares,6,classes,5,&backend,&output.selected);
        if(fwrite(&output,sizeof(output),1,stdout)!=1)return 3;
    }
    return ferror(stdin)?3:0;
}
