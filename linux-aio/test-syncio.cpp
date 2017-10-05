#include <iostream>
#include <random>
#include <ctime>

#include "syncio.hpp"

int main(int argc, char* argv[]) {

    if (argc < 3) {
        fprintf(stderr, "usage: ./test-syncio filename npart <nelems>\n");
        exit(EXIT_FAILURE);
    }

    std::string fn = argv[1];
    auto npart = atoi(argv[2]);

    auto nelems = 33554432;

    if (argc > 3)
        nelems = atoi(argv[3]);

    srand(123);
    std::default_random_engine generator;
    std::uniform_real_distribution<double> distribution(0.0, 9.0);

    std::vector<double> data;

    printf("Generating randoms ...");
    for (auto i = 0; i < nelems; i++) {
        double d = distribution(generator);
        data.push_back(d);
    }
    printf("Done!\n");
#if 0
    // Echo it ...
    std::cout << "Echo:\n[ ";
    for (std::vector<double>::iterator it = data.begin();
            it != data.end(); ++it)
        std::cout << *it << " ";
    std::cout << "\n";
#endif

    // Write it ...
    std::cout << "Writing ...\n";
    std::clock_t start = std::clock();
    PartitionedFile<double>::ptr file = PartitionedFile<double>::create(fn, npart);
    file->write(data);

    auto duration = ( std::clock() - start ) / (double) CLOCKS_PER_SEC;
    std::cout << "Duration: " << duration << "\n";
    return EXIT_SUCCESS;
}
