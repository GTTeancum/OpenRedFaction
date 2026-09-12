static int clutter_classes_probe(void)
{
    rf_clutter_definition *defs;rf_clutter_class_binding *bindings;rf_clutter_classes owner={0},before={0};
    uint32_t count,budget,i,j,n;int32_t *ids;int status;
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    if(sizeof(rf_clutter_class)!=96 || fread(&count,4,1,stdin)!=1 || fread(&budget,4,1,stdin)!=1 || count>1024)return 2;
    defs=calloc(count?count:1,sizeof(*defs));bindings=calloc(count?count:1,sizeof(*bindings));ids=calloc(count?count*16:1,4);
    if(!defs || !bindings || !ids)return 2;
    if(fread(defs,sizeof(*defs),count,stdin)!=count)return 2;
    for(i=0;i<count;++i) {
        if(defs[i].emitter_count>16 || fread(bindings+i,20,1,stdin)!=1 || fread(ids+i*16,4,defs[i].emitter_count,stdin)!=defs[i].emitter_count)return 2;
        bindings[i].emitters=ids+i*16;
    }
    status=rf_clutter_classes_open(defs,bindings,count,budget,&owner);
    if(status && memcmp(&owner,&before,sizeof(owner)))return 3;
    memset(defs,0xcc,count*sizeof(*defs));memset(bindings,0xcc,count*sizeof(*bindings));memset(ids,0xcc,count*16*4);
    free(defs);free(bindings);free(ids);
    fwrite(&status,4,1,stdout);fwrite(&owner.allocated_bytes,4,1,stdout);fwrite(&owner.count,4,1,stdout);
    if(!status)for(i=0;i<count;++i) {
        const rf_clutter_class *c=owner.items+i;const char *names[]={c->name,c->model,c->corpse};
        fwrite(&c->emitter_count,80,1,stdout);
        for(j=0;j<3;++j){n=(uint32_t)strlen(names[j]);fwrite(&n,4,1,stdout);fwrite(names[j],1,n,stdout);}
        fwrite(c->emitters,4,c->emitter_count,stdout);
    }
    rf_clutter_classes_close(&owner);rf_clutter_classes_close(&owner);
    return memcmp(&owner,&before,sizeof(owner))?4:0;
}
