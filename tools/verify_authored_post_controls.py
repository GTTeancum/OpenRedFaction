"""Verify retained authored-post PC controls; no process launches or builds.

Missing/truncated logs, recordings or required numeric-check logs are failures.
Body comparisons use exported float bits and authored post bounds, no fitted
position tolerance. This is gameplay/numeric evidence, not visual acceptance.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[1]
UINTMAX = 0xffffffff
CASES = {
    'two-shot': (550, 2, 2, 2, 0, 2, 2),
    'walk-blocked': (715, 0, 0, 0, 0, 0, 0),
    'walk-open': (715, 2, 2, 2, 0, 2, 2),
    'reset-safe': (640, 2, 0, 3, 0, 3, 3),
    'reset-blocked': (730, 2, 2, 2, UINTMAX-2, 3, 2),
    'reset-recut': (840, 3, 1, 4, 0, 4, 4),
    'reset-restored': (805, 2, 0, 3, 0, 3, 3),
}

def require(condition, message):
    if not condition:
        raise ValueError(message)

def words(text, label, count=None):
    rows = [line.split()[1:] for line in text.splitlines() if line.startswith(label+' ')]
    require(len(rows) == 1, f'{label}: expected one row, got {len(rows)}')
    result = list(map(int, rows[0]))
    require(count is None or len(result) == count, f'{label}: wrong field count')
    return result

def recording(path):
    data = path.read_bytes()
    require(data[:8] == b'RFI6'+struct.pack('<I',48) and (len(data)-8) % 48 == 0, 'invalid RFI6')
    return data, list(struct.iter_unpack('<5f7I', data[8:]))

def check_case(folder, name, contract):
    frames, shots, cuts, serial, status, attempts, successes = contract
    path = folder/(name+'.log');text = path.read_text()
    data, inputs = recording(folder/(name+'.bin'))
    require(len(inputs) == frames, 'recording frame count mismatch')
    completed = re.findall(r'^Completed (\d+) frames,', text, re.M)
    require(completed == [str(frames)], f'incomplete/duplicate runtime completion: {completed}')
    require(not re.search(r'failed \(-\d+\)|^NOISE_FAILURE |^GEOMOD_CHECKPOINT_ERROR ', text, re.M), 'runtime failure marker')
    fires = [i for i,r in enumerate(inputs) if r[8] and (not i or not inputs[i-1][8])]
    require(len(fires) == shots, 'fire input count differs from scenario')
    reset_edges = [i for i,r in enumerate(inputs) if r[5] and r[7] and r[11] and
                   (not i or not (inputs[i-1][7] and inputs[i-1][11]))]
    require(len(reset_edges) == int(name.startswith('reset-')), 'reset input edge count mismatch')
    geomod = words(text,'GEOMOD',8);publication = words(text,'TERRAIN_PUBLICATION',8)
    require(geomod[:3] == [1,cuts,serial], f'cut/serial mismatch: {geomod[:3]}')
    require(geomod[5:] == [status,attempts,successes], f'edit result/counters: {geomod[5:]}')
    require(geomod[4] <= 12*1024*1024, 'destruction reservation exceeds source ceiling12MiB')
    require(publication[2:4] == [cuts,serial] and publication[6:] == [0,0], 'publication not committed')
    require((publication[0] == publication[1] == 0) if not cuts else min(publication[:2]) > 0,
            'empty/generated publication inconsistent with cuts')
    if cuts == 2:
        require(publication[:2] == [12,49], 'verified two-cut geometry count changed')
    require(words(text,'ROCKETS',8) == [shots,shots,0,0,shots,0,0,0], 'rocket lifecycle mismatch')
    ammo = words(text,'PLAYER_AMMO',8)
    require(ammo[:3] == [7,18,6-shots] and ammo[-1] == 0, 'launcher supply/consumption mismatch')
    require(words(text,'WEAPON_SELECTION',8)[0] == 4, 'launcher not selected')
    vitals = words(text,'PICKUP_VITALS',4)
    require(vitals[:2] == [0x42c80000,0x42c80000], 'health/armor not exactly100')
    require(words(text,'PLAYER_LIFE',8)[0] == 0, 'player died')
    require(words(text,'ROCKET_BLAST',8)[4] == 0, 'self blast damage occurred')
    body = words(text,'PC_PLAY_BODY')
    require(len(body) >= 28, 'body position missing')
    position = struct.unpack('<3f',struct.pack('<3I',*body[22:25]))
    require(all(math.isfinite(v) for v in position), 'nonfinite body position')
    summary = [line.split()[1:] for line in text.splitlines() if line.startswith('CAMPAIGN_FINAL_POSITION ')]
    require(summary == [[format(v,'.6f') for v in position]], 'position summary differs from actual body words')
    require(position[2] == 2.5, 'body left intended corridorZ2.5')
    if name == 'walk-open':
        require(position[0] < -5.25, 'body did not cross the authored post backplane')
    elif name == 'reset-blocked':
        require(-5.25 <= position[0] <= -4.75, 'reset rejection fixture not inside original postX slab')
    else:
        require(position[0] > -4.75, 'safe/control body crossed intact post face')
    resets = [list(map(int,line.split()[1:])) for line in text.splitlines() if line.startswith('AUTHORED_RESET ')]
    if name == 'reset-blocked':
        require(len(resets) == 1 and resets[0][0] == -3 and resets[0][1] < 8 and resets[0][2] in (1,2) and
                resets[0][3:] == [2,2], f'inside reset was not geometry-rejected: {resets}')
    elif name.startswith('reset-'):
        require(resets == [[0,UINTMAX,0,0,3]], f'safe reset did not restore original publication: {resets}')
    else:
        require(not resets,'unexpected reset event')
    return dict(result='PASS',frames=frames,shots=fires,reset_edges=reset_edges,position=position,
                position_words=body[22:25],health=100,armor=100,ammo=ammo[:3],geomod=geomod,
                publication=publication,reset_events=resets,log_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                recording_sha256=hashlib.sha256(data).hexdigest())

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--folder',type=Path,default=ROOT/'artifacts/authored-post-live')
    ap.add_argument('--output',type=Path)
    args = ap.parse_args();results = {};errors = []
    for name,contract in CASES.items():
        try:
            results[name] = check_case(args.folder,name,contract)
        except (OSError,ValueError,struct.error,IndexError) as error:
            results[name] = dict(result='FAIL',error=str(error));errors.append(f'{name}: {error}')
    try:
        intact,opened = (recording(args.folder/(n+'.bin'))[1] for n in ('walk-blocked','walk-open'))
        require(len(intact) == len(opened),'walk recordings differ in length')
        require(all(a[:8]+a[9:] == b[:8]+b[9:] for a,b in zip(intact,opened)),
                'walk controls differ beyond fire inputs')
        restored = recording(args.folder/'reset-restored.bin')[1]
        require(restored[640:] == opened[550:], 'restored-post walking inputs differ from open-post control')
        for suffix in ('.rgch','.rgp'):
            baseline=(args.folder/('two-shot'+suffix)).read_bytes()
            rejected=(args.folder/('reset-blocked'+suffix)).read_bytes()
            require(baseline == rejected, 'rejected reset changed exported '+suffix+' bytes')
        for name,cuts,faces,vertices in [('one-shot-late',1,11,43),('two-shot',2,12,49)]:
            text=(args.folder/(name+'-checker.log')).read_text()
            require('LIVE_PUBLICATION geometry_uv_provenance_exact1 material_slots_compared0' in text,
                    f'{name}: no live-array equality evidence')
            require(f'PASS cuts{cuts} publication{faces}/{vertices} retained786 liquids82 floor_patches6 composed{786+faces}' in text,
                    f'{name}: numerical preservation check missing')
        results['cross_case'] = dict(result='PASS',walk_controls='identical except fire',
                                     unchanged_base_faces=786,liquids=82,live_publication_arrays='exact')
    except (OSError,ValueError,struct.error) as error:
        results['cross_case'] = dict(result='FAIL',error=str(error));errors.append(f'cross_case: {error}')
    report=dict(result='FAIL' if errors else 'PASS',cases=results,errors=errors,
                scope='Retained PC process-local gameplay logs and exact-history/live-publication checker evidence. No launches, visual acceptance or native verification performed by this tool.')
    output=args.output or args.folder/'controls-verification.json'
    output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(report,indent=2)+'\n')
    print(report['result'],output)
    for error in errors:print(error)
    return bool(errors)

if __name__ == '__main__':
    raise SystemExit(main())
