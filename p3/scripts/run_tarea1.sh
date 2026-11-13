#!/usr/bin/env bash
# Tarea 1 - Perfilado con gprof del codigo propio (N-cuerpos) y del codigo
# proporcionado (knn2). Genera informes planos + grafo de llamadas.
set -euo pipefail
cd -- "$(dirname -- "$0")/.."          # p3/
K="$(cd ../knn2 && pwd)"
NB="$(cd ../src && pwd)/main.cpp"
T1=tarea1_profiling

# --- N-cuerpos (src/main.cpp) a -O0 y -O2 ---
mkdir -p "$T1/nbody"
cd "$T1/nbody"
for opt in O0 O2; do
    g++ -$opt -g -pg -std=c++17 "$NB" -o nbody_${opt}_pg
    ./nbody_${opt}_pg 3000 50 > run_${opt}.log 2>&1
    gprof -b -p -q ./nbody_${opt}_pg gmon.out > perfil_${opt}.txt 2>/dev/null
    echo "nbody -$opt -> perfil_${opt}.txt"
done
cd ../..

# --- knn2 (codigo proporcionado) con un driver reducido a -O0 y -O2 ---
cd "$T1/knn2"
mkdir -p run_O2 run_O0 build_pg_O2 build_pg_O0
ln -sfn "$K/build/train" run_O2/train; ln -sfn "$K/build/test" run_O2/test
ln -sfn "$K/build/train" run_O0/train; ln -sfn "$K/build/test" run_O0/test
for opt in O2 O0; do
    g++ -$opt -g -pg -std=c++17 -I"$K" ../knn_profile.cc \
        "$K/crossValidation.cc" "$K/mnist.cc" "$K/knn.cc" "$K/sample.cc" \
        -o build_pg_${opt}/knn_profile_${opt}_pg
    ( cd run_${opt} && ../build_pg_${opt}/knn_profile_${opt}_pg 4000 1 3 3 1000 > prof_${opt}.log 2>&1 )
    gprof -b -p -q build_pg_${opt}/knn_profile_${opt}_pg run_${opt}/gmon.out \
        > perfil_knn_profile_${opt}.txt 2>/dev/null
    echo "knn2 -$opt -> perfil_knn_profile_${opt}.txt"
done
echo "Tarea 1 lista (los informes completos de gprof estan en $T1/)."
