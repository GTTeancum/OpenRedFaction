"""Static instruction evidence for original projectile factory lifetime boundaries.
This is an audit, not execution coverage or a reconstructed factory.
"""
import hashlib,json,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'local/python'))
import pefile,capstone
exe=root/'Installed_Game/RF.exe';sha=hashlib.sha256(exe.read_bytes()).hexdigest()
assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
p=pefile.PE(str(exe));base=p.OPTIONAL_HEADER.ImageBase;data=p.get_memory_mapped_image();decoder=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);decoder.detail=True
start,end=0x4c77a0,0x4c8030;instructions=list(decoder.disasm(data[start-base:end-base],start))
assert instructions[0].address==start and instructions[-1].address+instructions[-1].size==end
names={0x486da0:'generic object creation',0x4c8030:'projectile model setup',0x4d8ed0:'light creation',0x497ca0:'effect attachment',0x5056a0:'positional sound',0x48c9a0:'collision registration',0x4c2570:'special projectile setup',0x49a420:'ordinary collision',0x49afe0:'model collision',0x4c4b50:'projectile hit processing',0x48ab40:'mark object flag7c bit2 for later retirement'}
calls=[]
for ins in instructions:
 if ins.mnemonic=='call' and ins.operands[0].type==capstone.x86.X86_OP_IMM:
  dest=ins.operands[0].imm;calls.append(dict(site=f'{ins.address:08x}',destination=f'{dest:08x}',boundary=names.get(dest)))
for target in names:assert any(int(c['destination'],16)==target for c in calls),hex(target)
creation=next(int(c['site'],16) for c in calls if int(c['destination'],16)==0x486da0)
retirement=next(int(c['site'],16) for c in calls if int(c['destination'],16)==0x48ab40)
hit=next(int(c['site'],16) for c in calls if int(c['destination'],16)==0x4c4b50)
assert creation<hit<retirement
# Retained original instruction listing makes address claims independently reviewable.
listing='\n'.join(f'{i.address:08x} {i.bytes.hex():<20} {i.mnemonic} {i.op_str}' for i in instructions)+'\n'
(root/'artifacts/projectile-factory-instructions.txt').write_text(listing)
report=dict(result='AUDITED',original_sha256=sha,start=f'{start:08x}',end_exclusive=f'{end:08x}',instruction_count=len(instructions),direct_calls=calls,scope='Static decoding only. Verifies boundary presence and address order; conditional reachability and runtime effects require execution tests. Factory reconstruction remains open.')
(root/'artifacts/projectile-factory-audit.json').write_text(json.dumps(report,indent=2));print(json.dumps({k:v for k,v in report.items() if k!='direct_calls'}));print(json.dumps([c for c in calls if c['boundary']],indent=2))
