"""Exercise generated RFDS negative fixtures in fresh headless PC processes.
Optional audit verifies repeated validation preserves recorded published state.
No build, original executable, desktop input, or visual acceptance claim.
"""
import argparse
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import time
ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, default=ROOT/'artifacts/geomod-rfds-validation/manifest.json')
    parser.add_argument('--binary', type=Path, default=ROOT/'build/pc/Release/rf_pc_play.exe')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--timeout', type=float, default=120)
    parser.add_argument('--audit-validation', action='store_true', help='Require two pure-validation calls with unchanged state for every candidate')
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text())
    source = Path(manifest['source'])
    if hashlib.sha256(source.read_bytes()).hexdigest() != manifest['source_sha256']:
        raise ValueError('Fixture source changed; regenerate manifest')
    folder = args.output or ROOT/'artifacts/geomod-rfds-validation-runs'/datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    folder.mkdir(parents=True, exist_ok=False)
    binary = args.binary.resolve()
    binary_hash = hashlib.sha256(binary.read_bytes()).hexdigest()
    inputs = folder/'no-input.bin'
    inputs.write_bytes(b'RFI6'+struct.pack('<I',48)+bytes(8*48))
    env_base = {k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env_base.pop('RF_DEV_GEOMOD_CHECKPOINT_VALIDATE_AUDIT', None)
    if args.audit_validation:
        env_base['RF_DEV_GEOMOD_CHECKPOINT_VALIDATE_AUDIT'] = '1'
    results = []
    cases = [dict(name='valid-control', expected='accept', source_stage='control', path=None,
                  candidate_sha256=manifest['source_sha256'])] + manifest['cases']
    for case in cases:
        case_dir = folder/case['name']; case_dir.mkdir()
        incoming = source if case['path'] is None else ROOT/Path(case['path'])/'candidate.rfds'
        digest = hashlib.sha256(incoming.read_bytes()).hexdigest()
        if case['expected']=='reject' and digest==manifest['source_sha256']:
            raise ValueError('Negative fixture is unchanged: '+case['name'])
        if digest != case['candidate_sha256']:
            raise ValueError('Candidate changed: '+case['name'])
        output = case_dir/'output.rfds'
        env = dict(env_base, RF_REPLAY_GEOMOD_CHECKPOINT_IN=str(incoming.resolve()),
                   RF_REPLAY_GEOMOD_CHECKPOINT_OUT=str(output.resolve()))
        command = [str(binary), '--dev-room-replay', str(ROOT/'Installed_Game'),
                   str(inputs.resolve()), str((case_dir/'frame.ppm').resolve())]
        start = time.monotonic()
        try:
            run = subprocess.run(command, cwd=ROOT, env=env, capture_output=True,
                                 text=True, timeout=args.timeout)
            log = run.stdout+run.stderr
            accepted = run.returncode == 0 and output.exists() and 'GEOMOD_CHECKPOINT_LOAD ' in log
            rejected = run.returncode != 0 and not output.exists() and 'GEOMOD_CHECKPOINT_ERROR load ' in log
            row = dict(name=case['name'], expected=case['expected'], source_stage=case['source_stage'],
                       returncode=run.returncode, accepted=accepted, rejected=rejected,
                       output_exists=output.exists(),
                       checkpoint_log=[line for line in log.splitlines() if 'GEOMOD_CHECKPOINT_' in line],
                       candidate_sha256=digest, candidate_changed=digest!=manifest['source_sha256'], elapsed_seconds=round(time.monotonic()-start,3))
            row['pass'] = accepted if case['expected']=='accept' else rejected if case['expected']=='reject' else accepted or rejected
            if args.audit_validation:
                audit = [line.split() for line in log.splitlines() if line.startswith('GEOMOD_CHECKPOINT_VALIDATE_AUDIT ')]
                valid_audit = len(audit)==1 and len(audit[0])==4 and audit[0][1]=='PASS'
                if valid_audit:
                    audit_status = int(audit[0][2])
                    valid_audit = int(audit[0][3])==len(incoming.read_bytes()) and ((audit_status==0) if accepted else (audit_status!=0))
                row['validation_audit_pass'] = valid_audit
                row['pass'] = row['pass'] and valid_audit
            if accepted:
                row['output_sha256']=hashlib.sha256(output.read_bytes()).hexdigest()
                row['exact_payload']=output.read_bytes()==incoming.read_bytes()
                if case['name']=='valid-control':row['pass']=row['pass'] and row['exact_payload']
            (case_dir/'run.log').write_text(log)
        except subprocess.TimeoutExpired as error:
            row = dict(name=case['name'], expected=case['expected'], source_stage=case['source_stage'],
                       pass_=False, timeout=True, elapsed_seconds=round(time.monotonic()-start,3))
            row['pass']=False
            (case_dir/'run.log').write_bytes((error.stdout or b'')+(error.stderr or b''))
        results.append(row)
        report = dict(binary=str(binary), binary_sha256=binary_hash,
                      manifest=str(args.manifest.resolve()), source_sha256=manifest['source_sha256'],
                      cases=results, validation_audit=args.audit_validation,
                      scope=('Restore outcomes plus repeated-validation status and published-state fingerprints; terrain scratch/peak excluded. '
                             if args.audit_validation else 'Fresh-process PC restore outcomes only; no validation purity claim. ')
                            + 'No visual acceptance or full game-state rollback claim.')
        (folder/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        print(('PASS' if row['pass'] else 'FAIL'),case['name'],row.get('checkpoint_log',[]),flush=True)
        if case['name']=='valid-control' and not row['pass']:
            raise RuntimeError('Valid source failed; remaining rejection evidence would be ambiguous')
    if hashlib.sha256(binary.read_bytes()).hexdigest()!=binary_hash:
        raise RuntimeError('PC binary changed during run; discard mixed-build evidence')
    failures=[r['name'] for r in results if not r['pass']]
    print('Report:',folder/'report.json',flush=True)
    if failures:raise SystemExit('Failed cases: '+', '.join(failures))
if __name__=='__main__':main()
