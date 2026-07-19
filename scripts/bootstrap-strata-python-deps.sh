#!/usr/bin/env bash
# Install Python runtime packages needed by core QGIS plugins in a dev build.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${STRATA_BUILD_DIR:-${GEOAI_BUILD_DIR:-${ROOT}/build}}"
PYTHON_OUTPUT="${BUILD}/output/python"
PYTHON_EXECUTABLE="${STRATA_PYTHON_EXECUTABLE:-${GEOAI_PYTHON_EXECUTABLE:-}}"
REQUIREMENTS_FILE="${ROOT}/scripts/strata-python-runtime-requirements.txt"
RUNTIME_PREFIX="${STRATA_RUNTIME_PREFIX:-}"
RUNTIME_TOOL_DIR="${BUILD}/.strata-runtime-tools"
GDAL_RUNTIME_VERSION="${STRATA_GDAL_VERSION:-}"

if [[ ! -d "${BUILD}" ]]; then
  echo "Build directory not found: ${BUILD}" >&2
  exit 1
fi

if [[ ! -d "${PYTHON_OUTPUT}" ]]; then
  echo "Python output directory not found: ${PYTHON_OUTPUT}" >&2
  echo "Build Strata first with: STRATA_BUILD_DIR=${BUILD} ${ROOT}/scripts/build-strata-app.sh" >&2
  exit 1
fi

if [[ -z "${PYTHON_EXECUTABLE}" && -f "${BUILD}/CMakeCache.txt" ]]; then
  PYTHON_EXECUTABLE="$(awk -F= '/^Python_EXECUTABLE:[^=]*=/ { print $2; exit }' "${BUILD}/CMakeCache.txt")"
fi

if [[ -z "${PYTHON_EXECUTABLE}" ]]; then
  PYTHON_EXECUTABLE="$(command -v python3 || true)"
fi

if [[ -z "${PYTHON_EXECUTABLE}" || ! -x "${PYTHON_EXECUTABLE}" ]]; then
  echo "Python executable not found. Set STRATA_PYTHON_EXECUTABLE=/path/to/python." >&2
  exit 1
fi

if [[ ! -f "${REQUIREMENTS_FILE}" ]]; then
  echo "Python runtime requirements file not found: ${REQUIREMENTS_FILE}" >&2
  exit 1
fi

# psycopg2 and the GDAL Python bindings are intentionally compiled from source
# and must use the libraries selected by the Strata build, never user-installed
# binary wheels. vcpkg packages libraries and headers but deliberately omits
# pg_config and gdal-config, so make build-local shims when a vcpkg prefix is
# available and no explicit configuration tool was supplied.
PG_CONFIG="${STRATA_PG_CONFIG:-${PG_CONFIG:-}}"
PG_CONFIG_EXPLICIT="${PG_CONFIG}"
if [[ -z "${PG_CONFIG}" ]]; then
  PG_CONFIG="$(command -v pg_config || true)"
fi

GDAL_CONFIG="${STRATA_GDAL_CONFIG:-${GDAL_CONFIG:-}}"
GDAL_CONFIG_EXPLICIT="${GDAL_CONFIG}"
if [[ -z "${GDAL_CONFIG}" ]]; then
  GDAL_CONFIG="$(command -v gdal-config || true)"
fi

# A vcpkg configuration supplies this through CMake. The file lookup is a
# defensive fallback for invoking the script directly against an existing
# vcpkg build tree.
if [[ -z "${GDAL_RUNTIME_VERSION}" && -n "${RUNTIME_PREFIX}" ]]; then
  GDAL_VERSION_FILE="${RUNTIME_PREFIX}/share/gdal/GDALConfigVersion.cmake"
  if [[ -f "${GDAL_VERSION_FILE}" ]]; then
    GDAL_RUNTIME_VERSION="$(awk -F '"' '/PACKAGE_VERSION/ { print $2; exit }' "${GDAL_VERSION_FILE}")"
  fi
fi

create_vcpkg_config_shims() {
  if [[ -z "${RUNTIME_PREFIX}" || ! -d "${RUNTIME_PREFIX}/include" || ! -d "${RUNTIME_PREFIX}/lib" ]]; then
    return 1
  fi

  mkdir -p "${RUNTIME_TOOL_DIR}"
  export STRATA_RUNTIME_PREFIX="${RUNTIME_PREFIX}"
  export STRATA_GDAL_VERSION="${GDAL_RUNTIME_VERSION}"

  printf '%s\n' \
    '#!/usr/bin/env bash' \
    'set -euo pipefail' \
    'prefix="${STRATA_RUNTIME_PREFIX:?}"' \
    'case "${1:-}" in' \
    '  --libdir) printf "%s\\n" "${prefix}/lib" ;;' \
    '  --includedir|--includedir-server) printf "%s\\n" "${prefix}/include" ;;' \
    '  --ldflags) printf "%s\\n" "-L${prefix}/lib" ;;' \
    '  --cppflags) printf "%s\\n" "-I${prefix}/include" ;;' \
    '  --libs) printf "%s\\n" "-lpq -lssl -lcrypto" ;;' \
    '  --version) printf "%s\\n" "Strata vcpkg libpq" ;;' \
    '  *) printf "Unsupported pg_config option: %s\\n" "${1:-}" >&2; exit 1 ;;' \
    'esac' > "${RUNTIME_TOOL_DIR}/pg_config"

  printf '%s\n' \
    '#!/usr/bin/env bash' \
    'set -euo pipefail' \
    'prefix="${STRATA_RUNTIME_PREFIX:?}"' \
    'version="${STRATA_GDAL_VERSION:?}"' \
    'case "${1:-}" in' \
    '  --prefix) printf "%s\\n" "${prefix}" ;;' \
    '  --cflags) printf "%s\\n" "-I${prefix}/include" ;;' \
    '  --libs) printf "%s\\n" "-L${prefix}/lib -lgdal" ;;' \
    '  --version) printf "%s\\n" "${version}" ;;' \
    '  *) printf "Unsupported gdal-config option: %s\\n" "${1:-}" >&2; exit 1 ;;' \
    'esac' > "${RUNTIME_TOOL_DIR}/gdal-config"

  chmod +x "${RUNTIME_TOOL_DIR}/pg_config" "${RUNTIME_TOOL_DIR}/gdal-config"
}

if [[ -n "${RUNTIME_PREFIX}" && ( -z "${PG_CONFIG_EXPLICIT}" || -z "${GDAL_CONFIG_EXPLICIT}" ) ]]; then
  if [[ -z "${GDAL_CONFIG_EXPLICIT}" && -z "${GDAL_RUNTIME_VERSION}" ]]; then
    echo "GDAL version not available from the Strata build. Set STRATA_GDAL_VERSION." >&2
    exit 1
  fi
  create_vcpkg_config_shims
fi

# When CMake gave us a vcpkg prefix, use its headers and libraries rather than
# a configuration program that might be present elsewhere on the build host.
if [[ -n "${RUNTIME_PREFIX}" && -z "${PG_CONFIG_EXPLICIT}" && -x "${RUNTIME_TOOL_DIR}/pg_config" ]]; then
  PG_CONFIG="${RUNTIME_TOOL_DIR}/pg_config"
fi
if [[ -n "${RUNTIME_PREFIX}" && -z "${GDAL_CONFIG_EXPLICIT}" && -x "${RUNTIME_TOOL_DIR}/gdal-config" ]]; then
  GDAL_CONFIG="${RUNTIME_TOOL_DIR}/gdal-config"
fi

if [[ -z "${PG_CONFIG}" || ! -x "${PG_CONFIG}" ]]; then
  echo "pg_config not found. Set STRATA_PG_CONFIG to the libpq used by the build." >&2
  exit 1
fi
if [[ -z "${GDAL_CONFIG}" || ! -x "${GDAL_CONFIG}" ]]; then
  echo "gdal-config not found. Set STRATA_GDAL_CONFIG to the GDAL used by the build." >&2
  exit 1
fi

if [[ -z "${GDAL_RUNTIME_VERSION}" ]]; then
  GDAL_RUNTIME_VERSION="$("${GDAL_CONFIG}" --version)"
fi
if [[ ! "${GDAL_RUNTIME_VERSION}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Invalid GDAL version from the Strata build: ${GDAL_RUNTIME_VERSION}" >&2
  exit 1
fi
export STRATA_GDAL_VERSION="${GDAL_RUNTIME_VERSION}"

# Both source distributions discover their configuration tools via PATH. Keep
# the resolved tool locations first so explicit user tools still take priority
# and the vcpkg shims work inside pip's isolated build environments.
PG_CONFIG_DIR="$(dirname "${PG_CONFIG}")"
GDAL_CONFIG_DIR="$(dirname "${GDAL_CONFIG}")"
export PATH="${PG_CONFIG_DIR}:${GDAL_CONFIG_DIR}:${PATH}"

echo "Installing Python dev runtime packages into: ${PYTHON_OUTPUT}"
shopt -s nullglob
for package_path in \
  "${PYTHON_OUTPUT}"/packaging \
  "${PYTHON_OUTPUT}"/packaging-*.dist-info \
  "${PYTHON_OUTPUT}"/jinja2 \
  "${PYTHON_OUTPUT}"/jinja2-*.dist-info \
  "${PYTHON_OUTPUT}"/markupsafe \
  "${PYTHON_OUTPUT}"/markupsafe-*.dist-info \
  "${PYTHON_OUTPUT}"/psycopg2 \
  "${PYTHON_OUTPUT}"/psycopg2-*.dist-info \
  "${PYTHON_OUTPUT}"/osgeo \
  "${PYTHON_OUTPUT}"/[Gg][Dd][Aa][Ll]-*.dist-info
do
  rm -rf "${package_path}"
done

# Do not resolve imports from ~/Library/Python while building or validating the
# bundle payload. Each package is installed independently so the two native
# extensions are always compiled for the selected Python ABI.
export PYTHONNOUSERSITE=1
export PG_CONFIG GDAL_CONFIG
"${PYTHON_EXECUTABLE}" -m pip install --no-deps --no-cache-dir --target "${PYTHON_OUTPUT}" \
  "packaging==$(awk -F== '$1 == "packaging" { print $2 }' "${REQUIREMENTS_FILE}")"
"${PYTHON_EXECUTABLE}" -m pip install --no-deps --no-cache-dir --target "${PYTHON_OUTPUT}" \
  "Jinja2==$(awk -F== '$1 == "Jinja2" { print $2 }' "${REQUIREMENTS_FILE}")"
"${PYTHON_EXECUTABLE}" -m pip install --no-deps --no-cache-dir --no-binary MarkupSafe --target "${PYTHON_OUTPUT}" \
  "MarkupSafe==$(awk -F== '$1 == "MarkupSafe" { print $2 }' "${REQUIREMENTS_FILE}")"
"${PYTHON_EXECUTABLE}" -m pip install --no-deps --no-cache-dir --no-binary psycopg2 --target "${PYTHON_OUTPUT}" \
  "psycopg2==$(awk -F== '$1 == "psycopg2" { print $2 }' "${REQUIREMENTS_FILE}")"
"${PYTHON_EXECUTABLE}" -m pip install --no-deps --no-cache-dir --no-binary GDAL --target "${PYTHON_OUTPUT}" \
  "GDAL==${GDAL_RUNTIME_VERSION}"

PYTHONPATH="${PYTHON_OUTPUT}" "${PYTHON_EXECUTABLE}" - <<'PY'
from osgeo import gdal, ogr, osr
import jinja2
import markupsafe
import packaging
import psycopg2

print("Python runtime deps OK:")
print("  packaging", packaging.__version__)
print("  jinja2", jinja2.__version__)
print("  MarkupSafe", markupsafe.__version__)
print("  psycopg2", psycopg2.__version__)
print("  GDAL", gdal.VersionInfo("RELEASE_NAME"))
PY
