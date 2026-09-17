"""Execute4667f4..4668b6 cutter dimensions/pose, supplying allocation and CRT draws."""
from probe_debris_motion import ROOT,hashlib,json,struct,pefile,Uc,UC_ARCH_X86,UC_MODE_32
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_FPCW

def main():
 raw=(ROOT/'Installed_Game/RF.exe').read_bytes();sha=hashlib.sha256(raw).hexdigest()
 assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
 image=pefile.PE(data=raw).get_memory_mapped_image();u=Uc(UC_ARCH_X86,UC_MODE_32)
 u.mem_map(0x400000,(len(image)+4095)&~4095);u.mem_write(0x400000,image)
 base=0x30000000;u.mem_map(base,65536);node=base+0x1000;sp=base+0xe000
 state=0;draws=0;args=[]
 def hook(cpu,a,size,data):
  nonlocal state,draws,args
  esp=cpu.reg_read(UC_X86_REG_ESP)
  if a==0x57312d:
   state=(state*214013+2531011)&0xffffffff;draws+=1;cpu.reg_write(UC_X86_REG_EAX,(state>>16)&32767)
  elif a==0x4eed90:
   args=list(struct.unpack('<6I',cpu.mem_read(esp+4,24)));cpu.reg_write(UC_X86_REG_EAX,base+0x3000)
  elif a!=0x4f80a0:return
  target=struct.unpack('<I',cpu.mem_read(esp,4))[0]
  cpu.reg_write(UC_X86_REG_ESP,esp+4);cpu.reg_write(UC_X86_REG_EIP,target)
 for address in [0x57312d,0x4eed90,0x4f80a0]:u.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
 rows=[]
 for axis in [(1,0,0),(0,1,0),(0,0,1)]:
  for length in [3.,10.,20.25]:
   for seed in [0,1,42,123456789,0xffffffff]:
    state=seed;draws=0;u.mem_write(base,bytes(65536));u.mem_write(node+8,struct.pack('<I',base+0x2000))
    u.mem_write(node+0x18,struct.pack('<5f',*axis,4.,length))
    for register,value in [(UC_X86_REG_ESP,sp),(UC_X86_REG_EDI,node),(UC_X86_REG_ESI,node),(UC_X86_REG_EBX,node+8),(UC_X86_REG_EAX,0),(UC_X86_REG_FPCW,0x27f)]:u.reg_write(register,value)
    u.emu_start(0x4667f4,0x4668b6,count=100000)
    assert u.reg_read(UC_X86_REG_EIP)==0x4668b6 and draws==3
    assert args[3:]==[0,0,0]
    values=args[:3]+list(struct.unpack('<9I',u.mem_read(0x647c70,36)))+list(struct.unpack('<3I',u.mem_read(0x648580,12)))
    rows.append(dict(axis=axis,length=length,seed=seed,next=state,output=values))
 def words(v):return ','.join(str(x)+'u' for x in v)
 def floatwords(v):return words(struct.unpack('<'+'I'*len(v),struct.pack('<'+'f'*len(v),*v)))
 (ROOT/'tests/fixtures/geomod_piece_cutter.inc').write_text('/* Original4667f4..4668b6; constructor bypassed, actual pose math. */\n'+''.join(' {{'+floatwords(r['axis'])+'},'+floatwords([r['length']])+','+str(r['seed'])+'u,'+str(r['next'])+'u,{'+words(r['output'])+'}},\n' for r in rows))
 (ROOT/'artifacts/geomod-postedit-re/piece-cutter.json').write_text(json.dumps(dict(sha256=sha,cases=rows,scope=__doc__),indent=2)+'\n')
 print('PASS',len(rows),'original cutter poses, three draws each')
if __name__=='__main__':main()
