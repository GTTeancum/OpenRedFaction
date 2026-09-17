# Native Xbox crater texture readback

Stock64MiB run `artifacts/xemu/render-20260916-225322` passes59 checks over930
ordinary replay frames. It restores the fixed16-cut checkpoint, walks into the
excavation and returns to the room. PC/Xbox body and checkpoint comparisons
remain exact. Endpoint available pages8090 (31.6015625MiB).

At guest frame3 the harness pauses through QMP, resolves the live
`stream_textures` symbol, reads material49's actual renderer descriptor, and
copies its GPU-visible allocation through native `pmemsave`. It resumes the
same process immediately. There is no host input or desktop capture.

All262144 bytes match the independent swizzle of the PC256x256 RGBA owner.
That PC owner was separately verified against the installed rock02 TGA.
Descriptor142686761 selects swizzled A8B8G8R8,256x256,2D,one mip level,opaque.
Guest pointer2204823552 resolves to physical57339904 within stock RAM.
Expected and actual swizzled SHA256:
`f4ad8635af17ecde55bd1bd054aaa2cbb6dbb78b9313fabcef5d8a5367a88228`.

The opt-in `--terrain-texture-audit` harness switch requires DEV mode and fails
if no live stream frame is observed. `tools/xemu_texture_audit.py` checks image
dimensions, storage count, descriptor and every pixel byte. It relies on the
current32-bit renderer's three-word gpu_texture ABI, documented at the reader.
Two independent rectangular-layout/invalid-input tests pass. No Xbox runtime
instrumentation or extra guest allocation was added. The NXDK rebuild succeeds
with the existing .edata merge warning.

The native endpoint framebuffer was inspected: room wall and floor textures,
weapon/HUD and dark faceted crater material are visible. The crater still has
the ambiguous rock-like silhouette; this is not visual acceptance or a claimed
appearance improvement. Only the endpoint image was inspected, not every frame;
audio was disabled. The owned emulator exited and disc staging was restored.

This closes the CPU-owner-to-GPU-visible-allocation question for this fixture.
It does not trace every submitted texture-state command, prove sample coordinates
per fragment, or prove original rendered parity. Existing original lifecycle
research provides no basis for an unconditional static bake or brightness gain.
Further appearance changes need a demonstrated projection/shading discrepancy;
the dark substrate itself is supported by asset and lighting evidence.
