#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

echo "== segmenter (C++) =="
cmake -B segmenter/build -S segmenter
cmake --build segmenter/build

echo "== server (Java) =="
(cd server && ./gradlew compileJava)

echo "Build complete."
echo "  segmenter binary: segmenter/build/segmenter"
echo "  server classes:   server/build/classes/java/main"
