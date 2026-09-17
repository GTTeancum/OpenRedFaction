"""Attribute changed depth pixels to recorded world triangles, without tolerance changes."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import numpy as np


def depth(path):
    data=path.read_bytes()
    assert data[:4]==b'RFD1'
    width,height=struct.unpack_from('<2I',data,4)
    assert len(data)==12+width*height*4
    return np.frombuffer(data,dtype='<f4',offset=12).reshape(height,width)


def owners(path, pixels, recorded):
    data=path.read_bytes()
    assert data[:4]==b'RFM1'
    count,world,stride=struct.unpack_from('<3I',data,4)
    assert stride==56 and world%3==0 and world<=count and len(data)==16+count*stride
    values=np.frombuffer(data,dtype='<f4',offset=16).reshape(count,14)[:world]
    triangles=values[:,:3].reshape(-1,3,3)
    materials=np.frombuffer(data,dtype='<u4',offset=16).reshape(count,14)[:world:3,9]
    maps=np.frombuffer(data,dtype='<u4',offset=16).reshape(count,14)[:world:3,13]
    a,b,c=(triangles[:,i,:] for i in range(3))
    area=(c[:,0]-a[:,0])*(b[:,1]-a[:,1])-(c[:,1]-a[:,1])*(b[:,0]-a[:,0])
    safe=np.where(abs(area)>=.00001,area,1)
    output=[]
    for x,y in pixels:
        px,py=np.float32(x+.5),np.float32(y+.5)
        u=((px-b[:,0])*(c[:,1]-b[:,1])-(py-b[:,1])*(c[:,0]-b[:,0]))/safe
        v=((px-c[:,0])*(a[:,1]-c[:,1])-(py-c[:,1])*(a[:,0]-c[:,0]))/safe
        w=np.float32(1)-u-v
        z=u*a[:,2]+v*b[:,2]+w*c[:,2]
        valid=(abs(area)>=.00001)&(u>=0)&(v>=0)&(w>=0)&(maps<0xfffffc00)
        z=np.where(valid,z,np.inf)
        first=int(np.argmin(z));error=abs(float(z[first])-float(recorded[y,x]))
        # Float32 reconstruction can differ slightly from x87 intermediates.
        assert error<=4, (x,y,error,'Cannot attribute recorded depth reliably')
        output.append(dict(pixel=[int(x),int(y)],triangle=first,material=int(materials[first]),
                           lightmap=int(maps[first]),reconstruction_error=error))
    return output


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder',type=Path)
    args=parser.parse_args();folder=args.folder
    summary=json.loads((folder/'report.json').read_text())
    cut,intact=depth(folder/'cut.depth'),depth(folder/'intact.depth')
    assert cut.shape==intact.shape
    delta=cut-intact
    rows=summary['rows_checked'];threshold=summary['depth_tolerance']
    near_y,near_x=np.where(delta[:rows]<-threshold)
    far_y,far_x=np.where((delta[:rows]>threshold)&(cut[:rows]<16777216))
    near=list(zip(near_x,near_y));far=list(zip(far_x,far_y))
    result=dict(scope=__doc__,nearer_count=len(near),farther_count=len(far),
                nearer={name:owners(folder/(name+'.mesh'),near,values) for name,values in [('cut',cut),('intact',intact)]})
    result['input_sha256']={name:hashlib.sha256((folder/name).read_bytes()).hexdigest()
                           for name in ('cut.mesh','intact.mesh','cut.depth','intact.depth','report.json')}
    far_owners=owners(folder/'cut.mesh',far,cut)
    result['farther_material_counts']=dict(Counter(row['material'] for row in far_owners))
    result['farther_max_reconstruction_error']=max((row['reconstruction_error'] for row in far_owners),default=0)
    (folder/'depth-owners.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))


if __name__=='__main__':main()
