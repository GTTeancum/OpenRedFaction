"""Compare level spawn images with an explicitly retained pre-change PC renderer."""
import argparse,hashlib,json,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--reference',type=Path,required=True);args=p.parse_args()
root=Path(__file__).resolve().parents[1];reference=args.reference.resolve();current=root/'build/pc/Release/rf_pc_preview.exe'
report={'reference_sha256':hashlib.sha256(reference.read_bytes()).hexdigest(),'current_sha256':hashlib.sha256(current.read_bytes()).hexdigest(),'scope':'94 untextured level spawn views; images and exit status, not full camera coverage.','rows':[]}
folder=root/'artifacts/projection-corpus';folder.mkdir(exist_ok=True)
for item in json.loads((root/'artifacts/geometry.json').read_text()):
 results=[]
 for name,exe in [('before',reference),('after',current)]:
  output=folder/(name+'.ppm');output.unlink(missing_ok=True)
  start=time.perf_counter();run=subprocess.run([str(exe),str(root/'Installed_Game'/item['archive']),item['file'],str(output)],capture_output=True)
  results.append((run.returncode,output.read_bytes() if output.exists() else None,time.perf_counter()-start))
 assert results[0][:2]==results[1][:2],item['file']
 report['rows'].append({'level':item['file'],'status':results[0][0],'image_sha256':hashlib.sha256(results[0][1]).hexdigest() if results[0][1] else None,'seconds':[r[2] for r in results]})
 if len(report['rows'])%10==0:print(len(report['rows']),'level views matched',flush=True)
assert all(row['status']==0 and row['image_sha256'] for row in report['rows'])
report['result']='PASS';(root/'artifacts/projection-corpus.json').write_text(json.dumps(report,indent=2));print('PASS',len(report['rows']),'level images exact')
