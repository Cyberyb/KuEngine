# VolumetricIntegration example

An independent KuEngine example of [SebH's Shadertoy XlBSRz](https://www.shadertoy.com/view/XlBSRz).
Read [NOTICE.md](NOTICE.md) for source, license and the replacement noise difference.

From the repository root:

```powershell
cmake --preset vs2022-vcpkg
cmake --build --preset debug --target VolumetricIntegrationApp
Push-Location build/cmake-tools/vs2022-vcpkg/bin/Debug
./VolumetricIntegrationApp.exe
Pop-Location
```

Or run `run_volumetric_integration.bat [Debug|Release]`. It uses the preset's
build directory; set `KUENGINE_BUILD_DIR` to select a different build directory.
Run the executable from its own directory so relative SPIR-V paths resolve.

The sidebar exposes pause, time, analytical/basic/transmittance-first/split
integration, fog density, strong height fog, light multiplier, view/shadow steps,
noise, volumetric shadows and reference reset. Split shows basic integration on
the left and analytical integration on the right. Strong fog makes the difference
easier to see. Reset restores original default parameters and animated time zero.

Example-local CLI options (all may be combined with public Runtime smoke options):

| Option | Default / allowed value |
|---|---|
| `--volumetric-time T` | Fixed time, 0..1000000 seconds; otherwise animated |
| `--volumetric-time-step T` | 0..100 seconds per actual submitted frame, requires fixed time |
| `--volumetric-integration MODE` | `improved`, `basic`, `transmittance-first`, `split` |
| `--volumetric-density N` | 1 / 0..20 |
| `--volumetric-light N` | 1 / 0..10 |
| `--volumetric-strong-fog N` | 0 / 0..10 |
| `--volumetric-steps N` | 100 / integer 16..256 |
| `--volumetric-shadow-steps N` | 16 / integer 1..64 |
| `--volumetric-no-noise` | Disable height-fog noise |
| `--volumetric-no-shadows` | Disable medium shadow integration |
| `--volumetric-readback-prefix PATH` | Optional GPU evidence output; requires positive `--smoke-frames` |

For reproducible GPU evidence:

```powershell
./VolumetricIntegrationApp.exe --smoke-frames 4 --frames-in-flight 2 `
  --volumetric-time 1.25 --volumetric-readback-prefix C:/temp/volumetric/reference
```

Every actual submitted frame writes a cropped scene `.ppm` (original gamma),
little-endian interleaved RGBA half-float `.rgba16f` (linear radiance), and `.txt`
metadata containing dimensions, actual submission, slot, time, parameters, hash
and radiance sum. Pending captures are inspected after their own slot fence;
resize/recompile/destruction drain all slots, including the last captures.
Capture errors fail the application even if detected during destruction.
The optional staging buffers support at most 2048x2048 total attachment pixels;
ordinary interactive rendering has no such example-local readback limit.

The pass graph is `VolumetricIntegration → VolumetricDisplay → [VolumetricReadback]`.
The scene uses a per-frame Graph-owned RGBA16F image. A fullscreen triangle and
48-byte push constants produce radiance; display uses slot-specific descriptors
and performs the reference's single gamma conversion for UNORM or sRGB output.
No common PBR shaders, Runtime frame loop or swapchain implementation are copied.
