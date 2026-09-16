"""Generate checksum-valid/semantic-invalid newer RFSG and verify transport fallback.
No native process, HDD editing or scene build. Host callback is an exact baseline
oracle, NOT the full scene validator; its role is explicitly transport-only.
"""
import argparse,datetime,hashlib,json,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def word(b,at):return struct.unpack_from('<I',b,at)[0]
def fnv(b):
 h=2166136261
 for v in b:h=((h^v)*16777619)&0xffffffff
 return h
def envelope(b,g):
 h=b'RFSG'+struct.pack('<IIII',1,g,len(b),fnv(b));return h+struct.pack('<I',fnv(h))+b

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);p.add_argument('--host-check',action='store_true');args=p.parse_args()
 good=args.source.read_bytes()
 if good[:8]!=b'RFDS\1\0\0\0' or word(good,8)!=len(good):raise ValueError('Expected valid RFDS v1')
 admissions,maps,core,faces=[word(good,i) for i in (240,248,252,272)];at=288+core+48*admissions+88*maps
 if at+2*faces!=len(good) or not maps:raise ValueError('Need nonempty generated terrain baseline')
 face=next(i for i in range(faces) if struct.unpack_from('<H',good,at+2*i)[0]!=65535)
 bad=bytearray(good);struct.pack_into('<H',bad,at+2*face,maps)
 run=ROOT/'artifacts/geomod-hdd-fallback'/datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f');run.mkdir(parents=True)
 (run/'baseline.rfds').write_bytes(good);(run/'candidate.rfds').write_bytes(bad)
 for name,data in [('seed-slot0.rfsg',envelope(good,1)),('seed-slot1.rfsg',envelope(bad,2))]:(run/name).write_bytes(data)
 report=dict(result='GENERATED',source=str(args.source.resolve()),source_sha256=hashlib.sha256(good).hexdigest(),candidate_sha256=hashlib.sha256(bad).hexdigest(),invalid_face=face,map_count=maps,invalid_binding_offset=at+face*2,expected='pure scene validator RF_FORMAT at generated map index check; older generation1 selected; save rewrites only slot1 as generation2',scope='Checksummed fixture; optional real transport + exact-baseline oracle, not scene/native proof')
 if args.host_check:
  (run/'checkpoint.0').write_bytes(envelope(good,1));(run/'checkpoint.1').write_bytes(envelope(bad,2));protected=(run/'checkpoint.0').read_bytes()
  code=r"""
#include "rf/checkpoint_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(v) do{if(!(v)){fprintf(stderr,"line%d %s\n",__LINE__,#v);exit(1);}}while(0)
static unsigned char expected[RF_CHECKPOINT_FILE_MAX],buffer[RF_CHECKPOINT_FILE_MAX];static uint32_t length,calls,rejected;
static int oracle(const void *data,uint32_t n,void *context){(void)context;calls++;if(n!=length||memcmp(data,expected,n)){rejected++;return RF_FORMAT;}return RF_OK;}
int main(int argc,char **argv){FILE *f;rf_checkpoint_file_selection selected;uint32_t n;CHECK(argc==3);f=fopen(argv[2],"rb");CHECK(f);length=(uint32_t)fread(expected,1,sizeof(expected),f);CHECK(!ferror(f)&&fgetc(f)==EOF&&!fclose(f));
CHECK(!rf_checkpoint_file_load(argv[1],buffer,sizeof(buffer),&n,oracle,NULL,&selected));CHECK(selected.slot==0&&selected.generation==1&&n==length&&!memcmp(buffer,expected,n)&&rejected==1);
CHECK(!rf_checkpoint_file_store(argv[1],buffer,n,oracle,NULL,&selected));CHECK(selected.slot==1&&selected.generation==2);
CHECK(!rf_checkpoint_file_load(argv[1],buffer,sizeof(buffer),&n,oracle,NULL,&selected));CHECK(selected.slot==1&&selected.generation==2&&!memcmp(buffer,expected,n));
printf("PASS semantic callback fallback and next-save selection (%u calls, %u rejected)\n",calls,rejected);return 0;}
"""
  (run/'test.c').write_text(code)
  command=['C:/msys64/clang64/bin/clang.exe','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),str(ROOT/'src/core/checkpoint_file.c'),str(run/'test.c'),'-o',str(run/'test.exe')]
  subprocess.run(command,cwd=ROOT,check=True);output=subprocess.check_output([str(run/'test.exe'),str(run/'checkpoint'),str(run/'baseline.rfds')],cwd=ROOT,text=True)
  if (run/'checkpoint.0').read_bytes()!=protected:raise RuntimeError('Protected older slot changed')
  if (run/'checkpoint.1').read_bytes()!=envelope(good,2):raise RuntimeError('Replacement is not exact generation2 baseline')
  report.update(result='PASS_HOST_TRANSPORT',output=output.strip(),protected_sha256=hashlib.sha256(protected).hexdigest(),core_sha256=hashlib.sha256((ROOT/'src/core/checkpoint_file.c').read_bytes()).hexdigest())
 (run/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(run,report['result'])
if __name__=='__main__':main()
