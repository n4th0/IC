#!/usr/bin/env bash
# cluster_lib.sh - Utilidades comunes para lanzar la Practica 3 en un
# multicomputador (varios PCs) con Open MPI.
#
# Lo usan cluster_setup.sh y run_cluster.sh. No se ejecuta directamente.

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
P3_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd -- "$P3_ROOT/.." && pwd)"

# --- Salida con color (se desactiva si no es una terminal) -----------------
if [[ -t 1 ]]; then
    C_BLUE=$'\033[1;34m'; C_GREEN=$'\033[1;32m'
    C_YELLOW=$'\033[1;33m'; C_RED=$'\033[1;31m'; C_OFF=$'\033[0m'
else
    C_BLUE=''; C_GREEN=''; C_YELLOW=''; C_RED=''; C_OFF=''
fi

log()  { printf '%s[cluster]%s %s\n' "$C_BLUE" "$C_OFF" "$*"; }
ok()   { printf '%s[  ok  ]%s %s\n' "$C_GREEN" "$C_OFF" "$*"; }
warn() { printf '%s[warn]%s %s\n' "$C_YELLOW" "$C_OFF" "$*" >&2; }
die()  { printf '%s[error]%s %s\n' "$C_RED" "$C_OFF" "$*" >&2; exit 1; }

# --- Carga de la configuracion --------------------------------------------
# Prioridad: $CLUSTER_ENV > scripts/cluster.env
load_config() {
    local envfile="${CLUSTER_ENV:-$SCRIPT_DIR/cluster.env}"
    [[ -f "$envfile" ]] || die "no existe '$envfile'.
  Copia la plantilla y edita las IPs:
      cp $SCRIPT_DIR/cluster.env.example $SCRIPT_DIR/cluster.env
      \$EDITOR $SCRIPT_DIR/cluster.env"

    # shellcheck disable=SC1090
    source "$envfile"

    # Permite tanto NODES=(a b c) como NODES="a b c".
    if ! declare -p NODES 2>/dev/null | grep -q 'declare -a'; then
        read -r -a NODES <<< "${NODES:-}"
    fi
    (( ${#NODES[@]} >= 1 )) || die "NODES esta vacio en $envfile"

    MASTER="${NODES[0]}"
    SSH_USER="${SSH_USER:-$USER}"
    WORKDIR="${WORKDIR:-$HOME/IC}"
    SLOTS_PER_NODE="${SLOTS_PER_NODE:-4}"
    MPI_FLAGS="${MPI_FLAGS:--mca plm_rsh_no_tree_spawn 1}"
    MAP_BY="${MAP_BY:-node}"
    BIND_TO="${BIND_TO:-core}"
    SYNC_MODE="${SYNC_MODE:-auto}"
    RESULTS_DIR="${RESULTS_DIR:-$P3_ROOT/results/cluster}"
    HOSTFILE="${HOSTFILE:-$P3_ROOT/hostfile}"
    NET_IFACE="${NET_IFACE:-}"
    USE_SSH="${USE_SSH:-1}"

    N_NODES=${#NODES[@]}
    TOTAL_SLOTS=$(( N_NODES * SLOTS_PER_NODE ))
    export CLUSTER_ENV_FILE="$envfile"
}

# --- Ejecucion remota ------------------------------------------------------
# remote <nodo> <orden...>
remote() {
    local node="$1"; shift
    if [[ "$USE_SSH" == "0" || "$node" == "localhost" ]]; then
        bash -c "$*"
    else
        ssh -o BatchMode=yes -o StrictHostKeyChecking=accept-new \
            -o ConnectTimeout=8 "$SSH_USER@$node" "$*"
    fi
}

can_ssh() { remote "$1" true >/dev/null 2>&1; }

# --- Hostfile --------------------------------------------------------------
generate_hostfile() {
    local out="${1:-$HOSTFILE}"
    mkdir -p "$(dirname -- "$out")"
    {
        echo "# Hostfile generado por cluster_setup.sh - NO editar a mano."
        echo "# Edita NODES y SLOTS_PER_NODE en scripts/cluster.env y reejecuta."
        echo "# Generado: $(date -Is)"
        local n
        for n in "${NODES[@]}"; do
            echo "$n slots=$SLOTS_PER_NODE"
        done
    } > "$out"
    chmod 600 "$out"
}

# Detecta si $WORKDIR es el mismo sistema de ficheros en todos los nodos.
workdir_is_shared() {
    local probe="$WORKDIR/.cluster_probe_$$_$RANDOM"
    mkdir -p "$WORKDIR" 2>/dev/null || return 1
    : > "$probe" || return 1
    local n shared=0
    for n in "${NODES[@]}"; do
        if remote "$n" "test -f '$probe'" >/dev/null 2>&1; then
            shared=1
        else
            shared=0
            break
        fi
    done
    rm -f "$probe"
    (( shared == 1 ))
}

# --- Lanzador MPI ----------------------------------------------------------
# mpi_launch <np> <programa> [args...]
# Usa el hostfile y los flags configurados. Imprime la orden antes de lanzarla.
mpi_launch() {
    local np="$1"; shift
    local -a cmd=(mpirun)

    # shellcheck disable=SC2206
    cmd+=($MPI_FLAGS)

    if [[ -n "$NET_IFACE" ]]; then
        cmd+=(--mca "btl_tcp_if_include" "$NET_IFACE"
              --mca "oob_tcp_if_include" "$NET_IFACE")
    fi

    if [[ "${USE_HOSTFILE:-1}" == "1" && -f "$HOSTFILE" ]]; then
        cmd+=(-hostfile "$HOSTFILE" --map-by "$MAP_BY" --bind-to "$BIND_TO")
    fi

    cmd+=(-np "$np" "$@")

    log "ejecutando: ${cmd[*]}"
    "${cmd[@]}"
}

# Numero de procesos por defecto para el "hola mundo": 2 por nodo, al menos 3.
default_hello_np() {
    local np=$(( N_NODES * 2 ))
    (( np > TOTAL_SLOTS )) && np=$TOTAL_SLOTS
    (( np < 3 )) && np=3
    (( np > TOTAL_SLOTS )) && np=$TOTAL_SLOTS
    echo "$np"
}
