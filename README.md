# Hero Vid Studios — AppMint Android source project (package 0.7.3)

**This ZIP's top level is the Android project root.** `settings.gradle`, `build.gradle`, `app/`, `native-engine/`, `build-environment/`, and `tests/` are at the top level. Import/build this root; do not select `web-preview/` or an HTML file as the app project.

## Existing app identity
- Application ID / namespace: `com.webapp.herostudios`
- Version name: `0.5.14`
- Version code: `15`
- Minimum Android API: 26; compile/target SDK: 35
- App launcher: `app/src/main/java/com/webapp/herostudios/MainActivity.java`
- Bundled editor: `app/src/main/assets/editor/index.html`

## Native rendering path

The existing editor calls the Android Java host, which calls the C++ timeline engine in-process through JNI (`libhero_runtime.so`). There is no `ProcessBuilder` renderer bridge and no browser `MediaRecorder` export fallback. The C++ engine stages original media, composes the timeline, invokes the packaged Android FFmpeg/FFprobe PIE executables, verifies output metadata, and returns the output path to the Java host for MediaStore saving.

Gradle's `preBuild` tasks prepare Boost, verify the FFmpeg 7.1.1 release signature, cross-build FFmpeg/FFprobe for `arm64-v8a`, and place the executable payloads in `app/src/main/jniLibs/arm64-v8a/`. CMake/NDK compiles the C++ JNI library. The build fails rather than producing an APK without these native dependencies.

## Build environment expected inside AppMint

The build service—not the phone user—must provide network access and JDK 17, Gradle 8.9, Android SDK 35, NDK 27.2.12479018, and CMake 3.22.1. It must run the Android Gradle build from this ZIP's root. See `APP-MINT-BUILD-REQUIREMENTS.md` and `build-environment/README.md`.

The source package is not itself an APK. The build output is not considered complete until `tests/verify-android-native-executables.sh` passes against both APKs. Physical-device export still needs to be tested after installation.

## Validation already run against this source

Host-side editor, schema/platform, JNI-envelope, Android package, direct-JNI C++ engine, and media-render integration tests pass. The Android NDK build, APK packaging, Android install, and physical-device export have **not** been run in this environment; this package does not claim those results.

The optional `renderer-reference-0.2.0/` service is separate reference material. It is not required for local rendering and does not replace the in-app C++ engine.
