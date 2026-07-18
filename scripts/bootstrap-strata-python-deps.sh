#!/usr/bin/env bash
# Install Python runtime packages needed by core QGIS plugins in a dev build.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${STRATA_BUILD_DIR:-${GEOAI_BUILD_DIR:-${ROOT}/build}}"
PYTHON_OUTPUT="${BUILD}/output/python"
PYTHON_EXECUTABLE="${STRATA_PYTHON_EXECUTABLE:-${GEOAI_PYTHON_EXECUTABLE:-}}"
REQUIREMENTS_FILE="${ROOT}/scripts/strata-python-runtime-requirements.txt"

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
# binary wheels.
PG_CONFIG="${STRATA_PG_CONFIG:-${PG_CONFIG:-}}"
if [[ -z "${PG_CONFIG}" ]]; then
  PG_CONFIG="$(command -v pg_config || true)"
fi
if [[ -z "${PG_CONFIG}" || ! -x "${PG_CONFIG}" ]]; then
  echo "pg_config not found. Set STRATA_PG_CONFIG to the libpq used by the build." >&2
  exit 1
fi

GDAL_CONFIG="${STRATA_GDAL_CONFIG:-${GDAL_CONFIG:-}}"
if [[ -z "${GDAL_CONFIG}" ]]; then
  GDAL_CONFIG="$(command -v gdal-config || true)"
fi
if [[ -z "${GDAL_CONFIG}" || ! -x "${GDAL_CONFIG}" ]]; then
  echo "gdal-config not found. Set STRATA_GDAL_CONFIG to the GDAL used by the build." >&2
  exit 1
fi

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
  "GDAL==$(awk -F== '$1 == "GDAL" { print $2 }' "${REQUIREMENTS_FILE}")"

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
