"""Compare level spawn images with an explicitly retained pre-change PC renderer."""
import argparse,hashlib,json,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--reference',type=Path,required=True);p.add_argument('--report-differences',action='store_true');args=p.parse_args()
root=Path(__file__).resolve().parents[1];reference=args.reference.resolve();current=root/'build/pc/Release/rf_pc_preview.exe'
report={'reference_sha256':hashlib.sha256(reference.read_bytes()).hexdigest(),'current_sha256':hashlib.sha256(current.read_bytes()).hexdigest(),'scope':'94 untextured level spawn views; images and exit status, not full camera coverage.','rows':[]}
folder=root/'artifacts/projection-corpus';folder.mkdir(exist_ok=True)
for item in json.loads((root/'artifacts/geometry.json').read_text()):
 results=[]
 for name,exe in [('before',reference),('after',current)]:
  output=folder/(name+'.ppm');output.unlink(missing_ok=True)
  start=time.perf_counter();run=subprocess.run([str(exe),str(root/'Installed_Game'/item['archive']),item['file'],str(output)],capture_output=True)
  results.append((run.returncode,output.read_bytes() if output.exists() else None,time.perf_counter()-start))
 if not args.report_differences:assert results[0][:2]==results[1][:2],item['file']
 else:
  from PIL import Image
  import io
  assert results[0][0]==results[1][0]==0 and results[0][1] and results[1][1],item['file']
  a=Image.open(io.BytesIO(results[0][1])).convert('RGB');b=Image.open(io.BytesIO(results[1][1])).convert('RGB')
  assert a.size==b.size
  pixels_a,pixels_b=a.tobytes(),b.tobytes()
  changed=sum(pixels_a[i:i+3]!=pixels_b[i:i+3] for i in range(0,len(pixels_a),3))
 report['rows'].append({'level':item['file'],'status':results[0][0],'image_sha256':hashlib.sha256(results[0][1]).hexdigest() if results[0][1] else None,'seconds':[r[2] for r in results]})
 if args.report_differences:
  report['rows'][-1].update(changed_pixels=changed,after_sha256=hashlib.sha256(results[1][1]).hexdigest())
  if changed:
   for label,result in zip(('before','after'),results):(folder/(item['file']+'-'+label+'.ppm')).write_bytes(result[1])
 if len(report['rows'])%10==0:print(len(report['rows']),'level views compared',flush=True)
assert all(row['status']==0 and row['image_sha256'] for row in report['rows'])
different=sum(r.get('changed_pixels',0)>0 for r in report['rows'])
report['result']='DIFFERENCES' if different else 'PASS'
report['scope']+=' Difference-report mode measures changes without accepting them as visual parity.' if args.report_differences else ''
(root/('artifacts/projection-corpus-differences.json' if args.report_differences else 'artifacts/projection-corpus.json')).write_text(json.dumps(report,indent=2))
print(report['result'],len(report['rows']),'level images;',different,'differing views')
