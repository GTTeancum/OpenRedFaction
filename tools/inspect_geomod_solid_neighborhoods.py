"""Read-only room3 solid-neighborhood census for generalized GeoMod loading.
AABB candidates are not exact solid intersections or permission to destroy.
"""
import collections,hashlib,json,struct
from pathlib import Path
from inspect_geomod_source_topology import load,topology,convex
ROOT=Path(__file__).resolve().parents[1]
F32=lambda x:struct.unpack('<f',struct.pack('<f',x))[0]
def main():
    export=ROOT/'artifacts/future-vehicles-re/ctf06-editor-brushes.json'
    brushes=json.loads(export.read_text(encoding='utf-8'));faces,meta=load('ctf06.rfl')
    owners={};records=[]
    for index,b in enumerate(brushes):
        basis=b['basis'][3:]+b['basis'][:3];world=[]
        for f in b['faces']:
            points=[tuple(F32(F32(sum(p[j]*basis[j*3+k] for j in range(3)))+b['position'][k]) for k in range(3)) for p in f['points']]
            token=f['source_word'];assert token not in owners;owners[token]=b['uid']
            world.append(dict(id=token,points=points))
        ps=[p for f in world for p in f['points']]
        records.append(dict(uid=b['uid'],index=index,flags=b['tail'][2],world=world,
                            minimum=[min(p[k] for p in ps) for k in range(3)],maximum=[max(p[k] for p in ps) for k in range(3)]))
    compiled=collections.defaultdict(list)
    for f in faces:
        if f['source_word'] in owners:compiled[owners[f['source_word']]].append(f)
    rows=[]
    for a in records:
        visible=[f for f in compiled[a['uid']] if f['room']==3]
        if a['flags'] or not visible:continue
        neighbors=[]
        for b in records:
            if a is b:continue
            if all(a['minimum'][k]<=b['maximum'][k]+1e-5 and a['maximum'][k]>=b['minimum'][k]-1e-5 for k in range(3)):
                neighbors.append(dict(uid=b['uid'],index=b['index'],flags=b['flags'],earlier=b['index']<a['index'],
                    overlap_extent=[min(a['maximum'][k],b['maximum'][k])-max(a['minimum'][k],b['minimum'][k]) for k in range(3)]))
        rows.append(dict(uid=a['uid'],index=a['index'],minimum=a['minimum'],maximum=a['maximum'],
                         authored_faces=len(a['world']),compiled_faces=len(visible),compiled_ids=[f['id'] for f in visible],
                         rooms=sorted({f['room'] for f in compiled[a['uid']]}),portal_faces=[f['id'] for f in visible if f['portal']],
                         topology=topology(a['world']),halfspaces=convex(a['world'],False),neighbors=neighbors))
    by_uid={r['uid']:r for r in rows}
    # Cross-check known admitted neighborhoods before trusting the larger census.
    for uid,expected in [(93,{66,70,71,95}),(94,{66,70,71,95}),(95,{66,80,85,93,94}),
                         (96,{66,70,71,98}),(97,{66,70,71,98}),(98,{66,82,86,96,97})]:
        assert {n['uid'] for n in by_uid[uid]['neighbors']}==expected,uid
    report=dict(scope=__doc__,geometry_sha256=meta['geometry_sha256'],export_sha256=hashlib.sha256(export.read_bytes()).hexdigest(),
                total_brushes=len(records),room3_solid_owners=len(rows),room3_solid_faces=sum(r['compiled_faces'] for r in rows),
                closed_convex=sum(r['topology']['closed_oriented'] and r['halfspaces']['convex'] for r in rows),rows=rows)
    (ROOT/'artifacts/geomod-solid-neighborhoods.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:report[k] for k in ('total_brushes','room3_solid_owners','room3_solid_faces','closed_convex')}))
    for r in rows:
        print(r['uid'],'faces',r['authored_faces'],r['compiled_faces'],'convex',r['halfspaces']['convex'],
              'rooms',r['rooms'],'neighbors',[(n['uid'],n['flags'],'earlier' if n['earlier'] else 'later') for n in r['neighbors']])
if __name__=='__main__':main()
