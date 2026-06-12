#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
COMPANION_DIR="$ROOT_DIR/RSVPNanoCompanion"
OUTPUT_DIR="$COMPANION_DIR/ios/RSVPNanoCompanion/SharedFrameworks"

cd "$ROOT_DIR"

./gradlew :shared:assembleXCFramework

mkdir -p "$OUTPUT_DIR"

XCFRAMEWORK_PATH="$(find "$COMPANION_DIR/shared/build" -name "shared.xcframework" -print -quit)"
if [[ -z "$XCFRAMEWORK_PATH" ]]; then
  echo "shared.xcframework not found under shared/build."
  exit 1
fi

rm -rf "$OUTPUT_DIR/shared.xcframework"
cp -R "$XCFRAMEWORK_PATH" "$OUTPUT_DIR/shared.xcframework"

echo "Copied shared.xcframework to $OUTPUT_DIR/shared.xcframework"
