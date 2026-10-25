#!/usr/bin/env python3
"""Tarea 3 - barrido de tamano/num. procesos del simulador N-cuerpos MPI.

En local (sin argumentos) reproduce el barrido historico 1/2/4/8 procesos.
En el cluster se lanza con --hostfile y el numero de PCs se refleja en la
lista de P (multiplos de 3, 6, 9...).

Ejemplos:
    python3 scripts/run_tarea3.py
    python3 scripts/run_tarea3.py --hostfile p3/hostfile --results-dir results/cluster
"""
import argparse
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mpi_common import add_cluster_args, mpirun_cmd, parse_np_list  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = os.path.join(ROOT, "tarea3_nbody")


def main():
    ap = argparse.ArgumentParser(description="Barrido N-cuerpos MPI")
    ap.add_argument("--sizes", default="2000 4000 8000",
                    help="tamanos n a probar (por defecto: 2000 4000 8000)")
    ap.add_argument("--steps", type=int, default=50)
    ap.add_argument("--parts", default="block cyclic")
    ap.add_argument("--skip-seq", action="store_true",
                    help="no reejecuta la referencia secuencial")
    add_cluster_args(ap, os.path.join(ROOT, "results"))
    args = ap.parse_args()

    ns = [int(x) for x in args.sizes.replace(",", " ").split()]
    parts = args.parts.split()
    ps = parse_np_list(args.np_list, args.hostfile)
    res = args.results_dir
    os.makedirs(res, exist_ok=True)

    seq_csv = os.path.join(res, "tarea3_seq.csv")
    seq = {}

    # --- Secuencial (referencia, solo en el maestro) ---
    if args.skip_seq and os.path.exists(seq_csv):
        with open(seq_csv) as f:
            for line in f.readlines()[1:]:
                n, _steps, wall = line.strip().split(",")
                seq[int(n)] = float(wall)
        print("seq: reutilizando", seq_csv)
    else:
        for n in ns:
            proc = subprocess.run([os.path.join(D, "nbody_seq"), str(n), str(args.steps)],
                                  capture_output=True, text=True)
            m = re.search(r"Tiempo total:\s*([\d.]+)", proc.stdout + proc.stderr)
            if not m:
                sys.exit(f"FALLO secuencial n={n}: {proc.stderr[-300:]}")
            seq[n] = float(m.group(1))
        with open(seq_csv, "w") as f:
            f.write("n,steps,wall\n")
            for n in ns:
                f.write(f"{n},{args.steps},{seq[n]:.6f}\n")
        print("seq:", {n: round(seq[n], 3) for n in ns})

    # --- Paralelo ---
    rows = []
    for part in parts:
        for n in ns:
            for P in ps:
                cmd = mpirun_cmd(P, args.hostfile, args.mpirun_extra,
                                 args.map_by, args.bind_to, args.net_iface)
                cmd += [os.path.join(D, "nbody_mpi"), str(n), str(args.steps), part]
                proc = subprocess.run(cmd, capture_output=True, text=True)
                out = proc.stdout + proc.stderr
                m = re.search(r"RESULT .*wall=([\d.eE+-]+) comp=([\d.eE+-]+) "
                              r"comm=([\d.eE+-]+)", out)
                if not m:
                    print("FALLO:", " ".join(cmd), out[-300:])
                    continue
                wall, comp, comm = map(float, m.groups())
                rows.append((part, P, n, args.steps, wall, comp, comm, seq[n] / wall))
                print(f"{part} n={n} P={P} wall={wall:.3f} speedup={seq[n]/wall:.2f}")

    with open(os.path.join(res, "tarea3_nbody.csv"), "w") as f:
        f.write("part,P,n,steps,wall,comp,comm,speedup\n")
        for r in rows:
            f.write(f"{r[0]},{r[1]},{r[2]},{r[3]},{r[4]:.6f},{r[5]:.6f},"
                    f"{r[6]:.6f},{r[7]:.4f}\n")
    print("Tarea 3 listo ->", res)


if __name__ == "__main__":
    main()
