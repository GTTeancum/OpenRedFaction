"""Process-local shallow-region replay: two impacts, one admitted crater."""
import os, subprocess
from pathlib import Path
from dev_destruction_check import ROOT,recording

def main():
 folder=ROOT/'artifacts/destruction';folder.mkdir(parents=True,exist_ok=True)
 replay=folder/'shallow-double.bin';replay.write_bytes(recording('double'))
 env={k:v for k,v in os.environ.items() if not k.startswith('RF_REPLAY_')}
 env.update(RF_REPLAY_SHALLOW_FIXTURE='1',RF_REPLAY_TRACE='1',RF_REPLAY_TRACE_FROM='0')
 result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--dev-room-replay',str(ROOT/'Installed_Game'),str(replay),str(folder/'shallow-double.ppm')],cwd=ROOT,env=env,capture_output=True,text=True)
 log=result.stdout+result.stderr;(ROOT/'artifacts/shallow-double.log').write_text(log);result.check_returncode()
 rows=lambda name:[line.split()[1:] for line in log.splitlines() if line.startswith(name+' ')]
 admission=rows('GEOMOD_ADMISSION');assert len(admission)==1 and admission[0][1:3]==['1','1'],admission
 assert list(map(int,rows('ROCKETS')[0]))==[2,2,0,0,1,1,0,0]
 terrain=list(map(int,rows('GEOMOD')[0]));assert terrain[:3]==[1,1,2] and terrain[6:]==[2,1],terrain
 assert len(rows('ROCKET_IMPACT'))==2
 print('PASS:2 live shallow impacts,1 admitted/deformed crater,1 duplicate rejection')
if __name__=='__main__':main()
