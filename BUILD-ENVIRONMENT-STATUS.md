# Build environment status

The Android source now uses a direct JNI call into a C++ timeline engine compiled into `libhero_runtime.so`. The FFmpeg and FFprobe tools are packaged separately in the APK native-library directory and invoked by the C++ engine using absolute paths.

Verified on the host: direct-JNI shared-library compilation and render test; C++ renderer MP4/WAV regression tests; editor runtime checks; schema/platform checks; Android package and architecture contract tests.

Not verified: Android NDK cross-compilation, Gradle APK build, APK native-library packaging, installation on Android, JNI load on device, native FFmpeg process launch on device, and on-device export. These must be confirmed by the Android build workflow and a real device test. The full source tree still needs to be pushed to the user's GitHub repository before that workflow can run there.
