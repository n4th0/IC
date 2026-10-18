// Tarea 0 - Recorrido de matrices.
// Compara recorrer una matriz NxN por columnas (a[j][i]) frente a recorrerla
// por filas (b[i][j]). La memoria se reserva en el heap para poder usar N
// grandes sin desbordar la pila.
//
// Compilar: g++ -O2 -std=c++17 matrix_bench.cpp -o matrix_bench
// Uso:      ./matrix_bench [N]     (N por defecto 1000)
#include <iostream>
#include <vector>
#include <ctime>

using namespace std;

int main(int argc, char **argv)
{
    const int SIZE = (argc > 1) ? atoi(argv[1]) : 1000;
    const long long cells = (long long)SIZE * SIZE;

    // Dos matrices planas en memoria, almacenadas por filas.
    vector<int> a(cells, 0);
    vector<int> b(cells, 0);

    // A: recorrido por columnas -> saltos de N elementos (mala localidad).
    clock_t begin = clock();
    for (int i = 0; i < SIZE; i++)
        for (int j = 0; j < SIZE; j++)
            a[(long long)j * SIZE + i] = i + j;
    double aTime = double(clock() - begin) / CLOCKS_PER_SEC;

    // B: recorrido por filas -> acceso secuencial (buena localidad).
    begin = clock();
    for (int i = 0; i < SIZE; i++)
        for (int j = 0; j < SIZE; j++)
            b[(long long)i * SIZE + j] = i + j;
    double bTime = double(clock() - begin) / CLOCKS_PER_SEC;

    // Consumimos b para evitar que el optimizador elimine el bucle.
    volatile long long sink = 0;
    for (long long k = 0; k < cells; k += 1024) sink += b[k];

    cout << "N=" << SIZE
         << " A_col=" << aTime
         << " B_row=" << bTime
         << " gain=" << (bTime > 0 ? aTime / bTime : 0.0)
         << " sum=" << sink << "\n";
    return 0;
}
