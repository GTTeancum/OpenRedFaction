static uint32_t skeletal_node_index(rf_model_skeletal_registration *nodes,rf_model_skeletal_registration *node)
{return node?(uint32_t)(node-nodes):UINT32_MAX;}
static int skeletal_release_probe(void)
{
    uint32_t meta[3],i,out[11];rf_motion_slot_state active;int32_t refs[16];rf_motion_playback_resource resources[16];
    rf_model_skeletal_registration nodes[4],*head;
    while(fread(meta,sizeof(meta),1,stdin)==1) {
        if(meta[0]<1 || meta[0]>4 || meta[1]>=meta[0] || fread(&active,sizeof(active),1,stdin)!=1 || fread(refs,sizeof(refs),1,stdin)!=1)return 2;
        memset(nodes,0,sizeof(nodes));memset(resources,0,sizeof(resources));
        for(i=0;i<16;++i)resources[i].references=refs[i];
        for(i=0;i<meta[0];++i){nodes[i].next=&nodes[(i+1)%meta[0]];nodes[i].previous=&nodes[(i+meta[0]-1)%meta[0]];}
        nodes[meta[1]].active=&active;nodes[meta[1]].loaded=meta[2];head=nodes;
        out[0]=(uint32_t)rf_model_skeletal_retire(nodes+meta[1],&head,4,resources,16);
        out[1]=skeletal_node_index(nodes,head);out[2]=nodes[meta[1]].loaded;
        for(i=0;i<4;++i){out[3+2*i]=skeletal_node_index(nodes,nodes[i].next);out[4+2*i]=skeletal_node_index(nodes,nodes[i].previous);}
        for(i=0;i<16;++i)refs[i]=resources[i].references;
        if(fwrite(out,sizeof(out),1,stdout)!=1 || fwrite(&active,sizeof(active),1,stdout)!=1 || fwrite(refs,sizeof(refs),1,stdout)!=1)return 2;
    }
    return ferror(stdin)?2:0;
}

static int skeletal_register_probe(void)
{
    uint32_t count,i,out[68];rf_model_skeletal_registration nodes[33],*head;rf_motion_slot_state active;
    while(fread(&count,4,1,stdin)==1) {
        if(count>32)return 2;memset(nodes,0,sizeof(nodes));memset(&active,0xa5,sizeof(active));
        for(i=0;i<count;++i){nodes[i].next=&nodes[(i+1)%count];nodes[i].previous=&nodes[(i+count-1)%count];}
        nodes[count].loaded=1;nodes[count].active=&active;head=count?nodes:NULL;
        out[0]=(uint32_t)rf_model_skeletal_register(nodes+count,&head,33);out[1]=skeletal_node_index(nodes,head);
        for(i=0;i<33;++i){out[2+2*i]=skeletal_node_index(nodes,nodes[i].next);out[3+2*i]=skeletal_node_index(nodes,nodes[i].previous);}
        for(i=0;i<sizeof(active);++i)if(((unsigned char *)&active)[i]!=0xa5)return 3;
        if(fwrite(out,sizeof(out),1,stdout)!=1)return 2;
    }
    return ferror(stdin)?2:0;
}
