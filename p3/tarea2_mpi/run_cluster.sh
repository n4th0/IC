#!/usr/bin/env bash
# run_cluster.sh (Tarea 2) - Atajo: lanza el "hola mundo" en el cluster.
#
# Equivale a:  ../scripts/run_cluster.sh --tarea 2 "$@"
# Requiere haber configurado ../scripts/cluster.env
set -euo pipefail
exec bash "$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../scripts" && pwd)/run_cluster.sh" \
    --tarea 2 "$@"
