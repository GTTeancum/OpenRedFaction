"""Inspect exact clipping inputs producing points near a selected world corner."""
import argparse,collections,json,math,struct
from pathlib import Path

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace',type=Path)
    parser.add_argument('--point',nargs=3,type=float,required=True)
    parser.add_argument('--radius',type=float,default=3e-6)
    parser.add_argument('--require-identical',action='store_true',
                        help='Require at least one match and identical recorded output positions')
    args=parser.parse_args()
    assert all(map(math.isfinite,args.point)) and math.isfinite(args.radius) and args.radius>0
    assert args.trace.stat().st_size<=64*1024*1024,'Trace exceeds analysis bound'
    data=args.trace.read_bytes();assert data[:4]==b'RFI1' and (len(data)-4)%52==0
    matches=collections.Counter()
    for row in struct.iter_unpack('<13f',data[4:]):
        assert all(map(math.isfinite,row))
        if max(abs(row[10+i]-args.point[i]) for i in range(3))<=args.radius:matches[row]+=1
    result=dict(events=(len(data)-4)//52,point=args.point,radius=args.radius,
        scope='Input planes/edges and rounded outputs; nearby points are not automatically equivalent.',
        matches=[dict(count=n,plane=r[:4],a=r[4:7],b=r[7:10],result=r[10:]) for r,n in matches.items()])
    output=args.trace.with_suffix('.intersection.json');output.write_text(json.dumps(result,indent=2)+'\n')
    print('events',result['events'],'unique nearby constructions',len(matches));print(output)
    if args.require_identical:
        assert matches and len({row[10:] for row in matches})==1,'Selected corner outputs differ or are absent'

if __name__=='__main__':main()
