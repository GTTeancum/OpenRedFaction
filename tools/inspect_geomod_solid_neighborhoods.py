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
    inventory=json.loads((ROOT/'artifacts/inventory.json').read_text())
    level=next(x for x in json.loads((ROOT/'artifacts/levels.json').read_text()) if x['file']=='ctf06.rfl')
    archive=next(x for x in inventory['files'] if x['path']==level['archive'])
    entry=next(x for x in archive['vpp']['entries'] if x['name']=='ctf06.rfl')
    section=next(x for x in level['sections'] if x['type']=='0x100')
    with (ROOT/'Installed_Game'/level['archive']).open('rb') as stream:
        stream.seek(entry['offset']+section['offset']+8);raw=stream.read(section['size'])
    cursor=6
    def take(n):
        nonlocal cursor
        assert 0<=n<=len(raw)-cursor
        out=raw[cursor:cursor+n];cursor+=n;return out
    def word():return struct.unpack('<I',take(4))[0]
    def string():return take(struct.unpack('<H',take(2))[0]).decode('cp1252')
    for _ in range(word()):string()
    take(word()*12);rooms=[]
    for index in range(word()):
        data=take(40);name=string();life=struct.unpack_from('<f',data,36)[0]
        rooms.append(dict(index=index,name=name,detail=data[34],life=life,initial_protected=not life>0,
                          minimum=struct.unpack_from('<3f',data,4),maximum=struct.unpack_from('<3f',data,16)))
        if data[32]:take(8);string();take(37)
        if data[33]:take(4)
    links={}
    for _ in range(word()):
        parent=word();links[parent]=[word() for _ in range(word())]
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
                    overlap_extent=[min(a['maximum'][k],b['maximum'][k])-max(a['minimum'][k],b['minimum'][k]) for k in range(3)],
                    compiled_ids=[f['id'] for f in compiled[b['uid']]],
                    compiled_rooms=[rooms[n] for n in sorted({f['room'] for f in compiled[b['uid']]})]))
        rows.append(dict(uid=a['uid'],index=a['index'],minimum=a['minimum'],maximum=a['maximum'],
                         authored_faces=len(a['world']),compiled_faces=len(visible),compiled_ids=[f['id'] for f in visible],
                         rooms=sorted({f['room'] for f in compiled[a['uid']]}),portal_faces=[f['id'] for f in visible if f['portal']],
                         topology=topology(a['world']),halfspaces=convex(a['world'],False),neighbors=neighbors))
    by_uid={r['uid']:r for r in rows}
    # Cross-check known admitted neighborhoods before trusting the larger census.
    for uid,expected in [(93,{66,70,71,95}),(94,{66,70,71,95}),(95,{66,80,85,93,94}),
                         (96,{66,70,71,98}),(97,{66,70,71,98}),(98,{66,82,86,96,97})]:
        assert {n['uid'] for n in by_uid[uid]['neighbors']}==expected,uid
    # Derive detail candidates from the selected posts rather than assuming UID order.
    post_details={75:11179,79:11178,99:11181,103:11180}
    detail_uids={12815}
    for uid,expected_detail in post_details.items():
        neighbors=by_uid[uid]['neighbors']
        actual={n['uid'] for n in neighbors if n['flags']==4}
        assert actual=={expected_detail},(uid,actual)
        assert {n['uid'] for n in neighbors if n['flags']==0}=={70,92 if uid<90 else 108},uid
        assert {n['uid'] for n in neighbors if n['flags']==2}=={66},uid
        detail_uids.update(actual)
    details=[]
    for uid in sorted(detail_uids):
        record=next(b for b in records if b['uid']==uid);selected=compiled[uid]
        details.append(dict(uid=uid,flags=record['flags'],minimum=record['minimum'],maximum=record['maximum'],
            compiled_ids=[f['id'] for f in selected],compiled_flags=sorted({f['flags'] for f in selected}),
            rooms=[rooms[n] for n in sorted({f['room'] for f in selected})],
            parents=sorted(parent for parent,children in links.items() if any(f['room'] in children for f in selected))))
    for d in details:
        assert d['flags']==4 and d['parents']==[3],d['uid']
        assert len(d['rooms'])==1 and d['rooms'][0]['detail']==1,d['uid']
        assert d['rooms'][0]['life']==-1 and d['rooms'][0]['initial_protected'],d['uid']
        if d['uid']!=12815:
            assert len(d['compiled_ids'])==2 and d['compiled_flags']==[0x1c8],d['uid']
            assert d['minimum'][2]==d['maximum'][2],d['uid']
    pillar=next(d for d in details if d['uid']==12815)
    assert pillar['flags']==4 and len(pillar['compiled_ids'])==10 and [r['index'] for r in pillar['rooms']]==[166]
    report=dict(scope=__doc__,geometry_sha256=meta['geometry_sha256'],export_sha256=hashlib.sha256(export.read_bytes()).hexdigest(),
                total_brushes=len(records),room3_solid_owners=len(rows),room3_solid_faces=sum(r['compiled_faces'] for r in rows),
                closed_convex=sum(r['topology']['closed_oriented'] and r['halfspaces']['convex'] for r in rows),detail_neighbors=details,rows=rows)
    (ROOT/'artifacts/geomod-solid-neighborhoods.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:report[k] for k in ('total_brushes','room3_solid_owners','room3_solid_faces','closed_convex')}))
    for d in details:print('DETAIL',json.dumps(d))
    for r in rows:
        print(r['uid'],'faces',r['authored_faces'],r['compiled_faces'],'convex',r['halfspaces']['convex'],
              'rooms',r['rooms'],'neighbors',[(n['uid'],n['flags'],'earlier' if n['earlier'] else 'later') for n in r['neighbors']])
if __name__=='__main__':main()
