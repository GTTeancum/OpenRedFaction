static int clutter_load_probe(const char *path,uint32_t budget)
{
    rf_vpp archive;rf_clutter_catalogs catalogs={0};rf_clutter_classes owner={0},before={0};
    rf_foley_owner sounds={0};uint32_t i,j,n,peak=0xa5a5a5a5;int status;
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    if(fread(&sounds.group_count,4,1,stdin)!=1 || sounds.group_count>640)return 2;
    sounds.groups=calloc(sounds.group_count?sounds.group_count:1,sizeof(*sounds.groups));if(!sounds.groups)return 2;
    for(i=0;i<sounds.group_count;++i)if(fread(sounds.groups[i].name,32,1,stdin)!=1)return 2;
    if(rf_vpp_open(&archive,path))return 2;
    status=rf_clutter_catalogs_open(&archive,&sounds,65536,&catalogs);if(status)return 3;
    status=rf_clutter_classes_load(&archive,&catalogs.names,budget,&owner,&peak);
    rf_vpp_close(&archive);rf_clutter_catalogs_close(&catalogs);free(sounds.groups);
    if(status && (memcmp(&owner,&before,sizeof(owner)) || peak!=0xa5a5a5a5))return 4;
    fwrite(&status,4,1,stdout);fwrite(&owner.allocated_bytes,4,1,stdout);fwrite(&owner.count,4,1,stdout);fwrite(&peak,4,1,stdout);
    if(!status)for(i=0;i<owner.count;++i) {
        rf_clutter_class *c=owner.items+i;const char *names[]={c->name,c->model,c->corpse};
        fwrite(&c->emitter_count,80,1,stdout);
        for(j=0;j<3;++j){n=(uint32_t)strlen(names[j]);fwrite(&n,4,1,stdout);fwrite(names[j],1,n,stdout);}
        fwrite(c->emitters,4,c->emitter_count,stdout);
    }
    rf_clutter_classes_close(&owner);rf_clutter_classes_close(&owner);
    return memcmp(&owner,&before,sizeof(owner))?5:0;
}
