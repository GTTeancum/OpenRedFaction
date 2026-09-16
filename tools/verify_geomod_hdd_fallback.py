"""Companion for explicit native RFSG seed/fallback diagnostics; never launches XEMU.

prepare writes a staging/observation contract from prepare_geomod_hdd_fallback
outputs. verify consumes captured QMP samples and RFDS from future native runs.
The seed helper is intentionally unlinked/uninvoked until primary integration.
Each sample JSON uses literal symbol names as keys, each value an integer array.
No claimed pass without seed, pre-store, post-store, fresh-reload and RFDS files.
"""
import argparse,hashlib,json,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def fnv(data):
 h=2166136261
 for v in data:h=((h^v)*16777619)&0xffffffff
 return h
def envelope(data,generation):
 h=b'RFSG'+struct.pack('<IIII',1,generation,len(data),fnv(data));return h+struct.pack('<I',fnv(h))+data

def main():
 p=argparse.ArgumentParser(description=__doc__);sub=p.add_subparsers(dest='command',required=True)
 prep=sub.add_parser('prepare');prep.add_argument('--fixtures',type=Path,required=True)
 verify=sub.add_parser('verify');verify.add_argument('--fixtures',type=Path,required=True)
 for name in ('seed','before','after','reload','rfds'):verify.add_argument('--'+name,type=Path,required=True)
 a=p.parse_args();base=a.fixtures.resolve();good=(base/'baseline.rfds').read_bytes();bad=(base/'candidate.rfds').read_bytes();slot0=(base/'seed-slot0.rfsg').read_bytes();slot1=(base/'seed-slot1.rfsg').read_bytes()
 if slot0!=envelope(good,1) or slot1!=envelope(bad,2) or bad==good:raise ValueError('Fixture envelope mismatch')
 contract=dict(scope='DEV semantic fallback; normal store unchanged; no physical durability claim',
  stage={'geomod-fallback-seed.flag':'empty file','geomod-fallback0.rfsg':str(base/'seed-slot0.rfsg'),'geomod-fallback1.rfsg':str(base/'seed-slot1.rfsg')},
  missing_integration=['Link checkpoint_fixture.c only for explicit diagnostic','Guest DEV seed mode owns mounted writable session, invokes seed then exits without regular save','Before normal load+save capture selection and hash_slots after pure load but before store','After store capture hash_slots before closing alias','Fresh process loads replacement with no seed files/flag and exports RFDS'],
  seed_expected=[5,0,0,0,fnv(slot0),fnv(slot1),len(slot0),len(slot1)],
  before_hashes=[0,0,fnv(slot0),len(slot0),fnv(slot1),len(slot1)],
  after_hashes=[0,0,fnv(slot0),len(slot0),fnv(envelope(good,2)),len(good)+24],
  protected_sha256=hashlib.sha256(slot0).hexdigest(),baseline_sha256=hashlib.sha256(good).hexdigest(),
  safety='Host launcher must use a new private standalone QCOW2 copy; never point seed mode at base/user HDD; refuse existing destination slots; partial seed failure requires new disposable copy')
 if a.command=='prepare':
  (base/'native-contract.json').write_text(json.dumps(contract,indent=2)+'\n');print(base/'native-contract.json');return
 samples={name:json.loads(getattr(a,name).read_text()) for name in ('seed','before','after','reload')}
 def need(ok,why):
  if not ok:raise RuntimeError(why)
 need(samples['seed']['rf_xbox_checkpoint_fixture_seed_state']==contract['seed_expected'],'Seed checks/flush/readback did not match')
 need(samples['before']['rf_xbox_checkpoint_fixture_slots']==contract['before_hashes'],'Before-store files differ from seeded envelopes')
 need(samples['after']['rf_xbox_checkpoint_fixture_slots']==contract['after_hashes'],'Protected older slot changed or replacement envelope wrong')
 before=samples['before']['rf_xbox_checkpoint_storage_state'];after=samples['after']['rf_xbox_checkpoint_storage_state'];reload=samples['reload']['rf_xbox_checkpoint_storage_state']
 need(before[0:2]==[3,0] and before[4:7]==[1,0,len(good)],'Pure load did not select older valid generation')
 need(after[0:2]==[5,0] and after[4:7]==[2,1,len(good)] and after[7]&4,'Normal save/flush did not replace bad newer slot')
 need(reload[0:2]==[3,0] and reload[4:7]==[2,1,len(good)],'Fresh load did not select repaired generation')
 need(a.rfds.read_bytes()==good,'Fresh RFDS differs from baseline')
 result=dict(result='PASS',scope=contract['scope'],samples=samples,baseline_sha256=contract['baseline_sha256'],limits='EnvelopeFNV telemetry is accidental-corruption evidence; independent fullslot bytes strengthen proof. Host-owned-HDD and separate-process provenance must come from launcher reports.')
 (base/'native-fallback-report.json').write_text(json.dumps(result,indent=2)+'\n');print('PASS native semantic fallback/older-slot preservation/fresh reload')
if __name__=='__main__':main()
