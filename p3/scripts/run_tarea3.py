#!/usr/bin/env python3
"""Tarea 3 - barrido de tamano/num. procesos del simulador N-cuerpos MPI."""
import os, re, subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = os.path.join(ROOT, "tarea3_nbody")
RES = os.path.join(ROOT, "results")
os.makedirs(RES, exist_ok=True)

STEPS = 50
NS = [2000, 4000, 8000]
PS = [1, 2, 4, 8]
PARTS = ["block", "cyclic"]

# --- Secuencial (referencia) ---
seq = {}
for n in NS:
    out = subprocess.run([os.path.join(D, "nbody_seq"), str(n), str(STEPS)],
                         capture_output=True, text=True).stdout
    seq[n] = float(re.search(r"Tiempo total:\s*([\d.]+)", out).group(1))
with open(os.path.join(RES, "tarea3_seq.csv"), "w") as f:
    f.write("n,steps,wall\n")
    for n in NS:
        f.write(f"{n},{STEPS},{seq[n]:.6f}\n")
    print("seq:", {n: round(seq[n], 3) for n in NS})

# --- Paralelo ---
rows = []
for part in PARTS:
    for n in NS:
        for P in PS:
            cmd = ["mpirun", "-np", str(P), os.path.join(D, "nbody_mpi"),
                   str(n), str(STEPS), part]
            out = subprocess.run(cmd, capture_output=True, text=True).stdout
            m = re.search(r"RESULT .*wall=([\d.eE+-]+) comp=([\d.eE+-]+) comm=([\d.eE+-]+)", out)
            if not m:
                print("FALLO:", cmd, out[-200:]); continue
            wall, comp, comm = map(float, m.groups())
            rows.append((part, P, n, STEPS, wall, comp, comm, seq[n] / wall))
            print(f"{part} n={n} P={P} wall={wall:.3f} speedup={seq[n]/wall:.2f}")

with open(os.path.join(RES, "tarea3_nbody.csv"), "w") as f:
    f.write("part,P,n,steps,wall,comp,comm,speedup\n")
    for r in rows:
        f.write(f"{r[0]},{r[1]},{r[2]},{r[3]},{r[4]:.6f},{r[5]:.6f},{r[6]:.6f},{r[7]:.4f}\n")
print("Tarea 3 listo")
