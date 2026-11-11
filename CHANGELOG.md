# Changelog

## 0.5.4 — Android startup hardening
- Retained the existing installed-app identity: `com.webapp.herostudios`.
- Increased Android version code to 5 and version name to 0.5.4.
- Added a preflight check that the bundled editor entry asset exists before attempting startup.
- Added an on-device diagnostic/retry screen for main-frame WebView startup failures instead of leaving users with Android's generic "Webpage not available" screen.
- Restricted top-level navigation to bundled editor assets and in-document URLs; remote website navigation is blocked.
- Added WebView console diagnostics to Android log output.
- Preserved the existing editor HTML, project schema, and C++ engine source unchanged.

## Known incomplete work
- Android APK compilation and installation/update remain unverified in this environment.
- The C++ engine is Linux/POSIX-only and is not connected to Android through JNI/NDK.
- This is not yet the requested fully native editing/rendering application.

## Android JNI bootstrap (engineering increment)
- Added an Android NDK/CMake shared-library target and Java JNI runtime handshake.
- Exposed read-only native runtime diagnostics to the bundled local editor WebView.
- Kept native rendering explicitly unavailable until an Android media backend and the full render bridge are implemented; no export capability is claimed by this increment.


## 0.5.5 — Android native source staging
- Advanced Android version code to 6 and version name to 0.5.5; retained `com.webapp.herostudios`.
- Expanded the JNI library with native project-directory creation and ordered source-media chunk staging.
- Added input validation, safe project/source identifiers, 1 MiB chunk bounds, sequence enforcement, declared-size enforcement, and final file-size verification.
- Exposed the staging API through the existing local `HeroRuntime` JavaScript interface.
- Kept rendering fail-closed: `mediaRendererAvailable` remains false; no native timeline renderer is claimed.
- Added JNI staging success, sequence rejection, path validation, and incomplete-session regression coverage.


## 0.5.6 — Android native media probing
- Advanced Android version code to 7 and version name to 0.5.6; retained the existing application ID.
- Added an Android Media NDK (`AMediaExtractor`) probe for finalized staged sources. It reports stream MIME types, duration, dimensions, sample rates and channel counts.
- Linked the Android `mediandk` library in the NDK target.
- Kept rendering unavailable until native timeline decode/encode, composition, audio mixing and verified output are implemented.
- Host tests continue to reject false claims of Android media probing; Android Media NDK execution still requires an Android NDK build and device validation.


## 0.5.7 — Android bridge handshake correction
- Advanced Android version code to 8 and version name to 0.5.7; retained the existing application ID.
- Corrected the editor's native bridge detection to use the actual injected `HeroRuntime` interface instead of the nonexistent `HeroNative` interface.
- Connected the editor's engine-status text to native runtime diagnostics, including source staging and Android media-probe capability.
- Kept Native Engine Export disabled unless a real `renderProject` implementation exists and the native runtime explicitly reports rendering available.
- Added a regression contract test for the JavaScript-to-Java bridge name and fail-closed export state.
- No native timeline rendering or Android APK build is claimed.


## 0.5.8 — Timeline track controls and audio solo
- Added per-track active selection controls directly in the timeline gutter for fast phone/tablet operation.
- Added per-audio-track mute and solo controls; solo is exclusive across audio tracks while preserving each track's mute state.
- Added clear active-track styling and accessible labels/pressed states for track controls.
- Persisted audio solo state in project snapshots and restore; extended project schema validation for solo arrays.
- Kept video/audio rendering and export capability claims unchanged.


## 0.5.9 — Keyboard editing workflow
- Advanced Android package version to 0.5.9 / version code 10 without changing the application ID.
- Added keyboard shortcuts for undo/redo, deleting the selected clip, frame nudging, and splitting at the playhead.
- Shortcuts ignore text-entry controls and content-editable fields so typing and form editing are not intercepted.
- Updated Quick Edit help text to document the shortcuts.
- No change to native rendering availability or export claims.

## 0.5.10 — Snap-to-grid control
- Advanced Android package version to 0.5.10 / version code 11 without changing the application ID.
- Replaced the small checkbox with a clear touch-friendly Snap to Grid ON/OFF button.
- Enabled mode snaps clip movement, trims, and frame nudges to the current project frame grid; disabled mode allows free placement.
- Added accessible pressed-state reporting and explicit UI status feedback.
- Added runtime regression checks for ON/OFF state and quantized versus free placement.


## 0.5.11 — Browser render preflight and honest codec selection
- Added an explicit browser export capability check against the exact requested container/video/audio codec combination.
- Browser preview export now refuses unsupported combinations instead of silently substituting a different container or codec.
- Exported filename extension follows the actual selected/supported container; configured audio bitrate is passed to MediaRecorder.
- Added an in-editor preflight report showing the actual MIME type and a clear warning that browser capture is real-time and not native/offline rendering.
- Native Engine Export remains disabled until a real Android renderer is connected and verified.


## 0.5.12 — Standalone FFmpeg export direction restored
- Removed browser MediaRecorder video export, browser WAV export, and browser export preflight controls from the editor.
- Kept browser playback as preview only; it does not produce export files.
- Renamed the export action to FFmpeg Engine Export and made its unavailable state explicit until the Android native host is connected.
- Confirmed the existing standalone C++ FFmpeg engine builds on the Linux development host and passes offline MP4 H.264/AAC plus 96 kHz / 24-bit PCM WAV smoke tests with ffprobe metadata verification.
- No Android FFmpeg integration, APK build, or device test is claimed.


## 0.5.13 — Android native renderer bridge integration
- Connected the actual Android launcher editor to the C++ timeline renderer through the Java `HeroRuntime` host and a Promise-compatible `HeroDesktop` adapter.
- Corrected native executable packaging for Android 10+: PIE renderer/FFmpeg/FFprobe executables are packaged under `jniLibs` and launched from `nativeLibraryDir`, never copied to writable app storage.
- Corrected Gradle task working directories and added signed FFmpeg source verification.
- Added track-level mute/solo/gain, audio automation, PIP and basic color processing to the C++ renderer; unsupported split-screen fails explicitly.
- Removed the arbitrary 25-track limit and the forced stereo conversion.
- Added host integration tests and GitHub Actions Android build/package checks.
- Host tests pass. Android NDK/Gradle build and device execution remain pending CI/device verification.

## 0.5.13 bridge integration — direct JNI revision

- Replaced the Java `ProcessBuilder` renderer launch with an in-process JNI call into the C++ timeline engine linked into `libhero_runtime.so`.
- Added an embedded-engine C ABI for JSON-RPC dispatch and kept source staging, project state, rendering, and FFprobe verification inside the C++ engine.
- Moved renderer working files explicitly into the app cache directory.
- Changed FFmpeg process startup to resolve the executable path before `fork` and use `execv` in the child, avoiding a standalone renderer executable and avoiding post-fork C++ allocation.
- Added a host test that builds the JNI-linked shared library and exercises a full direct-call render.
- Android NDK/APK/device verification remains pending; no device-build claim is made.


## 0.5.14 — JNI host and storage-policy correction
- Advanced versionCode to 15 and versionName to 0.5.14; preserved the existing application ID.
- Fixed the missing `HeroNativeRuntime.close()` method called during Activity teardown, which otherwise prevents Java compilation.
- Required packaged FFmpeg and FFprobe files to exist and be executable before advertising native rendering.
- Storage policy: do not disable export or warn based on generic free-space estimates. Report insufficient storage only after an actual write reports a no-space condition.
- Re-ran host C++ JNI rendering, MP4/WAV integration, schema, editor, and Android package-contract tests. Android NDK/Gradle APK build and physical-device export remain outstanding.

## 0.7.2 — Android JNI response-envelope correction
- Fixed the Android JavaScript adapter to unwrap the C++ JSON-RPC `result` object before returning it to the editor.
- Propagated the C++ JSON-RPC `error.message` so real renderer failures are visible instead of being mistaken for successful calls.
- Added regression coverage for `beginProject` result fields, `renderProject` output paths, and native error propagation.
- Kept the existing Android app identity, UI, project format, and native C++ rendering architecture unchanged.
- Confirmed that Android FFmpeg/FFprobe PIE executables are generated by the NDK build task; they are not prebuilt in this source ZIP. Physical-device export remains unverified.
