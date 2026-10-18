#!/usr/bin/env python3
"""Tarea 4 - barrido de estrategias/tamano/procesos del clasificador kNN MPI.

Compara:
  seq   : codigo secuencial proporcionado (clases knn2, con copias y prints)
  test  : paralelismo de datos sobre muestras de test/validacion
  k     : paralelismo de tareas sobre el hiperparametro K
  folds : paralelismo de tareas sobre los folds de la validacion cruzada
  spec  : paralelismo funcional + ejecucion especulativa (fine-tuning || test)
"""
import os, re, subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
K = os.path.join(os.path.dirname(ROOT), "knn2")
D = os.path.join(ROOT, "tarea4_knn")
RUNDIR = os.path.join(D, "run")
RES = os.path.join(ROOT, "results")
os.makedirs(RES, exist_ok=True)
os.makedirs(RUNDIR, exist_ok=True)
for _d in ("train", "test"):
    _link = os.path.join(RUNDIR, _d)
    _tgt = os.path.join(K, "build", _d)
    if not os.path.exists(_link) and os.path.isdir(_tgt):
        os.symlink(_tgt, _link)

# (train_cv, test, finaltrain, mink, maxk, folds)
CONFIGS = [
    (2000, 1000, 2000, 1, 10, 5),
    (4000, 1000, 2000, 1, 10, 5),
]
PS = [1, 2, 4, 8]
STRATS = ["test", "k", "folds", "spec"]


def parse(out):
    d = {}
    m = re.search(r"strategy=(\w+)", out)
    if m:
        d["strategy"] = m.group(1)
    for key in ["P", "bestK", "cvAcc", "testAcc", "tFine", "tFinal", "tTotal"]:
        m = re.search(rf"(?<![\w]){key}=([\d.eE+-]+)", out)
        if m:
            d[key] = m.group(1)
    m = re.search(r"\btrain=(\d+)", out); d["train"] = m.group(1)
    m = re.search(r"\btest=(\d+)", out); d["test"] = m.group(1)
    m = re.search(r"\bfinaltrain=(\d+)", out); d["finaltrain"] = m.group(1)
    m = re.search(r"K=\[(\d+),(\d+)\]", out); d["mink"], d["maxk"] = m.groups()
    m = re.search(r"\bfolds=(\d+)", out); d["folds"] = m.group(1)
    return d


rows = []


def run(cmd, groupA=-1):
    out = subprocess.run(cmd, capture_output=True, text=True, cwd=RUNDIR).stdout
    m = re.search(r"RESULT .*", out)
    if not m:
        print("FALLO:", " ".join(cmd)); return None
    d = parse(m.group(0))
    d["groupA"] = groupA
    rows.append(d)
    return d


# --- Referencia secuencial (codigo proporcionado) ---
for (tr, te, ft, k0, k1, fo) in CONFIGS:
    d = run([os.path.join(D, "seq_knn"), str(tr), str(k0), str(k1), str(fo), str(te), str(ft)])
    if d:
        print(f"seq train={tr} tTotal={float(d['tTotal']):.2f}s testAcc={d['testAcc']}")

# --- Estrategias paralelas ---
for (tr, te, ft, k0, k1, fo) in CONFIGS:
    for strat in STRATS:
        for P in PS:
            if strat == "spec" and P < 2:
                continue
            ga = P // 2
            cmd = ["mpirun", "-np", str(P), os.path.join(D, "knn_mpi"),
                   "--strategy", strat, "--train", str(tr), "--test", str(te),
                   "--finaltrain", str(ft), "--mink", str(k0), "--maxk", str(k1),
                   "--folds", str(fo)]
            if strat == "spec":
                cmd += ["--groupA", str(ga)]
            d = run(cmd, groupA=(ga if strat == "spec" else -1))
            if d:
                print(f"{strat} train={tr} P={P} tTotal={float(d['tTotal']):.2f}s "
                      f"bestK={d['bestK']} testAcc={d['testAcc']}")

# --- Barrido de reparto de grupos en modo especulativo (P=8) ---
for ga in [1, 2, 4, 6]:
    tr, te, ft, k0, k1, fo = 4000, 1000, 2000, 1, 10, 5
    cmd = ["mpirun", "-np", "8", os.path.join(D, "knn_mpi"),
           "--strategy", "spec", "--groupA", str(ga), "--train", str(tr),
           "--test", str(te), "--finaltrain", str(ft), "--mink", str(k0),
           "--maxk", str(k1), "--folds", str(fo)]
    d = run(cmd, groupA=ga)
    if d:
        print(f"spec-ga ga={ga} tTotal={float(d['tTotal']):.2f}s testAcc={d['testAcc']}")

cols = ["strategy", "P", "train", "test", "finaltrain", "mink", "maxk", "folds",
        "bestK", "cvAcc", "testAcc", "groupA", "tFine", "tFinal", "tTotal"]
with open(os.path.join(RES, "tarea4_knn.csv"), "w") as f:
    f.write(",".join(cols) + "\n")
    for d in rows:
        f.write(",".join(str(d.get(c, "")) for c in cols) + "\n")
print("Tarea 4 listo")
