"""Execute original4d0990 connectivity and largest-component selection on supplied graphs."""
from probe_debris_motion import ROOT,hashlib,json,struct,pefile,Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_ECX

def main():
    raw=(ROOT/'Installed_Game/RF.exe').read_bytes();sha=hashlib.sha256(raw).hexdigest()
    assert sha=='b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
    image=pefile.PE(data=raw).get_memory_mapped_image();rows=[]
    cases=[('single',[[0,1,2]],[]),('vertex_touch',[[0,1,2],[2,3,4]],[]),
           ('split',[[0,1,2],[2,3,4],[5,6,7]],[]),('tie',[[0,1,2],[3,4,5]],[]),
           ('largest_last',[[0,1,2],[3,4,5],[5,6,7]],[]),
           ('filtered_bridge',[[0,1,2],[2,3,4],[4,5,6]],[1])]
    for name,graph,excluded in cases:
      for accept in (0,1):
        u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,(len(image)+4095)&~4095);u.mem_write(0x400000,image)
        b=0x30000000;u.mem_map(b,0x100000);w=lambda *v:struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v])
        root=b;region=b+0x1000;faces=[b+0x2000+i*0x100 for i in range(len(graph))]
        vertices=[b+0x4000+i*0x100 for i in range(max(max(f) for f in graph)+1)]
        def array(address,values,data):u.mem_write(address,w(len(values),len(values),data));u.mem_write(data,w(*values))
        array(root+0x78,vertices,b+0x10000);array(root+0x90,[region],b+0x10100);array(root+0x9c,[region],b+0x10200)
        u.mem_write(root+0x70,w(faces[0]));u.mem_write(region+0x28,w(faces[0]))
        following={face:faces[i+1] if i+1<len(faces) else 0 for i,face in enumerate(faces)}
        for i,(face,indices) in enumerate(zip(faces,graph)):
            u.mem_write(face+0x28,w(4 if i in excluded else 0));u.mem_write(face+0x2c,w(123))
            nodes=[b+0x20000+i*0x100+j*0x20 for j in range(len(indices))]
            u.mem_write(face+0x40,w(nodes[0]))
            for j,(node,v) in enumerate(zip(nodes,indices)):
                u.mem_write(node,w(vertices[v]));u.mem_write(node+0x14,w(nodes[(j+1)%len(nodes)]))
        for i,v in enumerate(vertices):array(v+0x20,[faces[j] for j,f in enumerate(graph) if i in f],b+0x30000+i*0x100)
        stack=b+0xf0000;stop=b+0xff000;u.mem_write(stack,w(stop));classifier=[]
        def ret(value=0,pop=0):
            sp=u.reg_read(UC_X86_REG_ESP);address=struct.unpack('<I',u.mem_read(sp,4))[0]
            u.reg_write(UC_X86_REG_EAX,value);u.reg_write(UC_X86_REG_ESP,sp+4+pop);u.reg_write(UC_X86_REG_EIP,address)
        def hook(cpu,address,size,context):
            sp=cpu.reg_read(UC_X86_REG_ESP)
            if address in (0x45ec30,0x476900):ret(following[struct.unpack('<I',cpu.mem_read(sp+4,4))[0]],4)
            elif address==0x4ce2e0:ret(len(faces))
            elif address==0x573619:ret(b+0x80000)
            elif address==0x57360e:ret()
            elif address==0x45ebb0:ret(0)
            elif address==0x4e1180:
                classifier.append(struct.unpack('<I',cpu.mem_read(sp+4,4))[0]);ret(accept,4)
        u.hook_add(UC_HOOK_CODE,hook);u.reg_write(UC_X86_REG_ESP,stack);u.reg_write(UC_X86_REG_ECX,root)
        u.emu_start(0x4d0990,stop,count=200000);assert u.reg_read(UC_X86_REG_EIP)==stop
        labels=[struct.unpack('<i',u.mem_read(f+0x2c,4))[0] for f in faces];count=u.reg_read(UC_X86_REG_EAX)
        # Independent graph traversal excludes flagged faces and connects at ANY shared vertex.
        pending=set(range(len(graph)))-set(excluded);components=[]
        while pending:
            component={min(pending)};pending-=component
            while True:
                used={v for i in component for v in graph[i]};added={i for i in pending if used.intersection(graph[i])}
                if not added:break
                component|=added;pending-=added
            components.append(component)
        retained=max(components,key=len);expected_count=len(components)-1 if accept else 0
        assert count==expected_count,(name,accept,count,labels)
        if len(components)>1:
            for component in components:
                values={labels[i] for i in component};assert len(values)==1
                assert (next(iter(values))==-1)==(component==retained or not accept)
        for i in excluded:assert labels[i]==-1
        raw_labels=[next((n for n,c in enumerate(components) if i in c),0xffffffff) for i in range(len(graph))]
        rows.append(dict(name=name,graph=graph,excluded=excluded,classifier_accept=accept,labels=labels,returned=count,classifier_calls=classifier,
                         raw_labels=raw_labels,raw_count=len(components),largest=components.index(retained)))
    out=ROOT/'artifacts/geomod-postedit-re/components.json'
    out.write_text(json.dumps(dict(result='PASS',original_sha256=sha,cases=rows,
        scope='Full4d0990 with real array access, eligibility and graph walk. Linked iterators, allocation, region predicate and downstream4e1180 classification supplied; no full CSG/extraction claim.'),indent=2)+'\n')
    (ROOT/'tests/fixtures/geomod_components.inc').write_text('/* Graph cases independently checked against original4d0990. */\n'+''.join(
        ' {'+str(len(r['graph']))+'u,{'+','.join(str(v)+'u' for v in sum(r['graph'],[])+[0]*(9-3*len(r['graph'])))+'},'+
        str(sum(1<<i for i in r['excluded']))+'u,'+str(r['raw_count'])+'u,'+str(r['largest'])+'u,{'+
        ','.join(str(v)+'u' for v in r['raw_labels']+[0]*(3-len(r['graph'])))+'}},\n' for r in rows if r['classifier_accept']))
    print('PASS',len(rows),'original connectivity/selection cases')

if __name__=='__main__':main()
