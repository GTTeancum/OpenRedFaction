"""Independent full PCM reference for the current staged door plus ambient mix.
Scene start/stop/gain decisions are observed inputs; PCM decoding, cursor progress,
resampling, gain arithmetic and summation use Python, not the C mixer.
"""
import hashlib,io,json,os,struct,subprocess,wave
from pathlib import Path
root=Path(__file__).resolve().parents[1]
folder=root/'artifacts/door-audio-reference';folder.mkdir(exist_ok=True)
trace=folder/'voices.txt';inputs=folder/'inputs.bin'
inputs.write_bytes(struct.pack('<5fI',0,0,1,0,0,0)*180)
env=dict(os.environ)
for key in ('RF_REPLAY_LEVEL','RF_REPLAY_ARCHIVE','RF_REPLAY_REGION_START','RF_REPLAY_LIFT_START','RF_REPLAY_FORCE_UID'):
 env.pop(key,None)
env.update(RF_REPLAY_DOOR_START='1',RF_REPLAY_AUDIO_TRACE=str(trace))
exe=root/'build/pc/Release/rf_pc_play.exe'
run=subprocess.run([str(exe),'--spawn-replay',str(root/'Installed_Game'),str(inputs),str(folder/'final.ppm')],cwd=root,env=env,capture_output=True,text=True,check=True)
(folder/'pc.txt').write_text(run.stdout+run.stderr)
audio=list(map(int,next(l for l in run.stdout.splitlines() if l.startswith('LIVE_AUDIO ')).split()[1:]))
actual=Path(str(trace)+'.pcm').read_bytes();assert len(actual)==audio[6]*4
inventory=json.loads((root/'artifacts/inventory.json').read_text())
entries={e['name'].lower():e for a in inventory['files'] if a['path']=='audio.vpp' for e in a['vpp']['entries']}
cache={}
def sample(name):
 key=name.lower()
 if key not in cache:
  entry=entries[key]
  with (root/'Installed_Game/audio.vpp').open('rb') as f:f.seek(entry['offset']);data=f.read(entry['size'])
  with wave.open(io.BytesIO(data)) as f:
   channels,width,rate,count=f.getnchannels(),f.getsampwidth(),f.getframerate(),f.getnframes()
   assert channels in (1,2) and width in (1,2) and count>0
   raw=f.readframes(count)
  decoded=tuple((v-128)*256 for v in raw) if width==1 else struct.unpack('<'+'h'*(len(raw)//2),raw)
  cache[key]=(rate,count,channels,decoded)
 return cache[key]
def trunc(n,d):return n//d if n>=0 else -((-n)//d)
lines=iter(trace.read_text().splitlines());history={};starts=[];gain_changes=0;offset=blocks=0;complete=bytearray()
for line in lines:
 tag,amount=line.split();assert tag=='B';amount=int(amount);assert amount==800
 voices=[];seen=set()
 for row in lines:
  if row=='E':break
  fields=row.split(' ',7);assert len(fields)==8 and fields[0]=='V',row
  handle,frame,phase,left,right,loop=map(int,fields[1:7]);name=fields[7]
  assert handle not in seen and loop in (0,1) and 0<=left<=32768 and 0<=right<=32768;seen.add(handle)
  rate,count,channels,decoded=sample(name)
  if handle not in history:
   assert frame==phase==0,(handle,frame,phase)
   history[handle]=dict(time=0,name=name,loop=loop,left=left,right=right)
   starts.append(dict(handle=handle,name=name,output_frame=offset//4,loop=loop))
  h=history[handle];assert h['name']==name and h['loop']==loop
  want_frame,want_phase=divmod(h['time'],48000)
  if loop:want_frame%=count
  assert (frame,phase)==(want_frame,want_phase),(blocks,handle,frame,phase,want_frame,want_phase)
  gain_changes+=(left,right)!=(h['left'],h['right']);h.update(left=left,right=right)
  voices.append((h,rate,count,channels,decoded))
 else:raise AssertionError('Unterminated block')
 expected=bytearray()
 for output_frame in range(amount):
  total=[0,0]
  for h,rate,count,channels,decoded in voices:
   frame,phase=divmod(h['time'],48000)
   if h['loop']:frame%=count
   elif frame>=count:continue
   following=(frame+1)%count if h['loop'] else min(frame+1,count-1)
   for channel,gain in enumerate((h['left'],h['right'])):
    c=channel if channels==2 else 0
    interpolated=trunc(decoded[frame*channels+c]*(48000-phase)+decoded[following*channels+c]*phase,48000)
    total[channel]+=trunc(interpolated*gain,32768)
   h['time']+=rate
  expected+=struct.pack('<hh',*(max(-32768,min(32767,v)) for v in total))
 assert actual[offset:offset+len(expected)]==expected,('PCM mismatch',blocks,offset)
 complete+=expected;offset+=len(expected);blocks+=1
assert offset==len(actual)==179*800*4
assert len(starts)==audio[4] and any(s['loop'] for s in starts),starts
# Keep an independent original-verified controller timing check, even though
# ambient scheduling and changing gains are supplied by the scene observation.
motion=json.loads((root/'artifacts/authored-door-motion.json').read_text())
original=next(r['trace'] for r in motion['results'] if r['uid']==8591)
arrival=next(t['frame'] for t in original if t['current']==1)
# The current contact replay mixes one ambient-only block before controller start.
for name,at in [('DoorOpen_07.wav',800),('DoorEnd_07.wav',arrival*800)]:
 assert [(s['name'],s['output_frame']) for s in starts if s['name'].lower()==name.lower()]==[(name,at)],starts
hash_=2166136261
for value in complete:hash_=((hash_^value)*16777619)&0xffffffff
assert hash_==audio[7]
# The observer must not change scene behavior or the mixed stream.
plain_env=dict(env);plain_env.pop('RF_REPLAY_AUDIO_TRACE')
plain=subprocess.run([str(exe),'--spawn-replay',str(root/'Installed_Game'),str(inputs),str(folder/'plain.ppm')],cwd=root,env=plain_env,capture_output=True,text=True,check=True)
for label in ('LIVE_AUDIO ', 'PC_PLAY_BODY ', 'LIVE_MOTION ', 'AMBIENT_AUDIO '):
 assert next(l for l in run.stdout.splitlines() if l.startswith(label))==next(l for l in plain.stdout.splitlines() if l.startswith(label)),label
report=dict(result='PASS',blocks=blocks,stereo_frames=offset//4,bytes=offset,starts=starts,gain_changes=gain_changes,
 pcm_hash=hash_,pcm_sha256=hashlib.sha256(complete).hexdigest(),pc_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),
 scope='Every captured PCM byte in current180-frame door-plus-ambient replay compared with independent Python WAV decoding and resampling/mixing. Cursor continuity independently propagated; original-verified relative door travel timing retained. Ambient scheduling and Q15 gain changes are observed inputs, not independent gameplay fidelity or native device output proof.')
(root/'artifacts/live-door-audio-verification.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
