static int player_contact_probe(void)
{
 struct {float normal[3],basis[9];uint32_t present,flags,mark;} in;uint32_t out[3];
 _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
 while(fread(&in,sizeof(in),1,stdin)==1){out[1]=0xa5a5a5a5u;out[2]=in.flags;
 out[0]=(uint32_t)rf_player_contact_direction(in.normal,in.present?in.basis:NULL,out+1);
 rf_player_contact_mark(in.present?out+2:NULL,in.mark);
 if(fwrite(out,sizeof(out),1,stdout)!=1)return 2;}
 return ferror(stdin)?1:0;
}
