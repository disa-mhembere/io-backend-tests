#include <iostream>
#include <random>
#include <ctime>

#include "mp-rand-read.hpp"

int main(int argc, char* argv[]) {

    if (argc < 4) {
        fprintf(stderr, "usage: ./test-mp-rand-read filename1 "
                "filename2 nparts <nelems>\n");
        exit(EXIT_FAILURE);
    }

    auto nelems = 33554432;
    if (argc > 4)
        nelems = atoi(argv[4]);

    AddOverlord<double>::ptr ov = AddOverlord<double>::create(argv[1], argv[2],
            atoi(argv[3]), nelems);

    ov->run();
    return EXIT_SUCCESS;
}

