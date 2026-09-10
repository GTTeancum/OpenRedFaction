"""Independent PCM reference for the staged 180-frame lower-door replay.

Uses original-verified controller arrival timing and Python's WAV decoder;
does not call the C mixer to form expected audio. Unity/nonspatial adapter only.
Run verify_authored_door_motion.py and replay_door_contact.py first.
"""
import io,json,struct,wave
from pathlib import Path

root=Path(__file__).resolve().parents[1]
report=json.loads((root/'artifacts/door-contact/report.json').read_text())
assert report['frames']==180 and not report['cycle'] and not report['idle']
audio=report['live_audio'];assert audio[4:7]==[2,2,179*800],audio
motion=json.loads((root/'artifacts/authored-door-motion.json').read_text())
trace=next(r['trace'] for r in motion['results'] if r['uid']==8591)
# Original current-key changes to one when arrival finishes, after endpoint snap.
arrival=next(t['frame'] for t in trace if t['current']==1)
inventory=json.loads((root/'artifacts/inventory.json').read_text())
entries=next(a['vpp']['entries'] for a in inventory['files'] if a['path']=='audio.vpp')
tracks=[]
with (root/'Installed_Game/audio.vpp').open('rb') as source:
    for name,start in [('DoorOpen_07.wav',0),('DoorEnd_07.wav',(arrival-1)*800)]:
        entry=next(e for e in entries if e['name']==name)
        source.seek(entry['offset']);raw=source.read(entry['size'])
        with wave.open(io.BytesIO(raw)) as wav:
            assert wav.getnchannels()==1 and wav.getsampwidth()==2
            rate=wav.getframerate();count=wav.getnframes()
            samples=struct.unpack('<'+'h'*count,wav.readframes(count))
        tracks.append((start,rate,samples))
hash_=2166136261;nonzero=0
for frame in range(audio[6]):
    total=0
    for start,rate,samples in tracks:
        if frame<start:continue
        index,phase=divmod((frame-start)*rate,48000)
        if index>=len(samples):continue
        numerator=samples[index]*(48000-phase)+samples[min(index+1,len(samples)-1)]*phase
        total+=-(abs(numerator)//48000) if numerator<0 else numerator//48000
    value=max(-32768,min(32767,total));nonzero+=value!=0
    for byte in struct.pack('<2h',value,value):hash_=((hash_^byte)*16777619)&0xffffffff
assert hash_==audio[7],(hash_,audio,arrival)
result=dict(result='PASS',arrival_tick=arrival,stereo_frames=audio[6],nonzero_frames=nonzero,pcm_hash=hash_,
    scope='Independent full PCM stream for staged opening/arrival, unity nonspatial adapter. No device playback, attenuation or original Miles output equivalence.')
(root/'artifacts/live-door-audio-verification.json').write_text(json.dumps(result,indent=2)+'\n')
print(result)
