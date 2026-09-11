typedef struct burn_retarget_probe_context {int32_t indices[4],status;uint32_t calls[4];rf_burn_record *record;} burn_retarget_probe_context;
static int burn_retarget_probe_bones(void *context,uint32_t target,int32_t out[4])
{
    burn_retarget_probe_context *c=context;c->calls[0]++;c->calls[1]=target;c->calls[2]=c->record->target;
    memcpy(out,c->indices,16);return c->status;
}
static void burn_retarget_probe_release(void *context,uint32_t token)
{burn_retarget_probe_context *c=context;c->calls[3]=token;}
static int burn_retarget_probe(void)
{
    struct {rf_burn_record record;uint32_t target,present,flags;int32_t owners[4],indices[4],status;} in;
    while(fread(&in,sizeof(in),1,stdin)==1) {
        burn_retarget_probe_context c;rf_burn_retarget_backend be={burn_retarget_probe_bones,burn_retarget_probe_release,&c};
        int32_t *owners[4]={in.owners,in.owners+1,in.owners+2,in.owners+3};int status;
        memset(&c,0,sizeof(c));memcpy(c.indices,in.indices,16);c.status=in.status;c.record=&in.record;
        status=rf_burn_retarget(&in.record,3,in.target,in.present?&in.flags:NULL,owners,&be);
        fwrite(&status,4,1,stdout);fwrite(&in.record,64,1,stdout);fwrite(&in.flags,4,5,stdout);fwrite(c.calls,4,4,stdout);
    }
    return ferror(stdin)?1:0;
}
