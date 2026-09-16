"""Generate RFDS negative candidates and independently checksummed two-slot files.
Input is a retained nonempty RFDS from tools/dev_geomod_checkpoint_check.py.
No build/game execution; manifest records input hashes and expected source stages.
"""
import argparse, hashlib, json, struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def u32(b,o):return struct.unpack_from('<I',b,o)[0]
def put(b,o,v):struct.pack_into('<I',b,o,v)
def fnv(b):
 h=2166136261
 for v in b:h=((h^v)*16777619)&0xffffffff
 return h
def slot(payload,generation):
 h=b'RFSG'+struct.pack('<IIII',1,generation,len(payload),fnv(payload))
 return h+struct.pack('<I',fnv(h))+payload
parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,default=ROOT/'artifacts/geomod-checkpoint/repeated.rfds');parser.add_argument('--output',type=Path,default=ROOT/'artifacts/geomod-rfds-validation');args=parser.parse_args();OUT=args.output.resolve();OUT.mkdir(parents=True,exist_ok=True)
source=args.source.read_bytes();assert source[:8]==b'RFDS\1\0\0\0' and u32(source,8)==len(source)
admissions,core,maps,faces=u32(source,240),u32(source,252),u32(source,248),u32(source,272)
a0=288+core;m0=a0+48*admissions;f0=m0+88*maps
assert f0+faces*2==len(source) and admissions and maps
bindings=list(struct.unpack_from('<'+'H'*faces,source,f0));generated=next(i for i,m in enumerate(bindings) if m!=65535);authored=next(i for i,m in enumerate(bindings) if m==65535)
used=set(bindings)-{65535};unused=next((i for i in range(maps) if i not in used),None);cases=[]
def add(name,edit,stage,reason,expect='reject'):
 b=bytearray(source);edit(b);folder=OUT/name;folder.mkdir(exist_ok=True)
 (folder/'candidate.rfds').write_bytes(b);(folder/'checkpoint.0').write_bytes(slot(source,1));(folder/'checkpoint.1').write_bytes(slot(b,2))
 cases.append(dict(name=name,expected=expect,source_stage=stage,reason=reason,bytes=len(b),candidate_sha256=hashlib.sha256(b).hexdigest(),path=str(folder),slots='older valid generation1; candidate generation2 with valid RFSG checksums'))
add('reserved-header',lambda b:put(b,12,1),'before_admissions','RFDS reserved word must zero')
add('identity',lambda b:b.__setitem__(80,b[80]^1),'before_admissions','source/material identity mismatch')
add('name-padding',lambda b:b.__setitem__(16+len(source[16:80].split(bytes([0]))[0])+1,1),'before_admissions','nonzero byte after NUL level name')
add('reserved-tail',lambda b:b.__setitem__(287,1),'before_admissions','reserved header tail')
add('admission-nan',lambda b:put(b,a0,0x7fc00000),'admissions','nonfinite center')
add('admission-zero-scale',lambda b:put(b,a0+36,0),'admissions','scale must positive')
add('admission-reserved',lambda b:b.__setitem__(a0+47,1),'admissions','reserved admission byte')
add('map-zero-normal',lambda b:b.__setitem__(slice(m0,m0+12),bytes(12)),'maps','unit plane normal required')
add('map-reserved-material',lambda b:put(b,m0+40,1),'maps','serialized map material must0')
add('map-infinite-scale',lambda b:put(b,m0+72,0x7f800000),'maps','finite positive scale required')
add('map-infinite-offset',lambda b:put(b,m0+80,0x7f800000),'maps','finite offset required')
add('map-wrong-axis',lambda b:put(b,m0+64,3),'maps','axis must exact dominant-axis cyclic projection')
add('map-base-rng',lambda b:put(b,m0+60,u32(b,m0+60)^1),'maps','base seed chain mismatch')
add('map-packing',lambda b:put(b,m0+44,u32(b,m0+44)+1),'maps','atlas packed cursor mismatch')
add('final-noise-rng',lambda b:put(b,256,u32(b,256)^1),'after_maps','terminal noise chain mismatch')
# First RGCH record face span follows24-byte cutter header and20-byte vertices.
cf=288+28+24+u32(source,288+28+4)*20
add('core-source-injection',lambda b:put(b,cf+12,0),'core_decode','generated cutter source ID must sentinel')
add('core-material',lambda b:put(b,cf+8,1),'material_remap','serialized generated material must0')
add('generated-map-oob',lambda b:struct.pack_into('<H',b,f0+2*generated,maps),'after_core_publish','generated face map index out of range')
add('authored-map-not-sentinel',lambda b:struct.pack_into('<H',b,f0+2*authored,0),'after_core_publish','authored source face must map to65535')
if unused is not None:
 at=m0+88*unused
 add('unused-map-zero-normal',lambda b:b.__setitem__(slice(at,at+12),bytes(12)),'maps','even retained unreferenced maps require valid normals')
# Finite-only admission validation does not currently reject zero basis vectors.
# Whether this is malformed is a semantic question, not an asserted regression.
add('admission-zero-basis',lambda b:b.__setitem__(slice(a0+12,a0+36),bytes(24)),'admissions','finite-only admission check accepts basis zeros; determine admission policy before requiring rejection','investigate')
# Truncate final binding but correct RFDS length so total-span check is exercised.
add('truncated-bindings',lambda b:(b.pop(),put(b,8,len(b))),'before_admissions','total spans disagree despite rewritten outer length')
manifest=dict(root=str(ROOT),source=str(args.source.resolve()),source_sha256=hashlib.sha256(source).hexdigest(),counts=dict(admissions=admissions,maps=maps,faces=faces,core_bytes=core,unused_map=unused),cases=cases,scope='Generated only. Expected stages derive from current scene_checkpoint_restore source; no runtime acceptance/rejection claim.')
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print('Generated',len(cases),'RFDS candidates with independently valid two-slot checksums')
