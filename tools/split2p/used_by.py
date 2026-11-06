"""Globals referenced by the player's routines that the per-player table does not list.
Usage: used_by.py FUNCTION [FUNCTION...] (functions in src/diagnostic/scene.c and its .inc files)."""
import os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from globals import out
from classify import inc
src_dir = os.path.join(HERE, '..', '..', 'src', 'diagnostic')
text = ''
for name in sorted(os.listdir(src_dir)):
    if name == 'scene.c' or name.endswith('.inc'):
        text += open(os.path.join(src_dir, name), encoding='utf-8', errors='replace').read() + '\n'
def body(fn):
    m = re.search(r'^(?:static )?[a-zA-Z_][\w \*]*\b' + fn + r'\([^;{]*\)\s*\{', text, re.M)
    if not m: return ''
    i = m.end(); depth = 1
    while depth and i < len(text):
        c = text[i]; depth += (c == '{') - (c == '}'); i += 1
    return text[m.start():i]
known = {n for n, s, t in out}
listed = {n for n, s, t in inc}
seen = {}
for fn in sys.argv[1:]:
    for ident in set(re.findall(r'\b[A-Za-z_]\w*\b', body(fn))):
        if ident in known and ident not in listed:
            seen.setdefault(ident, []).append(fn)
for ident in sorted(seen):
    print(ident, ','.join(seen[ident]))
