"""Verify authored impact sample selection and captured mixer timing, not speakers."""
import json,os,struct,subprocess,wave
from pathlib import Path
from dev_destruction_check import ROOT,recording

def main():
    out=ROOT/'artifacts/impact-audio';out.mkdir(parents=True,exist_ok=True)
    inp=out/'input.bin';inp.write_bytes(recording('approach')+b''.join(struct.pack('<5f7I',0,0,0,0,0,0,0,0,int(i==520),0,0,0) for i in range(500,800)))
    trace=out/'mixer.trace'
    env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
    env.update(RF_REPLAY_AUDIO_TRACE=str(trace),RF_REPLAY_TRACE='1',RF_REPLAY_TRACE_FROM='0')
    run=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--dev-room-replay',str(ROOT/'Installed_Game'),str(inp),str(out/'final.ppm')],cwd=ROOT,env=env,capture_output=True,text=True)
    log=run.stdout+run.stderr;(out/'run.log').write_text(log);run.check_returncode()
    def rows(label):return [l.split()[1:] for l in log.splitlines() if l.startswith(label+' ')]
    sounds=rows('IMPACT_SOUND');hits=rows('ROCKET_IMPACT');telemetry=list(map(int,rows('IMPACT_AUDIO')[0]))
    assert telemetry[:3]==[3,3,3] and telemetry[7]==0,telemetry
    assert len(sounds)==len(hits)==3
    assert all(s[0]==h[0] and s[2]=='0' and s[3:]==h[2:] for s,h in zip(sounds,hits))
    block=-1;seen=set();starts=[];last=[]
    for line in trace.read_text().splitlines():
        if line.startswith('B '):block+=1;last=[]
        elif line.startswith('V '):
            v=line.split();last.append(v);key=(v[1],v[-1])
            if v[-1].startswith('Boom_Md') and key not in seen:
                seen.add(key);starts.append(dict(frame=block,sample=v[-1],voice=int(v[1])))
    assert [v['frame'] for v in starts]==[int(v[0]) for v in sounds],starts
    assert {v['sample'] for v in starts}=={'Boom_Md01.wav','Boom_Md02.wav'}
    assert not any(v[-1].startswith('Boom_Md') for v in last),last
    pcm=Path(str(trace)+'.pcm').read_bytes();assert len(pcm)==799*800*4
    energy=[]
    for start in starts:
        samples=struct.unpack('<1600h',pcm[start['frame']*3200:(start['frame']+1)*3200]);energy.append(sum(abs(s) for s in samples))
    assert all(energy),energy
    with wave.open(str(out/'mixed.wav'),'wb') as f:f.setnchannels(2);f.setsampwidth(2);f.setframerate(48000);f.writeframes(pcm)
    report=dict(result='PASS',telemetry=telemetry,starts=starts,impact_block_absolute_energy=energy,pcm_bytes=len(pcm),scope=__doc__+' Mixed PCM is captured; listening, exact original attenuation and device output remain unverified.')
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
