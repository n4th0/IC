// Tarea 4 - Referencia SECUENCIAL usando EXACTAMENTE las clases del codigo
// proporcionado (knn::KNN con copia de train, crossValidation::CrossValidation
// con splitDataset que copia, y classifyAndEvaluate que imprime el progreso).
// Sirve de linea base para medir el speed-up frente a la version MPI.
//
// Uso: seq_knn <trainLimit> <minK> <maxK> <folds> <testLimit> <finalTrain>
#include "sample.h"
#include "knn.h"
#include "mnist.h"
#include "crossValidation.h"

#include <vector>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>

using namespace knn;
using namespace mnist;
using namespace crossValidation;

static double now() {
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

int main(int argc, char **argv) {
    int trainLimit = (argc > 1) ? atoi(argv[1]) : 4000;
    int minK       = (argc > 2) ? atoi(argv[2]) : 1;
    int maxK       = (argc > 3) ? atoi(argv[3]) : 10;
    int folds      = (argc > 4) ? atoi(argv[4]) : 5;
    int testLimit  = (argc > 5) ? atoi(argv[5]) : 1500;
    int finalTrain = (argc > 6) ? atoi(argv[6]) : 0;

    std::vector<TSample> train, test;
    if (!loadMnist("train/train-images-idx3-ubyte", "train/train-labels-idx1-ubyte", train)) return 1;
    if (!loadMnist("test/t10k-images-idx3-ubyte", "test/t10k-labels-idx1-ubyte", test)) return 1;

    const std::size_t cvN = std::min<std::size_t>(train.size(), trainLimit);
    const std::size_t testN = std::min<std::size_t>(test.size(), testLimit);
    std::vector<TSample> cvDs(train.begin(), train.begin() + cvN);
    std::vector<TSample> testDs(test.begin(), test.begin() + testN);
    const std::size_t fN = (finalTrain > 0)
        ? std::min<std::size_t>(train.size(), finalTrain) : train.size();
    std::vector<TSample> fTrain(train.begin(), train.begin() + fN);

    const double t0 = now();

    CrossValidation cv = CrossValidation::NFold(cvDs, folds);
    int bestK = minK; double bestAcc = 0.0;
    const double tf0 = now();
    for (int k = minK; k <= maxK; k++) {
        double a = cv.validate(k);
        if (a > bestAcc) { bestAcc = a; bestK = k; }
    }
    const double tFine = now() - tf0;

    KNN knn(fTrain, bestK);
    const double te0 = now();
    long matches = 0;
    for (std::size_t i = 0; i < testDs.size(); i++)
        if (knn.classify(testDs[i]) == testDs[i].label) matches++;
    const double tFinal = now() - te0;
    const double testAcc = double(matches) / double(testDs.size());
    const double tTotal = now() - t0;

    printf("RESULT strategy=seq P=1 train=%zu test=%zu finaltrain=%zu K=[%d,%d] folds=%d "
           "bestK=%d cvAcc=%.4f testAcc=%.4f tFine=%.4f tFinal=%.4f tTotal=%.4f\n",
           cvN, testN, fN, minK, maxK, folds, bestK, bestAcc, testAcc, tFine, tFinal, tTotal);
    return 0;
}
