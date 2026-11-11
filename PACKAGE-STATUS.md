# Package 0.7.3 status

- Source project layout: corrected so Android Gradle/CMake project is at ZIP root.
- Direct JNI bridge and C++ timeline renderer: retained, not replaced.
- Android FFmpeg/FFprobe build tasks: retained; build output must package both PIE executables.
- Host-side tests executed in this session: all passed (editor, schema/platform, JNI envelope, package/root contract, native bootstrap, direct-JNI shared-engine smoke, MP4/WAV renderer integration).
- Android SDK/NDK/Gradle build: not run here; Android toolchain is not installed and outbound network resolution is unavailable in this environment.
- APK/physical-device export: not verified; no APK is included in this source package.

The purpose of this package revision is to ensure AppMint sees the native Android project as its build root instead of mistaking the top-level web preview for the app. A successful AppMint build must still show the three required native payloads in both APKs before device testing.
