# VolumetricIntegration attribution

`shaders/volumetric.frag` is adapted from **VolumetricIntegration**, by **SebH
(Sébastien Hillaire)**, published on Shadertoy in 2015:
<https://www.shadertoy.com/view/XlBSRz>.

The author's original introductory comment and algorithm comments are retained.
The source describes the improved analytical scattering integration presented
in the author's Frostbite SIGGRAPH 2015 presentation (slide 28):
<https://www.frostbite.com/2015/08/physically-based-unified-volumetric-rendering-in-frostbite/>.

The original site is protected by Cloudflare and returned HTTP 403 during this
port. The exact shader source and metadata were recovered from the public
[Vipitis/shadertoys-dataset archive](https://github.com/Vipitis/shadertoys-dataset/blob/7a5d34083b03501a4d42238bbe65183b0b531f00/data/annotated/20k_2015-08.jsonl),
commit `7a5d34083b03501a4d42238bbe65183b0b531f00`, record `XlBSRz`, retrieved by the
archive on 2021-10-01. The decoded original GLSL SHA-256 is
`696eda6d9c36df3c053b411eb25a0e3308243e46bbef54fccf830cebb32cedb9`.

The shader record specifies **CC BY-NC-SA 3.0**:
<https://creativecommons.org/licenses/by-nc-sa/3.0/> (legal text:
<https://creativecommons.org/licenses/by-nc-sa/3.0/legalcode>). This adaptation
of the shader is distributed under that same license. Attribution, noncommercial
use and ShareAlike conditions apply to it. The dataset's software license is
not a replacement for the shader's license. No endorsement by the author is implied.

KuEngine adaptation, 2026-10-24:

- Port the original default `D_DEMO_FREE` scene, camera, moving inverse-square
  point light, isotropic scattering, height fog, spherical fog, distance-limited
  view march and 16-step volumetric shadow integration to Vulkan GLSL 450.
- Replace Shadertoy uniforms with push constants and expose the original
  integration choices and medium/quality controls through UI and CLI.
- Replace inaccessible `iChannel0` with a self-contained deterministic 256x256
  hash grid with bilinear filtering and repeat. The original texture URL is
  <https://www.shadertoy.com/media/a/0a40562379b63dfb89227e6d172f39fdce9022cba76623f1054a2c83d6c0ba5d.png>.
  Its download also returned HTTP 403. The fog's exact fine noise pattern differs;
  this is a scene/algorithm reproduction, not a claim of pixel identity.
- Keep camera `(20,18,-50)` fixed, omit Shadertoy mouse camera displacement,
  invert viewport-local Y for Shadertoy coordinates, and move the original
  `pow(color,1/2.2)` to a separate display pass. No tone mapping is added.
- Guard the point-light singularity and skip surface shading when a reduced
  view-step budget does not hit a wall, preserving finite parameter extremes.

The room has red side walls, green floor and blue back wall, with no ceiling.
The original surface term is Lambert lighting attenuated by the same medium's
shadow transmittance. It has no opaque interior objects or separate opaque
shadow-map pass. This port keeps that scene and shadow model.
