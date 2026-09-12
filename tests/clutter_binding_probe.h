static int clutter_binding_probe(void)
{
    uint32_t counts[3],i,capacity;char en[256][64],gn[64][64],vn[64][64];
    const char *ep[256],*gp[64],*vp[64];rf_foley_group groups[640]={0};rf_foley_owner sounds={0};
    rf_clutter_resource_names names;rf_clutter_definition d;rf_clutter_class_binding b,before;
    int32_t ids[16],saved[16];int status;
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    if(fread(counts,4,3,stdin)!=3 || counts[0]>256 || counts[1]>64 || counts[2]>640)return 2;
    if(fread(en,64,counts[0],stdin)!=counts[0] || fread(gn,64,counts[1],stdin)!=counts[1] || fread(vn,64,64,stdin)!=64)return 2;
    for(i=0;i<counts[0];++i){if(!memchr(en[i],0,64))return 2;ep[i]=en[i];}
    for(i=0;i<counts[1];++i){if(!memchr(gn[i],0,64))return 2;gp[i]=gn[i];}
    for(i=0;i<64;++i){if(!memchr(vn[i],0,64))return 2;vp[i]=vn[i];}
    for(i=0;i<counts[2];++i)if(fread(groups[i].name,32,1,stdin)!=1 || !memchr(groups[i].name,0,32))return 2;
    sounds.groups=groups;sounds.group_count=counts[2];
    names=(rf_clutter_resource_names){ep,counts[0],gp,counts[1],vp,&sounds};
    while(fread(&d,sizeof(d),1,stdin)==1) {
        if(fread(&capacity,4,1,stdin)!=1)return 2;
        memset(&b,0xa5,sizeof(b));before=b;memset(ids,0xa5,sizeof(ids));memcpy(saved,ids,sizeof(ids));
        status=rf_clutter_definition_bind(&d,&names,ids,capacity,&b);
        if(status && (memcmp(&b,&before,sizeof(b)) || memcmp(ids,saved,sizeof(ids))))return 3;
        if(!status){if(b.emitters && b.emitters!=ids)return 4;b.emitters=(const int32_t *)(b.emitters?1:0);}
        fwrite(&status,4,1,stdout);fwrite(&b,sizeof(b),1,stdout);fwrite(ids,sizeof(ids),1,stdout);
    }
    return 0;
}
