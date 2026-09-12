static int collision_cache_probe(void)
{
    uint32_t count,g,i;float evaluated[50][12]={{0}},stored[50][12]={{0}};uint16_t stamps[50];
    for(count=1;count<=50;++count)for(g=0;g<3;++g) {
        rf_entity_pose pose={0},moved;rf_entity_collision_cache cache={0};rf_collision_model_skin_pose view,before;
        rf_collision_model_part_query query={0};rf_collision_model_response_hit hit={0};uint32_t accepted=99;
        uint32_t generation=g==0?0:g==1?65535:1,budget=sizeof(cache)+count*50;
        pose.skeleton=7;pose.bone_count=count;pose.matrices=evaluated;pose.generations=stamps;pose.playback.generation=generation;
        for(i=0;i<count;++i){memset(evaluated[i],0,48);memset(stored[i],0,48);evaluated[i][0]=evaluated[i][4]=evaluated[i][8]=1;stored[i][0]=stored[i][4]=stored[i][8]=1;evaluated[i][9]=(float)i;stamps[i]=(uint16_t)generation;}
        if(rf_entity_collision_cache_open(&pose,budget-1,&cache)!=RF_RANGE || cache.matrices || cache.allocated_bytes)return 30;
        if(rf_entity_collision_cache_open(&pose,budget,&cache) || cache.allocated_bytes!=budget)return 31;
        for(i=0;i<count;++i)if(cache.generations[i]==generation)return 32;
        memset(&before,0xa5,sizeof(before));view=before;stamps[count-1]=(uint16_t)(generation-1);
        if(rf_entity_collision_cache_view(&cache,&pose,stored,&view)!=RF_RANGE || memcmp(&view,&before,sizeof(view)))return 33;
        stamps[count-1]=(uint16_t)generation;moved=pose;
        if(rf_entity_collision_cache_view(&cache,&moved,stored,&view))return 34;
        query.input.flags=2;
        if(rf_collision_model_skinning_query(NULL,0,&view,&query,&hit,NULL,1,&accepted) || accepted || hit.time!=1)return 35;
        for(i=0;i<count;++i)if(cache.generations[i]!=generation || memcmp(cache.matrices[i],evaluated[i],48))return 36;
        /* Current stamps retain cache contents; next generation refreshes. */
        evaluated[0][9]=123;
        if(rf_collision_model_skinning_query(NULL,0,&view,&query,&hit,NULL,0,&accepted) || cache.matrices[0][9]!=0)return 37;
        moved.playback.generation=(generation+1)&65535;for(i=0;i<count;++i)stamps[i]=(uint16_t)moved.playback.generation;
        if(rf_entity_collision_cache_view(&cache,&moved,stored,&view) || rf_collision_model_skinning_query(NULL,0,&view,&query,&hit,NULL,0,&accepted) || cache.matrices[0][9]!=123)return 38;
        moved.skeleton=8;view=before;
        if(rf_entity_collision_cache_view(&cache,&moved,stored,&view)!=RF_RANGE || memcmp(&view,&before,sizeof(view)))return 39;
        rf_entity_collision_cache_close(&cache);rf_entity_collision_cache_close(&cache);
        if(cache.matrices || cache.generations || cache.bone_count || cache.skeleton || cache.allocated_bytes)return 40;
    }
    return 0;
}
static int registered_pose_probe(void)
{
    uint32_t bones,count,i;rf_model_bone_override overrides[50];
    for(bones=1;bones<=50;++bones)for(count=0;count<=16;count+=8) {
        rf_entity_pose source={0},*published=&source;rf_entity_owned_pose owned={0};
        rf_motion_playback_resource clips[16]={0};rf_entity_playback_model model={clips,NULL,16};
        rf_entity_playback_resources resources={0};rf_model_skeletal_registration node={0},*head=NULL;
        float matrices[50][12]={{0}};uint16_t stamps[50]={0};uint32_t budget=sizeof(owned)+bones*50;
        resources.models=&model;resources.model_count=1;source.bone_count=bones;
        source.matrices=matrices;source.generations=stamps;source.overrides=overrides;
        memset(overrides,0x5a,sizeof(overrides));budget+=bones*sizeof(*overrides);rf_motion_playback_initialize(&source.playback);
        source.playback.completion.active.count=count;
        for(i=0;i<16;++i){clips[i].references=2;source.playback.completion.active.slots[i].motion=i;}
        node.loaded=1;node.active=&source.playback.completion.active;
        if(rf_entity_registered_pose_take(&node,&published,&resources,budget,&owned)!=RF_RANGE)return 20;
        if(rf_model_skeletal_register(&node,&head,1))return 21;
        if(rf_entity_registered_pose_take(&node,&published,&resources,budget-1,&owned)!=RF_RANGE ||
           published!=&source || node.active!=&source.playback.completion.active || owned.storage || source.skeleton)return 22;
        if(rf_entity_registered_pose_take(&node,&published,&resources,budget,&owned) || published!=&owned.pose ||
           node.active!=&owned.pose.playback.completion.active || head!=&node || node.next!=&node || node.previous!=&node)return 23;
        memset(matrices,0xdd,sizeof(matrices));memset(stamps,0xdd,sizeof(stamps));
        memset(overrides,0xdd,sizeof(overrides));
        for(i=0;i<bones*sizeof(*overrides);++i)if(((unsigned char *)owned.pose.overrides)[i]!=0x5a)return 28;
        for(i=0;i<16;++i)if(clips[i].references!=2)return 24;
        if(rf_model_skeletal_retire(&node,&head,1,clips,16) || head || node.next || node.previous)return 25;
        if(rf_entity_owned_pose_close(&owned,&resources) || rf_entity_owned_pose_close(&owned,&resources))return 26;
        for(i=0;i<16;++i)if(clips[i].references!=(i<count?1:2))return 27;
    }
    return 0;
}
static int owned_pose_probe(void)
{
    rf_entity_pose source,before;rf_entity_owned_pose owned={0},saved;
    rf_motion_playback_resource clips[16]={0};rf_entity_playback_model model={clips,NULL,16};
    rf_entity_playback_resources resources={0};float matrices[50][12],expected[50][12];uint16_t stamps[50],saved_stamps[50];
    rf_model_bone_override overrides[50],expected_overrides[50];
    uint32_t bones,count,i,j,budget;resources.models=&model;resources.model_count=1;
    for(bones=1;bones<=50;bones++)for(count=0;count<=16;count+=8) {
        memset(&source,0,sizeof(source));memset(&owned,0,sizeof(owned));source.skeleton=0;source.bone_count=bones;
        source.matrices=matrices;source.generations=stamps;rf_motion_playback_initialize(&source.playback);
        source.playback.completion.active.count=count;source.playback.phase=.25f;source.controller.current=3;
        for(i=0;i<16;i++) {clips[i].references=2;source.playback.completion.active.slots[i].motion=(int32_t)i;}
        for(i=0;i<bones;i++) {stamps[i]=(uint16_t)(i+3);for(j=0;j<12;j++)matrices[i][j]=(float)(i*12+j)*.25f;}
        source.overrides=count?overrides:NULL;memset(overrides,0x5a,sizeof(overrides));memcpy(expected_overrides,overrides,sizeof(overrides));
        memcpy(expected,matrices,bones*48);memcpy(saved_stamps,stamps,bones*2);before=source;budget=sizeof(owned)+bones*(50+(source.overrides?sizeof(*overrides):0));
        if(rf_entity_pose_take(&source,&resources,budget-1,&owned)!=RF_RANGE || memcmp(&source,&before,sizeof(source)) || owned.storage)return 2;
        if(rf_entity_pose_take(&source,&resources,budget,&owned) || owned.allocated_bytes!=budget || source.skeleton!=UINT32_MAX ||
           source.playback.completion.active.count || source.matrices!=matrices || source.generations!=stamps)return 3;
        if(memcmp(&owned.pose.playback,&before.playback,sizeof(before.playback)) || memcmp(&owned.pose.controller,&before.controller,sizeof(before.controller)))return 4;
        if(count) {
            if(owned.pose.overrides==overrides || memcmp(owned.pose.overrides,expected_overrides,bones*sizeof(*overrides)))return 10;
            for(i=0;i<bones*sizeof(*overrides);++i)if(((unsigned char *)overrides)[i])return 11;
            memset(overrides,0xdd,sizeof(overrides));
            if(memcmp(owned.pose.overrides,expected_overrides,bones*sizeof(*overrides)))return 12;
        } else if(owned.pose.overrides)return 13;
        for(i=0;i<16;i++)if(clips[i].references!=2)return 5;
        for(i=0;i<bones;i++)if(stamps[i])return 6;
        memset(matrices,0xdd,sizeof(matrices));memset(stamps,0xdd,sizeof(stamps));
        if(memcmp(owned.pose.matrices,expected,bones*48) || memcmp(owned.pose.generations,saved_stamps,bones*2))return 7;
        if(count) {
            saved=owned;clips[0].references=0;
            if(rf_entity_owned_pose_close(&owned,&resources)!=RF_RANGE || memcmp(&owned,&saved,sizeof(owned)))return 8;
            clips[0].references=2;
        }
        if(rf_entity_owned_pose_close(&owned,&resources) || owned.storage || owned.allocated_bytes || rf_entity_owned_pose_close(&owned,&resources))return 9;
        for(i=0;i<16;i++)if(clips[i].references!=(i<count?1:2))return 10;
    }
    {int status=registered_pose_probe();if(status)return status;status=collision_cache_probe();if(status)return status;}
    printf("PASS 150 pose transfers; independent caches, moved references and repeatable close; PC owner %u bytes\n",(unsigned)sizeof(owned));return 0;
}
