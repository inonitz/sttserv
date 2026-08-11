#!/bin/bash
# ASR model accuracy + latency sweep. Sits at the workspace root next to build.sh.
#
# Build the test targets first:
#   ./build.sh release static build
#
# Models are read from dependencies/models/, recordings from dependencies/recordings/.
# Per-model logs are written to bench_out/, which is git-ignored: the logs hold
# transcript text and speaker labels and must not be committed.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BIN="$ROOT/build/release/static/bin/accuracy_test"
MODELS="$ROOT/dependencies/models"
OUT="$ROOT/bench_out"
mkdir -p "$OUT"

# name | model file (relative to dependencies/models) | backend | threads
M=(
"parakeet-f32|nvidia--parakeet-tdt-0.6b-v3/ggml-parakeet-tdt-0.6b-v3-f32.bin|whisper-parakeet|2"
"parakeet-f16|nvidia--parakeet-tdt-0.6b-v3/ggml-parakeet-tdt-0.6b-v3-f16.bin|whisper-parakeet|2"
"parakeet-q8|nvidia--parakeet-tdt-0.6b-v3/ggml-parakeet-tdt-0.6b-v3-q8_0.bin|whisper-parakeet|2"
"parakeet-q4|nvidia--parakeet-tdt-0.6b-v3/ggml-parakeet-tdt-0.6b-v3-q4_k.bin|whisper-parakeet|2"
"distil-whisper-q5k|distil-whisper-large-v3.5/ggml-model-q5k.bin|whisper-whisper|1"
"whisper-turbo-q4|xviers-whisper-large-v3-turbo-gguf/whisper-large-v3-turbo-q4_k.gguf|whisper-whisper|1"
)

for m in "${M[@]}"; do
  IFS='|' read -r name file backend threads <<< "$m"
  echo ">>> $name  (size $(du -h "$MODELS/$file" 2>/dev/null | cut -f1))"
  "$BIN" --model="$MODELS/$file" --backend="$backend" --fa --gid=0 \
    --threads="$threads" --captureid=1 --playbackid=0 > "$OUT/$name.log" 2>&1
  echo "    done $name"
done
echo "ALL DONE — logs in $OUT"
