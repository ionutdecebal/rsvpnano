#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROPERTIES_FILE="$ROOT_DIR/gradle/wrapper/gradle-wrapper.properties"

if [[ ! -f "$PROPERTIES_FILE" ]]; then
  echo "Missing $PROPERTIES_FILE" >&2
  exit 1
fi

DISTRIBUTION_URL="$(grep '^distributionUrl=' "$PROPERTIES_FILE" | cut -d= -f2- | tr -d '\r' | sed 's#\\:##')"
DISTRIBUTION_FILE_NAME="$(basename "$DISTRIBUTION_URL")"
GRADLE_VERSION="${DISTRIBUTION_FILE_NAME#gradle-}"
GRADLE_VERSION="${GRADLE_VERSION%-bin.zip}"
GRADLE_USER_HOME_DIR="${GRADLE_USER_HOME:-$HOME/.gradle}"
INSTALL_BASE_DIR="$GRADLE_USER_HOME_DIR/wrapper/dists/gradle-$GRADLE_VERSION-bin"
ZIP_FILE="$INSTALL_BASE_DIR/$DISTRIBUTION_FILE_NAME"
INSTALL_DIR="$INSTALL_BASE_DIR/gradle-$GRADLE_VERSION"

mkdir -p "$INSTALL_BASE_DIR"

if [[ ! -d "$INSTALL_DIR" ]]; then
  if [[ ! -f "$ZIP_FILE" ]]; then
    curl -fsSL "$DISTRIBUTION_URL" -o "$ZIP_FILE"
  fi
  rm -rf "$INSTALL_DIR"
  unzip -q "$ZIP_FILE" -d "$INSTALL_BASE_DIR"
fi

exec "$INSTALL_DIR/bin/gradle" -p "$ROOT_DIR" "$@"
