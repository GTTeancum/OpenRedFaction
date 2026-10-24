#!/usr/bin/env python3
"""Derive the Xbox fclose correction without changing the installed SDK.

A failed native close leaves retirement uncertain. Cleanup remains once-only;
callers must not retry the freed FILE or its former native handle.
"""
import argparse
import hashlib
from pathlib import Path

SOURCE_SHA256 = "26e58ed5d2ff31ea3575de3bde0eff5b53d3995f7a1fdd8dea77cc25ebc92fbd"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--nxdk-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = args.nxdk_dir / "lib/pdclib/functions/stdio/fclose.c"
    original = source.read_bytes()
    # Windows checkouts may use CRLF. Authenticate normalized source bytes;
    # never edit the installed SDK or relax the expected source content.
    normalized = original.replace(b"\r\n", b"\n")
    if hashlib.sha256(normalized).hexdigest() != SOURCE_SHA256:
        raise SystemExit("Refusing unreviewed PDCLib fclose source")
    text = normalized.decode("utf-8")
    edits = [
        ("    _PDCLIB_LOCK( stream->mtx );", "    int native_close_status;\n    _PDCLIB_LOCK( stream->mtx );"),
        ("    _PDCLIB_close( stream->handle );", "    native_close_status = _PDCLIB_close( stream->handle );"),
        ("    return 0;\n}\n\n#endif", "    return native_close_status ? EOF : 0;\n}\n\n#endif"),
        # fopen creates a kernel mutant per stream (NXDK mtx_init ->
        # NtCreateMutant) and upstream fclose never destroys it: every
        # fopen/fclose pair leaked one handle and ~70 bytes of kernel pool
        # (measured on the console, about 55 handles per level load).
        ("    _PDCLIB_UNLOCK( stream->mtx );\n\n    /* Free stream */",
         "    _PDCLIB_UNLOCK( stream->mtx );\n\n#ifndef __STDC_NO_THREADS__\n"
         "    if ( ! ( stream->status & _PDCLIB_STATIC ) )\n    {\n        mtx_destroy( &stream->mtx );\n    }\n"
         "#endif\n\n    /* Free stream */"),
    ]
    for before, after in edits:
        if text.count(before) != 1:
            raise SystemExit("Refusing ambiguous fclose transformation")
        text = text.replace(before, after)
    output = text.encode("utf-8")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.output.exists():
        if args.output.read_bytes() != output:
            raise SystemExit("Refusing unexpected generated fclose contents")
    else:
        with args.output.open("xb") as handle:
            handle.write(output)


if __name__ == "__main__":
    main()
