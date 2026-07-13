#!/usr/bin/env bash
# Build a single looping hero demo video from selected timestamps.
# Usage: ./extract-demo-clips.sh [path-to-strata-video.mp4]

set -euo pipefail

VIDEO="${1:-$HOME/Downloads/strata-gif/strata-video.mp4}"
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DOCS="$REPO_ROOT/docs/assets"
OUT_FE="$(cd "$REPO_ROOT/../strata-fe" && pwd)/public"
SEGMENT_DURATION=10
WEBM_WIDTH=1280

if [[ ! -f "$VIDEO" ]]; then
  echo "Error: video not found at $VIDEO" >&2
  exit 1
fi

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

# Demo highlights (not from 00:00 — intro is empty)
SEGMENTS=(
  "00:02:30"
  "00:03:03"
  "00:04:06"
  "00:05:00"
  "00:07:01"
  "00:08:05"
)

echo "==> Extracting ${#SEGMENTS[@]} segments (${SEGMENT_DURATION}s each)"

part_files=()
index=0
for start in "${SEGMENTS[@]}"; do
  part="$WORK_DIR/part-$(printf '%02d' "$index").webm"
  echo "    part $index @ $start"
  ffmpeg -y -ss "$start" -t "$SEGMENT_DURATION" -i "$VIDEO" \
    -vf "scale=${WEBM_WIDTH}:-2" -c:v libvpx-vp9 -crf 32 -b:v 0 -an \
    "$part" 2>/dev/null
  part_files+=("$part")
  index=$((index + 1))
done

concat_list="$WORK_DIR/concat.txt"
: > "$concat_list"
for part in "${part_files[@]}"; do
  printf "file '%s'\n" "$part" >> "$concat_list"
done

hero_webm="$WORK_DIR/demo-hero.webm"
echo "==> Concatenating into single hero video"
ffmpeg -y -f concat -safe 0 -i "$concat_list" -c copy "$hero_webm" 2>/dev/null

poster_png="$WORK_DIR/demo-hero-poster.png"
poster_webp="$WORK_DIR/demo-hero-poster.webp"
ffmpeg -y -ss "${SEGMENTS[0]}" -t 1 -i "$VIDEO" \
  -frames:v 1 -vf "scale=${WEBM_WIDTH}:-2" -update 1 "$poster_png" 2>/dev/null
cwebp -q 80 "$poster_png" -o "$poster_webp" 2>/dev/null

mkdir -p "$OUT_DOCS" "$OUT_FE"
cp "$hero_webm" "$OUT_DOCS/demo-hero.webm"
cp "$poster_webp" "$OUT_DOCS/demo-hero-poster.webp"
cp "$hero_webm" "$OUT_FE/demo-hero.webm"
cp "$poster_webp" "$OUT_FE/demo-hero-poster.webp"

ls -lh "$OUT_DOCS/demo-hero.webm" "$OUT_DOCS/demo-hero-poster.webp"
echo "Done."
