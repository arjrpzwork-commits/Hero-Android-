# Hero native timeline engine

The C++20 engine implements the Hero timeline protocol and delegates media encoding/probing to explicitly selected FFmpeg/FFprobe tools. The desktop/host target remains a standalone executable for development and regression tests.

For Android, the same `Engine` class is compiled into `libhero_runtime.so` with `HERO_ANDROID_EMBEDDED=1`. The exported `hero_engine_call_json` C ABI receives the Java JNI calls in-process; there is no standalone Android C++ renderer process. The engine stores projects and staged media under the Android app cache directory and invokes packaged FFmpeg/FFprobe PIE tools by absolute path using `fork`/`execv`.

Android NDK compilation, APK packaging, and physical-device execution are separate verification steps and are not claimed by host tests.
