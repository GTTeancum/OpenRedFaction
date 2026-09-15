"""Offline support-matched edge splitting; never publishes gameplay geometry."""
import argparse
import csv
import json
import re
import struct
import subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv',type=Path)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args();args.out.mkdir(parents=True,exist_ok=True)
    faces={};groups={}
    for row in csv.DictReader(args.csv.open()):
        p=struct.unpack('<3f',struct.pack('<3f',*[float(row[k]) for k in ('x','y','z')]))
        f,c=int(row['face']),int(row['corner'])
        support,edge=int(row['face_support']),int(row['edge_support'])
        faces.setdefault(f,{})[c]=(p,tuple(sorted((support,edge))))
    for corners in faces.values():
        for c,(p,pair) in corners.items():
            if 65535 in pair:continue
            group=groups.setdefault(pair,set());group.add(p);group.add(corners[(c+1)%len(corners)][0])
    original=[];expanded=[];ambiguous=[]
    for f,corners in sorted(faces.items()):
        original.append([corners[c][0] for c in range(len(corners))]);polygon=[]
        for c,(a,pair) in sorted(corners.items()):
            b=corners[(c+1)%len(corners)][0];d=[b[k]-a[k] for k in range(3)]
            axis=max(range(3),key=lambda k:abs(d[k]));polygon.append(a)
            if d[axis]==0:raise ValueError('Degenerate source edge')
            selected=[]
            for p in groups.get(pair,()):
                t=(p[axis]-a[axis])/d[axis]
                if 0<t<1:selected.append((t,p))
            selected.sort()
            if any(selected[i][0]==selected[i-1][0] and selected[i][1]!=selected[i-1][1] for i in range(1,len(selected))):
                ambiguous.append([f,c]);continue
            polygon.extend(p for t,p in selected)
        expanded.append(polygon)
    results={}
    variants=[('original',original),('candidate',expanded)]
    for name,mesh in variants:
        vertices=[];records=[]
        for p in mesh:
            records.append((len(vertices),len(p),0,0xffffffff));vertices.extend(p)
        data=b'RGM1'+struct.pack('<2I',len(vertices),len(records))
        data+=b''.join(struct.pack('<5f',*p,0,0) for p in vertices)
        data+=b''.join(struct.pack('<4I',*r) for r in records)
        path=args.out/(name+'.bin');path.write_bytes(data)
        run=subprocess.run([str(ROOT/'build/pc/Release/rf_geomod_mesh_probe.exe'),str(path)],capture_output=True,text=True,cwd=ROOT)
        (args.out/(name+'.log')).write_text(run.stdout+run.stderr)
        results[name]=dict(vertices=len(vertices),faces=len(records),exit=run.returncode,validation=run.stdout)
        if name=='candidate' and run.returncode==1:
            invalid={int(f) for f in re.findall(r'^INVALID_FACE (\d+) ',run.stdout,re.M)}
            triangulated=[]
            for i,polygon in enumerate(expanded):
                if i not in invalid:
                    triangulated.append(polygon)
                    continue
                center=struct.unpack('<3f',struct.pack('<3f',*[sum(p[k] for p in polygon)/len(polygon) for k in range(3)]))
                for j,p in enumerate(polygon):
                    triangulated.append([center,p,polygon[(j+1)%len(polygon)]])
            variants.append(('triangulated',triangulated))
    results['ambiguous_edges']=ambiguous
    results['scope']='Offline geometry experiment; validator success alone does not prove closure or gameplay correctness. UVs are zero placeholders.'
    (args.out/'report.json').write_text(json.dumps(results,indent=2)+'\n')
    print(json.dumps(results,indent=2))
    if results['original']['exit']:raise RuntimeError('Original exported mesh does not validate')


if __name__=='__main__':main()
