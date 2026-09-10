"""Prepare/build an isolated APU evaluation, keeping dependencies/assets ignored."""
import argparse,hashlib,json,os,shutil,subprocess,tarfile,urllib.request
from pathlib import Path
root=Path(__file__).resolve().parents[1];local=root/'local';dependency=local/'nxdk-audio'
parser=argparse.ArgumentParser();parser.add_argument('--backend-only',action='store_true');args=parser.parse_args()
revision='fc2deca2cc1e434805ac03ca7c2f500b3b028f36'
if not dependency.exists():
    subprocess.run(['git','clone','https://github.com/Ryzee119/nxdk-audio.git',str(dependency)],cwd=root,check=True)
    subprocess.run(['git','checkout','--detach',revision],cwd=dependency,check=True)
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=dependency,text=True).strip()==revision
subprocess.run(['git','diff','--exit-code',revision,'--'],cwd=dependency,check=True,stdout=subprocess.DEVNULL)
package='dsp56300-0.1.3-x86_64-pc-windows-gnu'
archive=local/'dsp56300-windows.tar.gz'
url=f'https://github.com/mborgerson/dsp56300/releases/download/v0.1.3/{package}.tar.gz'
if not archive.exists():urllib.request.urlretrieve(url,archive)
digest=hashlib.sha256(archive.read_bytes()).hexdigest()
assert digest=='ff031c6daf89f4c2c78a5943920bf483b044e8ec805feaf1efe32c80f991b09c'
assembler=local/package/'bin/dsp56300-asm.exe'
if not assembler.exists():
    with tarfile.open(archive) as tar:tar.extractall(local,filter='data')
subprocess.run([str(assembler),'-f','lod','-o',str(dependency/'passthrough.out'),str(dependency/'passthrough.a56')],cwd=root,check=True)
build=root/'build/apu-probe';(build/'disc').mkdir(parents=True,exist_ok=True)
backend=root/'build/nxaudio' if args.backend_only else build/'backend'
shutil.copytree(dependency,backend,dirs_exist_ok=True,ignore=shutil.ignore_patterns('.git','*.obj','*.c.d','example'))
core=backend/'audio_core.c';text=core.read_text()
text=text.replace('static void *g_hw_ac97_buffer = NULL;', 'void *g_hw_ac97_buffer = NULL; /* Isolated probe visibility. */')
# Local adaptation: descriptors are ordinary image data, not contiguous
# allocations. Pin them before asking the debug kernel for physical addresses.
text=text.replace('static ac97_descriptor_t pcm_output_descriptor[1];',
    'static ac97_descriptor_t pcm_output_descriptor[1];\nstatic bool rf_descriptors_locked;')
text=text.replace('        pcm_output_descriptor[0].buffer_start_address',
    '        /* OpenRedFaction: pin DMA descriptor pages for their hardware lifetime. */\n'
    '        MmLockUnlockBufferPages(pcm_output_descriptor, sizeof(pcm_output_descriptor), FALSE);\n'
    '        MmLockUnlockBufferPages(spdif_output_descriptor, sizeof(spdif_output_descriptor), FALSE);\n'
    '        rf_descriptors_locked = true;\n'
    '        pcm_output_descriptor[0].buffer_start_address',1)
text=text.replace('#define APU_FREE(ptr)',
    '    if (rf_descriptors_locked) {\n'
    '        MmLockUnlockBufferPages(pcm_output_descriptor, sizeof(pcm_output_descriptor), TRUE);\n'
    '        MmLockUnlockBufferPages(spdif_output_descriptor, sizeof(spdif_output_descriptor), TRUE);\n'
    '        rf_descriptors_locked = false;\n'
    '    }\n\n#define APU_FREE(ptr)',1)
core.write_text(text)
old='''                    MmLockUnlockBufferPages((PVOID)buffer->buffer, buffer->size_bytes, TRUE);
                    if (voice->streaming) {'''
new='''                    /* OpenRedFaction: static buffers remain owned until destroy/replacement. */
                    if (voice->streaming) {
                        MmLockUnlockBufferPages((PVOID)buffer->buffer, buffer->size_bytes, TRUE);'''
assert old in text
core.write_text(text.replace(old,new,1))
if args.backend_only:
    (backend/'provenance.json').write_text(json.dumps(dict(nxaudio_revision=revision,assembler_archive_sha256=digest,
        adapted_core_sha256=hashlib.sha256(core.read_bytes()).hexdigest()),indent=2)+'\n')
    raise SystemExit(0)
# Isolated probe only: fail each contiguous allocation without changing production.
probe_core=core.read_text()
probe_core=probe_core.replace('bool nxAudioInit (',
    'extern unsigned int rf_apu_fail_allocation, rf_apu_allocation_index;\nbool nxAudioInit (',1)
allocation='MmAllocateContiguousMemoryEx((size), 0, 0xFFFFFFFF, (align), PAGE_READWRITE)'
assert allocation in probe_core
probe_core=probe_core.replace(allocation,
    '(rf_apu_fail_allocation && ++rf_apu_allocation_index == rf_apu_fail_allocation ? NULL : '+allocation+')',1)
core.write_text(probe_core)
for name,source in [('probe.c','tests/xbox_apu_probe.c'),('audio.c','src/core/audio.c'),('vpp.c','src/core/vpp.c'),('xbox_audio.c','src/platform/xbox/audio.c'),('audio.h','src/platform/xbox/audio.h')]:
    shutil.copyfile(root/source,build/name)
inventory=json.loads((root/'artifacts/inventory.json').read_text())
entry=next(e for a in inventory['files'] if a['path']=='audio.vpp' for e in a['vpp']['entries'] if e['name']=='DoorOpen_07.wav')
with (root/'Installed_Game/audio.vpp').open('rb') as source:
    source.seek(entry['offset']);data=source.read(entry['size'])
assert hashlib.sha256(data).hexdigest()=='1f78088850b4d256bbfd919efd82daa97f3866b3cd9ed06f7c830f463c321393'
(build/'disc/door.wav').write_bytes(data)
prefix='/'+root.drive[0].lower()+root.as_posix()[2:]
(build/'Makefile').write_text(f'''XBE_TITLE = RF-APU-Probe
NXDK_DIR = /c/nxdk
OUTPUT_DIR = {prefix}/build/apu-probe/disc
GEN_XISO = apu-probe.iso
SRCS = {prefix}/build/apu-probe/xbox_audio.c {prefix}/build/apu-probe/probe.c {prefix}/build/apu-probe/audio.c {prefix}/build/apu-probe/vpp.c {prefix}/build/apu-probe/backend/audio_core.c {prefix}/build/apu-probe/backend/audio_buffer.c {prefix}/build/apu-probe/backend/audio_voice.c
CFLAGS = -std=c23 -O2 -I{prefix}/include -I{prefix}/build/apu-probe/backend/include -I{prefix}/build/apu-probe/backend
LDFLAGS = -map:main.map
include $(NXDK_DIR)/Makefile
''')
command='export NXDK_DIR=/c/nxdk; export PATH=/c/nxdk/bin:/clang64/bin:/mingw64/bin:/usr/bin:$PATH; cd build/apu-probe; make -j4'
subprocess.run(['C:/msys64/usr/bin/bash.exe','--noprofile','--norc','-c',command],cwd=root,env=dict(os.environ,MSYSTEM='CLANG64'),check=True)
(build/'provenance.json').write_text(json.dumps(dict(nxaudio_revision=revision,assembler_url=url,assembler_archive_sha256=digest,
    dsp_sha256=hashlib.sha256((dependency/'passthrough.out').read_bytes()).hexdigest(),sample_sha256=hashlib.sha256(data).hexdigest(),
    adapted_core_sha256=hashlib.sha256(core.read_bytes()).hexdigest(),
    adaptations=['Pin/unpin AC97 descriptor image pages for DMA lifetime','Retain static sample page locks until destroy/replacement','Expose DMA buffer for isolated observation']),indent=2)+'\n')
