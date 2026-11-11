# AppMint build requirements — existing Hero Vid Studios 0.5.14 project

## Critical project-root instruction

The root of this ZIP is the Android project. Build/import the directory containing `settings.gradle`, `build.gradle`, `app/`, `native-engine/`, and `build-environment/`. Do **not** select `web-preview/index.html`, and do not search for or expect a nested `android-project/` directory. The web preview is reference-only.

Do not create a new app, WebView-only wrapper, demo, placeholder, or replacement project. Update the existing Hero Vid Studios application while preserving its identity, editor, timeline, project format, media access, and editing behavior.

## Application identity

- `applicationId` / namespace: `com.webapp.herostudios`
- `versionName`: `0.5.14`
- `versionCode`: `15`
- `minSdk`: 26; compile/target SDK: 35
- Launcher editor: `file:///android_asset/editor/index.html`

## Required builder capabilities

The AppMint build environment must supply JDK 17, Gradle 8.9, Android SDK platform 35, Android NDK 27.2.12479018, and CMake 3.22.1. It also needs network access for pinned Boost headers, the FFmpeg 7.1.1 source archive, and FFmpeg signature verification. These are build-service requirements; the phone user should not need to install or operate these tools manually.

The build must execute the Gradle tasks in `app/build.gradle`. They prepare Boost, verify the upstream FFmpeg source signature, build FFmpeg and FFprobe for `arm64-v8a`, compile `libhero_runtime.so` from the JNI bridge plus C++ timeline engine, and package all three native payloads into the APK.

Do not replace the C++ renderer with browser MediaRecorder, a WebView-only wrapper, a `ProcessBuilder` renderer bridge, or an unrelated cloud-only renderer. The JavaScript editor is the control surface; media rendering runs in the native C++ engine.

## Mandatory build and package checks

Run `bash build-environment/scripts/build-android.sh` from the project root. It runs editor/schema/JNI/engine tests, builds debug and release APKs, then runs `tests/verify-android-native-executables.sh`. That verifier must confirm that `libhero_runtime.so`, `libhero_ffmpeg.so`, and `libhero_ffprobe.so` are present in each APK under `lib/arm64-v8a/`, and that the FFmpeg/FFprobe payloads are Android PIE executables.

A build is incomplete and must fail if the native renderer or either FFmpeg tool is missing. Do not produce a web-only APK and call it complete.

## Device acceptance still required

After the native APK build succeeds, install on a physical Android device and verify: app launch and preserved editor features; JNI health handshake; local multi-clip MP4 render; supported audio-only render; exact requested output metadata; and saving to a user-accessible MediaStore location. Confirm the signing certificate separately before claiming in-place update compatibility.

## Existing renderer limitations (do not conceal)

Split-screen layout, native progress/cancellation, and cloud rendering are not implemented in this increment. The Android video path selects the requested MediaCodec encoder explicitly; a device that lacks that encoder must return a clear error, never silently substitute a different codec or quality setting. The FFmpeg build is LGPL-oriented and excludes GPL/nonfree external codec libraries.
