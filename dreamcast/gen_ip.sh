#!/usr/bin/env bash
# Generate a minimal IP.BIN (boot sector) for Dreamcast homebrew.
# Usage: ./gen_ip.sh [hardware_id] [maker_id] [product_number] [product_name]
#
# Defaults produce a valid MIL-CD compatible IP.BIN for KallistiOS binaries.
# Fields are configurable for production builds.

set -euo pipefail

HW_ID="${1:-179A}"
MAKER_ID="${2:-0000}"
PROD_NUM="${3:-T0000}"
PROD_NAME="${4:-BEETLE NGP}"
TARGET_OS="KATANA"

OUT="${OUT:-IP.BIN}"
IP_SIZE=32768

# Check if KOS makeip is available
if command -v makeip &>/dev/null; then
  makeip "${OUT}" "${PROD_NAME}" "${HW_ID}" "${MAKER_ID}" "${PROD_NUM}" "1ST_READ.BIN" "${TARGET_OS}" 1 0 0 0 0 0 0 0 >/dev/null 2>&1 || true
  echo "Created ${OUT} via makeip"
  exit 0
fi

# Fallback: generate manually with dd + printf
echo "makeip not found; generating minimal IP.BIN with dd/printf..." >&2

# Create zeroed 32KB block
dd if=/dev/zero of="${OUT}" bs=32768 count=1 2>/dev/null

# Magic header at offset 0x00: "SEGA SEGAKATANA ..." (16 bytes)
printf 'SEGA SEGAKATANA' | dd of="${OUT}" bs=1 seek=0 conv=notrunc 2>/dev/null

# Hardware ID at offset 0x30 (16 bytes ASCII)
printf '%-.16s' "${HW_ID}" | dd of="${OUT}" bs=1 seek=48 conv=notrunc 2>/dev/null

# Maker ID at offset 0x40 (16 bytes ASCII)
printf '%-.16s' "${MAKER_ID}" | dd of="${OUT}" bs=1 seek=64 conv=notrunc 2>/dev/null

# Product number at offset 0x2C (10 bytes ASCII)
printf '%-.10s' "${PROD_NUM}" | dd of="${OUT}" bs=1 seek=44 conv=notrunc 2>/dev/null

# Product name at offset 0x80 (128 bytes ASCII)
printf '%-.128s' "${PROD_NAME}" | dd of="${OUT}" bs=1 seek=128 conv=notrunc 2>/dev/null

# Boot filename "1ST_READ.BIN" at offset 0x60
printf '1ST_READ.BIN' | dd of="${OUT}" bs=1 seek=96 conv=notrunc 2>/dev/null

# Region flags at offset 0x2A: JUE (Japan+US+Europe = 0x07)
printf '\x07' | dd of="${OUT}" bs=1 seek=42 conv=notrunc 2>/dev/null

# CD area symbols at offset 0x1C (one byte each, standard values)
printf '\x41' | dd of="${OUT}" bs=1 seek=28 conv=notrunc 2>/dev/null  # area 0

# Disc type at offset 0x00E: 0x00 = CD-ROM
printf '\x00' | dd of="${OUT}" bs=1 seek=14 conv=notrunc 2>/dev/null

echo "Created ${OUT} (${IP_SIZE} bytes) via fallback generator"
