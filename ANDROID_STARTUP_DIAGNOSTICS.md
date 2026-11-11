# Android startup hardening — 0.5.4

The Android launcher now checks for `assets/editor/index.html` before opening WebView. If the asset is missing or the main document fails to load, the app presents a Hero-branded diagnostic screen with a retry action. The message explicitly distinguishes a missing bundled editor/startup failure from a missing website.

Top-level WebView navigation is limited to `file:///android_asset/` and in-document `about:`, `data:`, and `blob:` URLs. Remote website navigation remains blocked. The launcher records JavaScript console messages under the `HeroVidStudios` Android log tag.

This is a startup reliability and diagnosis improvement. It does not transform the HTML editor into a native editor, and it does not connect the C++ renderer to Android. Those require a real Android NDK/JNI port, Android-compatible FFmpeg libraries/binaries, host-side media staging, output access handling, and device testing.
