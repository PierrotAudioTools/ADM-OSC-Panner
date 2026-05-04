#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
ARTEFACTS_DIR="${BUILD_DIR}/ADM_OSC_Music_Panner_artefacts"
DIST_DIR="${ROOT_DIR}/dist"

PLUGIN_NAME="${PLUGIN_NAME:-ADM-OSC Panner}"
VERSION="${VERSION:-1.0.1}"

AU_BUILD_PATH="${ARTEFACTS_DIR}/AU/${PLUGIN_NAME}.component"
VST3_BUILD_PATH="${ARTEFACTS_DIR}/VST3/${PLUGIN_NAME}.vst3"

PKG_WORK_DIR="${BUILD_DIR}/pkg_work"
PKG_STAGE_DIR="${PKG_WORK_DIR}/stage"
PKG_OUT_DIR="${PKG_WORK_DIR}/pkg"
DIST_XML="${PKG_WORK_DIR}/distribution.xml"
FINAL_PKG="${DIST_DIR}/${PLUGIN_NAME} v${VERSION} Installer.pkg"

echo "[1/6] Building AU + VST3 (Release)..."
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -G Ninja
cmake --build "${BUILD_DIR}" --config Release --target ADM_OSC_Music_Panner_AU ADM_OSC_Music_Panner_VST3 -j8

if [[ ! -d "${AU_BUILD_PATH}" ]]; then
  echo "ERROR: AU build output not found: ${AU_BUILD_PATH}" >&2
  exit 1
fi

if [[ ! -d "${VST3_BUILD_PATH}" ]]; then
  echo "ERROR: VST3 build output not found: ${VST3_BUILD_PATH}" >&2
  exit 1
fi

echo "[2/6] Preparing package staging folders..."
rm -rf "${PKG_WORK_DIR}"
mkdir -p "${PKG_STAGE_DIR}/au/Library/Audio/Plug-Ins/Components"
mkdir -p "${PKG_STAGE_DIR}/vst3/Library/Audio/Plug-Ins/VST3"
mkdir -p "${PKG_OUT_DIR}"
mkdir -p "${DIST_DIR}"

echo "[3/6] Copying plugin binaries into staging..."
cp -R "${AU_BUILD_PATH}" "${PKG_STAGE_DIR}/au/Library/Audio/Plug-Ins/Components/"
cp -R "${VST3_BUILD_PATH}" "${PKG_STAGE_DIR}/vst3/Library/Audio/Plug-Ins/VST3/"

AU_COMPONENT_PKG="${PKG_OUT_DIR}/ADM-OSC-Panner-AU.pkg"
VST3_COMPONENT_PKG="${PKG_OUT_DIR}/ADM-OSC-Panner-VST3.pkg"

echo "[4/6] Building component packages..."
pkgbuild \
  --root "${PKG_STAGE_DIR}/au" \
  --identifier "com.pierrot.admosc.panner.au" \
  --version "${VERSION}" \
  --install-location "/" \
  "${AU_COMPONENT_PKG}"

pkgbuild \
  --root "${PKG_STAGE_DIR}/vst3" \
  --identifier "com.pierrot.admosc.panner.vst3" \
  --version "${VERSION}" \
  --install-location "/" \
  "${VST3_COMPONENT_PKG}"

echo "[5/6] Creating distribution definition..."
productbuild \
  --synthesize \
  --package "${AU_COMPONENT_PKG}" \
  --package "${VST3_COMPONENT_PKG}" \
  "${DIST_XML}"

echo "[6/6] Building final installer package..."
productbuild \
  --distribution "${DIST_XML}" \
  --package-path "${PKG_OUT_DIR}" \
  "${FINAL_PKG}"

echo "Done."
echo "Installer created at:"
echo "${FINAL_PKG}"
