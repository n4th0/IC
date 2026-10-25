# Práctica 3 — Paralelismo en sistemas de memoria distribuida (MPI)

Entrega de la Práctica 3 de Ingeniería de los Computadores (curso 2026-27).

## Estructura

```
p3/
├── tarea0/               Micro-benchmarks (vectores dinamicos, recorrido de matrices)
├── tarea1_profiling/     Informes de gprof (N-cuerpos y knn2) + driver de perfilado
├── tarea2_mpi/           "Hola mundo" MPI, hostfile, instrucciones de clúster (README_MPI.md)
├── tarea3_nbody/         Version MPI del simulador de N cuerpos (src/main.cpp)
├── tarea4_knn/           Version MPI del clasificador kNN (proyecto knn2)
├── scripts/              Automatizacion: builds, experimentos y figuras
├── results/             Resultados numericos en CSV
├── figures/             Figuras y grafos de flujo
└── memoria/             Memoria de la practica (memoria.md / memoria.pdf)
```

## Requisitos

- Open MPI (`mpirun`, `mpic++`/`mpicc`), g++, gprof.
- Python 3 con `matplotlib` y `numpy` (figuras) y `markdown` (PDF).
- Dataset MNIST en `knn2/build/{train,test}` (`knn2/get_dataset.sh`).

## Reproducir todos los experimentos (un solo PC)

```bash
cd p3
./scripts/run_all.sh
```

Genera `results/*.csv`, `figures/*.png` e informes de gprof en `tarea1_profiling/`.

## Ejecutar en varios PCs (cluster MPI)

La practica debe ejecutarse en un multicomputador de **al menos 3 PCs**. Todo
esta automatizado; la guia completa paso a paso esta en
**[`docs/CLUSTER.md`](docs/CLUSTER.md)**.

1. Edita **solo** `scripts/cluster.env` (IPs, usuario, directorio y nucleos):

   ```bash
   cp scripts/cluster.env.example scripts/cluster.env
   nano scripts/cluster.env      # NODES=(111.111.111.111 222.222.222.222 333.333.333.333)
   ```

2. Prepara el cluster (claves SSH, hostfile, copia del codigo/dataset, build):

   ```bash
   ./scripts/cluster_setup.sh
   ```

3. Lanza las tareas:

   ```bash
   ./scripts/run_cluster.sh --tarea 2     # hola mundo en los 3 PCs
   ./scripts/run_cluster.sh               # tareas 2 + 3 + 4
   ```

   Resultados en `results/cluster/` y figuras con
   `python3 scripts/plot.py --results results/cluster`.

4. Ensayo en un solo PC antes de ir al laboratorio:

   ```bash
   ./scripts/run_cluster.sh --rehearsal --quick
   ```


## Memoria a PDF

```bash
python3 scripts/make_pdf.py     # memoria/memoria.md -> memoria/memoria.pdf (chromium)
```

## Ejemplos de uso rápido

```bash
# Tarea 3
cd tarea3_nbody
mpic++ -O2 -std=c++17 nbody_mpi.cpp -o nbody_mpi
mpirun -np 8 ./nbody_mpi 4000 50 block

# Tarea 4 (estrategias: test | k | folds | spec)
cd ../tarea4_knn
K=../../knn2
mpic++ -O2 -std=c++17 -I$K knn_mpi.cpp $K/{crossValidation,mnist,knn,sample}.cc -o knn_mpi
cd run
mpirun -np 8 ../knn_mpi --strategy test  --train 4000 --test 1000 --finaltrain 2000 --mink 1 --maxk 10 --folds 5
mpirun -np 8 ../knn_mpi --strategy spec --groupA 6 --train 4000 --test 1000 --finaltrain 2000 --mink 1 --maxk 10 --folds 5
```
