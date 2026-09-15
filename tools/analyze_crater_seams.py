"""Check uncovered pixels against exact fixed-grid projected triangle edges.

Run dev_crater_depth_check.py first. RFM1 stores count/world_count/stride and
raw preview vertices. Only world triangles are inspected; the paired replay
requires no live debris. This distinguishes missing triangle coverage from a
software raster edge test error, without changing clipping or collision.
"""
import json,math,struct
from dev_destruction_check import ROOT

def main():
    folder=ROOT/'artifacts/destruction/depth-audit'
    audit=json.loads((folder/'report.json').read_text());raw=(folder/'cut.mesh').read_bytes()
    assert raw[:4]==b'RFM1';count,world,stride=struct.unpack_from('<3I',raw,4)
    assert stride==56 and world<=count and world%3==0 and len(raw)==16+count*stride
    vertices=[]
    for i in range(world):
        x,y,z=struct.unpack_from('<3f',raw,16+i*stride);point=(round(x*16),round(y*16))
        assert all(math.isfinite(v) for v in (x,y,z)) and point==(x*16,y*16)
        vertices.append(point)
    def edge(a,b,p):return (p[0]-a[0])*(b[1]-a[1])-(p[1]-a[1])*(b[0]-a[0])
    rows=[]
    for x,y in audit['uncovered_coordinates']:
        point=(x*16+8,y*16+8);covering=[];near=[]
        for first in range(0,world,3):
            tri=vertices[first:first+3];area=edge(tri[0],tri[1],tri[2])
            if not area:continue
            sign=1 if area>0 else -1
            distances=[sign*edge(tri[j],tri[(j+1)%3],point) for j in range(3)]
            if min(distances)>=0:covering.append(first)
            if sum(d<0 for d in distances)==1:
                j=distances.index(min(distances));a,b=tri[j],tri[(j+1)%3]
                separation=-distances[j]/(16*math.hypot(b[0]-a[0],b[1]-a[1]))
                near.append(dict(first_vertex=first,edge=j,separation_pixels=separation,endpoints=[[v/16 for v in a],[v/16 for v in b]]))
        rows.append(dict(pixel=[x,y],covering_triangles=covering,nearest_edges=sorted(near,key=lambda n:n['separation_pixels'])[:2]))
    result=dict(world_vertices=world,uncovered=len(rows),covered_by_exact_triangles=sum(bool(r['covering_triangles']) for r in rows),pixels=rows,
        limitation='Projected topology evidence; original world-edge and clipping causes require separate diagnosis.')
    (folder/'seams.json').write_text(json.dumps(result,indent=2)+'\n')
    print('Exact projected coverage:',result['covered_by_exact_triangles'],'of',len(rows),'uncovered pixels')
    print('OPEN:',len(rows)-result['covered_by_exact_triangles'],'pixels fall outside every world triangle')
if __name__=='__main__':main()
