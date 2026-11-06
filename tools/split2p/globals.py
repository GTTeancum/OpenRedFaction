"""Scene globals with sizes (address gaps within .bss/.data of a no-LTO scene
object). build/split2p/scene_syms.txt is `llvm-nm -S --defined-only` of
src/diagnostic/scene.c compiled with the Xbox flags minus -flto."""
import os
HERE = os.path.dirname(os.path.abspath(__file__))
import re, sys
rows = [l.split() for l in open(os.path.join(HERE, '..', '..', 'build', 'split2p', 'scene_syms.txt'))]
rows = [(int(r[0], 16), r[2], r[3]) for r in rows if len(r) == 4 and r[2] in 'bBdD']
out = []
for kind in ('bB', 'dD'):
    sec = sorted([r for r in rows if r[1] in kind])
    for i, (a, t, n) in enumerate(sec):
        nxt = sec[i + 1][0] if i + 1 < len(sec) else a
        out.append((n.lstrip('_'), nxt - a, t))
if __name__ == '__main__':
    pat = re.compile(sys.argv[1] if len(sys.argv) > 1 else '.', re.I)
    sel = [o for o in out if pat.search(o[0])]
    print(len(out), 'globals', sum(o[1] for o in out), 'bytes;', len(sel), 'matching', sum(o[1] for o in sel), 'bytes')
    for n, s, t in sorted(sel, key=lambda o: -o[1])[:40]: print(s, n)
