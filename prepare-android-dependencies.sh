#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BOOST_VERSION=1.86.0
BOOST_DIR="$ROOT/third_party/boost"
if [ -f "$BOOST_DIR/boost/property_tree/json_parser.hpp" ]; then exit 0; fi
mkdir -p "$ROOT/third_party" "${TMPDIR:-/tmp}/hero-boost"
ARCHIVE="${TMPDIR:-/tmp}/hero-boost/boost_${BOOST_VERSION//./_}.tar.bz2"
curl --fail --location --retry 3 "https://archives.boost.io/release/$BOOST_VERSION/source/boost_${BOOST_VERSION//./_}.tar.bz2" -o "$ARCHIVE"
mkdir -p "$BOOST_DIR"
tar -xjf "$ARCHIVE" --strip-components=1 -C "$BOOST_DIR"
