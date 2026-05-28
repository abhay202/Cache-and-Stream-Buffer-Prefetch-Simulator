#include <stdio.h> 
#include <stdlib.h>
#include <inttypes.h>
#include <iostream>
#include <cmath>
#include "sim.h"
#include "Class.cpp"

int main(int argc, char* argv[]) {
    FILE* fp;                // File pointer.
    char* trace_file;        // This variable holds the trace file name.
    cache_params_t params;   // Struct to hold cache parameters.
    char rw;                 // This variable holds the request's type (read or write) obtained from the trace.
    uint32_t addr;           // This variable holds the request's address obtained from the trace.

    // Exit with an error if the number of command-line arguments is incorrect.
    if (argc != 9) {
        printf("Error: Expected 9 command-line arguments but was provided %d. Exiting.\n", (argc - 1));
        exit(EXIT_FAILURE);
    }

    // Parse the command-line arguments into our cache parameters struct.
    params.BLOCKSIZE = atoi(argv[1]);
    params.L1_SIZE = atoi(argv[2]);
    params.L1_ASSOC = atoi(argv[3]);
    params.L2_SIZE = atoi(argv[4]);
    params.L2_ASSOC = atoi(argv[5]);
    params.PREF_N = atoi(argv[6]);
    params.PREF_M = atoi(argv[7]);
    trace_file = argv[8];

    // Open the trace file for reading.
    fp = fopen(trace_file, "r");
    if (fp == NULL) {
        printf("Error: Unable to open file %s\n", trace_file);
        exit(EXIT_FAILURE);
    }

    // Simulator configuration output
    printf("===== Simulator configuration =====\n");
    printf("BLOCKSIZE:  %u\n", params.BLOCKSIZE);
    printf("L1_SIZE:    %u\n", params.L1_SIZE);
    printf("L1_ASSOC:   %u\n", params.L1_ASSOC);
    printf("L2_SIZE:    %u\n", params.L2_SIZE);
    printf("L2_ASSOC:   %u\n", params.L2_ASSOC);
    printf("PREF_N:     %u\n", params.PREF_N);
    printf("PREF_M:     %u\n", params.PREF_M);
    printf("trace_file: %s\n", trace_file);

    // Initialize the L2 cache if its size is greater than 0, otherwise leave it as null.
    Cache* L2 = nullptr;
    if (params.L2_SIZE > 0) {
        L2 = new Cache(params.L2_SIZE, params.L2_ASSOC, params.BLOCKSIZE, nullptr); // L2 cache, next level is memory
    }

    // Initialize the L1 cache, passing the L2 cache as its next level (or null if no L2 cache).
    Cache L1(params.L1_SIZE, params.L1_ASSOC, params.BLOCKSIZE, L2);

    // Read requests from the trace file and issue them to the L1 cache.
    while (fscanf(fp, "%c %x\n", &rw, &addr) == 2) {
        L1.request(rw, addr);  // Issue the request to the L1 cache
    }

    // Print the contents and measurements for both caches.
    std::cout << std::endl << "===== L1 contents =====" << std::endl;
    L1.printContents();
    if (L2) {
        std::cout << std::endl << "===== L2 contents =====" << std::endl;
        L2->printContents();
    }

    std::cout << std::endl << "===== Measurements =====" << std::endl;
    L1.printMeasurements();
    if (L2) {
        L2->printL2Measurements();
    } else {
        L2->printL2zeroMeasurement();
    }

    std::cout << std::left << std::setw(30) << "q. memory traffic:" << memoryTraffic << std::endl;

    fclose(fp);
    return 0;
}
