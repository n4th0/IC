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

Instalar una implementacion de MPI (Open MPI) y su compilador:

```bash
sudo apt update && sudo apt install -y openmpi-bin libopenmpi-dev
mpirun --version
```

## 2. Compilacion

```bash
mpicc hello_mpi.c -o hello_mpi
```

El ejecutable debe tener el **mismo nombre** y estar en la **misma carpeta**
en todos los nodos. Se recomienda usar la carpeta personal (`$HOME`) en todas
las maquinas.

## 3. Archivo de hosts

Fichero de texto plano con el nombre o la IP de cada nodo, uno por linea. Se usa
`hostfile` (incluido) como plantilla:

```
# hostfile  ->  una linea por nodo con sus slots (nucleos fisicos)
192.168.1.10 slots=4
192.168.1.11 slots=4
192.168.1.12 slots=4
```

> En la version automatizada este fichero lo genera `scripts/cluster_setup.sh`
> a partir de `scripts/cluster.env`: **no lo edites a mano**.

Para pruebas locales (una sola maquina) se puede usar `hostfile.local`:

```
localhost slots=16
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
mpirun -mca plm_rsh_no_tree_spawn 1 -hostfile hostfile -n 6 ./hello_mpi
```

- `-mca plm_rsh_no_tree_spawn 1` evita que Open MPI use el "tree spawn" al lanzar
  por SSH (recomendado en la guia).
- `-n <N>` numero total de procesos MPI.

### Prueba local (una sola maquina)

```bash
./run_local.sh 4        # lanza 4 procesos en localhost
```

## 6. Resultado observado (maquina local, 8 nucleos / 16 hilos)

Ver `results/hello_local.txt`. Cada proceso imprime su `rank`, el nombre del
procesador y el nucleo (`sched_getcpu()`); el proceso de rango 2 imprime ademas
el saludo en castellano.
