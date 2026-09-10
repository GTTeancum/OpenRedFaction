"""Shared owned-player state selection versus original full-selector fixtures."""
import hashlib,json,re,struct,subprocess,sys
from pathlib import Path
import pefile
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
subprocess.run([sys.executable,str(root/'tools/inspect_player_motion.py')],cwd=root,check=True,stdout=subprocess.DEVNULL)
reference=json.loads((root/'artifacts/player-motion-reference.json').read_text());assert reference['result']=='PASS'
pack=lambda v:struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v])
commands=[];expected=[]
for case in reference['records']:
    commands.append(struct.pack('<3f',*case['direction'])+pack([case.get('present',1),case.get('parent_kind',-1),
        case.get('first_seat',0),case.get('second_seat',0),case['crouched'],int(case['mode'] in (3,8)),
        int(case['mode'] in (4,7)),case['attachment'],case['primary'],case['hidden']]))
    expected.append(case['selected'])
probe=root/'build/pc/Release/rf_entity_probe.exe'
assert subprocess.check_output([str(probe),'--player-motion'],input=b''.join(commands))==pack(expected)
binary=root/'build/xbox/main.exe';pe=pefile.PE(str(binary));image=pe.get_memory_mapped_image();origin=pe.OPTIONAL_HEADER.ImageBase
u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(origin,(len(image)+4095)//4096*4096);u.mem_write(origin,image)
base=0x30000000;u.mem_map(base,65536);output,stack,stop=base+0x1000,base+0xe000,base+0xf000
entry=int(re.search(r'_rf_player_motion_choose\s+([0-9a-fA-F]+)',(root/'build/xbox/main.map').read_text())[1],16)
def execute(command,status,want):
    u.mem_write(base,command);u.mem_write(output,pack([0xa5a5a5a5]));u.mem_write(stack,pack([stop,base,output]))
    u.reg_write(UC_X86_REG_ESP,stack);u.emu_start(entry,stop,count=10000)
    assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_EAX)==status&0xffffffff
    assert bytes(u.mem_read(base,len(command)))==command and bytes(u.mem_read(output,4))==pack([want])
for command,want in zip(commands,expected):execute(command,0,want)
guards=[]
for offset in (0,4,8):
    for bad in (0x7fc00000,0x7f800000):
        command=bytearray(commands[0]);command[offset:offset+4]=pack([bad]);guards.append(bytes(command))
for offset in (12,20,24,28,32,36,48):
    command=bytearray(commands[0]);command[offset:offset+4]=pack([2]);guards.append(bytes(command))
for command in guards:execute(command,-4,0xa5a5a5a5)
report=dict(result='PASS',pc_cases=len(commands),nxdk_cases=len(commands),nxdk_guards=len(guards),
 original_sha256=reference['original_sha256'],pc_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),nxdk_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
 scope='C selected logical state matches full original 4a5cd0 fixtures, including parent/seat priority. The caller resolves original predicates and handles controller availability/transitions. No live integration, stance-effect or loaded-pose claim.')
(root/'artifacts/player-motion-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
