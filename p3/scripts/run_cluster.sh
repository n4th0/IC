#!/usr/bin/env bash
# run_cluster.sh - Lanza TODA la Practica 3 (Tareas 2, 3 y 4) en un
# multicomputador de varios PCs con Open MPI.
#
#   ./scripts/run_cluster.sh                    # tareas 2 + 3 + 4
#   ./scripts/run_cluster.sh --tarea 2          # solo el hola mundo
#   ./scripts/run_cluster.sh --tarea 4 --ps "1 3 6 9 12"
#   ./scripts/run_cluster.sh --no-setup         # no repetir la preparacion
#   ./scripts/run_cluster.sh --rehearsal        # ensayo en un solo PC
#
# Las IPs y demas parametros se editan en scripts/cluster.env
set -euo pipefail
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=cluster_lib.sh
source "$SCRIPT_DIR/cluster_lib.sh"

TAREA=all
PS=""
NPROCS=""
NO_SETUP=0
REHEARSAL=0
QUICK=0
DRY_RUN=0
SETUP_EXTRA=()

usage() {
    cat <<EOF
Uso: $(basename "$0") [opciones]

  --tarea 2|3|4|all   que tareas ejecutar (por defecto: all)
  --ps "1 3 6 9"     lista de numeros de procesos para los barridos
  --np N              procesos del 'hola mundo' (Tarea 2)
  --no-setup          no ejecuta cluster_setup.sh antes
  --quick             barrido reducido (para probar rapidamente)
  --rehearsal         ensayo en un solo PC (cluster.rehearsal.env)
  --dry-run           muestra las ordenes sin ejecutarlas
  -h, --help          esta ayuda
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --tarea)      TAREA="$2"; shift 2 ;;
        --ps)         PS="$2"; shift 2 ;;
        --np|--procs) NPROCS="$2"; shift 2 ;;
        --no-setup)   NO_SETUP=1; shift ;;
        --quick)      QUICK=1; shift ;;
        --rehearsal)  REHEARSAL=1; shift ;;
        --dry-run)    DRY_RUN=1; SETUP_EXTRA+=(--dry-run); shift ;;
        -h|--help)    usage; exit 0 ;;
        *) die "opcion desconocida: $1 (usa --help)" ;;
    esac
done

[[ "$TAREA" =~ ^(2|3|4|all)$ ]] || die "--tarea debe ser 2, 3, 4 o all"

if (( REHEARSAL )); then
    export CLUSTER_ENV="$SCRIPT_DIR/cluster.rehearsal.env"
    log "MODO ENSAYO: un solo PC ($CLUSTER_ENV)"
fi
load_config

# El repo debe estar exactamente en WORKDIR: los rangos remotos abren el
# dataset con rutas relativas y deben encontrar la misma ruta en cada PC.
if [[ "$REPO_ROOT" != "$WORKDIR" ]]; then
    die "WORKDIR ($WORKDIR) debe ser la ruta de este repo en el maestro ($REPO_ROOT).
  Edita WORKDIR en $CLUSTER_ENV_FILE o mueve/clona el repo ahi."
fi

mkdir -p "$RESULTS_DIR/logs"
RUN_STAMP="$(date +%Y%m%d_%H%M%S)"

# ---------------------------------------------------------------------------
# Preparacion
# ---------------------------------------------------------------------------
if (( NO_SETUP )); then
    log "preparacion omitida (--no-setup)"
    [[ -f "$HOSTFILE" ]] || die "no existe el hostfile $HOSTFILE; ejecuta cluster_setup.sh"
else
    log "ejecutando cluster_setup.sh"
    bash "$SCRIPT_DIR/cluster_setup.sh" "${SETUP_EXTRA[@]}"
fi

# ---------------------------------------------------------------------------
# Informacion del cluster (para la memoria)
# ---------------------------------------------------------------------------
INFO="$RESULTS_DIR/cluster_info.txt"
{
    echo "Fecha            : $(date -Is)"
    echo "Nodos            : ${NODES[*]}"
    echo "Slots por nodo   : $SLOTS_PER_NODE (total $TOTAL_SLOTS)"
    echo "Map-by / bind-to : $MAP_BY / $BIND_TO"
    echo "Hostfile         : $HOSTFILE"
    echo "MPI (maestro)    : $(mpirun --version 2>&1 | head -n1)"
    echo
    echo "== Detalle por nodo =="
    for n in "${NODES[@]}"; do
        if (( DRY_RUN )); then
            printf '%-16s (dry-run)\n' "$n"
        elif [[ "$USE_SSH" == "0" || "$n" == "localhost" ]]; then
            printf '%-16s host=%s nucleos=%s\n' "$n" "$(hostname)" "$(nproc)"
        else
            printf '%-16s %s\n' "$n" \
                "$(remote "$n" "printf 'host=%s nucleos=%s' \"\$(hostname)\" \"\$(nproc)\"" 2>/dev/null || echo 'sin acceso')"
        fi
    done
    echo
    echo "== Hostfile =="
    if [[ -f "$HOSTFILE" ]]; then cat "$HOSTFILE"; else echo "(no generado)"; fi
} > "$INFO"
ok "informacion del cluster -> $INFO"

# ---------------------------------------------------------------------------
# Tarea 2: hola mundo distribuido
# ---------------------------------------------------------------------------
run_tarea2() {
    local np="${NPROCS:-$(default_hello_np)}"
    if (( np > TOTAL_SLOTS )); then np=$TOTAL_SLOTS; fi
    if (( np < 3 )); then die "se necesitan al menos 3 procesos para que exista el rango 2"; fi
    local logf="$RESULTS_DIR/hello_cluster_${RUN_STAMP}.txt"
    log "TAREA 2: hola mundo con $np procesos en ${N_NODES} PC(s)"
    if (( DRY_RUN )); then
        printf '  [dry-run] (cd %s && ' "$P3_ROOT/tarea2_mpi"
        printf 'mpirun %s -hostfile %s --map-by %s --bind-to %s -np %s ./hello_mpi)\n' \
            "$MPI_FLAGS" "$HOSTFILE" "$MAP_BY" "$BIND_TO" "$np"
        return
    fi
    set +e
    (
        cd "$P3_ROOT/tarea2_mpi"
        mpi_launch "$np" ./hello_mpi
    ) 2>&1 | tee "$logf"
    # tee devuelve 0; el fallo real lo detecta mpirun (PIPESTATUS).
    local rc=${PIPESTATUS[0]}
    set -e
    (( rc == 0 )) || die "mpirun fallo (codigo $rc); revisa $logf"

    local nhost
    nhost=$(sed -n 's/.* on \([^ ]*\) processor [0-9]*.*/\1/p' "$logf" | sort -u | wc -l)
    grep -q "Hola" "$logf" || warn "no aparece el saludo del rango 2 (¿menos de 3 procesos?)"
    if (( nhost < 2 && N_NODES >= 2 )); then
        warn "solo se ha visto 1 hostname distinto en la salida."
        warn "Prueba a lanzar con --np ${N_NODES} y MAP_BY=node en cluster.env."
    else
        ok "ejecucion distribuida verificada: $nhost hostname(s) distintos"
    fi
    cp -f "$logf" "$RESULTS_DIR/hello_cluster.txt"
}

# ---------------------------------------------------------------------------
# Tareas 3 y 4: barridos MPI
# ---------------------------------------------------------------------------
sweep_args() {
    local -a a=(--hostfile "$HOSTFILE" --results-dir "$RESULTS_DIR"
                --mpirun-extra="$MPI_FLAGS" --map-by "$MAP_BY"
                --bind-to "$BIND_TO")
    [[ -n "$NET_IFACE" ]] && a+=(--net-iface "$NET_IFACE")
    [[ -n "$PS" ]] && a+=(--np-list "$PS")
    printf '%s\n' "${a[@]}"
}

run_tarea3() {
    local -a a
    mapfile -t a < <(sweep_args)
    if (( QUICK )); then
        a+=(--sizes "1000" --steps 20 --parts "block" --np-list "1 2")
    fi
    log "TAREA 3: barrido N-cuerpos"
    if (( DRY_RUN )); then
        printf '  [dry-run] python3 %s ' "$SCRIPT_DIR/run_tarea3.py"; printf '%q ' "${a[@]}"; echo
        return
    fi
    set +e
    python3 "$SCRIPT_DIR/run_tarea3.py" "${a[@]}" 2>&1 \
        | tee "$RESULTS_DIR/logs/tarea3_${RUN_STAMP}.log"
    local rc=${PIPESTATUS[0]}
    set -e
    (( rc == 0 )) || die "el barrido de Tarea 3 fallo"
}

run_tarea4() {
    local -a a
    mapfile -t a < <(sweep_args)
    if (( QUICK )); then
        a+=(--trains "500" --test 200 --finaltrain 500 --strategies "test spec"
            --np-list "1 2")
    fi
    log "TAREA 4: barrido kNN"
    if (( DRY_RUN )); then
        printf '  [dry-run] python3 %s ' "$SCRIPT_DIR/run_tarea4.py"; printf '%q ' "${a[@]}"; echo
        return
    fi
    set +e
    python3 "$SCRIPT_DIR/run_tarea4.py" "${a[@]}" 2>&1 \
        | tee "$RESULTS_DIR/logs/tarea4_${RUN_STAMP}.log"
    local rc=${PIPESTATUS[0]}
    set -e
    (( rc == 0 )) || die "el barrido de Tarea 4 fallo"
}

case "$TAREA" in
    2)   run_tarea2 ;;
    3)   run_tarea3 ;;
    4)   run_tarea4 ;;
    all) run_tarea2; run_tarea3; run_tarea4 ;;
esac

cat <<EOF

============================================================
 Completado (tarea: $TAREA)
============================================================
 Resultados : $RESULTS_DIR
 Logs       : $RESULTS_DIR/logs
 Info       : $INFO

 Figuras (opcional):
   python3 scripts/plot.py --results "$RESULTS_DIR"
============================================================
EOF
