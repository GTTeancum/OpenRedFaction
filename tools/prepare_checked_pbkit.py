#!/usr/bin/env python3
"""Derive a pbkit.c with a selectable frame-buffer count, without changing the installed SDK.

Upstream pbkit always allocates three colour buffers (triple buffering). At
1280x720x32 each costs 3.5 MiB, which stock 64 MiB cannot spare in the heaviest
levels. The copy adds:
  pb_set_back_buffers(1|2)  call before pb_init; 1 = double buffering
  pb_flips_pending()        frames handed to the VBlank ISR but not yet shown
With one back buffer the caller must not target the next buffer while a flip is
pending, or it would draw into the picture being scanned out. The generated
file keeps the upstream license header; it lives under build/, not in git.
"""
import argparse
import hashlib
from pathlib import Path

SOURCE_SHA256 = "07323e1b5e7c3f3206868ed0b6b91ed3afe445bfc571d628fbd3bceedbba3e2a"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--nxdk-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = args.nxdk_dir / "lib/pbkit/pbkit.c"
    normalized = source.read_bytes().replace(b"\r\n", b"\n")
    if hashlib.sha256(normalized).hexdigest() != SOURCE_SHA256:
        raise SystemExit("Refusing unreviewed pbkit source")
    text = normalized.decode("utf-8")
    edits = [
        ("static  DWORD           pb_FBAddr[3];       //frame buffers addresses\n",
         "static  DWORD           pb_FBAddr[3];       //frame buffers addresses\n"
         "static  DWORD           pb_requested_back_buffers=2; /* OpenRF: 1 = double buffering */\n"
         "void pb_set_back_buffers(int count){pb_requested_back_buffers=count==1?1:2;}\n"
         "int pb_flips_pending(void){return pb_BackBufferbReady[0]+pb_BackBufferbReady[1]+pb_BackBufferbReady[2];}\n"),
        ("    BackBufferCount=2;          //triple buffering technic!\n",
         "    BackBufferCount=pb_requested_back_buffers; //OpenRF: 2 = triple buffering, 1 = double\n"),
        ("    pb_back_index=(pb_back_index+1)%3;\n",
         "    pb_back_index=(pb_back_index+1)%pb_FrameBuffersCount;\n"),
    ]
    for before, after in edits:
        if text.count(before) != 1:
            raise SystemExit("Refusing ambiguous pbkit transformation")
        text = text.replace(before, after)
    output = text.encode("utf-8")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.output.exists():
        if args.output.read_bytes() != output:
            raise SystemExit("Refusing unexpected generated pbkit contents")
    else:
        with args.output.open("xb") as handle:
            handle.write(output)


if __name__ == "__main__":
    main()
