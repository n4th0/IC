# Tarea 2 - Ejecucion distribuida con MPI

Objetivo: ejecutar el "hola mundo" paralelo (`hello_mpi.c`) sobre varios nodos
de un multicomputador, segun los pasos de la guia de la Practica 3.

> **Version automatizada (recomendada):** toda la preparacion (claves SSH,
> hostfile, copia a los nodos y compilacion) y el lanzamiento estan en
> `../scripts/cluster_setup.sh` y `../scripts/run_cluster.sh`.
> Guia completa: [`../docs/CLUSTER.md`](../docs/CLUSTER.md).
>
> ```bash
> cd ..
> ./scripts/cluster_setup.sh          # una sola vez
> ./scripts/run_cluster.sh --tarea 2  # hola mundo en los 3 PCs
> ```
>
> El resto de este documento describe el procedimiento manual.


## 1. Requisitos previos

Instalar MPICH y su compilador. En este repo se usa MPICH; el lanzador es
`mpiexec` (Hydra):

```bash
sudo apt update && sudo apt install -y mpich build-essential
mpiexec --version
mpichversion
```

> Si tambien teneis Open MPI instalado, comprobad que `mpiexec` y `mpicc`
> apuntan a MPICH con `sudo update-alternatives --config mpi` y recompilad los
> ejecutables. Los binarios de Open MPI y MPICH no son intercambiables.

## 2. Compilacion

```bash
mpicc hello_mpi.c -o hello_mpi
```

El ejecutable debe tener el **mismo nombre** y estar en la **misma carpeta**
en todos los nodos. Se recomienda usar la carpeta personal (`$HOME`) en todas
las maquinas.

## 3. Archivo de hosts

Fichero de texto plano con el nombre o la IP de cada nodo y sus slots (nucleos
fisicos). El formato de MPICH/Hydra es `ip:N` (no `ip slots=N`, que es de
Open MPI). Se usa `hostfile` (incluido) como plantilla:

```
# hostfile  ->  una linea por nodo con sus slots (nucleos fisicos)
192.168.1.10:4
192.168.1.11:4
192.168.1.12:4
```

> En la version automatizada este fichero lo genera `scripts/cluster_setup.sh`
> a partir de `scripts/cluster.env`: **no lo edites a mano**.

Para pruebas locales (una sola maquina) se puede usar `hostfile.local`:

```
localhost:16
```

Copiar el hostfile a todas las maquinas, en el mismo directorio de trabajo, y
restringir sus permisos:

```bash
chmod 600 hostfile
chmod o+x hello_mpi      # permitir ejecucion remota
```

## 4. Certificado de clave publica SSH

En uno de los nodos (maestro):

```bash
ssh-keygen -t rsa        # aceptar valores por defecto; genera ~/.ssh/id_rsa
```

Copiar `~/.ssh/id_rsa.pub` a la carpeta `~/.ssh/authorized_keys` del resto de
nodos. Si en alguna maquina no existe `~/.ssh`, crearla antes con `ssh-keygen -t rsa`.

Verificar que el login sin contrasena funciona en todos los nodos:

```bash
ssh <nodo> hostname
```

## 5. Ejecucion

Desde la maquina donde se creo el certificado:

```bash
mpiexec -iface enp4s0 -f hostfile -n 6 ./hello_mpi
```

- `-iface enp4s0` fija la interfaz de red del cluster (ajustala a la de tus
  PCs; si se omite, Hydra elige una automaticamente).
- `-f hostfile` indica los nodos y sus slots.
- `-n <N>` numero total de procesos MPI.

> El hostfile debe usar `ip:N`. Las opciones `--hostfile`, `-np`, `--map-by` o
> `--mca` son de Open MPI y no funcionan con MPICH/Hydra.

### Prueba local (una sola maquina)

```bash
./run_local.sh 4        # lanza 4 procesos en localhost
```

## 6. Resultado observado (maquina local, 8 nucleos / 16 hilos)

Ver `results/hello_local.txt`. Cada proceso imprime su `rank`, el nombre del
procesador y el nucleo (`sched_getcpu()`); el proceso de rango 2 imprime ademas
el saludo en castellano.
