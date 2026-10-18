// Tarea 3 - Version paralela con MPI de la simulacion de N cuerpos (src/main.cpp).
//
// Estrategia: descomposicion de datos (domain decomposition) del bucle externo
// de compute_forces. Cada proceso es propietario de un subconjunto de
// particulas (reparto block o cyclic). Como la fuerza sobre i depende de TODAS
// las particulas, en cada paso se hace un MPI_Allgatherv de las posiciones
// (x,y) para que todos los procesos dispongan del estado global actualizado.
//
// Balance de costes por paso:
//   - Computo : O(n_local * n)  (distancias y fuerzas de las particulas propias)
//   - Comunic.: O(n)            (Allgatherv de posiciones)
// => relacion comun/calculo ~ 1/n, por lo que escala bien en n.
//
// Ademas del paralelismo de datos se solapa paralelismo funcional: el proceso
// 0 hace de rank de E/S (imprime las estadisticas) mientras el resto calcula.
//
// Compilar: mpic++ -O2 -std=c++17 nbody_mpi.cpp -o nbody_mpi
// Ejecutar: mpirun -np <P> ./nbody_mpi <n_particles> <n_steps> [block|cyclic]
#include <mpi.h>

#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

struct Particle {
    double x, y;
    double vx, vy;
    double fx, fy;
    double mass;
};

constexpr double G = 6.674e-3;     // constante gravitacional (escalada)
constexpr double SOFTENING = 1e-2; // evita divisiones por cero

// Reproduce exactamente la inicializacion secuencial (misma semilla 42).
static std::vector<Particle> init_particles(int n, unsigned seed = 42) {
    std::vector<Particle> particles(n);
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> pos_dist(-100.0, 100.0);
    std::uniform_real_distribution<double> mass_dist(1.0, 10.0);
    for (auto &p : particles) {
        p.x = pos_dist(rng);
        p.y = pos_dist(rng);
        p.vx = p.vy = p.fx = p.fy = 0.0;
        p.mass = mass_dist(rng);
    }
    return particles;
}

// Rango [begin,end) de particulas asignado al proceso `rank` de `size`.
static void ownership(int n, int rank, int size, bool cyclic, int &begin, int &count) {
    if (cyclic) {
        begin = rank;
        count = 0;
        for (int i = rank; i < n; i += size) count++;
    } else {
        const int base = n / size, rem = n % size;
        begin = rank * base + std::min(rank, rem);
        count = base + (rank < rem ? 1 : 0);
    }
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    const int n_particles = (argc > 1) ? atoi(argv[1]) : 3000;
    const int n_steps     = (argc > 2) ? atoi(argv[2]) : 50;
    const bool cyclic     = (argc > 3 && strcmp(argv[3], "cyclic") == 0);
    const double dt = 0.01;

    // Todos los procesos reproducen el mismo vector inicial (determinista).
    std::vector<Particle> all = init_particles(n_particles);

    int begin, local_n;
    ownership(n_particles, rank, size, cyclic, begin, local_n);

    // Particulas propias.
    std::vector<Particle> local(local_n);
    for (int i = 0; i < local_n; i++) local[i] = all[begin + i];

    // Masas globales replicadas (constantes durante la simulacion).
    std::vector<double> mass(n_particles);
    for (int i = 0; i < n_particles; i++) mass[i] = all[i].mass;

    // Buffers de comunicacion: segmento local y estado global de posiciones.
    std::vector<int> counts(size), displs(size);
    for (int r = 0; r < size; r++) {
        int b, c; ownership(n_particles, r, size, cyclic, b, c);
        counts[r] = c; displs[r] = b;
    }
    std::vector<double> send_x(local_n), send_y(local_n);
    std::vector<double> gx(n_particles), gy(n_particles);

    double comm_time = 0.0, comp_time = 0.0;

    MPI_Barrier(MPI_COMM_WORLD);
    const double t_start = MPI_Wtime();

    for (int step = 0; step < n_steps; step++) {
        // --- Comunicacion colectiva: reunir todas las posiciones ---
        for (int i = 0; i < local_n; i++) { send_x[i] = local[i].x; send_y[i] = local[i].y; }
        double c0 = MPI_Wtime();
        MPI_Allgatherv(send_x.data(), local_n, MPI_DOUBLE,
                       gx.data(), counts.data(), displs.data(), MPI_DOUBLE, MPI_COMM_WORLD);
        MPI_Allgatherv(send_y.data(), local_n, MPI_DOUBLE,
                       gy.data(), counts.data(), displs.data(), MPI_DOUBLE, MPI_COMM_WORLD);
        comm_time += MPI_Wtime() - c0;

        // --- Computo de fuerzas sobre las particulas propias ---
        double p0 = MPI_Wtime();
        for (int i = 0; i < local_n; i++) {
            const double xi = local[i].x, yi = local[i].y;
            const double mi = local[i].mass;
            const int gi = cyclic ? begin + i * size : begin + i; // indice global
            double fx = 0.0, fy = 0.0;
            for (int j = 0; j < n_particles; j++) {
                if (j == gi) continue; // no auto-interaccion (i == j)
                const double dx = gx[j] - xi;
                const double dy = gy[j] - yi;
                const double dist2 = dx * dx + dy * dy + SOFTENING;
                const double dist = std::sqrt(dist2);
                const double force = (G * mi * mass[j]) / dist2;
                fx += force * dx / dist;
                fy += force * dy / dist;
            }
            local[i].fx = fx;
            local[i].fy = fy;
        }

        // --- Integracion (actualizacion propia) ---
        for (int i = 0; i < local_n; i++) {
            local[i].vx += (local[i].fx / local[i].mass) * dt;
            local[i].vy += (local[i].fy / local[i].mass) * dt;
            local[i].x  += local[i].vx * dt;
            local[i].y  += local[i].vy * dt;
        }
        comp_time += MPI_Wtime() - p0;

        // --- Paralelismo funcional: rank 0 hace de E/S; resto sigue calculando ---
        if (step % 10 == 0) {
            double local_energy = 0.0;
            for (int i = 0; i < local_n; i++) {
                const double v2 = local[i].vx * local[i].vx + local[i].vy * local[i].vy;
                local_energy += 0.5 * local[i].mass * v2;
            }
            double total_energy = 0.0;
            MPI_Reduce(&local_energy, &total_energy, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
            if (rank == 0)
                printf("Paso %4d | energia cinetica total: %.4f\n", step, total_energy);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    const double wall = MPI_Wtime() - t_start;

    double max_comp = 0.0, max_comm = 0.0;
    MPI_Reduce(&comp_time, &max_comp, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&comm_time, &max_comm, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("\nSimulacion MPI: %d particulas, %d pasos, %d procesos (%s)\n",
               n_particles, n_steps, size, cyclic ? "cyclic" : "block");
        printf("RESULT n=%d steps=%d P=%d part=%s wall=%.6f comp=%.6f comm=%.6f\n",
               n_particles, n_steps, size, cyclic ? "cyclic" : "block",
               wall, max_comp, max_comm);
    }

    MPI_Finalize();
    return 0;
}
