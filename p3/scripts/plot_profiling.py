#!/usr/bin/env python3
"""Genera una figura resumen (formato visual) de los informes de gprof."""
import os, re
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
T1 = os.path.join(ROOT, "tarea1_profiling")
FIG = os.path.join(ROOT, "figures")
os.makedirs(FIG, exist_ok=True)


def parse_flat(path):
    """Devuelve [(%tiempo, nombre)] del perfil plano de un informe de gprof."""
    out = []
    if not os.path.exists(path):
        return out
    started = False
    for line in open(path, errors="replace"):
        if line.strip().startswith("%") and "cumulative" in line:
            started = True
            continue
        if not started:
            continue
        if line.strip() == "" or line.startswith("Call graph"):
            if out:
                break
            continue
        m = re.match(r"\s*(\d+\.\d+)\s+(\S+)\s+(\S+)\s+(.*)$", line)
        if not m:
            continue
        pct = float(m.group(1))
        parts = m.group(4).split()
        i = 0
        while i < len(parts) and re.fullmatch(r"[\d.]+", parts[i]):
            i += 1
        name = " ".join(parts[i:]) or "(?)"
        name = name.split("(")[0].strip()
        if pct > 0:
            out.append((pct, name))
    return out


def simplify(rows):
    """Agrupa las funciones de ordenacion (std::sort) en una sola entrada."""
    agg = {}
    for pct, name in rows:
        key = name
        low = name.lower()
        if ("sort" in low or "unguarded" in low or "linear_insert" in low
                or "operator<" in low):
            key = "std::sort<KNNResult> (aprox.)"
        agg[key] = agg.get(key, 0.0) + pct
    items = sorted(agg.items(), key=lambda kv: -kv[1])
    return [(pct, name) for name, pct in items]


panels = [
    (os.path.join(T1, "nbody", "perfil_O0.txt"), "N-cuerpos  -O0 (3000/50)"),
    (os.path.join(T1, "nbody", "perfil_O2.txt"), "N-cuerpos  -O2 (3000/50)"),
    (os.path.join(T1, "knn2", "perfil_knn_profile_O0.txt"), "kNN  -O0 (driver reducido)"),
    (os.path.join(T1, "knn2", "perfil_knn_O2_full.txt"), "kNN  -O2 (ejecucion completa)"),
]

fig, axes = plt.subplots(2, 2, figsize=(13, 8))
for ax, (path, title) in zip(axes.ravel(), panels):
    rows = simplify(parse_flat(path))[:6]
    rows = rows[::-1]
    names = [n if len(n) < 34 else n[:31] + "..." for _, n in rows]
    vals = [p for p, _ in rows]
    ax.barh(range(len(rows)), vals, color="#2b6cb0")
    ax.set_yticks(range(len(rows)))
    ax.set_yticklabels(names, fontsize=8)
    ax.set_xlabel("% del tiempo de CPU (gprof)")
    ax.set_title(title, fontsize=10)
    for i, v in enumerate(vals):
        ax.text(v + 0.5, i, f"{v:.1f}", va="center", fontsize=8)
    ax.set_xlim(0, max(vals + [10]) * 1.18)
    ax.grid(axis="x", alpha=0.3)

fig.suptitle("Perfiles de gprof (-O0 vs -O2)", fontsize=13, weight="bold")
fig.tight_layout(rect=[0, 0, 1, 0.97])
fig.savefig(os.path.join(FIG, "fig_profiling.png"), dpi=130)
print("figura ->", os.path.join(FIG, "fig_profiling.png"))
