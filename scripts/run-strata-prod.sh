#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=scripts/strata-backend-endpoints.sh
source "${ROOT}/scripts/strata-backend-endpoints.sh"

# The launcher mode is authoritative. In particular, never inherit a local
# STRATA_PLAN_ENDPOINT from a terminal previously used for development.
STRATA_BACKEND_PROD="${STRATA_BACKEND_PROD%/}"
export STRATA_PLAN_ENDPOINT="${STRATA_BACKEND_PROD}/ai/messages"
exec "${ROOT}/scripts/_run-strata-app.sh" "$@"
