"""Compare the port's PSS decoding with FFmpeg's (development only).

Copies one movie out of the user's PS2 ISO into an output folder, decodes it
with tools/pss_check (built here with the host compiler) and with FFmpeg, and
reports per-picture PSNR for Y, U and V plus the audio difference. Nothing
is written inside the repository.
  python tools/check_pss_decode.py <iso> <MOVIE.PSS> <out dir> [pictures]"""
import math, os, shutil, struct, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def extract(iso, name, out):
    data = open(iso, 'rb').read(64 * 1024 * 1024)  # directory lives near the start
    i = data.find(name.encode() + b';1')
    if i < 0: raise SystemExit('movie not found in ISO directory')
    rec = i - 33
    extent, size = struct.unpack('<I', data[rec + 2:rec + 6])[0], struct.unpack('<I', data[rec + 10:rec + 14])[0]
    with open(iso, 'rb') as f, open(out, 'wb') as o:
        f.seek(extent * 2048); left = size
        while left:
            chunk = f.read(min(left, 1 << 20)); o.write(chunk); left -= len(chunk)

def psnr(a, b):
    se = sum((x - y) * (x - y) for x, y in zip(a, b))
    return 99.0 if not se else 10 * math.log10(255 * 255 * len(a) / se)

def main():
    iso, movie, out = sys.argv[1], sys.argv[2], sys.argv[3]
    limit = int(sys.argv[4]) if len(sys.argv) > 4 else 60
    os.makedirs(out, exist_ok=True)
    pss = os.path.join(out, movie.lower()); extract(iso, movie, pss)
    exe = os.path.join(out, 'pss_check.exe')
    cc = shutil.which('gcc') or shutil.which('clang') or r'C:\msys64\clang64\bin\clang.exe'
    subprocess.run([cc, '-O2', '-I', os.path.join(ROOT, 'include'), os.path.join(ROOT, 'tools', 'pss_check.c'),
                    os.path.join(ROOT, 'src', 'core', 'pss.c'), os.path.join(ROOT, 'src', 'core', 'mpeg2_intra.c'), '-o', exe], check=True)
    ours, wav = os.path.join(out, 'ours.yuv'), os.path.join(out, 'ours.wav')
    print(subprocess.run([exe, pss, ours, wav, str(limit)], capture_output=True, text=True, check=True).stdout)
    ref = os.path.join(out, 'ffmpeg.yuv')
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', pss, '-frames:v', str(limit), '-f', 'rawvideo', '-pix_fmt', 'yuv420p', ref], check=True)
    w, h = 640, 448; size = w * h * 3 // 2
    a, b = open(ours, 'rb').read(), open(ref, 'rb').read()
    n = min(len(a), len(b)) // size
    print('pictures compared', n, 'ours', len(a) // size, 'ffmpeg', len(b) // size)
    worst = [99.0, 99.0, 99.0]
    for k in range(n):
        fa, fb = a[k * size:(k + 1) * size], b[k * size:(k + 1) * size]
        planes = [(0, w * h), (w * h, w * h // 4), (w * h * 5 // 4, w * h // 4)]
        values = [psnr(fa[o:o + s], fb[o:o + s]) for o, s in planes]
        worst = [min(x, y) for x, y in zip(worst, values)]
        if k < 3 or k == n - 1: print('picture %d PSNR Y %.2f U %.2f V %.2f' % (k, *values))
    print('worst PSNR Y %.2f U %.2f V %.2f' % tuple(worst))
    # FFmpeg does not read the PSS audio substream (0xFF), so the audio is
    # checked on its own: its length against the pictures and that the
    # samples form a smooth waveform (noise would mean a wrong layout).
    raw = open(wav, 'rb').read()[44:]
    frames = len(raw) // 4
    s = struct.unpack('<%dh' % (frames * 2), raw[:frames * 4])
    step = sum(abs(s[i + 2] - s[i]) for i in range(0, min(len(s) - 2, 400000), 2)) / max(1, min(len(s) - 2, 400000) // 2)
    peak = max(map(abs, s)) if s else 0
    print('audio %.2f s for %.2f s of pictures; mean step %.1f, peak %d' % (frames / 48000.0, len(a) // size / 30.0, step, peak))

if __name__ == '__main__':
    main()
