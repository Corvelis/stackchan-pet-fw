#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENV_NAME="${1:-m5stack-cores3-release}"
BUILD_DIR="${ROOT_DIR}/.pio/build/${ENV_NAME}"
DIST_DIR="${ROOT_DIR}/dist"
PIO_BIN="${PIO:-}"
PYTHON_BIN="${PYTHON:-python3}"
ESPTOOL_PY="${ESPTOOL_PY:-${HOME}/.platformio/packages/tool-esptoolpy/esptool.py}"
BOOT_APP0_BIN="${BOOT_APP0_BIN:-${HOME}/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin}"

if [[ -z "${PIO_BIN}" ]]; then
  if command -v pio >/dev/null 2>&1; then
    PIO_BIN="pio"
  elif [[ -x "${HOME}/.platformio/penv/bin/pio" ]]; then
    PIO_BIN="${HOME}/.platformio/penv/bin/pio"
  else
    echo "PlatformIO CLI not found. Set PIO=/path/to/pio." >&2
    exit 1
  fi
fi

mkdir -p "${DIST_DIR}"

"${PIO_BIN}" run -e "${ENV_NAME}"
"${PIO_BIN}" run -e "${ENV_NAME}" -t buildfs

rm -f "${DIST_DIR}/stackchan_cores3_firmware.bin" \
      "${DIST_DIR}/stackchan_cores3_factory.bin" \
      "${DIST_DIR}/stackchan_cores3_littlefs.bin" \
      "${DIST_DIR}/SHA256SUMS"

cp "${BUILD_DIR}/firmware.bin" "${DIST_DIR}/stackchan_cores3_firmware.bin"

"${PYTHON_BIN}" "${ESPTOOL_PY}" --chip esp32s3 merge_bin \
  --flash_mode keep \
  --flash_freq keep \
  --flash_size keep \
  -o "${DIST_DIR}/stackchan_cores3_factory.bin" \
  0x0000 "${BUILD_DIR}/bootloader.bin" \
  0x8000 "${BUILD_DIR}/partitions.bin" \
  0xe000 "${BOOT_APP0_BIN}" \
  0x10000 "${BUILD_DIR}/firmware.bin" \
  0xc90000 "${BUILD_DIR}/littlefs.bin"

(cd "${DIST_DIR}" && shasum -a 256 stackchan_cores3_firmware.bin stackchan_cores3_factory.bin > SHA256SUMS)

echo "Release assets written to ${DIST_DIR}:"
ls -lh "${DIST_DIR}/stackchan_cores3_firmware.bin" \
       "${DIST_DIR}/stackchan_cores3_factory.bin" \
       "${DIST_DIR}/SHA256SUMS"
