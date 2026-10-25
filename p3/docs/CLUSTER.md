# Guía: ejecutar la Práctica 3 en varios PCs (cluster MPI)

Esta guía explica, paso a paso, cómo lanzar **las Tareas 2, 3 y 4** en un
multicomputador de **3 PCs** (o más) usando Open MPI, tal y como pide el
enunciado. Todos los comandos están automatizados en `p3/scripts/`, pero aquí
se explica qué hace cada uno para poder defenderlo en la memoria.

```
        PC1 (maestro)                PC2                     PC3
     111.111.111.111           222.222.222.222         333.333.333.333
   +----------------+        +----------------+      +----------------+
   |  mpirun        |--ssh-->|  proceso MPI   |      |  proceso MPI   |
   |  proceso MPI   |--ssh-->|  proceso MPI   |----->|  proceso MPI   |
   |  proceso MPI   |        |  proceso MPI   |      |  proceso MPI   |
   +----------------+        +----------------+      +----------------+
        mismo usuario, mismo $HOME/IC (NFS) o copia automática por SSH
```

---

## 0. Dónde se cambian las IPs (lo primero)

**Todos los parámetros del cluster están en un único fichero:**

```
p3/scripts/cluster.env
```

Si no existe, se crea a partir de la plantilla:

```bash
cp p3/scripts/cluster.env.example p3/scripts/cluster.env
nano p3/scripts/cluster.env      # o el editor que prefieras
```

Y se edita **solo esto**:

```bash
# Las 3 IPs de vuestros PCs. El PRIMERO es el maestro (aquí se lanza mpirun).
NODES=(111.111.111.111 222.222.222.222 333.333.333.333)

SSH_USER="${USER}"          # mismo usuario en los 3 PCs
WORKDIR="${HOME}/IC"        # MISMA ruta en los 3 PCs (aquí está el repo)
SLOTS_PER_NODE=4            # núcleos FÍSICOS de cada PC

SYNC_MODE=auto              # auto | nfs | copy
NET_IFACE=""                # solo si mpirun elige la interfaz de red equivocada
```

> **No se edita el hostfile a mano.** `cluster_setup.sh` lo genera a partir de
> `NODES` y `SLOTS_PER_NODE` en `p3/hostfile`.
>
> Si usáis **nombres** en lugar de IPs (`pc1`, `pc2`, `pc3`), añadidlos a
> `/etc/hosts` en los tres PCs:
> ```
> 111.111.111.111 pc1
> 222.222.222.222 pc2
> 333.333.333.333 pc3
> ```

---

## 1. Requisitos en los 3 PCs

- Mismo **usuario** y misma contraseña (o claves SSH) en los tres.
- Misma versión de **Open MPI** y del compilador.
- Todos en la **misma red local** (ping entre ellos).
- El repositorio en la **misma ruta** (`$WORKDIR`) en los tres.

Instalación (en **cada** PC):

```bash
sudo apt update
sudo apt install -y openmpi-bin libopenmpi-dev build-essential rsync
mpirun --version        # debe ser la MISMA versión en los 3
```

Comprobación de red (desde el maestro):

```bash
ping -c 2 222.222.222.222
ping -c 2 333.333.333.333
```

Si tenéis cortafuegos (`ufw`), abrid el tráfico entre los nodos:

```bash
sudo ufw allow from 192.168.1.0/24     # ajusta a vuestra subred
```

---

## 2. Copiar el proyecto al maestro

En el **maestro**:

```bash
# Debe ser exactamente la ruta indicada en WORKDIR
git clone <url-del-repo> "$HOME/IC"
# o, si ya lo tienes:
mv ~/IC ~/IC.bak 2>/dev/null; cp -a ~/IC.bak ~/IC   # ejemplo
```

Comprueba que el dataset MNIST está descargado:

```bash
ls ~/IC/knn2/build/train ~/IC/knn2/build/test
```

Si no está:

```bash
cd ~/IC/knn2
chmod a+x get_dataset.sh
./get_dataset.sh
```

---

## 3. Claves SSH sin contraseña

`mpirun` lanza procesos remotos por SSH; debe poder entrar **sin contraseña**.
La guía pide hacerlo a mano; los pasos son:

### 3.1 Manualmente (lo que pide el enunciado)

En el maestro:

```bash
ssh-keygen -t rsa                 # aceptar todo por defecto
cat ~/.ssh/id_rsa.pub
```

Copiad esa clave pública a los otros dos PCs, dentro de `~/.ssh/authorized_keys`:

```bash
# Opción cómoda (pide la contraseña una vez):
ssh-copy-id usuario@222.222.222.222
ssh-copy-id usuario@333.333.333.333

# Opción manual:
ssh usuario@222.222.222.222
mkdir -p ~/.ssh && chmod 700 ~/.ssh
echo "PEGA_AQUI_LA_CLAVE_PUBLICA" >> ~/.ssh/authorized_keys
chmod 600 ~/.ssh/authorized_keys
exit
```

Verificación (no debe pedir contraseña):

```bash
ssh 222.222.222.222 hostname
ssh 333.333.333.333 hostname
```

### 3.2 Automáticamente

`cluster_setup.sh` lo hace por ti: crea la clave RSA si no existe, ejecuta
`ssh-copy-id` con cada nodo y verifica el acceso. Solo tendrás que escribir la
contraseña una vez por PC.

---

## 4. Preparar el cluster (un solo comando)

```bash
cd ~/IC/p3
./scripts/cluster_setup.sh
```

El script, en orden:

1. Comprueba `mpirun`, `mpicc`, `g++`, `ssh` y `tar` en el maestro.
2. Verifica (y crea si hace falta) el acceso SSH sin contraseña a los 3 PCs.
3. Comprueba que Open MPI está instalado y es **la misma versión** en los 3.
4. Detecta si `$WORKDIR` es compartido:
   - **NFS**: no copia nada.
   - **discos separados**: copia el repo y el dataset MNIST por SSH
     (`tar | ssh`) a cada PC.
5. Genera `p3/hostfile` con `slots=N` por nodo y permisos `600`.
6. Compila `hello_mpi`, `nbody_mpi`, `knn_mpi` y las versiones secuenciales
   (`p3/scripts/build_all.sh`).
7. Verifica en cada PC que existen los ejecutables y el dataset.

Ejemplo de salida correcta:

```
[  ok  ] ssh usuario@111.111.111.111
[  ok  ] ssh usuario@222.222.222.222
[  ok  ] ssh usuario@333.333.333.333
[  ok  ] 111.111.111.111 -> mpirun (Open MPI) 5.0.7
[  ok  ] 222.222.222.222 -> mpirun (Open MPI) 5.0.7
[  ok  ] 333.333.333.333 -> mpirun (Open MPI) 5.0.7
[cluster] modo de sincronizacion: nfs
[  ok  ] hostfile creado (chmod 600)
[  ok  ] 222.222.222.222 -> ejecutables y dataset OK
```

Opciones útiles:

```bash
./scripts/cluster_setup.sh --dry-run    # ver qué haría, sin tocar nada
./scripts/cluster_setup.sh --no-keys    # no tocar claves SSH
./scripts/cluster_setup.sh --test       # acabar lanzando el hola mundo
```

---

## 5. Tarea 2: "hola mundo" distribuido

```bash
./scripts/run_cluster.sh --tarea 2
```

Equivale a:

```bash
cd tarea2_mpi
mpirun -mca plm_rsh_no_tree_spawn 1 \
       -hostfile ../hostfile \
       --map-by node \
       -np 6 ./hello_mpi
```

Salida esperada (fijaos en el **nombre de la máquina**, que debe cambiar):

```
 PID: 4131 Hello from process 0 out of 6 on pc1 processor 3
 PID: 4131 Hello from process 1 out of 6 on pc1 processor 4
 PID: 4131 Hello from process 2 out of 6 on pc2 processor 0
 ¡Hola! desde el procesador 2
 PID: 4131 Hello from process 3 out of 6 on pc2 processor 1
 PID: 4131 Hello from process 4 out of 6 on pc3 processor 0
 PID: 4131 Hello from process 5 out of 6 on pc3 processor 1
[  ok  ] ejecucion distribuida verificada: 3 hostname(s) distintos
```

El script comprueba automáticamente que aparecen **al menos 2 hostnames
distintos**; esa es la prueba de que se ha ejecutado en varios PCs. El
resultado queda en `results/cluster/hello_cluster.txt`.

> `--map-by node` es importante: sin él Open MPI llena el primer PC antes de
> usar el segundo, y con `-np 6` podrías estar usando solo 2 PCs.

---

## 6. Tareas 3 y 4: barridos completos

```bash
./scripts/run_cluster.sh                # tareas 2 + 3 + 4
./scripts/run_cluster.sh --tarea 3
./scripts/run_cluster.sh --tarea 4
```

Genera:

| Fichero | Contenido |
|---|---|
| `results/cluster/hello_cluster.txt` | salida del hola mundo |
| `results/cluster/tarea3_seq.csv` | referencia secuencial |
| `results/cluster/tarea3_nbody.csv` | speed-up, cómputo y comunicación |
| `results/cluster/tarea4_knn.csv` | accuracy y tiempos por estrategia |
| `results/cluster/cluster_info.txt` | CPUs, núcleos, versión MPI, hostfile |
| `results/cluster/logs/*.log` | traza completa de cada barrido |

La lista de procesos se deduce del hostfile: con **3 PCs × 4 núcleos** prueba
`1 2 3 4 6 8 9 12`, incluyendo siempre múltiplos de 3 para poder dibujar el
speed-up en función del **número de PCs**. Para fijarla a mano:

```bash
./scripts/run_cluster.sh --ps "1 3 6 9 12"
```

Y para el modo especulativo (`--strategy spec`) con grupos que abarquen varios
PCs, `MAP_BY=node` reparte los rangos 0..P-1 round-robin entre los nodos.

### Reanudar sin repetir la preparación

```bash
./scripts/run_cluster.sh --tarea 4 --no-setup --ps "1 3 6 9"
```

### Figuras con los datos del cluster

```bash
python3 scripts/plot.py --results results/cluster --figures figures/cluster
```

---

## 7. Ensayo en un solo PC (antes de ir al laboratorio)

Para comprobar que toda la tubería funciona sin depender de la red:

```bash
./scripts/run_cluster.sh --rehearsal --quick
```

Usa `localhost` como único nodo con 8 slots y un barrido reducido. **No
sustituye** a la ejecución real: sirve para validar scripts, hostfile,
ejecutables y CSVs.

---

## 8. Problemas frecuentes

| Síntoma | Causa / solución |
|---|---|
| `Permission denied (publickey)` | La clave no está en `~/.ssh/authorized_keys` del nodo. Repite el paso 3 y comprueba `chmod 700 ~/.ssh`, `chmod 600 ~/.ssh/authorized_keys`. |
| `Host key verification failed` | `ssh-keygen -R <ip>` y vuelve a conectar, o usa `StrictHostKeyChecking=accept-new` (ya lo hace el script). |
| `mpirun was unable to launch` | El ejecutable no existe en esa ruta del nodo, o no tiene permiso: `chmod o+x programa`. |
| `not enough slots available` | Pides más procesos que `NODES × SLOTS_PER_NODE`. Sube `SLOTS_PER_NODE` o baja `-np`. |
| `mca_base_component_repository_open` / versiones | Open MPI distinto en los nodos. Instala la **misma** versión en los 3. |
| Se queda colgado al arrancar | Cortafuegos. Abre los puertos TCP entre nodos (Open MPI usa 1024-65535). |
| `No route to host` / elige la interfaz equivocada | Pon `NET_IFACE=eth0` (o `enp3s0`, `wlan0`...) en `cluster.env`. |
| `train/train-images-idx3-ubyte` no encontrado | Falta el dataset en ese nodo: `knn2/get_dataset.sh` o `SYNC_MODE=copy`. |
| `WORKDIR (...) debe coincidir...` | El repo no está en la ruta de `WORKDIR`. Clónalo ahí o cambia `WORKDIR`. |
| Funciona con `-np 3` pero no con `-np 6` | Hostfile con `slots=1` por defecto: revisa `SLOTS_PER_NODE`. |

Para depurar un fallo de lanzamiento:

```bash
mpirun --hostfile ../hostfile -np 3 hostname
```

Si eso falla, el problema es de SSH/hostfile, no de vuestro código MPI.

---

## 9. Qué incluir en la memoria

Con los datos de `results/cluster/`:

1. Tabla de arquitectura: 3 PCs, modelo de CPU, núcleos físicos, RAM, red
   (a partir de `cluster_info.txt`).
2. Speed-up y eficiencia **en función del número de PCs** (P = 1, 3, 6, 9, 12),
   no solo del número de procesos.
3. Porcentaje de tiempo en comunicación (`comm / (comp + comm)`) al crecer P:
   en un cluster real el coste de red es mayor que en localhost.
4. Comparación de estrategias de la Tarea 4 (`test`, `k`, `folds`, `spec`) y
   reparto de grupos en `spec`.
5. Justificación del modelo (paso de mensajes, SPMD), estilo (grano grueso),
   comunicaciones (colectivas `Allgatherv`/`Reduce`/`Bcast`, punto a punto en
   `spec`) y estructura (maestro con `--map-by node`).

---

## 10. Referencia rápida de ficheros

| Fichero | Para qué sirve |
|---|---|
| `scripts/cluster.env` | **Único sitio donde se editan las IPs.** |
| `scripts/cluster.env.example` | Plantilla con IPs de ejemplo. |
| `scripts/cluster.rehearsal.env` | Ensayo local (1 PC). |
| `scripts/cluster_lib.sh` | Funciones comunes (SSH, hostfile, mpirun). |
| `scripts/cluster_setup.sh` | Prepara claves, sincroniza, hostfile y compila. |
| `scripts/run_cluster.sh` | Lanza Tareas 2/3/4 en el cluster. |
| `scripts/build_all.sh` | Compila todos los ejecutables. |
| `scripts/mpi_common.py` | Construcción de `mpirun` y listas de P. |
| `scripts/run_tarea3.py` | Barrido N-cuerpos (`--hostfile`). |
| `scripts/run_tarea4.py` | Barrido kNN (`--hostfile`). |
| `tarea2_mpi/run_cluster.sh` | Atajo para la Tarea 2. |
| `p3/hostfile` | Generado automáticamente; **no editar**. |
