#!/usr/bin/env python3
"""Genera los grafos de flujo (secuencial vs paralelo) para la memoria."""
import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIG = os.path.join(ROOT, "figures")
os.makedirs(FIG, exist_ok=True)


def box(ax, x, y, w, h, text, color="#dbe9f6", ec="#2b6cb0", fs=9):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.02,rounding_size=0.02",
                                fc=color, ec=ec, lw=1.2))
    ax.text(x + w / 2, y + h / 2, text, ha="center", va="center", fontsize=fs, wrap=True)


def arrow(ax, p, q, color="#333", style="-|>", rad=0.0):
    ax.add_patch(FancyArrowPatch(p, q, arrowstyle=style, mutation_scale=12,
                                 color=color, lw=1.2,
                                 connectionstyle=f"arc3,rad={rad}"))


def tarea3_diagram():
    fig, ax = plt.subplots(figsize=(11, 5.5))
    ax.set_xlim(0, 11); ax.set_ylim(0, 5.5); ax.axis("off")
    ax.text(2.4, 5.2, "Secuencial (src/main.cpp)", ha="center", fontsize=11, weight="bold")
    ax.text(8.4, 5.2, "MPI: descomposicion de datos", ha="center", fontsize=11, weight="bold")

    # secuencial
    seq = [("init_particles(n)", 4.4), ("compute_forces  O(n^2)", 3.6), ("integrate", 2.8),
           ("report_step (I/O)", 2.0)]
    for t, y in seq:
        box(ax, 0.4, y, 4.0, 0.6, t)
    for i in range(len(seq) - 1):
        arrow(ax, (2.4, seq[i][1]), (2.4, seq[i + 1][1] + 0.6))
    arrow(ax, (2.4, 2.0), (2.4, 4.4), color="#b03", rad=-0.55)
    ax.text(3.9, 3.3, "n_steps", color="#b03", fontsize=8, rotation=90)

    # paralelo
    par = [("init_particles + reparto", 4.4), ("MPI_Allgatherv posiciones  O(n)", 3.6),
           ("compute_forces (subconjunto)  O(n_local*n)", 2.8),
           ("integrate (subconjunto)", 2.0), ("MPI_Reduce energia -> rank 0", 1.2)]
    for t, y in par:
        box(ax, 6.4, y, 4.2, 0.6, t, color="#e6f6ea", ec="#2f855a")
    for i in range(len(par) - 1):
        arrow(ax, (8.5, par[i][1]), (8.5, par[i + 1][1] + 0.6), color="#2f855a")
    arrow(ax, (8.5, 1.2), (8.5, 4.4), color="#b03", rad=-0.5)
    ax.text(10.6, 2.9, "n_steps", color="#b03", fontsize=8, rotation=90)
    fig.tight_layout(); fig.savefig(os.path.join(FIG, "fig_tarea3_flow.png"), dpi=130)
    plt.close(fig)


def tarea4_diagram():
    fig, ax = plt.subplots(figsize=(11.5, 6))
    ax.set_xlim(0, 11.5); ax.set_ylim(0, 6); ax.axis("off")
    ax.text(2.2, 5.7, "Secuencial (knn2)", ha="center", fontsize=11, weight="bold")
    ax.text(8.6, 5.7, "MPI (estrategias)", ha="center", fontsize=11, weight="bold")

    seq = [("load MNIST", 5.0), ("for k in [minK,maxK]", 4.2),
           ("  for fold in folds", 3.4), ("    KNN(train).classify(val)", 2.6),
           ("best k = argmax accuracy", 1.8), ("KNN(train).classify(test)", 1.0)]
    for t, y in seq:
        box(ax, 0.2, y, 4.0, 0.6, t)
    for i in range(len(seq) - 1):
        arrow(ax, (2.2, seq[i][1]), (2.2, seq[i + 1][1] + 0.6))

    strs = [("test : reparto de muestras de test/val", 5.0),
            ("k : reparto de valores de K", 4.2),
            ("folds : reparto de folds", 3.4),
            ("spec : fine-tuning || evaluacion especulativa", 2.6),
            ("MPI_Allreduce / MPI_Allgatherv de resultados", 1.8)]
    for t, y in strs:
        box(ax, 6.2, y, 5.1, 0.6, t, color="#fdeede", ec="#b7791f")
    for i in range(len(strs) - 1):
        arrow(ax, (8.75, strs[i][1]), (8.75, strs[i + 1][1] + 0.6), color="#b7791f")
    fig.tight_layout(); fig.savefig(os.path.join(FIG, "fig_tarea4_flow.png"), dpi=130)
    plt.close(fig)


if __name__ == "__main__":
    tarea3_diagram(); tarea4_diagram()
    print("Diagramas ->", FIG)
