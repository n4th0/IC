#!/usr/bin/env bash
# Lanza hello_mpi en localhost con N procesos (por defecto 4).
set -euo pipefail
cd -- "$(dirname -- "$0")"
N="${1:-4}"
mpicc -O2 hello_mpi.c -o hello_mpi
mpiexec -f hostfile.local -n "$N" ./hello_mpi
