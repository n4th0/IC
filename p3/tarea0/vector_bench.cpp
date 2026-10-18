// Tarea 0 - El problema de los vectores dinamicos en el paralelismo.
// Comprueba la diferencia entre anadir elementos con push_back (con realojos)
// y reservar el tamano de antemano escribiendo por indice.
//
// Compilar: g++ -O2 -std=c++17 vector_bench.cpp -o vector_bench
#include <iostream>
#include <vector>
#include <ctime>

using namespace std;

#ifndef SIZE
#define SIZE 1000000
#endif

int main()
{
    // A: vector sin inicializar + push_back -> posibles realojos y copias.
    clock_t begin = clock();
    std::vector<int> a;
    for (int i = 0; i < SIZE; i++)
    {
        a.push_back(i);
    }
    double aTime = double(clock() - begin) / CLOCKS_PER_SEC;

    // B: vector inicializado al tamano requerido -> una sola reserva.
    begin = clock();
    std::vector<int> b(SIZE);
    for (int i = 0; i < SIZE; i++)
    {
        b[i] = i;
    }
    double bTime = double(clock() - begin) / CLOCKS_PER_SEC;

    cout << "SIZE=" << SIZE << "\n";
    cout << "A (push_back)      : " << aTime << " s\n";
    cout << "B (pre-sized [i])  : " << bTime << " s\n";
    cout << "Gain (A/B)         : " << (bTime > 0 ? aTime / bTime : 0.0) << "\n";
    return 0;
}
