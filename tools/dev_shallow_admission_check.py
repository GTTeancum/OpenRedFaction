"""Process-local shallow-region replay: two impacts, one admitted crater."""
import os, subprocess, struct
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
 data=bytearray(recording('double'));data.extend(bytes(48*100))
 data[8+310*48:8+311*48]=struct.pack('<5f7I',0,0,0,0,0,1,0,1,0,0,0,1)
 struct.pack_into('<I',data,8+330*48+32,1)
 replay=folder/'shallow-reset.bin';replay.write_bytes(data)
 result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--dev-room-replay',str(ROOT/'Installed_Game'),str(replay),str(folder/'shallow-reset.ppm')],cwd=ROOT,env=env,capture_output=True,text=True)
 log=result.stdout+result.stderr;(ROOT/'artifacts/shallow-reset.log').write_text(log);result.check_returncode()
 admission=rows('GEOMOD_ADMISSION');assert len(admission)==2 and all(r[1:3]==['1','1'] for r in admission),admission
 assert list(map(int,rows('ROCKETS')[0]))==[3,3,0,0,2,1,0,0]
 terrain=list(map(int,rows('GEOMOD')[0]));assert terrain[:3]==[1,1,4] and terrain[5:]==[0,4,3],terrain
 print('PASS:reset clears admission history and permits another shallow cut')
 data=bytearray(recording('double'))
 for i in range(190,210):struct.pack_into('<f',data,8+i*48+16,.1)
 replay=folder/'shallow-offset.bin';replay.write_bytes(data)
 snapshot=folder/'shallow-offset-mesh.bin';env['RF_REPLAY_TERRAIN_PHYSICAL_SNAPSHOT']=str(snapshot)
 result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--dev-room-replay',str(ROOT/'Installed_Game'),str(replay),str(folder/'shallow-offset.ppm')],cwd=ROOT,env=env,capture_output=True,text=True)
 log=result.stdout+result.stderr;(ROOT/'artifacts/shallow-offset.log').write_text(log);result.check_returncode()
 assert list(map(int,rows('ROCKETS')[0]))==[2,2,0,0,2,0,0,0]
 terrain=list(map(int,rows('GEOMOD')[0]));assert terrain[:3]==[1,2,3] and terrain[5:]==[0,2,2],terrain
 admission=rows('GEOMOD_ADMISSION');assert len(admission)==2 and admission[1][1:3]==['2','1'],admission
 assert float(admission[1][3])==-16,admission
 subprocess.run([str(ROOT/'build/pc/Release/rf_geomod_interior_tests.exe'),'--mesh',str(snapshot)],cwd=ROOT,check=True)
 print('PASS:two overlapping aligned shallow cuts with closed output mesh')
 env['RF_REPLAY_SHALLOW_FIXTURE']='2';snapshot=folder/'shallow-two-mesh.bin';env['RF_REPLAY_TERRAIN_PHYSICAL_SNAPSHOT']=str(snapshot)
 result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--dev-room-replay',str(ROOT/'Installed_Game'),str(replay),str(folder/'shallow-two.ppm')],cwd=ROOT,env=env,capture_output=True,text=True)
 log=result.stdout+result.stderr;(ROOT/'artifacts/shallow-two.log').write_text(log);result.check_returncode()
 assert list(map(int,rows('ROCKETS')[0]))==[2,2,0,0,2,0,0,0]
 admission=rows('GEOMOD_ADMISSION');assert len(admission)==2 and all(r[2]=='2' for r in admission),admission
 assert admission[0][3:5]==admission[1][3:5],admission
 subprocess.run([str(ROOT/'build/pc/Release/rf_geomod_interior_tests.exe'),'--mesh',str(snapshot)],cwd=ROOT,check=True)
 print('PASS:two-limit repeated cuts align to both old planes and remain closed')
 env['RF_REPLAY_SHALLOW_FIXTURE']='3';snapshot=folder/'shallow-oblique-mesh.bin';env['RF_REPLAY_TERRAIN_PHYSICAL_SNAPSHOT']=str(snapshot)
 result=subprocess.run([str(ROOT/'build/pc/Release/rf_pc_play.exe'),'--dev-room-replay',str(ROOT/'Installed_Game'),str(replay),str(folder/'shallow-oblique.ppm')],cwd=ROOT,env=env,capture_output=True,text=True)
 log=result.stdout+result.stderr;(ROOT/'artifacts/shallow-oblique.log').write_text(log);result.check_returncode()
 assert list(map(int,rows('ROCKETS')[0]))==[2,2,0,0,2,0,0,0]
 admission=rows('GEOMOD_ADMISSION');assert len(admission)==2 and all(r[2]=='2' for r in admission),admission
 assert float(admission[1][3])!=float(admission[0][3]),admission
 subprocess.run([str(ROOT/'build/pc/Release/rf_geomod_interior_tests.exe'),'--mesh',str(snapshot)],cwd=ROOT,check=True)
 print('PASS:oblique two-limit repeated cuts retain closed physical geometry')




if __name__=='__main__':main()
