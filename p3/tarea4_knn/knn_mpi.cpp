// Tarea 4 - Version paralela con MPI del clasificador kNN de knn2.
//
// Reutiliza el codigo secuencial proporcionado (sample/mnist/knn) y anade una
// evaluacion MPI con cuatro ESTRATEGIAS seleccionables:
//
//   test  : paralelismo de datos sobre las MUESTRAS DE TEST/VALIDACION. El
//           conjunto de entrenamiento es replicado en cada proceso y cada uno
//           clasifica una particion de las muestras de validacion/test.
//   k     : paralelismo de TAREAS sobre el hiperparametro K (grid search).
//           Cada proceso evalua un subconjunto de valores de K; al final se
//           combinan los resultados.
//   folds : paralelismo de TAREAS sobre las particiones (folds) de la
//           validacion cruzada. Cada proceso evalua un subconjunto de folds.
//   spec  : paralelismo FUNCIONAL + ejecucion ESPECULATIVA. Un grupo de
//           procesos hace el ajuste de K (fine-tuning) mientras otro grupo
//           realiza en paralelo la evaluacion final con el mejor K conocido,
//           invalidando y reiniciando el trabajo cuando aparece un K mejor.
//
// Para evitar el coste de copia de TSample del codigo original (~22% del
// tiempo secuencial segun gprof), la evaluacion usa indices sobre el dataset
// en lugar de materializar subconjuntos train/validation. El resultado es
// identico al secuencial (mismo comparador de distancia y mismo desempate).
//
// Compilar: mpic++ -O2 -std=c++17 knn_mpi.cpp <fuentes knn2> -I<knn2> -o knn_mpi
// Ejecutar: mpirun -np <P> ./knn_mpi --strategy test --train 4000 --test 1500 \
//                                     --finaltrain 4000 --mink 1 --maxk 10 --folds 5
#include <mpi.h>

#include "sample.h"
#include "mnist.h"

#include <vector>
#include <algorithm>
#include <map>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

// euclidean() esta definida con enlazado externo en knn2/knn.cc.
float euclidean(const TSample &a, const TSample &b);

// ---------------------------------------------------------------------------
// Evaluador secuencial (replica la semantica de knn::KNN::classify y de
// knn::KNNResult: orden por distancia ascendente y desempate por la etiqueta
// mas pequena, igual que maxVotesLabel sobre std::map ordenado).
// ---------------------------------------------------------------------------
struct Neighbor { float dist; int label; };

static int classifyOne(const std::vector<TSample> &ds,
                       const std::vector<int> &trainIdx, int k,
                       const TSample &test, std::vector<Neighbor> &scratch)
{
    scratch.clear();
    for (int i : trainIdx) {
        Neighbor nb;
        nb.dist = euclidean(test, ds[i]);
        nb.label = ds[i].label;
        scratch.push_back(nb);
    }
    std::sort(scratch.begin(), scratch.end(),
              [](const Neighbor &a, const Neighbor &b) { return a.dist < b.dist; });
    std::map<int, int> votes;
    for (int i = 0; i < k; i++) votes[scratch[i].label]++;
    int maxVotes = 0, maxLabel = 0;
    for (const auto &kv : votes)
        if (kv.second > maxVotes) { maxVotes = kv.second; maxLabel = kv.first; }
    return maxLabel;
}

// Numero de aciertos al clasificar testDs[lo,hi) contra ds[trainIdx].
static long evalRange(const std::vector<TSample> &ds,
                      const std::vector<int> &trainIdx, int k,
                      const std::vector<TSample> &testDs, size_t lo, size_t hi)
{
    std::vector<Neighbor> scratch;
    scratch.reserve(trainIdx.size());
    long matches = 0;
    for (size_t i = lo; i < hi; i++)
        if (classifyOne(ds, trainIdx, k, testDs[i], scratch) == testDs[i].label)
            matches++;
    return matches;
}

// Reparto contiguo [lo,hi) de n elementos entre `size` procesos.
static void rangeOf(size_t n, int rank, int size, size_t &lo, size_t &hi)
{
    const size_t base = n / size, rem = n % size;
    lo = rank * base + std::min<size_t>(rank, rem);
    hi = lo + base + (rank < rem ? 1 : 0);
}

// Indices de las muestras de entrenamiento del fold `fold` (todas menos la
// particion de validacion [start,end)), igual que CrossValidation::splitDataset.
static std::vector<int> foldTrainIdx(size_t n, size_t fold, size_t folds)
{
    const size_t start = n * fold / folds;
    const size_t end = n * (fold + 1) / folds;
    std::vector<int> idx;
    idx.reserve(n - (end - start));
    for (size_t i = 0; i < n; i++)
        if (i < start || i >= end) idx.push_back(static_cast<int>(i));
    return idx;
}

static std::vector<TSample> loadMnistFile(const char *img, const char *lbl)
{
    std::vector<TSample> out;
    if (!mnist::loadMnist(img, lbl, out)) {
        if (MPI_COMM_WORLD != MPI_COMM_NULL) { /* keep simple */ }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Estrategias
// ---------------------------------------------------------------------------
struct Options {
    int mink = 1, maxk = 10, folds = 5;
    int trainLimit = 4000;     // muestras para validacion cruzada
    int testLimit = 1500;      // muestras para evaluacion final
    int finalTrain = 0;        // 0 = usar todo el train en la evaluacion final
    int groupA = 0;            // procesos dedicados al fine-tuning en modo spec
    std::string strategy = "test";
};

// Devuelve el mejor K y su exactitud de validacion cruzada (todos los procesos
// obtienen el mismo resultado).
static void fineTuneParallel(const std::vector<TSample> &cvDs,
                             const Options &opt, int rank, int size,
                             int &bestK, double &bestAcc)
{
    const size_t n = cvDs.size();
    std::vector<double> acc(opt.maxk - opt.mink + 1, -1.0);

    if (opt.strategy == "k") {
        // Paralelismo sobre K: cada proceso evalua sus K y se combina por MAX.
        for (int k = opt.mink + rank; k <= opt.maxk; k += size) {
            double a = 0.0;
            for (int f = 0; f < opt.folds; f++) {
                const std::vector<int> tr = foldTrainIdx(n, f, opt.folds);
                const size_t start = n * f / opt.folds, end = n * (f + 1) / opt.folds;
                long m = evalRange(cvDs, tr, k, cvDs, start, end);
                a += double(m) / double(end - start);
            }
            acc[k - opt.mink] = a / opt.folds;
        }
        std::vector<double> global(acc.size());
        MPI_Allreduce(acc.data(), global.data(), (int)acc.size(), MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
        acc.swap(global);
    } else if (opt.strategy == "folds") {
        // Paralelismo sobre folds: cada proceso evalua sus folds para cada k.
        for (int k = opt.mink; k <= opt.maxk; k++) {
            double local = 0.0;
            for (int f = 0; f < opt.folds; f++) {
                size_t flo, fhi;
                rangeOf(opt.folds, rank, size, flo, fhi);
                if ((int)f < (int)flo || (int)f >= (int)fhi) continue;
                const std::vector<int> tr = foldTrainIdx(n, f, opt.folds);
                const size_t start = n * f / opt.folds, end = n * (f + 1) / opt.folds;
                long m = evalRange(cvDs, tr, k, cvDs, start, end);
                local += double(m) / double(end - start);
            }
            double total = 0.0;
            MPI_Allreduce(&local, &total, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
            acc[k - opt.mink] = total / opt.folds;
        }
    } else { // "test" (y base del modo spec)
        // Paralelismo sobre muestras de validacion.
        for (int k = opt.mink; k <= opt.maxk; k++) {
            double a = 0.0;
            for (int f = 0; f < opt.folds; f++) {
                const std::vector<int> tr = foldTrainIdx(n, f, opt.folds);
                const size_t start = n * f / opt.folds, end = n * (f + 1) / opt.folds;
                const size_t cnt = end - start;
                size_t lo, hi;
                rangeOf(cnt, rank, size, lo, hi);
                long m = evalRange(cvDs, tr, k, cvDs, start + lo, start + hi);
                long total = 0;
                MPI_Allreduce(&m, &total, 1, MPI_LONG, MPI_SUM, MPI_COMM_WORLD);
                a += double(total) / double(cnt);
            }
            acc[k - opt.mink] = a / opt.folds;
        }
    }

    bestK = opt.mink; bestAcc = 0.0;
    for (int k = opt.mink; k <= opt.maxk; k++) {
        const double a = acc[k - opt.mink];
        if (a > bestAcc) { bestAcc = a; bestK = k; }
    }
}

// Evaluacion final paralela sobre muestras de test.
static double finalEvalParallel(const std::vector<TSample> &trainDs,
                                const std::vector<TSample> &testDs, int k,
                                int rank, int size)
{
    std::vector<int> allIdx(trainDs.size());
    for (size_t i = 0; i < trainDs.size(); i++) allIdx[i] = (int)i;
    size_t lo, hi;
    rangeOf(testDs.size(), rank, size, lo, hi);
    long m = evalRange(trainDs, allIdx, k, testDs, lo, hi);
    long total = 0;
    MPI_Allreduce(&m, &total, 1, MPI_LONG, MPI_SUM, MPI_COMM_WORLD);
    return double(total) / double(testDs.size());
}

// ---------------------------------------------------------------------------
// Modo especulativo (paralelismo funcional)
// ---------------------------------------------------------------------------
enum { TAG_NEWBEST = 7, TAG_DONE = 8, TAG_ACC = 9 };

// Grupo B: evalua las muestras de test con el mejor K conocido, reiniciando si
// llega un K mejor. Devuelve el numero de aciertos de su particion.
static long speculativeEval(const std::vector<TSample> &trainDs,
                            const std::vector<TSample> &testDs,
                            size_t lo, size_t hi, int initialK)
{
    std::vector<int> allIdx(trainDs.size());
    for (size_t i = 0; i < trainDs.size(); i++) allIdx[i] = (int)i;

    const size_t CHUNK = 64;
    int kCur = initialK;
    bool haveFinal = false;
    int finalK = -1;
    long matches = 0;
    size_t idx = lo;

    auto poll = [&](bool &restart) {
        int flag; MPI_Status st;
        while (true) {
            MPI_Iprobe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &st);
            if (!flag) break;
            if (st.MPI_TAG == TAG_NEWBEST) {
                int nk; MPI_Recv(&nk, 1, MPI_INT, 0, TAG_NEWBEST, MPI_COMM_WORLD, &st);
                if (!haveFinal && nk != kCur) { kCur = nk; restart = true; }
            } else if (st.MPI_TAG == TAG_DONE) {
                int dk; MPI_Recv(&dk, 1, MPI_INT, 0, TAG_DONE, MPI_COMM_WORLD, &st);
                haveFinal = true; finalK = dk;
                if (dk != kCur) { kCur = dk; restart = true; }
            }
        }
    };

    while (true) {
        matches = 0; idx = lo;
        bool restart = false;
        while (idx < hi && !restart) {
            const size_t end = std::min(idx + CHUNK, hi);
            matches += evalRange(trainDs, allIdx, kCur, testDs, idx, end);
            idx = end;
            poll(restart);
        }
        if (restart) continue;
        // Pasada completa con kCur. Esperar a DONE si aun no ha llegado.
        while (!haveFinal && !restart) {
            poll(restart);
            if (!haveFinal && !restart) usleep(200);
        }
        if (restart) continue;
        if (kCur == finalK) break;   // resultado valido con el K optimo
        kCur = finalK;               // rehacer con el K definitivo
    }
    return matches;
}

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    Options opt;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() { return (i + 1 < argc) ? argv[++i] : ""; };
        if (a == "--strategy") opt.strategy = next();
        else if (a == "--train") opt.trainLimit = atoi(next());
        else if (a == "--test") opt.testLimit = atoi(next());
        else if (a == "--finaltrain") opt.finalTrain = atoi(next());
        else if (a == "--mink") opt.mink = atoi(next());
        else if (a == "--maxk") opt.maxk = atoi(next());
        else if (a == "--folds") opt.folds = atoi(next());
        else if (a == "--groupA") opt.groupA = atoi(next());
    }

    const double t0 = MPI_Wtime();

    // Carga del dataset (todos los procesos cargan de disco; el coste es
    // constante y pequeno frente al computo). Se usa la primera particion de
    // train para la validacion cruzada, igual que knn2/main.cc.
    std::vector<TSample> trainAll = loadMnistFile("train/train-images-idx3-ubyte",
                                                  "train/train-labels-idx1-ubyte");
    std::vector<TSample> testAll = loadMnistFile("test/t10k-images-idx3-ubyte",
                                                 "test/t10k-labels-idx1-ubyte");
    if (trainAll.empty() || testAll.empty()) { MPI_Finalize(); return 1; }

    const size_t cvN = std::min<size_t>(trainAll.size(), opt.trainLimit);
    std::vector<TSample> cvDs(trainAll.begin(), trainAll.begin() + cvN);
    const size_t testN = std::min<size_t>(testAll.size(), opt.testLimit);
    std::vector<TSample> testDs(testAll.begin(), testAll.begin() + testN);
    const size_t fTrainN = (opt.finalTrain > 0)
        ? std::min<size_t>(trainAll.size(), opt.finalTrain) : trainAll.size();
    std::vector<TSample> finalTrain(trainAll.begin(), trainAll.begin() + fTrainN);

    const double tLoad = MPI_Wtime();

    int bestK = opt.mink;
    double cvAcc = 0.0, testAcc = 0.0;
    double tFine = 0.0, tFinal = 0.0;

    if (opt.strategy == "spec") {
        // Grupo A = fine-tuning (ranks [0,A)), Grupo B = evaluacion especulativa
        // (ranks [A,size)). La comunicacion entre grupos usa MPI_COMM_WORLD con
        // destinos globales A+i; la intra-grupo usa el comunicador `group`.
        int A = opt.groupA > 0 ? opt.groupA : std::max(1, size / 2);
        if (A >= size) A = size - 1;
        if (A < 1) A = 1;
        const int color = (rank < A) ? 0 : 1;
        const int bSize = size - A;
        MPI_Comm group;
        MPI_Comm_split(MPI_COMM_WORLD, color, rank, &group);

        const double ts0 = MPI_Wtime();
        if (color == 0) {
            // --- Fine-tuning: k secuencial, folds repartidos en el grupo A ---
            int arank, asize;
            MPI_Comm_rank(group, &arank);
            MPI_Comm_size(group, &asize);
            const size_t n = cvDs.size();
            int localBest = opt.mink; double localBestAcc = 0.0;
            for (int k = opt.mink; k <= opt.maxk; k++) {
                double part = 0.0;
                size_t flo, fhi;
                rangeOf(opt.folds, arank, asize, flo, fhi);
                for (int f = (int)flo; f < (int)fhi; f++) {
                    const std::vector<int> tr = foldTrainIdx(n, f, opt.folds);
                    const size_t start = n * f / opt.folds, end = n * (f + 1) / opt.folds;
                    long m = evalRange(cvDs, tr, k, cvDs, start, end);
                    part += double(m) / double(end - start);
                }
                double total = 0.0;
                MPI_Allreduce(&part, &total, 1, MPI_DOUBLE, MPI_SUM, group);
                const double a = total / opt.folds;
                if (a > localBestAcc) { localBestAcc = a; localBest = k; }
                if (arank == 0) {
                    // Avisar al grupo B del nuevo mejor K (mensaje eager de 1 int).
                    for (int i = 0; i < bSize; i++)
                        MPI_Send(&localBest, 1, MPI_INT, A + i, TAG_NEWBEST, MPI_COMM_WORLD);
                }
            }
            if (arank == 0) {
                for (int i = 0; i < bSize; i++)
                    MPI_Send(&localBest, 1, MPI_INT, A + i, TAG_DONE, MPI_COMM_WORLD);
                tFine = MPI_Wtime() - ts0;
            }
            MPI_Bcast(&localBest, 1, MPI_INT, 0, group);
            bestK = localBest; cvAcc = localBestAcc;
        } else {
            // --- Grupo B: evaluacion final especulativa ---
            int brank, bs;
            MPI_Comm_rank(group, &brank);
            MPI_Comm_size(group, &bs);
            size_t lo, hi;
            rangeOf(testN, brank, bs, lo, hi);
            long m = speculativeEval(finalTrain, testDs, lo, hi, opt.mink);
            long total = 0;
            MPI_Allreduce(&m, &total, 1, MPI_LONG, MPI_SUM, group);
            if (brank == 0) {
                double pack[2] = { double(total) / double(testN), MPI_Wtime() - ts0 };
                MPI_Send(pack, 2, MPI_DOUBLE, 0, TAG_ACC, MPI_COMM_WORLD);
            }
        }
        MPI_Comm_free(&group);

        // El proceso 0 recibe la exactitud y el tiempo del grupo de evaluacion.
        if (rank == 0) {
            double pack[2] = {0.0, 0.0};
            MPI_Recv(pack, 2, MPI_DOUBLE, A, TAG_ACC, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            testAcc = pack[0];
            tFinal = pack[1];
        }
        // Difundir el mejor K final (ya lo conocen A y B, pero unifica).
        MPI_Bcast(&bestK, 1, MPI_INT, 0, MPI_COMM_WORLD);
    } else {

        double tf0 = MPI_Wtime();
        fineTuneParallel(cvDs, opt, rank, size, bestK, cvAcc);
        tFine = MPI_Wtime() - tf0;
        double te0 = MPI_Wtime();
        testAcc = finalEvalParallel(finalTrain, testDs, bestK, rank, size);
        tFinal = MPI_Wtime() - te0;
    }

    const double tTotal = MPI_Wtime() - t0;
    if (rank == 0) {
        printf("RESULT strategy=%s P=%d train=%zu test=%zu finaltrain=%zu K=[%d,%d] folds=%d "
               "bestK=%d cvAcc=%.4f testAcc=%.4f load=%.4f tFine=%.4f tFinal=%.4f tTotal=%.4f\n",
               opt.strategy.c_str(), size, cvN, testN, fTrainN, opt.mink, opt.maxk, opt.folds,
               bestK, cvAcc, testAcc, tLoad - t0, tFine, tFinal, tTotal);
    }

    MPI_Finalize();
    return 0;
}
