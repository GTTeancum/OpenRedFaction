"""Ordinary process-local rockets against the DEV room; no host input."""
import argparse, hashlib, json, os, struct, subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--shots', type=int, default=16)
    parser.add_argument('--pattern', choices=('fixed','wall-sweep'), default='fixed', help='Fixed crater or three small yaw steps between blast groups')
    parser.add_argument('--expected-cuts', type=int, default=16)
    parser.add_argument('--checkpoint-in', type=Path)
    parser.add_argument('--no-fire', action='store_true')
    parser.add_argument('--compare-to', type=Path, help='Require exact checkpoint, mesh and retained atlas equality')
    parser.add_argument('--build-dir', type=Path, default=ROOT/'build/pc-expanded')
    parser.add_argument('--closure-probe', type=Path, default=ROOT/'build/pc/Release/rf_geomod_capacity_stress_probe.exe', help='Independent physical mesh closure checker; must cover the captured mesh capacity')
    parser.add_argument('--output-dir', type=Path, default=ROOT/'artifacts/geomod-expanded-live')
    args = parser.parse_args()
    if not 1 <= args.shots <= 20: parser.error('shots must be1..20')
    folder=args.output_dir.resolve();folder.mkdir(parents=True,exist_ok=True)
    exe=args.build_dir.resolve()/'Release/rf_pc_play.exe'
    shots={110*(i+1) for i in range(args.shots)};frames=max(shots)+300
    def yaw(frame):
        if frame<90:return .7
        if args.pattern=='wall-sweep' and any(start<=frame<start+30 for start in (360,800,1240)):
            return .3
        return 0
    data=b'RFI6'+struct.pack('<I',48)+b''.join(struct.pack('<5f7I',0,0,0,0,yaw(i),
        0,0,0,int(i in shots and not args.no_fire),0,int(i in (10,20,30,40)),0) for i in range(frames))
    (folder/'input.bin').write_bytes(data)
    state=folder/'state.rfds';state.unlink(missing_ok=True)
    env={k:v for k,v in os.environ.items() if not k.startswith(('RF_REPLAY_','RF_DEV_'))}
    env.update(RF_REPLAY_TRACE='1',RF_REPLAY_TRACE_FROM='0',RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(state),RF_REPLAY_TERRAIN_PHYSICAL_SNAPSHOT=str(folder/'physical.mesh'),
        RF_REPLAY_TERRAIN_BASE_AUDIT=str(folder/'atlas.csv'))
    if args.checkpoint_in: env['RF_REPLAY_GEOMOD_CHECKPOINT_IN']=str(args.checkpoint_in.resolve())
    report={'pattern':args.pattern,'expected_cuts':args.expected_cuts,'no_fire':args.no_fire,'result':'FAIL','shots':args.shots,'frames':frames,'binary_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),
        'scope':'PC live rocket/geometry/save capacity; frame requires separate inspection; no Xbox acceptance'}
    try:
        with (folder/'run.log').open('wb') as log:
            r=subprocess.run([str(exe),'--dev-room-replay',str(ROOT/'Installed_Game'),str(folder/'input.bin'),str(folder/'frame.ppm')],
                cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=300)
        report['returncode']=r.returncode
        report['telemetry']=[line for line in (folder/'run.log').read_text().splitlines() if line.startswith(
            ('GEOMOD ','ROCKETS ','TERRAIN_','GEOMOD_ADMISSION ','GEOMOD_REJECT ','GEOMOD_CHECKPOINT_','NOISE_FAILURE'))]
        assert r.returncode==0 and state.exists(),'Replay failed; inspect run.log'
        blob=state.read_bytes();assert blob[:4]==b'RFDS' and struct.unpack_from('<I',blob,4)[0]==1
        report.update(cuts=struct.unpack_from('<I',blob,300)[0],admissions=struct.unpack_from('<I',blob,240)[0],save_bytes=len(blob))
        assert report['cuts']==args.expected_cuts, f"Only {report['cuts']} of {args.expected_cuts} expected cuts committed"
        closure=subprocess.run([str(args.closure_probe.resolve()), '--mesh', str(folder/'physical.mesh')],
            cwd=ROOT, capture_output=True, text=True, timeout=120)
        (folder/'closure.log').write_text(closure.stdout+closure.stderr)
        report['closure']={'returncode':closure.returncode,'probe_sha256':hashlib.sha256(args.closure_probe.read_bytes()).hexdigest()}
        assert closure.returncode==0,'Physical mesh closure failed; inspect closure.log'
        if args.compare_to:
            prior=args.compare_to.resolve()
            report['exact']={name:(folder/name).read_bytes()==(prior/name).read_bytes() for name in ('state.rfds','physical.mesh','atlas.csv')}
            assert all(report['exact'].values()),'Restored state differs: '+str(report['exact'])
        report['result']='PASS'
    finally:
        (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report,indent=2))
if __name__=='__main__':main()
