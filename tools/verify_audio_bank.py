"""Owned archive-backed PCM remains valid after archive closure."""
import io,json,re,struct,subprocess,wave
from pathlib import Path
root=Path(__file__).resolve().parents[1]
output=subprocess.check_output([str(root/'build/pc/Release/rf_audio_probe.exe'),'--bank',str(root/'Installed_Game/audio.vpp')],text=True)
match=re.fullmatch(r'PASS audio bank bytes=(\d+) pcm_after_archive_close_hash=(\d+)\s*',output);assert match,output
inv=json.loads((root/'artifacts/inventory.json').read_text());archive=next(a for a in inv['files'] if a['path']=='audio.vpp');entry=next(e for e in archive['vpp']['entries'] if e['name']=='DoorOpen_07.wav')
with (root/'Installed_Game/audio.vpp').open('rb') as f:f.seek(entry['offset']);raw=f.read(entry['size'])
with wave.open(io.BytesIO(raw)) as f:rate=f.getframerate();frames=f.getnframes();pcm=f.readframes(frames)
samples=struct.unpack('<'+'h'*frames,pcm);expected=bytearray()
for i in range(256):
 index,phase=divmod(i*rate,48000);n=samples[index]*(48000-phase)+samples[index+1]*phase
 value=-(abs(n)//48000) if n<0 else n//48000;expected.extend(struct.pack('<2h',value,value))
hash_=2166136261
for byte in expected:hash_=((hash_^byte)*16777619)&0xffffffff
assert int(match[1])==93402 and int(match[2])==hash_
report=dict(result='PASS',retained_bytes=93402,post_close_pcm_hash=hash_,scope='PC VPP bank: exact/one-byte-short budgets, case-insensitive deduplication, missing file and output preservation, playback data after archive close, invalid sample index, repeat bank close. First256 stereo frames match independent Python wave/integer interpolation. NXDK build only for ownership; no live device output or sample eviction.')
(root/'artifacts/audio-bank-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
