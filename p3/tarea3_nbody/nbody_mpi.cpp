// Tarea 3 - Version paralela con MPI de la simulacion de N cuerpos (Corregida)
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

constexpr double G = 6.674e-3;     // Constante gravitacional
constexpr double SOFTENING = 1e-2; // Evita divisiones por cero

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

    std::vector<Particle> all = init_particles(n_particles);

    int begin, local_n;
    ownership(n_particles, rank, size, cyclic, begin, local_n);

    std::vector<Particle> local(local_n);
    for (int i = 0; i < local_n; i++) {
        int idx = cyclic ? begin + i * size : begin + i;
        local[i] = all[idx];
    }

    std::vector<double> mass(n_particles);
    for (int i = 0; i < n_particles; i++) mass[i] = all[i].mass;

    // Buffers para comunicacion global de posiciones
    std::vector<double> gx(n_particles), gy(n_particles);
    std::vector<double> send_x(local_n), send_y(local_n);

    // Preparar conteos y desplazamientos para Allgatherv (valido para block; 
    // para cyclic se maneja mediante empaquetado o Alltoallv).
    std::vector<int> counts(size), displs(size);
    for (int r = 0; r < size; r++) {
        int b, c; ownership(n_particles, r, size, cyclic, b, c);
        counts[r] = c;
        displs[r] = cyclic ? r : b; // Ajuste conceptual para indexacion
    }

    double comm_time = 0.0, comp_time = 0.0;

    MPI_Barrier(MPI_COMM_WORLD);
    const double t_start = MPI_Wtime();

    for (int step = 0; step < n_steps; step++) {
        // --- Sincronizacion de posiciones ---
        for (int i = 0; i < local_n; i++) {
            send_x[i] = local[i].x;
            send_y[i] = local[i].y;
        }

        double c0 = MPI_Wtime();
        if (cyclic) {
            // Para distribucion ciclica, colocamos cada elemento en su posicion global correcta
            std::vector<double> temp_x(n_particles, 0.0), temp_y(n_particles, 0.0);
            for (int i = 0; i < local_n; i++) {
                int gid = begin + i * size;
                temp_x[gid] = send_x[i];
                temp_y[gid] = send_y[i];
            }
            MPI_Allreduce(temp_x.data(), gx.data(), n_particles, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
            MPI_Allreduce(temp_y.data(), gy.data(), n_particles, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        } else {
            MPI_Allgatherv(send_x.data(), local_n, MPI_DOUBLE,
                           gx.data(), counts.data(), displs.data(), MPI_DOUBLE, MPI_COMM_WORLD);
            MPI_Allgatherv(send_y.data(), local_n, MPI_DOUBLE,
                           gy.data(), counts.data(), displs.data(), MPI_DOUBLE, MPI_COMM_WORLD);
        }
        comm_time += MPI_Wtime() - c0;

        // --- Computo de fuerzas ---
        double p0 = MPI_Wtime();
        for (int i = 0; i < local_n; i++) {
            const double xi = local[i].x, yi = local[i].y;
            const double mi = local[i].mass;
            const int gi = cyclic ? begin + i * size : begin + i;
            double fx = 0.0, fy = 0.0;
            for (int j = 0; j < n_particles; j++) {
                if (j == gi) continue;
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

        // --- Integracion ---
        for (int i = 0; i < local_n; i++) {
            local[i].vx += (local[i].fx / local[i].mass) * dt;
            local[i].vy += (local[i].fy / local[i].mass) * dt;
            local[i].x  += local[i].vx * dt;
            local[i].y  += local[i].vy * dt;
        }
        comp_time += MPI_Wtime() - p0;

        // --- Estadisticas (Sincronizado) ---
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
        printf("\nRESULT n=%d steps=%d P=%d part=%s wall=%.6f comp=%.6f comm=%.6f\n",
               n_particles, n_steps, size, cyclic ? "cyclic" : "block",
               wall, max_comp, max_comm);
    }

    MPI_Finalize();
    return 0;
}