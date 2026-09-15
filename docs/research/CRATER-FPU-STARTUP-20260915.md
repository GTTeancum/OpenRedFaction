# Original CRT x87 precision setup — 2026-09-15

`tools/verify_geomod_fpu_startup.py` executes unmodified original5779e9 with its complete control-word conversion callees. Four starting control words pass; JSON retains before/after values. Same RF.exe SHA-256 as CRATER-CENTER-QUANTIZATION-20260915.md.

The static startup chain is:

1. PE entry5760c3 calls575d8e at576168.
2. 575d8e reads5ac0e4; the executable image stores5730d0 there and calls it at575d97.
3. 5730d0 calls5779e9 at5730df.
4. 5779e9 pushes mask0x30000 and value0x10000, then calls5803cb at5779f3.
5. Executed5803cb and its callees set the hardware x87 precision bits to0x200: **53-bit significand precision**.

Observed initial037f,027f and007f all become023f; initial0f7f becomes0e3f. The conversion also clears reserved bit6. Numerically this supports the usual027f oracle setting;023f has the same precision and rounding controls. The original startup call changes precision while preserving rounding mode.

A linear executable-section disassembly audit found only5779f3 directly calling5803cb and only5730df directly calling5779e9. This is not an exhaustive proof against indirect calls or external DLL/graphics code changing the FPU state later. The entire process startup and a live crater call were not run here.

Recommendation: target **53-bit** arithmetic for crater-center encoding, as supported by original startup. The earlier quantization report's64-bit fixtures deliberately exposed mode dependence and should not define the default runtime policy. Its explicit027f comparison gives(-500,0,500) in bounds[-1000,1000] as(16384,32768,49152), whereas037f yields the three one-less codes. Retain a documented numerical policy rather than depending on whichever host/compiler mode happens to be active.
