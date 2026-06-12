#!/usr/bin/env bash
# Build RetroArch for Dreamcast with Beetle NGP statically linked.
#
# Usage:
#   source $KOS_BASE/environ.sh
#   ./retroarch/build-retroarch.sh [path-to-RetroArch]
#
# Environment:
#   RETROARCH_DIR  - RetroArch checkout (default: ../RetroArch)
#   RETROARCH_REF  - git ref to clone when missing (default: master)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CORE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
RETROARCH_DIR="${1:-${RETROARCH_DIR:-${CORE_DIR}/../RetroArch}}"
RETROARCH_REF="${RETROARCH_REF:-master}"

if [[ -z "${KOS_BASE:-}" || ! -f "${KOS_BASE}/environ.sh" ]]; then
   echo "error: source KOS environ.sh before running this script" >&2
   echo "  source \"\$KOS_BASE/environ.sh\"" >&2
   exit 1
fi

# shellcheck disable=SC1090
source "${KOS_BASE}/environ.sh"

if [[ ! -d "${RETROARCH_DIR}/.git" ]]; then
   echo "Cloning RetroArch (${RETROARCH_REF}) into ${RETROARCH_DIR}"
   git clone --depth 1 --branch "${RETROARCH_REF}" \
      https://github.com/libretro/RetroArch.git "${RETROARCH_DIR}"
fi

echo "Building Beetle NGP core for Dreamcast..."
make -C "${CORE_DIR}" platform=dreamcast clean
make -C "${CORE_DIR}" platform=dreamcast

echo "Installing libretro_dreamcast.a into RetroArch tree..."
cp -f "${CORE_DIR}/libretro_dreamcast.a" "${RETROARCH_DIR}/"

echo "Copying Dreamcast platform overlay..."
cp -rf "${SCRIPT_DIR}/overlay/"* "${RETROARCH_DIR}/"
mkdir -p "${RETROARCH_DIR}/autoconfig/dc"
cp -f "${SCRIPT_DIR}/overlay/autoconfig/dc/"*.cfg "${RETROARCH_DIR}/autoconfig/dc/" 2>/dev/null || true
mkdir -p "${RETROARCH_DIR}/dreamcast"
cp -f "${CORE_DIR}/dreamcast/dc_audio.c" "${CORE_DIR}/dreamcast/dc_audio.h" \
   "${CORE_DIR}/dreamcast/dc_settings.c" "${CORE_DIR}/dreamcast/dc_settings.h" \
   "${CORE_DIR}/dreamcast/dc_video.c" "${CORE_DIR}/dreamcast/dc_video.h" \
   "${CORE_DIR}/dreamcast/dc_pvr.c" "${CORE_DIR}/dreamcast/dc_pvr.h" \
   "${RETROARCH_DIR}/dreamcast/"

echo "Applying Dreamcast driver patches..."
for patch in "${SCRIPT_DIR}"/patches/*.patch; do
   [[ -f "${patch}" ]] || continue
   echo "  ${patch##*/}"
   patch -d "${RETROARCH_DIR}" -p1 -N < "${patch}"
done

cp -f "${SCRIPT_DIR}/Makefile.dreamcast" "${RETROARCH_DIR}/"
cp -f "${SCRIPT_DIR}/retroarch.cfg" "${RETROARCH_DIR}/retroarch.cfg.dc"

echo "Building RetroArch..."
make -C "${RETROARCH_DIR}" -f Makefile.dreamcast clean
make -C "${RETROARCH_DIR}" -f Makefile.dreamcast

echo
echo "Done: ${RETROARCH_DIR}/retroarch_dreamcast.elf"
echo "Copy retroarch.cfg.dc to /sd/retroarch/retroarch.cfg on your device."
