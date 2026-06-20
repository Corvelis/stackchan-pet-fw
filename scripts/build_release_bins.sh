#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST_DIR="${ROOT_DIR}/dist"
PIO_BIN="${PIO:-}"
PYTHON_BIN="${PYTHON:-python3}"
ESPTOOL_PY="${ESPTOOL_PY:-${HOME}/.platformio/packages/tool-esptoolpy/esptool.py}"
BOOT_APP0_BIN="${BOOT_APP0_BIN:-${HOME}/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin}"

ALL_TARGETS=(cores3 stopwatch atoms3r)
RELEASE_ASSETS=(
  stackchan_cores3_firmware.bin
  stackchan_cores3_factory.bin
  stackchan_stopwatch_firmware.bin
  stackchan_stopwatch_factory.bin
  stackchan_atoms3r_firmware.bin
  stackchan_atoms3r_factory.bin
)

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

if [[ ! -f "${ESPTOOL_PY}" ]]; then
  echo "esptool.py not found at ${ESPTOOL_PY}. Set ESPTOOL_PY=/path/to/esptool.py." >&2
  exit 1
fi

if [[ ! -f "${BOOT_APP0_BIN}" ]]; then
  echo "boot_app0.bin not found at ${BOOT_APP0_BIN}. Set BOOT_APP0_BIN=/path/to/boot_app0.bin." >&2
  exit 1
fi

usage() {
  cat <<'EOF'
Usage:
  scripts/build_release_bins.sh [all|cores3|stopwatch|atoms3r|<platformio-env>...]

Examples:
  scripts/build_release_bins.sh all
  scripts/build_release_bins.sh cores3 stopwatch
  scripts/build_release_bins.sh m5stack-atoms3r-chatbot-release

Outputs:
  dist/stackchan_cores3_firmware.bin
  dist/stackchan_cores3_factory.bin
  dist/stackchan_stopwatch_firmware.bin
  dist/stackchan_stopwatch_factory.bin
  dist/stackchan_atoms3r_firmware.bin
  dist/stackchan_atoms3r_factory.bin
  dist/SHA256SUMS
EOF
}

set_target_config() {
  case "$1" in
    cores3|m5stack-cores3|m5stack-cores3-release)
      TARGET_NAME="cores3"
      ENV_NAME="m5stack-cores3-release"
      ASSET_PREFIX="stackchan_cores3"
      LITTLEFS_OFFSET="0x810000"
      ;;
    stopwatch|m5stack-stopwatch|m5stack-stopwatch-release)
      TARGET_NAME="stopwatch"
      ENV_NAME="m5stack-stopwatch-release"
      ASSET_PREFIX="stackchan_stopwatch"
      LITTLEFS_OFFSET="0x290000"
      ;;
    atoms3r|atoms3r-chatbot|m5stack-atoms3r-chatbot|m5stack-atoms3r-chatbot-release)
      TARGET_NAME="atoms3r"
      ENV_NAME="m5stack-atoms3r-chatbot-release"
      ASSET_PREFIX="stackchan_atoms3r"
      LITTLEFS_OFFSET="0x410000"
      ;;
    -h|--help|help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown release target: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
}

expand_targets() {
  if [[ "$#" -eq 0 ]]; then
    printf '%s\n' "${ALL_TARGETS[@]}"
    return
  fi

  local arg
  for arg in "$@"; do
    case "${arg}" in
      all)
        printf '%s\n' "${ALL_TARGETS[@]}"
        ;;
      *)
        printf '%s\n' "${arg}"
        ;;
    esac
  done
}

build_target() {
  set_target_config "$1"

  local build_dir="${ROOT_DIR}/.pio/build/${ENV_NAME}"
  local firmware_asset="${DIST_DIR}/${ASSET_PREFIX}_firmware.bin"
  local factory_asset="${DIST_DIR}/${ASSET_PREFIX}_factory.bin"

  echo "==> Building ${TARGET_NAME} (${ENV_NAME})"
  "${PIO_BIN}" run -e "${ENV_NAME}"
  "${PIO_BIN}" run -e "${ENV_NAME}" -t buildfs

  local firmware_bin="${build_dir}/firmware.bin"
  local bootloader_bin="${build_dir}/bootloader.bin"
  local partitions_bin="${build_dir}/partitions.bin"
  local littlefs_bin="${build_dir}/littlefs.bin"

  for file in "${firmware_bin}" "${bootloader_bin}" "${partitions_bin}" "${littlefs_bin}"; do
    if [[ ! -f "${file}" ]]; then
      echo "Expected build output not found: ${file}" >&2
      exit 1
    fi
  done

  rm -f "${firmware_asset}" "${factory_asset}"
  cp "${firmware_bin}" "${firmware_asset}"

  "${PYTHON_BIN}" "${ESPTOOL_PY}" --chip esp32s3 merge_bin \
    --flash_mode keep \
    --flash_freq keep \
    --flash_size keep \
    -o "${factory_asset}" \
    0x0000 "${bootloader_bin}" \
    0x8000 "${partitions_bin}" \
    0xe000 "${BOOT_APP0_BIN}" \
    0x10000 "${firmware_bin}" \
    "${LITTLEFS_OFFSET}" "${littlefs_bin}"

  echo "Wrote:"
  ls -lh "${firmware_asset}" "${factory_asset}"
}

write_checksums() {
  local checksum_inputs=()
  local asset

  for asset in "${RELEASE_ASSETS[@]}"; do
    if [[ -f "${DIST_DIR}/${asset}" ]]; then
      checksum_inputs+=("${asset}")
    fi
  done

  if [[ "${#checksum_inputs[@]}" -eq 0 ]]; then
    echo "No release assets found for checksum generation." >&2
    exit 1
  fi

  (cd "${DIST_DIR}" && shasum -a 256 "${checksum_inputs[@]}" > SHA256SUMS)
  echo "Wrote ${DIST_DIR}/SHA256SUMS"
}

mkdir -p "${DIST_DIR}"

targets=()
while IFS= read -r target; do
  targets+=("${target}")
done < <(expand_targets "$@")

for target in "${targets[@]}"; do
  build_target "${target}"
done

write_checksums

echo "Release assets in ${DIST_DIR}:"
existing_assets=()
for asset in "${RELEASE_ASSETS[@]}"; do
  if [[ -f "${DIST_DIR}/${asset}" ]]; then
    existing_assets+=("${DIST_DIR}/${asset}")
  fi
done
ls -lh "${existing_assets[@]}" "${DIST_DIR}/SHA256SUMS"
