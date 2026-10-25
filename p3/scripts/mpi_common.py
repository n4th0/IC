#!/usr/bin/env python3
"""Utilidades MPI compartidas por los barridos de Tarea 3 y Tarea 4.

Permiten que los mismos scripts funcionen en local (sin hostfile) y en el
multicomputador (con --hostfile), sin duplicar la construccion de 'mpirun'.
"""
import os
import shlex


def hostfile_info(path):
    """Devuelve (lista_de_nodos, slots_totales) de un hostfile de Open MPI."""
    nodes, total = [], 0
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            parts = line.split()
            host = parts[0]
            slots = 1
            for p in parts[1:]:
                if p.startswith("slots="):
                    try:
                        slots = int(p.split("=", 1)[1])
                    except ValueError:
                        pass
            nodes.append(host)
            total += slots
    return nodes, total


def default_np_list(hostfile):
    """Lista de P por defecto.

    En local mantiene el barrido historico [1, 2, 4, 8]. En cluster anade los
    multiplos del numero de PCs (1, 3, 6, 9...) para poder representar el
    speed-up en funcion del numero de nodos, que es lo que pide la guia.
    """
    if not hostfile or not os.path.exists(hostfile):
        return [1, 2, 4, 8]

    nodes, total = hostfile_info(hostfile)
    n = max(1, len(nodes))
    ps = {1, total}
    for k in range(1, total // n + 1):
        ps.add(min(k * n, total))
    for p in (2, 4, 6, 8, 12, 16, 24, 32):
        if p <= total:
            ps.add(p)
    return sorted(ps)


def mpirun_cmd(np, hostfile=None, extra="", map_by="node", bind_to="core",
               net_iface=""):
    """Construye la orden mpirun para P procesos."""
    cmd = ["mpirun"]
    if extra:
        cmd += shlex.split(extra)
    if net_iface:
        cmd += ["--mca", "btl_tcp_if_include", net_iface,
                "--mca", "oob_tcp_if_include", net_iface]
    if hostfile and os.path.exists(hostfile):
        cmd += ["-hostfile", hostfile, "--map-by", map_by,
                "--bind-to", bind_to]
    cmd += ["-np", str(np)]
    return cmd


def add_cluster_args(parser, default_results):
    """Anade las opciones comunes de cluster/local a un ArgumentParser."""
    g = parser.add_argument_group("ejecucion")
    g.add_argument("--hostfile", default=os.environ.get("HOSTFILE", ""),
                   help="hostfile de Open MPI (si se omite, todo en localhost)")
    g.add_argument("--np-list", default="",
                   help="lista de numeros de procesos, p.ej. '1 2 4 8'")
    g.add_argument("--results-dir", default=default_results,
                   help="directorio donde escribir los CSV")
    g.add_argument("--mpirun-extra", default=os.environ.get("MPIRUN_EXTRA", ""),
                   help="flags extra para mpirun (p.ej. -mca ...)")
    g.add_argument("--map-by", default="node", choices=["node", "slot", "core"],
                   help="reparto de rangos entre nodos (por defecto: node)")
    g.add_argument("--bind-to", default="core",
                   help="fijado de procesos a nucleos (por defecto: core)")
    g.add_argument("--net-iface", default=os.environ.get("NET_IFACE", ""),
                   help="interfaz de red para Open MPI (p.ej. eth0)")
    return g


def parse_np_list(text, hostfile):
    if text.strip():
        return sorted({int(x) for x in text.replace(",", " ").split()})
    return default_np_list(hostfile)
