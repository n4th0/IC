// Driver de profiling configurable para knn2 (Tarea 1).
// Reproduce el flujo de knn2/main.cc (fine-tuning por validacion cruzada +
// evaluacion final) pero permite reducir el tamano del problema por linea de
// comandos, para poder perfilar con gprof en tiempos razonables tanto a -O0
// como a -O2 sin modificar el codigo proporcionado.
//
// Uso: knn_profile [trainLimit] [minK] [maxK] [folds] [testLimit]
//   trainLimit : numero maximo de muestras de train usadas en el fine-tuning
//   minK/maxK  : rango de k explorado
//   folds      : numero de particiones de la validacion cruzada
//   testLimit  : numero maximo de muestras de test usadas en la evaluacion final
#include "sample.h"
#include "knn.h"
#include "mnist.h"
#include "crossValidation.h"
#include "timer.h"

#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdlib>

using namespace knn;
using namespace mnist;
using namespace crossValidation;

static bool loadDataset(std::vector<TSample> &train, std::vector<TSample> &test)
{
    if (!loadMnist("train/train-images-idx3-ubyte", "train/train-labels-idx1-ubyte", train)) return false;
    if (!loadMnist("test/t10k-images-idx3-ubyte", "test/t10k-labels-idx1-ubyte", test)) return false;
    return true;
}

int main(int argc, char **argv)
{
    int trainLimit = (argc > 1) ? atoi(argv[1]) : 20000;
    int minK       = (argc > 2) ? atoi(argv[2]) : 1;
    int maxK       = (argc > 3) ? atoi(argv[3]) : 10;
    int folds      = (argc > 4) ? atoi(argv[4]) : 5;
    int testLimit  = (argc > 5) ? atoi(argv[5]) : 100000;

    std::vector<TSample> train, test;
    if (!loadDataset(train, test)) return 1;

    std::size_t trainN = std::min<std::size_t>(train.size(), trainLimit);
    std::size_t testN  = std::min<std::size_t>(test.size(), testLimit);
    std::vector<TSample> trainSub(train.begin(), train.begin() + trainN);
    std::vector<TSample> testSub(test.begin(), test.begin() + testN);

    CrossValidation cv = CrossValidation::NFold(trainSub, folds);

    int bestK = minK; double bestAcc = 0.0;
    for (int k = minK; k <= maxK; k++) {
        double acc = cv.validate(k);
        if (acc > bestAcc) { bestAcc = acc; bestK = k; }
    }

    KNN knn(train, bestK);
    std::vector<int> labels;
    double testAcc = knn.classifyAndEvaluate(testSub, labels);

    std::cerr << "PROFILE_DONE bestK=" << bestK
              << " bestAcc=" << bestAcc
              << " testAcc=" << testAcc << "\n";
    return 0;
}
