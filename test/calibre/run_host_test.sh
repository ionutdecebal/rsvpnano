#!/usr/bin/env bash
# Build + run the host unit test for the pure Calibre ajax JSON parsers,
# off-device, with plain g++ -std=c++17.
#
# Mirrors how test/test_release_parser exercises releaseparser, but standalone
# (no PlatformIO) so it runs anywhere g++ is available.
#
# NOTE ON ArduinoJson: this repo does NOT use ArduinoJson -- src/ and
# platformio.ini have zero dependency on it, and the existing parsers
# (ReleaseParser, FeedParser, CompanionSyncManager) hand-roll String-based JSON.
# To match firmware conventions, CalibreClient does the same, so NO ArduinoJson
# vendoring is required here. We still create the gitignored vendor/ dir the
# issue asked for (in case a future parser wants the single header) and, if an
# ArduinoJson.h is found under .pio/libdeps, we symlink it in -- but the test
# compiles and passes without it.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
VENDOR_DIR="${SCRIPT_DIR}/vendor"
BUILD_DIR="${SCRIPT_DIR}/build"

mkdir -p "${VENDOR_DIR}" "${BUILD_DIR}"

# Best-effort: locate a single-header ArduinoJson under PlatformIO libdeps and
# vendor it. Not required for this test (see note above); purely opportunistic.
if [ ! -f "${VENDOR_DIR}/ArduinoJson.h" ]; then
  FOUND_AJSON="$(find "${REPO_ROOT}/.pio" -iname 'ArduinoJson.h' 2>/dev/null | head -n1 || true)"
  if [ -n "${FOUND_AJSON}" ]; then
    cp "${FOUND_AJSON}" "${VENDOR_DIR}/ArduinoJson.h"
    echo "[run_host_test] vendored ArduinoJson.h from ${FOUND_AJSON} (unused by this test)"
  else
    echo "[run_host_test] ArduinoJson.h not found under .pio/libdeps -- not needed; the repo uses hand-rolled String JSON."
  fi
fi

CXX="${CXX:-g++}"
BIN="${BUILD_DIR}/test_calibre_parse"

echo "[run_host_test] compiling parsers test with ${CXX} -std=c++17"
"${CXX}" -std=c++17 -Wall -Wextra \
  -I"${REPO_ROOT}/src" \
  -I"${REPO_ROOT}/test/support" \
  -I"${VENDOR_DIR}" \
  "${SCRIPT_DIR}/test_calibre_parse.cpp" \
  "${REPO_ROOT}/src/calibre/CalibreClient.cpp" \
  "${REPO_ROOT}/src/net/HttpFetch.cpp" \
  -o "${BIN}"

# build + run the PURE reconcile-core test
# (src/sync/CalibreSyncPlan.h, header-only -- no .cpp to compile).
SYNC_BIN="${BUILD_DIR}/test_sync_plan"
echo "[run_host_test] compiling sync plan test with ${CXX} -std=c++17"
"${CXX}" -std=c++17 -Wall -Wextra \
  -I"${REPO_ROOT}/src" \
  -I"${REPO_ROOT}/test/support" \
  -I"${VENDOR_DIR}" \
  "${SCRIPT_DIR}/test_sync_plan.cpp" \
  -o "${SYNC_BIN}"

# build + run the Calibre settings JSON serialize/parse test.
# src/calibre/CalibreSettingsJson.h is header-only; CalibreSettings.h is also
# header-only for the host (the #if ARDUINO guard in CalibreSettings.cpp means
# no .cpp to compile here). Only the shim + test TU are needed.
SETTINGS_JSON_BIN="${BUILD_DIR}/test_calibre_settings_json"
echo "[run_host_test] compiling calibre settings JSON test with ${CXX} -std=c++17"
"${CXX}" -std=c++17 -Wall -Wextra \
  -I"${REPO_ROOT}/src" \
  -I"${REPO_ROOT}/test/support" \
  -I"${VENDOR_DIR}" \
  "${SCRIPT_DIR}/test_calibre_settings_json.cpp" \
  -o "${SETTINGS_JSON_BIN}"

echo "[run_host_test] running"
# Run from the repo root so the test can find tools/calibre-sync/fixtures/.
cd "${REPO_ROOT}"
"${BIN}"
"${SYNC_BIN}"
"${SETTINGS_JSON_BIN}"
