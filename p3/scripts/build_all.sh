#!/usr/bin/env bash
# build_all.sh - Compila todos los ejecutables de la Practica 3.
#
#   p3/scripts/build_all.sh
#
# Lo usan tanto run_all.sh (modo local) como cluster_setup.sh (modo cluster).
set -euo pipefail
cd -- "$(dirname -- "$0")/.."          # p3/
ROOT=$PWD
K="$(cd "$ROOT/../knn2" && pwd)"
echo "== Compilando Tarea 2 (hello MPI) =="
( cd tarea2_mpi && mpicc -O2 hello_mpi.c -o hello_mpi )

echo "== Compilando Tarea 3 (N-cuerpos) =="
( cd tarea3_nbody \
    && mpic++ -O2 -std=c++17 nbody_mpi.cpp -o nbody_mpi \
    && g++ -O2 -std=c++17 "$ROOT/../src/main.cpp" -o nbody_seq )

echo "== Compilando Tarea 4 (kNN) =="
( cd tarea4_knn \
    && mpic++ -O2 -std=c++17 -I"$K" knn_mpi.cpp \
         "$K/crossValidation.cc" "$K/mnist.cc" "$K/knn.cc" "$K/sample.cc" -o knn_mpi \
    && g++ -O2 -std=c++17 -I"$K" seq_knn.cpp \
         "$K/crossValidation.cc" "$K/mnist.cc" "$K/knn.cc" "$K/sample.cc" -o seq_knn )

# Permiso de ejecucion remota (la guia lo pide de forma explicita).
chmod o+x tarea2_mpi/hello_mpi tarea3_nbody/nbody_mpi tarea3_nbody/nbody_seq \
          tarea4_knn/knn_mpi tarea4_knn/seq_knn 2>/dev/null || true
echo "Ejecutables listos."
