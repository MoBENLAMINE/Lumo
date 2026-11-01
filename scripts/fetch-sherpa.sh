#!/bin/sh
# Fetches the sherpa-onnx C API (headers + shared libs, onnxruntime included)
# that runs Parakeet for dictation. Apache-2.0, https://github.com/k2-fsa/sherpa-onnx
set -eu
VERSION=1.13.8
NAME=sherpa-onnx-v$VERSION-linux-x64-shared
DEST="$(dirname "$0")/../third_party"
[ -d "$DEST/$NAME" ] && exit 0
mkdir -p "$DEST"
curl -fL "https://github.com/k2-fsa/sherpa-onnx/releases/download/v$VERSION/$NAME.tar.bz2" -o "$DEST/$NAME.tar.bz2"
tar -xjf "$DEST/$NAME.tar.bz2" -C "$DEST"
rm "$DEST/$NAME.tar.bz2"
