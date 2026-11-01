#!/bin/sh
# Fetches the sherpa-onnx C API (headers + shared libs, onnxruntime included)
# that runs Parakeet for dictation. Apache-2.0, https://github.com/k2-fsa/sherpa-onnx
#
# The archive is checked against a pinned SHA-256 before it is unpacked: these
# libraries end up inside Lumo, so a swapped download must fail the build.
# Bumping VERSION means updating SHA256 too (sha256sum the new archive).
set -eu
VERSION=1.13.8
SHA256=c0bdb7907d3a74bba1d55d22bf4d9fa75586cf1530614ebe88a27b9118e015c4
NAME=sherpa-onnx-v$VERSION-linux-x64-shared
DEST="$(dirname "$0")/../third_party"
[ -d "$DEST/$NAME" ] && exit 0
mkdir -p "$DEST"
ARCHIVE="$DEST/$NAME.tar.bz2"
curl -fL "https://github.com/k2-fsa/sherpa-onnx/releases/download/v$VERSION/$NAME.tar.bz2" -o "$ARCHIVE"
if ! echo "$SHA256  $ARCHIVE" | sha256sum -c - >/dev/null 2>&1; then
    echo "fetch-sherpa: checksum mismatch for $NAME.tar.bz2 (expected $SHA256)" >&2
    rm -f "$ARCHIVE"
    exit 1
fi
tar -xjf "$ARCHIVE" -C "$DEST"
rm "$ARCHIVE"
