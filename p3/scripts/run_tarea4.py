#!/usr/bin/env python3
"""Tarea 4 - barrido de estrategias/tamano/procesos del clasificador kNN MPI.

Compara:
  seq   : codigo secuencial proporcionado (clases knn2, con copias y prints)
  test  : paralelismo de datos sobre muestras de test/validacion
  k     : paralelismo de tareas sobre el hiperparametro K
  folds : paralelismo de tareas sobre los folds de la validacion cruzada
  spec  : paralelismo funcional + ejecucion especulativa (fine-tuning || test)

Ejemplos:
    python3 scripts/run_tarea4.py
    python3 scripts/run_tarea4.py --hostfile p3/hostfile --results-dir results/cluster
    python3 scripts/run_tarea4.py --trains "1000 2000" --skip-seq
"""
import argparse
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mpi_common import add_cluster_args, mpirun_cmd, parse_np_list  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
K = os.path.join(os.path.dirname(ROOT), "knn2")
D = os.path.join(ROOT, "tarea4_knn")
RUNDIR = os.path.join(D, "run")


def prepare_dataset_links():
    """Crea run/train y run/test apuntando al dataset, reparando enlaces roto."""
    os.makedirs(RUNDIR, exist_ok=True)
    for name in ("train", "test"):
        link = os.path.join(RUNDIR, name)
        target = os.path.join(K, "build", name)
        if os.path.islink(link) and not os.path.exists(link):
            os.unlink(link)          # enlace roto de una copia antigua
        if not os.path.lexists(link):
            if not os.path.isdir(target):
                sys.exit(f"FALTA el dataset MNIST en {target}.\n"
                         f"Ejecuta: chmod +x {K}/get_dataset.sh && {K}/get_dataset.sh")
            # Enlace RELATIVO: el repo funciona en cualquier ruta y en cada PC.
            os.symlink(os.path.relpath(target, RUNDIR), link)
        elif not os.path.isdir(link):
            sys.exit(f"{link} existe pero no es un directorio valido")


def parse(out):
    d = {}
    m = re.search(r"strategy=(\w+)", out)
    if m:
        d["strategy"] = m.group(1)
    for key in ["P", "bestK", "cvAcc", "testAcc", "tFine", "tFinal", "tTotal"]:
        m = re.search(rf"(?<![\w]){key}=([\d.eE+-]+)", out)
        if m:
            d[key] = m.group(1)
    for key, pat in [("train", r"\btrain=(\d+)"), ("test", r"\btest=(\d+)"),
                     ("finaltrain", r"\bfinaltrain=(\d+)"),
                     ("folds", r"\bfolds=(\d+)")]:
        m = re.search(pat, out)
        if m:
            d[key] = m.group(1)
    m = re.search(r"K=\[(\d+),(\d+)\]", out)
    if m:
        d["mink"], d["maxk"] = m.groups()
    return d


def main():
    ap = argparse.ArgumentParser(description="Barrido kNN MPI")
    ap.add_argument("--trains", default="2000 4000",
                    help="tamanos de train para validacion cruzada")
    ap.add_argument("--test", type=int, default=1000)
    ap.add_argument("--finaltrain", type=int, default=2000)
    ap.add_argument("--mink", type=int, default=1)
    ap.add_argument("--maxk", type=int, default=10)
    ap.add_argument("--folds", type=int, default=5)
    ap.add_argument("--strategies", default="test k folds spec")
    ap.add_argument("--skip-seq", action="store_true")
    ap.add_argument("--spec-ps", default="",
                    help="P donde barrer el reparto de grupos (defecto: maximo)")
    add_cluster_args(ap, os.path.join(ROOT, "results"))
    args = ap.parse_args()

    prepare_dataset_links()
    trains = [int(x) for x in args.trains.replace(",", " ").split()]
    strats = args.strategies.split()
    ps = parse_np_list(args.np_list, args.hostfile)
    res = args.results_dir
    os.makedirs(res, exist_ok=True)

    rows = []

    def run(cmd, groupA=-1):
        proc = subprocess.run(cmd, capture_output=True, text=True, cwd=RUNDIR)
        out = proc.stdout + proc.stderr
        m = re.search(r"RESULT .*", out)
        if not m:
            print("FALLO:", " ".join(cmd))
            print(out[-400:])
            return None
        d = parse(m.group(0))
        d["groupA"] = groupA
        rows.append(d)
        return d

    # --- Referencia secuencial (codigo proporcionado, solo maestro) ---
    if not args.skip_seq:
        for tr in trains:
            d = run([os.path.join(D, "seq_knn"), str(tr), str(args.mink),
                     str(args.maxk), str(args.folds), str(args.test),
                     str(args.finaltrain)])
            if d:
                print(f"seq train={tr} tTotal={float(d['tTotal']):.2f}s "
                      f"testAcc={d['testAcc']}")

    # --- Estrategias paralelas ---
    for tr in trains:
        for strat in strats:
            for P in ps:
                if strat == "spec" and P < 2:
                    continue
                ga = max(1, P // 2) if strat == "spec" else -1
                if strat == "spec" and ga >= P:
                    ga = P - 1
                cmd = mpirun_cmd(P, args.hostfile, args.mpirun_extra,
                                 args.map_by, args.bind_to, args.net_iface)
                cmd += [os.path.join(D, "knn_mpi"), "--strategy", strat,
                        "--train", str(tr), "--test", str(args.test),
                        "--finaltrain", str(args.finaltrain),
                        "--mink", str(args.mink), "--maxk", str(args.maxk),
                        "--folds", str(args.folds)]
                if strat == "spec":
                    cmd += ["--groupA", str(ga)]
                d = run(cmd, groupA=ga)
                if d:
                    print(f"{strat} train={tr} P={P} tTotal={float(d['tTotal']):.2f}s "
                          f"bestK={d['bestK']} testAcc={d['testAcc']}")

    # --- Barrido de reparto de grupos en modo especulativo ---
    spec_ps = [int(x) for x in args.spec_ps.split()] if args.spec_ps else [max(ps)]
    for tr in trains:
        for P in spec_ps:
            if P < 2:
                continue
            for ga in [1, 2, 4, 6]:
                if ga >= P:
                    continue
                cmd = mpirun_cmd(P, args.hostfile, args.mpirun_extra,
                                 args.map_by, args.bind_to, args.net_iface)
                cmd += [os.path.join(D, "knn_mpi"), "--strategy", "spec",
                        "--groupA", str(ga), "--train", str(tr),
                        "--test", str(args.test),
                        "--finaltrain", str(args.finaltrain),
                        "--mink", str(args.mink), "--maxk", str(args.maxk),
                        "--folds", str(args.folds)]
                d = run(cmd, groupA=ga)
                if d:
                    print(f"spec-ga train={tr} P={P} ga={ga} "
                          f"tTotal={float(d['tTotal']):.2f}s testAcc={d['testAcc']}")

    cols = ["strategy", "P", "train", "test", "finaltrain", "mink", "maxk",
            "folds", "bestK", "cvAcc", "testAcc", "groupA", "tFine", "tFinal",
            "tTotal"]
    with open(os.path.join(res, "tarea4_knn.csv"), "w") as f:
        f.write(",".join(cols) + "\n")
        for d in rows:
            f.write(",".join(str(d.get(c, "")) for c in cols) + "\n")
    print("Tarea 4 listo ->", res)


if __name__ == "__main__":
    main()
