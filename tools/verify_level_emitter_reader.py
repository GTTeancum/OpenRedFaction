"""Shared bounded level emitter reader vs independent installed section inventory."""
import ctypes as c,json,struct,subprocess,runpy
from pathlib import Path
root=Path(__file__).resolve().parents[1]
runpy.run_path(str(Path(__file__).with_name('inspect_level_emitters.py')),run_name='__main__')
class Emitter(c.Structure):
    _fields_=[(n,c.c_uint32) for n in ('offset','bytes','uid')]+[(n,c.c_char*256) for n in ('name','script','bitmap')]+[
        ('position',c.c_float*3),('orientation_disk',c.c_float*9),('header_byte',c.c_uint32),('header_word',c.c_uint32),
        ('spawn_radius',c.c_float),('unknown_floats',c.c_float*2),('delay',c.c_float*2),('speed',c.c_float*2),('acceleration',c.c_float),
        ('life',c.c_float*2),('radius',c.c_float*2),('growth',c.c_float),('gravity_scale',c.c_float),('cone_angle',c.c_float),
        ('color',c.c_uint8*4),('color_destination',c.c_uint8*4),('emitter_flags',c.c_uint32),('particle_flags',c.c_uint32),
        ('enabled',c.c_uint32),('cycle',c.c_float*4),('finish_age',c.c_float)]
inventory=json.loads((root/'artifacts/level-emitters.json').read_text());count=0
for level in inventory['results']:
    raw=subprocess.check_output([str(root/'build/pc/Release/rf_collision_probe.exe'),'--level-emitters',str(root/'Installed_Game'/level['archive']),level['file']])
    assert len(raw)==len(level['records'])*c.sizeof(Emitter)
    for i,reference in enumerate(level['records']):
        record=Emitter.from_buffer_copy(raw,i*c.sizeof(Emitter));count+=1
        for name,typ in Emitter._fields_:
            actual=getattr(record,name);expected=reference[name]
            if isinstance(actual,bytes):assert actual.decode('cp1252')==expected,(level['file'],i,name)
            elif typ==c.c_float:assert struct.pack('<f',actual)==struct.pack('<f',expected),(level['file'],i,name)
            elif issubclass(typ,c.Array):
                if typ._type_==c.c_float:assert bytes(actual)==struct.pack('<'+'f'*len(expected),*expected),(level['file'],i,name)
                else:assert list(actual)==expected,(level['file'],i,name)
            else:assert actual==expected,(level['file'],i,name)
report=dict(result='PASS',levels=len(inventory['results']),emitters=count,truncated_records=count,scope='PC bounded C A00 reader equals independent Python inventory field-for-field, with exact section exhaustion and every record truncated at its final byte rejected without output/reader mutation. Raw metadata only; original reader execution, template conversion, runtime creation and native gameplay excluded.')
(root/'artifacts/level-emitter-reader-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
