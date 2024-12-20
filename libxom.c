#define _GNU_SOURCE
#include <stdio.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <string.h>
#include <asm/vsyscall.h>
#include <time.h>

#include "libxom.h"

#include "vmpl-xom.h"

static int xom_fd = -1;

// Public API functions
int xom_init(void) {
    if (xom_fd >= 0) {
        return 0;  // Already initialized
    }

    xom_fd = open("/dev/" VMPL_XOM_DEVICE_NAME, O_RDWR);
    return (xom_fd >= 0) ? 0 : -1;
}

int xom_fini(void) {
    if (xom_fd < 0) {
        return 0;  // Already closed
    }

    close(xom_fd);
    xom_fd = -1;
    return 0;
}

int xom_protect(void *addr, size_t len) {
    if (xom_fd < 0) {
        return -1;
    }

    struct VmplXomRequest req = {
        .vstart = (uint64_t)addr,
        .vend = (uint64_t)addr + len
    };

    printf("Protecting region: %lx-%lx\n", req.vstart, req.vend);
    return ioctl(xom_fd, VMPL_XOM_IOC_PROTECT, &req);
}

int xom_unprotect(void *addr, size_t len) {
    if (xom_fd < 0) {
        return -1;
    }

    struct VmplXomRequest req = {
        .vstart = (uint64_t)addr,
        .vend = (uint64_t)addr + len
    };

    printf("Unprotecting region: %lx-%lx\n", req.vstart, req.vend);
    return ioctl(xom_fd, VMPL_XOM_IOC_UNPROTECT, &req);
}

typedef int (*callback_t)(void *addr, size_t len);

static int with_procmaps(callback_t callback, void *arg) {
    int ret = 0;
    clock_t start_time = clock();
    FILE *file = fopen("/proc/self/maps", "r");
    if (!file) {
        return -1;
    }

    char line[4096];
    while (fgets(line, sizeof(line), file)) {
        uint64_t start, end;
        char perms[5];  // rwxp\0
        
        // Parse line format: start-end perms offset dev inode path
        // Example: 7ff7c5d91000-7ff7c5db3000 r-xp 00000000 08:01 123456 /lib/x86_64-linux-gnu/libc-2.31.so
        if (sscanf(line, "%lx-%lx %4s", &start, &end, perms) != 3) {
            printf("Failed to parse line: %s\n", line);
            continue;
        }

        if (start == VSYSCALL_ADDR) {
            printf("Skipping vsyscall region: %lx-%lx\n", start, end);
            continue;
        }

        // Check if region is executable (x flag in perms)
        if (perms[2] == 'x') {
            ret = callback((void*)start, end - start);
            if (ret < 0) {
                return ret;
            }
        }
    }

    fclose(file);
    printf("Time taken: %.4f seconds\n", (double)(clock() - start_time) / CLOCKS_PER_SEC);
    return ret;
}

int xom_protect_all(void) {
    printf("Protecting all memory regions\n");
    return with_procmaps(xom_protect, NULL);
}

int xom_unprotect_all(void) {
    printf("Unprotecting all memory regions\n");
    return with_procmaps(xom_unprotect, NULL);
}
