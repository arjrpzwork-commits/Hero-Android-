# Hero Platform Contract

1. Hero owns project semantics, render settings and output acceptance criteria. Android, desktop and cloud are execution hosts only.
2. A host must pass every selected setting unchanged to the renderer and echo the same settings in its completion result.
3. A render is not successful until output exists, has non-zero size, and independent metadata inspection verifies the requested codec, dimensions, frame rate, sample rate and PCM bit depth.
4. Unsupported features or codec/build combinations must fail explicitly. No fallback, normalization, gain change, resampling, bit-depth change, channel-layout change, bitrate substitution, dimension change or frame-rate substitution is permitted without a new user choice.
5. Browser preview/export is a limited fallback only; it is not interchangeable with the independent native renderer.
6. Host adapters must validate project/source IDs, chunk ordering, declared byte counts, staging paths and output paths; never build shell commands from user-controlled strings.
7. Native engine portability requires separately tested host adapters. A Linux executable is never relabelled as Android, Windows or macOS.

## Current platform status
- Linux C++ engine: source build and focused render tests pass in this environment.
- Android app: Gradle project source exists; APK compilation not run; C++ JNI/NDK bridge not integrated.
- Windows/macOS: no native host adapter verified.
- Cloud: not implemented as a render host.


## Device storage behavior

The editor must not disable export or predict a storage failure from a generic device-space heuristic. It may report insufficient storage only when the actual staging, render, or destination write reports a genuine no-space condition (for example, ENOSPC / “No space left on device”). Do not reserve arbitrary large amounts of free space or impose a product-level minimum. Where a remote destination is integrated, the renderer should stream or write to that destination so local free space is not treated as the final-output capacity. A remote upload destination is not implemented by this package and must not be represented as available until its transfer path exists and is tested.
