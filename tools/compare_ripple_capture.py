"""Compare process-local PC/Xbox ripple captures without tolerance acceptance."""
import argparse
import json
from pathlib import Path
import struct

def read(path):
    data=path.read_bytes();header=struct.unpack_from('<5I',data);count=header[2]
    assert count<=384 and header[3]==56 and header[4]==0
    at=20;result={'header':list(header)}
    for name,n in [('vertices',count*14),('camera',12),('sources',96),('input_count',1),('inputs',240),('fp',4)]:
        result[name]=list(struct.unpack_from('<'+'I'*n,data,at));at+=n*4
    if len(data)>at:
        result['local']=list(struct.unpack_from('<240I',data,at));at+=960
    assert at==len(data)
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('directory',type=Path);args=parser.parse_args()
    pc=read(args.directory/'pc-ripple-vertices.bin');xbox=read(args.directory/'xbox-ripple-vertices.bin')
    assert pc.keys()==xbox.keys()
    report={'result':'PASS','sections':{}}
    for name,values in pc.items():
        other=xbox[name];assert len(values)==len(other)
        differences=[dict(index=i,pc=a,xbox=b) for i,(a,b) in enumerate(zip(values,other)) if a!=b]
        report['sections'][name]=dict(words=len(values),differences=differences)
        if differences:report['result']='FAIL'
        print(name,len(differences),'differing words of',len(values))
    fields={}
    to_float=lambda v:struct.unpack('<f',struct.pack('<I',v))[0]
    for field in range(14):
        differences=[(a,b) for a,b in zip(pc['vertices'][field::14],xbox['vertices'][field::14]) if a!=b]
        if differences:
            fields[str(field)]={'count':len(differences)}
            if field not in (9,13):fields[str(field)]['max_absolute_difference']=max(abs(to_float(a)-to_float(b)) for a,b in differences)
    report['vertex_fields']=fields
    (args.directory/'ripple-comparison.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report['result'],json.dumps(fields))
    return 0 if report['result']=='PASS' else 1

if __name__=='__main__':raise SystemExit(main())
