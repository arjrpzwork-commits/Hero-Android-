# Android native renderer bridge — direct JNI architecture

## What changed in this increment

- `HeroNativeRuntime.call()` now calls `nativeCall()` directly through JNI. It no longer starts a standalone C++ renderer process with `ProcessBuilder`.
- The C++ timeline engine source is compiled into `libhero_runtime.so` by the Android CMake target. The JNI entry point forwards the existing JSON-RPC methods (`health`, `beginProject`, `beginSource`, `appendSourceChunk`, `finishSource`, `renderProject`) to the same C++ `Engine` instance.
- The engine's working directory is explicitly placed under the app cache directory. Source staging, timeline composition, output probing, and output verification are handled by the C++ engine.
- FFmpeg and FFprobe remain separate Android PIE command-line tools, packaged as `libhero_ffmpeg.so` and `libhero_ffprobe.so` in the APK native-library directory. The C++ engine launches them with `fork`/`execv`; it does not try to execute a binary copied into writable app storage. The child path performs only the required low-level file-descriptor and exec operations after `fork`.
- The output is still copied to Android MediaStore by the Java host after the C++ engine returns verified metadata.

## Verification actually run here

- Direct-JNI host smoke: compiled `hero_runtime_jni.cpp` and the embedded C++ engine together as a shared library; invoked the same C ABI used by JNI; staged MP4/WAV sources; rendered an MP4; and verified H.264/AAC metadata with FFprobe — PASS.
- Native C++ host regression: multi-clip MP4, PIP, color adjustment, audio automation, explicit rejection of unsupported PIP range, and 96 kHz/24-bit mono WAV — PASS.
- Editor runtime, schema/platform, Android package, and direct-JNI architecture contract tests — PASS.

## Still not verified

- Android NDK cross-compilation and Gradle APK packaging have not been run in this environment.
- No APK has been installed on a physical Android device; JNI loading, native-library extraction/permissions, FFmpeg `execv`, and an on-device export therefore remain unverified.
- The current FFmpeg build profile is LGPL-oriented and intentionally does not include GPL/nonfree codec libraries. The engine rejects unavailable codecs rather than silently substituting them.
- Native progress reporting and cancellation are not implemented in this increment. Cloud rendering is also not wired into this bridge.

The 0.5.13 source baseline is preserved separately. This package is a new bridge integration increment, not a claim that an APK or device test has passed.

## 0.7.2 correction from repeated Android export failure

The Android JS adapter previously passed the raw JNI JSON-RPC envelope to the editor. The C++ ABI returns `{ "result": { ... } }` on success and `{ "error": { ... } }` on failure, while the editor expects the operation result directly. This prevented the editor from reading `projectId` and `outputPath`. The adapter now unwraps `result` and propagates the native `error.message`; a regression test covers both paths.

The 0.7.1 ZIP also has no prebuilt Android FFmpeg/FFprobe executables. The Gradle pre-build tasks are responsible for cross-building and packaging them. Without both executable files in `ApplicationInfo.nativeLibraryDir`, the runtime correctly keeps export disabled. This environment has no Android NDK, so it cannot verify or produce those Android binaries here.
