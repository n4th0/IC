#!/usr/bin/env bash
# Reproduce TODOS los experimentos de la Practica 3 y genera las figuras.
#
#   ./scripts/run_all.sh
#
# Requiere: OpenMPI (mpic++/mpirun), g++, gprof, python3 con matplotlib y numpy,
# y el dataset MNIST descargado en knn2/build/{train,test} (knn2/get_dataset.sh).
set -euo pipefail
cd -- "$(dirname -- "$0")/.."          # p3/
ROOT=$PWD
K="$(cd "$ROOT/../knn2" && pwd)"

echo "== Compilando (scripts/build_all.sh) =="
bash scripts/build_all.sh

echo "== Tarea 1: gprof =="
bash scripts/run_tarea1.sh

echo "== Tarea 2: hello MPI (local) =="
( cd tarea2_mpi && ./run_local.sh 4 > "$ROOT/results/hello_local.txt" 2>&1 )

echo "== Tarea 0: micro-benchmarks =="
python3 scripts/run_tarea0.py

echo "== Tarea 3: N-cuerpos MPI =="
python3 scripts/run_tarea3.py

echo "== Tarea 4: kNN MPI =="
python3 scripts/run_tarea4.py

echo "== Figuras =="
python3 scripts/plot.py
python3 scripts/plot_profiling.py
python3 scripts/flow_diagrams.py

echo "TODO LISTO. Resultados en results/, figuras en figures/."
