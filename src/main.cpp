// nbody_sim.cpp
// Simulacion basica de N cuerpos (particulas bajo gravedad mutua).
// Version secuencial de referencia. Puntos de paralelizacion marcados con TODO.
//
// Compilar version secuencial:
//   g++ -O2 -std=c++17 nbody_sim.cpp -o nbody_sim
// Compilar con paralelismo de datos (OpenMP):
//   g++ -O2 -std=c++17 -fopenmp -DUSE_OPENMP nbody_sim.cpp -o nbody_sim_omp

#include <vector>
#include <cmath>
#include <cstdio>
#include <random>
#include <chrono>

#ifdef USE_OPENMP
#include <omp.h>
#endif

struct Particle {
    double x, y;     // posicion
    double vx, vy;   // velocidad
    double fx, fy;   // fuerza acumulada
    double mass;
};

constexpr double G = 6.674e-3;     // constante gravitacional (escalada para la simulacion)
constexpr double SOFTENING = 1e-2; // evita divisiones por cero en colisiones casi exactas

// Inicializa N particulas con posiciones y masas aleatorias
std::vector<Particle> init_particles(int n, unsigned seed = 42) {
    std::vector<Particle> particles(n);
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> pos_dist(-100.0, 100.0);
    std::uniform_real_distribution<double> mass_dist(1.0, 10.0);

    for (auto &p : particles) {
        p.x = pos_dist(rng);
        p.y = pos_dist(rng);
        p.vx = 0.0;
        p.vy = 0.0;
        p.fx = 0.0;
        p.fy = 0.0;
        p.mass = mass_dist(rng);
    }
    return particles;
}

// -------------------------------------------------------------------------
// ETAPA 1: Calculo de fuerzas  ->  O(n^2), candidato principal a PARALELISMO
// DE DATOS: cada particula i acumula fuerza de forma independiente de las
// demas particulas i (no hay dependencia entre iteraciones del bucle externo).
// -------------------------------------------------------------------------
void compute_forces(std::vector<Particle> &particles) {
    int n = static_cast<int>(particles.size());

    // TODO(paralelismo de datos): descomentar para repartir el bucle externo
    // entre hilos. Cada hilo calcula fx,fy de un subconjunto de particulas
    // sin escribir en la misma posicion que otro hilo -> no hay carrera.
    #ifdef USE_OPENMP
    #pragma omp parallel for schedule(static)
    #endif
    for (int i = 0; i < n; i++) {
        double fx = 0.0, fy = 0.0;
        for (int j = 0; j < n; j++) {
            if (i == j) continue;
            double dx = particles[j].x - particles[i].x;
            double dy = particles[j].y - particles[i].y;
            double dist2 = dx * dx + dy * dy + SOFTENING;
            double dist = std::sqrt(dist2);
            double force = (G * particles[i].mass * particles[j].mass) / dist2;
            fx += force * dx / dist;
            fy += force * dy / dist;
        }
        particles[i].fx = fx;
        particles[i].fy = fy;
    }
}

// -------------------------------------------------------------------------
// ETAPA 2: Integracion (actualizar velocidad y posicion) -> tambien
// PARALELISMO DE DATOS puro: cada particula se actualiza con sus propios
// fx, fy, vx, vy sin tocar a las demas.
// -------------------------------------------------------------------------
void integrate(std::vector<Particle> &particles, double dt) {
    int n = static_cast<int>(particles.size());

    #ifdef USE_OPENMP
    #pragma omp parallel for schedule(static)
    #endif
    for (int i = 0; i < n; i++) {
        particles[i].vx += (particles[i].fx / particles[i].mass) * dt;
        particles[i].vy += (particles[i].fy / particles[i].mass) * dt;
        particles[i].x  += particles[i].vx * dt;
        particles[i].y  += particles[i].vy * dt;
    }
}

// -------------------------------------------------------------------------
// ETAPA 3: Salida / registro de estadisticas del paso actual.
// En una version con PARALELISMO FUNCIONAL, esta etapa se ejecutaria en un
// hilo/tarea aparte, escribiendo el resultado del paso N-1 mientras las
// etapas de fuerzas/integracion ya trabajan sobre el paso N. Aqui, en la
// version secuencial, simplemente se ejecuta despues de integrar.
// -------------------------------------------------------------------------
void report_step(const std::vector<Particle> &particles, int step) {
    double energy = 0.0;
    for (const auto &p : particles) {
        double v2 = p.vx * p.vx + p.vy * p.vy;
        energy += 0.5 * p.mass * v2;
    }
    printf("Paso %4d | energia cinetica total: %.4f\n", step, energy);
}

int main(int argc, char **argv) {
    int n_particles = (argc > 1) ? std::atoi(argv[1]) : 3000;
    int n_steps      = (argc > 2) ? std::atoi(argv[2]) : 50;
    double dt = 0.01;

    auto particles = init_particles(n_particles);

    auto t0 = std::chrono::high_resolution_clock::now();

    // Bucle principal de simulacion (secuencial entre pasos: el paso N
    // depende del paso N-1, asi que ESTO no se paraleliza sin mas).
    for (int step = 0; step < n_steps; step++) {
        compute_forces(particles);   // TODO(paralelismo de datos)
        integrate(particles, dt);    // TODO(paralelismo de datos)

        if (step % 10 == 0) {
            report_step(particles, step); // TODO(paralelismo funcional: mover a hilo de I/O)
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();

    printf("\nSimulacion completa: %d particulas, %d pasos\n", n_particles, n_steps);
    printf("Tiempo total: %.3f s\n", elapsed);

    return 0;
}
