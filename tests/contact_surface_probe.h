static int contact_surface_probe(void)
{
    rf_entity_contact_surface state;uint32_t out[2];
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    while(fread(&state,sizeof(state),1,stdin)==1){
        out[1]=0xa5a5a5a5u;out[0]=(uint32_t)rf_entity_contact_surface_route(&state,out+1);
        if(fwrite(out,sizeof(out),1,stdout)!=1)return 2;
    }
    return ferror(stdin)?1:0;
}
