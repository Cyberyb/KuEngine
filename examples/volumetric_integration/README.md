# VolumetricIntegration

Independent KuEngine adaptation of [SebH's Shadertoy XlBSRz](https://www.shadertoy.com/view/XlBSRz).

Read [NOTICE.md](NOTICE.md) for the CC BY-NC-SA 3.0 source/license and fixed-camera/replacement-noise differences.

Build/run, all local CLI options, reference display behavior and bounded RGBA16F/PPM readback are maintained in the [example manual](../../docs/usage/examples.md#volumetric). Common environment and Runtime smoke options are in [development and running](../../docs/usage/README.md).

From the repository root:

```powershell
./examples/volumetric_integration/run_volumetric_integration.bat Debug
```

The script defaults to the CMake preset build directory; set KUENGINE_BUILD_DIR for another absolute build directory. Direct executable startup requires its binary directory as the working directory.
