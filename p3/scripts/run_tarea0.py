#!/usr/bin/env python3
"""Tarea 0 - ejecuta los micro-benchmarks y guarda los resultados en CSV."""
import os, re, subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = os.path.join(ROOT, "tarea0")
RES = os.path.join(ROOT, "results")
os.makedirs(RES, exist_ok=True)

# --- Vectores dinamicos: push_back vs tamano reservado ---
sizes = [100_000, 1_000_000, 10_000_000, 50_000_000]
vec_rows = []
for s in sizes:
    exe = os.path.join(D, f"vb_{s}")
    subprocess.run(["g++", "-O2", "-std=c++17", f"-DSIZE={s}",
                    os.path.join(D, "vector_bench.cpp"), "-o", exe], check=True)
    out = subprocess.run([exe], capture_output=True, text=True).stdout
    a = float(re.search(r"A \(push_back\)\s*:\s*([\d.eE+-]+)", out).group(1))
    b = float(re.search(r"B \(pre-sized \[i\]\)\s*:\s*([\d.eE+-]+)", out).group(1))
    vec_rows.append((s, a, b, a / b))

with open(os.path.join(RES, "tarea0_vector.csv"), "w") as f:
    f.write("size,a_pushback_s,b_presized_s,gain\n")
    for r in vec_rows:
        f.write(f"{r[0]},{r[1]:.6f},{r[2]:.6f},{r[3]:.4f}\n")

# --- Recorrido de matrices: por columnas vs por filas ---
subprocess.run(["g++", "-O2", "-std=c++17", os.path.join(D, "matrix_bench.cpp"),
                "-o", os.path.join(D, "matrix_bench")], check=True)
mat_rows = []
for n in [500, 1000, 1500, 2000, 3000, 4000, 6000, 8000]:
    out = subprocess.run([os.path.join(D, "matrix_bench"), str(n)],
                         capture_output=True, text=True).stdout
    m = re.search(r"N=(\d+) A_col=([\d.eE+-]+) B_row=([\d.eE+-]+) gain=([\d.eE+-]+)", out)
    mat_rows.append((int(m.group(1)), float(m.group(2)), float(m.group(3)), float(m.group(4))))

with open(os.path.join(RES, "tarea0_matrix.csv"), "w") as f:
    f.write("N,a_colmajor_s,b_rowmajor_s,gain\n")
    for r in mat_rows:
        f.write(f"{r[0]},{r[1]:.6f},{r[2]:.6f},{r[3]:.4f}\n")

print("Tarea 0 listo ->", RES)
