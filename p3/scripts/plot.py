#!/usr/bin/env python3
"""Genera las figuras de la memoria a partir de los CSV en results/."""
import csv, os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RES = os.path.join(ROOT, "results")
FIG = os.path.join(ROOT, "figures")
os.makedirs(FIG, exist_ok=True)


def read(name):
    with open(os.path.join(RES, name)) as f:
        return list(csv.DictReader(f))


# ---------- Tarea 0 ----------
def tarea0():
    v = read("tarea0_vector.csv")
    m = read("tarea0_matrix.csv")
    fig, ax = plt.subplots(1, 2, figsize=(12, 4.5))

    sizes = [int(r["size"]) for r in v]
    a = [float(r["a_pushback_s"]) for r in v]
    b = [float(r["b_presized_s"]) for r in v]
    g = [float(r["gain"]) for r in v]
    x = range(len(v))
    ax[0].bar([i - 0.2 for i in x], a, width=0.4, label="push_back (A)")
    ax[0].bar([i + 0.2 for i in x], b, width=0.4, label="pre-sized [i] (B)")
    ax[0].set_xticks(list(x)); ax[0].set_xticklabels([f"{s:,}" for s in sizes], rotation=15)
    ax[0].set_xlabel("SIZE"); ax[0].set_ylabel("tiempo (s)")
    ax[0].set_title("Vectores dinamicos"); ax[0].legend()
    ax[0].grid(axis="y", alpha=0.3)
    ax2 = ax[0].twinx(); ax2.plot(list(x), g, "ko--", label="ganancia A/B")
    ax2.set_ylabel("ganancia"); ax2.legend(loc="upper right")

    N = [int(r["N"]) for r in m]
    ac = [float(r["a_colmajor_s"]) for r in m]
    br = [float(r["b_rowmajor_s"]) for r in m]
    gm = [float(r["gain"]) for r in m]
    ax[1].plot(N, ac, "o-", label="por columnas a[j][i]")
    ax[1].plot(N, br, "s-", label="por filas b[i][j]")
    ax[1].set_xlabel("N (matriz NxN)"); ax[1].set_ylabel("tiempo (s)")
    ax[1].set_title("Recorrido de matrices"); ax[1].legend(); ax[1].grid(alpha=0.3)
    ax3 = ax[1].twinx(); ax3.plot(N, gm, "k^--", label="ganancia col/fila")
    ax3.set_ylabel("ganancia"); ax3.legend(loc="upper left")

    fig.tight_layout(); fig.savefig(os.path.join(FIG, "fig_tarea0.png"), dpi=130)
    plt.close(fig)


# ---------- Tarea 3 ----------
def tarea3():
    seq = {int(r["n"]): float(r["wall"]) for r in read("tarea3_seq.csv")}
    rows = read("tarea3_nbody.csv")
    ns = sorted({int(r["n"]) for r in rows})
    fig, ax = plt.subplots(1, 2, figsize=(12, 4.5))
    for part, style in [("block", "-o"), ("cyclic", "--s")]:
        for n in ns:
            pts = sorted([r for r in rows if r["part"] == part and int(r["n"]) == n],
                         key=lambda r: int(r["P"]))
            Ps = [int(r["P"]) for r in pts]
            sp = [seq[n] / float(r["wall"]) for r in pts]
            ax[0].plot(Ps, sp, style, label=f"{part}, n={n}")
            ax[1].plot(Ps, [s / p for s, p in zip(sp, Ps)], style, label=f"{part}, n={n}")
    Ps = sorted({int(r["P"]) for r in rows})
    ax[0].plot(Ps, Ps, "k:", label="ideal")
    ax[0].set_xlabel("procesos (P)"); ax[0].set_ylabel("speed-up")
    ax[0].set_title("N-cuerpos: speed-up"); ax[0].legend(fontsize=8); ax[0].grid(alpha=0.3)
    ax[1].set_xlabel("procesos (P)"); ax[1].set_ylabel("eficiencia")
    ax[1].set_title("N-cuerpos: eficiencia"); ax[1].legend(fontsize=8); ax[1].grid(alpha=0.3)
    fig.tight_layout(); fig.savefig(os.path.join(FIG, "fig_tarea3_speedup.png"), dpi=130)
    plt.close(fig)

    # fraccion de comunicacion
    fig, ax = plt.subplots(figsize=(6.5, 4.5))
    for part, style in [("block", "-o"), ("cyclic", "--s")]:
        for n in ns:
            pts = sorted([r for r in rows if r["part"] == part and int(r["n"]) == n],
                         key=lambda r: int(r["P"]))
            Ps = [int(r["P"]) for r in pts]
            frac = [100 * float(r["comm"]) / (float(r["comp"]) + float(r["comm"])) for r in pts]
            ax.plot(Ps, frac, style, label=f"{part}, n={n}")
    ax.set_xlabel("procesos (P)"); ax.set_ylabel("% tiempo en comunicaciones")
    ax.set_title("N-cuerpos: coste de comunicacion"); ax.legend(fontsize=8); ax.grid(alpha=0.3)
    fig.tight_layout(); fig.savefig(os.path.join(FIG, "fig_tarea3_comm.png"), dpi=130)
    plt.close(fig)


# ---------- Tarea 4 ----------
def tarea4():
    rows = read("tarea4_knn.csv")
    for r in rows:
        for k in ["P", "train", "test", "bestK", "groupA"]:
            r[k] = int(r[k])
        for k in ["tTotal", "tFine", "tFinal", "testAcc", "cvAcc"]:
            r[k] = float(r[k])
    trains = sorted({r["train"] for r in rows})
    strats = ["test", "k", "folds", "spec"]
    colors = {"test": "C0", "k": "C1", "folds": "C2", "spec": "C3"}

    for tr in trains:
        base = [r for r in rows if r["strategy"] == "seq" and r["train"] == tr]
        if not base:
            continue
        tseq = base[0]["tTotal"]
        fig, ax = plt.subplots(1, 2, figsize=(12, 4.5))
        for s in strats:
            cand = [r for r in rows if r["strategy"] == s and r["train"] == tr]
            if s == "spec":
                cand = [r for r in cand if r["groupA"] == max(1, r["P"] // 2)]
            pts = []
            seenP = set()
            for r in sorted(cand, key=lambda r: r["P"]):
                if r["P"] in seenP:
                    continue
                seenP.add(r["P"]); pts.append(r)
            if not pts:
                continue
            Ps = [r["P"] for r in pts]
            sp = [tseq / r["tTotal"] for r in pts]
            ax[0].plot(Ps, sp, "-o", color=colors[s], label=s)
            ax[1].plot(Ps, [x / p for x, p in zip(sp, Ps)], "-o", color=colors[s], label=s)
        ax[0].plot(Ps, Ps, "k:", label="ideal")
        ax[0].set_xlabel("procesos (P)"); ax[0].set_ylabel("speed-up")
        ax[0].set_title(f"kNN: speed-up (train={tr})"); ax[0].legend(); ax[0].grid(alpha=0.3)
        ax[1].set_xlabel("procesos (P)"); ax[1].set_ylabel("eficiencia")
        ax[1].set_title(f"kNN: eficiencia (train={tr})"); ax[1].legend(); ax[1].grid(alpha=0.3)
        fig.tight_layout()
        fig.savefig(os.path.join(FIG, f"fig_tarea4_speedup_train{tr}.png"), dpi=130)
        plt.close(fig)

    # reparto de grupos en modo especulativo (P=8, train=4000)
    allp = sorted([r for r in rows if r["strategy"] == "spec" and r["P"] == 8
                   and r["train"] == 4000], key=lambda r: r["groupA"])
    seen = set(); pts = []
    for r in allp:
        if r["groupA"] in seen:
            continue
        seen.add(r["groupA"]); pts.append(r)
    if pts:
        fig, ax = plt.subplots(figsize=(6.5, 4.5))
        ax.bar([str(r["groupA"]) for r in pts], [r["tTotal"] for r in pts], color="C3")
        ax.set_xlabel("tamano del grupo A (fine-tuning)")
        ax.set_ylabel("tiempo total (s)")
        ax.set_title("kNN especulativo (P=8): reparto de grupos")
        ax.grid(axis="y", alpha=0.3)
        fig.tight_layout(); fig.savefig(os.path.join(FIG, "fig_tarea4_spec_groups.png"), dpi=130)
        plt.close(fig)

    # exactitud por estrategia (control de correctitud)
    fig, ax = plt.subplots(figsize=(7.5, 4.5))
    labels, accs = [], []
    for s in ["seq"] + strats:
        cand = [r for r in rows if r["strategy"] == s and r["train"] == 4000]
        if not cand:
            continue
        r = max(cand, key=lambda z: z["P"])
        labels.append(f"{s}\n(P={r['P']})")
        accs.append(r["testAcc"])
    ax.bar(labels, accs, color="C0")
    ax.set_ylabel("accuracy de test")
    ax.set_ylim(0, 1); ax.grid(axis="y", alpha=0.3)
    ax.set_title("kNN: exactitud de test por estrategia (train=4000)")
    fig.tight_layout(); fig.savefig(os.path.join(FIG, "fig_tarea4_accuracy.png"), dpi=130)
    plt.close(fig)


if __name__ == "__main__":
    tarea0(); tarea3(); tarea4()
    print("Figuras ->", FIG)
