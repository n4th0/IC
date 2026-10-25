#!/usr/bin/env bash
# cluster_setup.sh - Prepara un multicomputador (>=3 PCs) para ejecutar la
# Practica 3 con MPI: claves SSH, hostfile, copia de codigo/dataset y build.
#
#   ./scripts/cluster_setup.sh              # configuracion normal
#   ./scripts/cluster_setup.sh --dry-run    # solo muestra lo que haria
#   ./scripts/cluster_setup.sh --test       # ademas lanza el hola mundo
#
# Toda la configuracion (IPs, usuario, directorio, slots) esta en
# scripts/cluster.env  -> ver scripts/cluster.env.example
set -euo pipefail
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=cluster_lib.sh
source "$SCRIPT_DIR/cluster_lib.sh"

DRY_RUN=0
SKIP_KEYS=0
DO_BUILD=1
DO_TEST=0

usage() {
    cat <<EOF
Uso: $(basename "$0") [opciones]

  --dry-run       muestra las acciones sin ejecutarlas
  --no-keys       no crea ni copia claves SSH (solo comprueba)
  --no-build      no compila los ejecutables
  --test          lanza el 'hola mundo' al terminar
  -h, --help      esta ayuda

Configuracion: $SCRIPT_DIR/cluster.env (copia cluster.env.example)
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dry-run)  DRY_RUN=1 ;;
        --no-keys)  SKIP_KEYS=1 ;;
        --no-build) DO_BUILD=0 ;;
        --test)     DO_TEST=1 ;;
        -h|--help)  usage; exit 0 ;;
        *) die "opcion desconocida: $1 (usa --help)" ;;
    esac
    shift
done

run() {
    if (( DRY_RUN )); then
        printf '  [dry-run] %s\n' "$*"
    else
        "$@"
    fi
}

load_config
log "fichero de configuracion: $CLUSTER_ENV_FILE"
log "nodos (${N_NODES}): ${NODES[*]}  -> maestro: $MASTER"
log "directorio de trabajo: $WORKDIR   slots/nodo: $SLOTS_PER_NODE   slots totales: $TOTAL_SLOTS"

if [[ "$REPO_ROOT" != "$WORKDIR" ]]; then
    die "WORKDIR ($WORKDIR) debe coincidir con la ruta del repo en el maestro ($REPO_ROOT).
  Los procesos remotos abren el dataset con rutas relativas, asi que todos los
  nodos deben ver el proyecto en la MISMA ruta. Ajusta WORKDIR en:
      $CLUSTER_ENV_FILE"
fi

# ---------------------------------------------------------------------------
# 1. Herramientas locales
# ---------------------------------------------------------------------------
log "1/7 comprobando herramientas locales"
for t in mpirun mpicc mpic++ g++ ssh tar; do
    command -v "$t" >/dev/null 2>&1 || die "falta '$t'. Instala Open MPI y las herramientas de compilacion:
    sudo apt update && sudo apt install -y openmpi-bin libopenmpi-dev build-essential"
done
LOCAL_MPI_VERSION="$(mpirun --version 2>&1 | head -n1)"
ok "$LOCAL_MPI_VERSION"

# ---------------------------------------------------------------------------
# 2. Acceso SSH sin contrasena
# ---------------------------------------------------------------------------
log "2/7 comprobando acceso SSH sin contrasena"
if [[ "$USE_SSH" == "0" ]]; then
    warn "USE_SSH=0 (modo ensayo): se omite SSH y se ejecuta todo en local"
elif (( DRY_RUN )); then
    for n in "${NODES[@]}"; do
        printf '  [dry-run] ssh -o BatchMode=yes %s@%s true\n' "$SSH_USER" "$n"
    done
else
    missing=()
    for n in "${NODES[@]}"; do
        if can_ssh "$n"; then
            ok "ssh $SSH_USER@$n"
        else
            missing+=("$n")
        fi
    done

    if (( ${#missing[@]} > 0 )); then
        warn "sin acceso SSH a: ${missing[*]}"
        if (( SKIP_KEYS )); then
            die "configura las claves a mano (ver docs/CLUSTER.md, paso 4) o quita --no-keys"
        fi
        [[ -f "$HOME/.ssh/id_rsa" ]] || run ssh-keygen -t rsa -b 4096 -N "" -f "$HOME/.ssh/id_rsa"
        chmod 700 "$HOME/.ssh" 2>/dev/null || true
        if [[ -t 0 ]]; then
            for n in "${missing[@]}"; do
                log "copiando clave publica a $SSH_USER@$n (te pedira la contrasena UNA vez)"
                run ssh-copy-id -i "$HOME/.ssh/id_rsa.pub" "$SSH_USER@$n" \
                    || die "ssh-copy-id fallo para $n"
            done
        else
            die "no hay terminal interactiva para pedir contrasenas. Ejecuta a mano:
    ssh-keygen -t rsa
    ssh-copy-id $SSH_USER@${missing[0]}
    (repite para: ${missing[*]})"
        fi
        # Verificacion final
        for n in "${NODES[@]}"; do
            can_ssh "$n" || die "sigue sin haber acceso SSH sin contrasena a $n"
            ok "ssh $SSH_USER@$n"
        done
    fi
fi

# Asegura permisos correctos de authorized_keys en cada nodo.
if [[ "$USE_SSH" != "0" ]] && (( ! DRY_RUN )); then
    for n in "${NODES[@]}"; do
        run remote "$n" "chmod 700 ~/.ssh 2>/dev/null; chmod 600 ~/.ssh/authorized_keys 2>/dev/null; true"
    done
fi

# ---------------------------------------------------------------------------
# 3. Open MPI en todos los nodos y misma version
# ---------------------------------------------------------------------------
log "3/7 comprobando Open MPI en cada nodo"
versions=()
for n in "${NODES[@]}"; do
    if [[ "$USE_SSH" == "0" || "$n" == "localhost" ]]; then
        v="$LOCAL_MPI_VERSION"
    elif (( DRY_RUN )); then
        v="(se comprobara en la ejecucion real)"
    else
        if ! remote "$n" "command -v mpirun >/dev/null && command -v mpic++ >/dev/null"; then
            die "en $n falta Open MPI. Ejecuta alli:
    sudo apt update && sudo apt install -y openmpi-bin libopenmpi-dev"
        fi
        v="$(remote "$n" "mpirun --version 2>&1 | head -n1")"
    fi
    versions+=("$v")
    ok "$n -> $v"
done
# Compara la cadena de version (ignora diferencias de formato menores).
first="${versions[0]}"
for v in "${versions[@]}"; do
    [[ "$v" == "$first" ]] || warn "versiones de Open MPI distintas: '$first' vs '$v'. Puede fallar el lanzamiento."
done

# ---------------------------------------------------------------------------
# 4. Detectar almacenamiento compartido y sincronizar
# ---------------------------------------------------------------------------
log "4/7 comprobando si \$WORKDIR es compartido por los nodos"
mode="$SYNC_MODE"
if [[ "$mode" == "auto" ]]; then
    if (( DRY_RUN )); then
        mode="auto"
    elif workdir_is_shared; then
        mode="nfs"
    else
        mode="copy"
    fi
fi
log "modo de sincronizacion: $mode"

if [[ "$mode" == "copy" ]] || { [[ "$mode" == "auto" ]] && (( DRY_RUN )); }; then
    for n in "${NODES[@]}"; do
        run remote "$n" "mkdir -p '$WORKDIR'"
    done
    log "copiando codigo (sin .git ni resultados) a ${N_NODES} nodos..."
    # --exclude del .git y de resultados/figuras para que sea rapido.
    for n in "${NODES[@]}"; do
        if [[ "$n" == "$MASTER" ]]; then
            ok "$n es el maestro: ya tiene el codigo en $WORKDIR"
            continue
        fi
        if (( DRY_RUN )); then
            printf '  [dry-run] tar | ssh %s tar (copia del repo + dataset)\n' "$n"
        else
            tar czf - -C "$REPO_ROOT" \
                --exclude='./.git' \
                --exclude='./p3/results' \
                --exclude='./p3/figures' \
                --exclude='./p3/hostfile' \
                --exclude='*/build/*.o' \
                . | remote "$n" "tar xzf - -C '$WORKDIR'"
            ok "codigo copiado en $n:$WORKDIR"
        fi
    done
elif [[ "$mode" == "nfs" ]]; then
    ok "se usa el mismo $WORKDIR en todos los nodos (no se copia nada)"
else
    warn "modo '$mode' desconocido; se asume copy"
    mode="copy"
fi

# ---------------------------------------------------------------------------
# 5. Hostfile
# ---------------------------------------------------------------------------
log "5/7 generando hostfile: $HOSTFILE"
if (( DRY_RUN )); then
    sed 's/^/  /' <<< "$(for n in "${NODES[@]}"; do echo "$n slots=$SLOTS_PER_NODE"; done)"
else
    generate_hostfile
    ok "hostfile creado (chmod 600)"
    if [[ "$mode" == "copy" ]]; then
        for n in "${NODES[@]}"; do
            [[ "$n" == "$MASTER" ]] && continue
            remote "$n" "mkdir -p '$(dirname "$HOSTFILE")'" || true
            scp -q "$HOSTFILE" "$SSH_USER@$n:$HOSTFILE" || warn "no se pudo copiar el hostfile a $n"
        done
    fi
fi

# ---------------------------------------------------------------------------
# 6. Compilacion
# ---------------------------------------------------------------------------
if (( DO_BUILD )); then
    log "6/7 compilando ejecutables en el maestro"
    if (( DRY_RUN )); then
        printf '  [dry-run] bash %s\n' "$SCRIPT_DIR/build_all.sh"
    else
        bash "$SCRIPT_DIR/build_all.sh"
        # En modo copy los binarios no estan en los demas nodos: se copian.
        if [[ "$mode" == "copy" ]]; then
            for n in "${NODES[@]}"; do
                [[ "$n" == "$MASTER" ]] && continue
                for b in \
                    p3/tarea2_mpi/hello_mpi \
                    p3/tarea3_nbody/nbody_mpi \
                    p3/tarea3_nbody/nbody_seq \
                    p3/tarea4_knn/knn_mpi \
                    p3/tarea4_knn/seq_knn; do
                    scp -q "$REPO_ROOT/$b" "$SSH_USER@$n:$WORKDIR/$b" \
                        || warn "no se pudo copiar $b a $n"
                done
                remote "$n" "chmod o+x '$WORKDIR'/p3/tarea2_mpi/hello_mpi \
                    '$WORKDIR'/p3/tarea3_nbody/nbody_mpi \
                    '$WORKDIR'/p3/tarea4_knn/knn_mpi" || true
            done
            ok "binarios copiados a todos los nodos"
        fi
    fi
else
    log "6/7 compilacion omitida (--no-build)"
fi

# ---------------------------------------------------------------------------
# 7. Verificacion final
# ---------------------------------------------------------------------------
log "7/7 verificando rutas, dataset y ejecutables en cada nodo"
for n in "${NODES[@]}"; do
    if (( DRY_RUN )); then continue; fi
    if [[ "$USE_SSH" == "0" || "$n" == "localhost" ]]; then
        ok "$n -> local"
        continue
    fi
    remote "$n" "test -x '$WORKDIR/p3/tarea2_mpi/hello_mpi' \
        && test -x '$WORKDIR/p3/tarea3_nbody/nbody_mpi' \
        && test -x '$WORKDIR/p3/tarea4_knn/knn_mpi' \
        && test -f '$WORKDIR/knn2/build/train/train-images-idx3-ubyte' \
        && test -f '$WORKDIR/knn2/build/test/t10k-images-idx3-ubyte'" \
        || die "en $n faltan ejecutables o el dataset MNIST en $WORKDIR"
    ok "$n -> ejecutables y dataset OK"
done

cat <<EOF

============================================================
 Cluster preparado
============================================================
 Nodos        : ${NODES[*]}
 Slots/nodo   : $SLOTS_PER_NODE  (total $TOTAL_SLOTS)
 Hostfile     : $HOSTFILE
 Modo sync    : $mode
 Resultados   : $RESULTS_DIR

 Siguiente paso:
   $SCRIPT_DIR/run_cluster.sh --tarea 2      # hola mundo en los 3 PCs
   $SCRIPT_DIR/run_cluster.sh                # tareas 2 + 3 + 4
============================================================
EOF

if (( DO_TEST )); then
    log "lanzando prueba 'hola mundo' con $(default_hello_np) procesos"
    ( cd "$P3_ROOT/tarea2_mpi" && mpi_launch "$(default_hello_np)" ./hello_mpi )
fi
