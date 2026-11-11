#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
: "${ANDROID_HOME:=${ANDROID_SDK_ROOT:-}}"
if [[ -z "$ANDROID_HOME" || ! -d "$ANDROID_HOME/platforms/android-35" ]]; then
  echo 'Android SDK API 35 is missing. Use Dockerfile.android-build or the matching CI workflow.' >&2
  exit 2
fi
if ! command -v gradle >/dev/null 2>&1; then
  echo 'Gradle 8.9 is missing from the AppMint build environment. Use Dockerfile.android-build or the matching CI workflow.' >&2
  exit 2
fi
if ! gradle --version | grep -qE 'Gradle 8\.9([[:space:]]|$)'; then
  echo 'This project requires Gradle 8.9 in the AppMint build environment.' >&2
  exit 2
fi
# Fail early if the importer selected a web preview or a nested source folder.
python3 tests/appmint-project-root-contract.py
# Exercise the exact editor/host/native contracts before building Android binaries.
node tests/editor-runtime-smoke.cjs
node tests/schema-and-platform-tests.cjs
node tests/android-jni-envelope-regression.cjs
python3 tests/android-package-contract.py
python3 tests/android-native-bootstrap-contract.py
python3 tests/direct-jni-engine-smoke.py
python3 tests/native-renderer-integration-smoke.py

gradle --no-daemon --stacktrace clean assembleDebug assembleRelease
# Never publish an APK that omitted any of the native execution pieces.
bash tests/verify-android-native-executables.sh
mkdir -p build-environment/output
cp app/build/outputs/apk/debug/app-debug.apk build-environment/output/Hero-Vid-Studios-debug.apk
if [[ -f app/build/outputs/apk/release/app-release-unsigned.apk ]]; then
  cp app/build/outputs/apk/release/app-release-unsigned.apk build-environment/output/Hero-Vid-Studios-release-unsigned.apk
fi
sha256sum build-environment/output/*.apk | tee build-environment/output/SHA256SUMS.txt
printf '\nAndroid APK build and packaged-native-payload checks completed. Physical-device export and installed-app signing compatibility remain unverified.\n'
