# Native renderer test status

## Host checks

Run from `android-project/`:

```bash
python3 tests/direct-jni-engine-smoke.py
python3 tests/native-renderer-integration-smoke.py
python3 tests/android-native-bootstrap-contract.py
node tests/editor-runtime-smoke.cjs
node tests/schema-and-platform-tests.cjs
python3 tests/android-package-contract.py
```

The direct-JNI test builds the JNI translation unit and embedded C++ engine into a host shared library, calls the same exported C ABI used by JNI, stages media, renders an MP4, and checks the result with FFprobe. The renderer regression test also covers PIP, basic color adjustment, audio automation, unsupported-value rejection, and 96 kHz/24-bit mono WAV.

## Android build

The Gradle tasks prepare Boost headers, verify the FFmpeg 7.1.1 source signature, build the FFmpeg/FFprobe Android executables, and compile the JNI shared library with the Android NDK. The Android cross-build and APK packaging have **not** been run in this environment. Device installation, JNI load, FFmpeg execution, MP4 export, and audio-only export remain required verification.
