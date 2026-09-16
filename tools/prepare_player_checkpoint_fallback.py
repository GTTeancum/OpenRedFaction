"""Prepare RFCP semantic-invalid newer slots; no build/game/emulator launch.
Optional existing host transport executable performs real shared load/store;
its baseline-equality oracle is deliberately not called scene validation.
"""
import argparse,hashlib,json,struct,subprocess
from pathlib import Path
from verify_geomod_hdd_fallback import envelope,fnv
ROOT=Path(__file__).resolve().parents[1]
def sha(b):return hashlib.sha256(b).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--transport-probe',type=Path);a=p.parse_args()
 good=a.source.read_bytes()
 if good[:8]!=b'RFCP\1\0\0\0' or struct.unpack_from('<I',good,8)[0]!=len(good) or good[32:36]!=b'RFPL' or good[576:580]!=b'RFDS':raise ValueError('Expected RFCP v1 player+destruction')
 if a.out.exists():raise ValueError('Choose a new fixture directory; existing files are preserved')
 a.out.mkdir(parents=True);rows=[]
 for name,position,reason in [('unsupported-air',(10,-8,0),'standing support RF_NOT_FOUND'),('blocked-detail',(3.875,-5.375,1),'physical detail penetration RF_NOT_FOUND')]:
  directory=a.out/name;directory.mkdir();bad=bytearray(good);struct.pack_into('<3f',bad,64,*position);bad=bytes(bad)
  older=envelope(good,1);newer=envelope(bad,2);replacement=envelope(good,2)
  for file,data in [('baseline.rfcp',good),('candidate.rfcp',bad),('seed-slot0.rfsg',older),('seed-slot1.rfsg',newer),('expected-replacement-slot1.rfsg',replacement)]:
   (directory/file).write_bytes(data)
  row=dict(case=name,path=str(directory.resolve()),payload_bytes=len(good),slot_bytes=len(older),position=position,expected_rejection=reason,
    baseline_sha256=sha(good),candidate_sha256=sha(bad),seed_expected=[5,0,0,0,fnv(older),fnv(newer),len(older),len(newer)],
    before_hashes=[0,0,fnv(older),len(older),fnv(newer),len(newer)],after_hashes=[0,0,fnv(older),len(older),fnv(replacement),len(replacement)],
    protected_slot0_sha256=sha(older),replacement_slot1_sha256=sha(replacement),selection_before=dict(slot=0,generation=1),selection_after=dict(slot=1,generation=2),selection_reload=dict(slot=1,generation=2),
    player_mode_required=True,profile_id=1,scope='Valid checksum envelopes; semantic invalidity must be checked by actual scene validator before native PASS')
  if a.transport_probe:
   (directory/'checkpoint.0').write_bytes(older);(directory/'checkpoint.1').write_bytes(newer)
   result=subprocess.run([str(a.transport_probe.resolve()),str((directory/'checkpoint').resolve()),str((directory/'baseline.rfcp').resolve())],cwd=ROOT,text=True,capture_output=True,check=True)
   if (directory/'checkpoint.0').read_bytes()!=older or (directory/'checkpoint.1').read_bytes()!=replacement:raise RuntimeError('Shared transport failed protected/replacement byte comparison')
   row.update(host_transport_result='PASS',host_transport_stdout=result.stdout,host_transport_binary_sha256=sha(a.transport_probe.read_bytes()),host_transport_scope='Real shared checksum/load/store with exact-baseline callback; no scene semantic or native proof')
  (directory/'native-contract.json').write_text(json.dumps(row,indent=2)+'\n');rows.append(row)
 report=dict(result='FIXTURES_READY',source=str(a.source.resolve()),source_sha256=sha(good),cases=rows,safety='Explicit player-checkpoint flag on all three phases; seed only absent owned private-HDD slots. Never modify user/base HDD; existing runner must retain its instance guard and disc restoration.')
 (a.out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
