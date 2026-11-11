#!/usr/bin/env bash
set -euo pipefail

SDK_ROOT="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-}}"
: "${ANDROID_NDK_HOME:=${SDK_ROOT}/ndk/27.2.12479018}"
: "${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME to the installed Android NDK}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ABI="${ANDROID_ABI:-arm64-v8a}"
API="${ANDROID_API:-26}"
case "$ABI" in arm64-v8a) ARCH=aarch64; CPU=armv8-a; TRIPLE=aarch64-linux-android ;; *) echo "Unsupported ABI in this build profile: $ABI" >&2; exit 2 ;; esac
HOST="linux-x86_64"
TOOL="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/$HOST/bin"
WORK="${HERO_BUILD_TMP:-${TMPDIR:-/tmp}/hero-ffmpeg-android-$ABI}"
DEST="$ROOT/app/src/main/jniLibs/$ABI"
LICENSE_DEST="$ROOT/app/src/main/assets/native"
VERSION="7.1.1"
mkdir -p "$WORK" "$DEST" "$LICENSE_DEST"
if [[ -x "$DEST/libhero_ffmpeg.so" && -x "$DEST/libhero_ffprobe.so" && -f "$LICENSE_DEST/FFmpeg-BUILD-$ABI.txt" ]] && grep -q "version=$VERSION" "$LICENSE_DEST/FFmpeg-BUILD-$ABI.txt"; then
  echo "Using verified cached FFmpeg $VERSION for $ABI"
  exit 0
fi
cd "$WORK"
ARCHIVE="ffmpeg-$VERSION.tar.xz"
if [ ! -f "$ARCHIVE" ]; then curl --fail --location --retry 3 "https://ffmpeg.org/releases/$ARCHIVE" -o "$ARCHIVE"; fi
if [ ! -f "$ARCHIVE.asc" ]; then curl --fail --location --retry 3 "https://ffmpeg.org/releases/$ARCHIVE.asc" -o "$ARCHIVE.asc"; fi
export GNUPGHOME="$WORK/gnupg"
mkdir -p "$GNUPGHOME"
chmod 700 "$GNUPGHOME"
if ! gpg --batch --list-keys FCF986EA15E6E293A5644F10B4322F04D67658D8 >/dev/null 2>&1; then
  curl --fail --location --retry 3 https://ffmpeg.org/ffmpeg-devel.asc | gpg --batch --import
fi
gpg --batch --verify "$ARCHIVE.asc" "$ARCHIVE"
ACTUAL="$(sha256sum "$ARCHIVE" | awk '{print $1}')"
rm -rf "ffmpeg-$VERSION" out
mkdir "ffmpeg-$VERSION"
tar -xf "$ARCHIVE" --strip-components=1 -C "ffmpeg-$VERSION"
cd "ffmpeg-$VERSION"
export CC="$TOOL/${TRIPLE}${API}-clang"
export CXX="$TOOL/${TRIPLE}${API}-clang++"
export AR="$TOOL/llvm-ar" RANLIB="$TOOL/llvm-ranlib" STRIP="$TOOL/llvm-strip"
SYSROOT="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/$HOST/sysroot"
./configure \
  --prefix="$WORK/out" --target-os=android --arch="$ARCH" --cpu="$CPU" \
  --enable-cross-compile --cc="$CC" --cxx="$CXX" --ar="$AR" --ranlib="$RANLIB" \
  --strip="$STRIP" --sysroot="$SYSROOT" --enable-pic \
  --enable-static --disable-shared --disable-doc --disable-debug --disable-network \
  --disable-autodetect --disable-gpl --disable-version3 --disable-nonfree \
  --enable-jni --enable-mediacodec --extra-ldflags="-landroid -lmediandk -llog -pie"
make -j"$(getconf _NPROCESSORS_ONLN)"
make install
install -m 0755 "$WORK/out/bin/ffmpeg" "$DEST/libhero_ffmpeg.so"
install -m 0755 "$WORK/out/bin/ffprobe" "$DEST/libhero_ffprobe.so"
cp COPYING.LGPLv2.1 "$LICENSE_DEST/FFmpeg-COPYING-LGPLv2.1"
cp COPYING.LGPLv3 "$LICENSE_DEST/FFmpeg-COPYING-LGPLv3" 2>/dev/null || true
for executable in "$DEST/libhero_ffmpeg.so" "$DEST/libhero_ffprobe.so"; do
  "$TOOL/llvm-readelf" -h "$executable" | grep -Eq 'Type:.*DYN'
  "$TOOL/llvm-readelf" -l "$executable" | grep -q 'INTERP'
done
{
  echo "version=$VERSION"
  echo "abi=$ABI"
  echo "android_api=$API"
  echo "source_sha256=$ACTUAL"
  echo "packaging=nativeLibraryDir-executable"
  grep '^CONFIGURE_LINE=' ffbuild/config.mak | sed 's/^CONFIGURE_LINE=//'
} > "$LICENSE_DEST/FFmpeg-BUILD-$ABI.txt"
printf 'Built FFmpeg %s for %s (LGPL configuration, Android MediaCodec enabled).\n' "$VERSION" "$ABI"
