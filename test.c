#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <string.h>
#include <time.h>

#include "libxom.h"

int main() {
    // Initialize XOM
    if (xom_init() < 0) {
        fprintf(stderr, "Failed to initialize XOM\n");
        return 1;
    }
    printf("XOM initialized\n");

    // Protect some memory region
    void *mem = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, 
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    // The memory will be automatically protected by XOM
    // due to the PROT_EXEC flag
    printf("Memory protected by XOM\n");

    // Unprotect the memory region
    clock_t start = clock();
    if (xom_unprotect(mem, 4096) < 0) {
        fprintf(stderr, "Failed to unprotect memory region\n");
        return 1;
    }
    clock_t end = clock();
    printf("Memory unprotected by XOM in %ld seconds\n", end - start);

    // Finalize XOM
    if (xom_fini() < 0) {
        fprintf(stderr, "Failed to finalize XOM\n");
        return 1;
    }
    printf("XOM finalized\n");

    // Clean up
    munmap(mem, 4096);

    return 0;
}
