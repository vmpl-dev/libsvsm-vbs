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

#include "svsm-vbs.h"

static int xom_fd = -1;

// Public API functions
int xom_init(void) {
    if (xom_fd >= 0) {
        return 0;  // Already initialized
    }

    xom_fd = open("/dev/" SVSM_VBS_DEVICE_NAME, O_RDWR);
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

int xom_protect(const void *addr, size_t len) {
    if (xom_fd < 0) {
        return -1;
    }

    struct SvsmVbsRequest req = {
        .vstart = (uint64_t)addr,
        .vend = (uint64_t)addr + len
    };

    printf("Protecting region: %lx-%lx\n", req.vstart, req.vend);
    return ioctl(xom_fd, SVSM_VBS_IOC_PROTECT, &req);
}

int xom_unprotect(const void *addr, size_t len) {
    if (xom_fd < 0) {
        return -1;
    }

    struct SvsmVbsRequest req = {
        .vstart = (uint64_t)addr,
        .vend = (uint64_t)addr + len
    };

    printf("Unprotecting region: %lx-%lx\n", req.vstart, req.vend);
    return ioctl(xom_fd, SVSM_VBS_IOC_UNPROTECT, &req);
}

int xom_commit(void) {
    if (xom_fd < 0) {
        return -1;
    }

    return ioctl(xom_fd, SVSM_VBS_IOC_COMMIT, NULL);
}

#ifdef CONFIG_XOM_PROCMAPS
typedef int (*callback_t)(void *addr, size_t len);

static int with_procmaps(callback_t callback, void *arg) {
    int ret = 0;
    size_t num_pages = 0;
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
            num_pages += (end - start) >> 12;
        }
    }

    printf("Number of pages: %zu\n", num_pages);
    if (num_pages % SVSM_VBS_BATCH_SIZE != 0) {
        printf("Number of pages is not a multiple of %d\n", SVSM_VBS_BATCH_SIZE);
        ret = xom_commit();
        if (ret < 0) {
            printf("Failed to commit remaining %d pages\n", num_pages % SVSM_VBS_BATCH_SIZE);
            return ret;
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
#else
int xom_protect_all(void) {
    if (xom_fd < 0) {
        return -1;
    }

    clock_t start_time = clock();
    int ret = ioctl(xom_fd, SVSM_VBS_IOC_PROTECT_ALL);
    printf("Time taken: %.4f seconds\n", (double)(clock() - start_time) / CLOCKS_PER_SEC);
    return ret;
}

int xom_unprotect_all(void) {
    if (xom_fd < 0) {
        return -1;
    }

    clock_t start_time = clock();
    int ret = ioctl(xom_fd, SVSM_VBS_IOC_UNPROTECT_ALL);
    printf("Time taken: %.4f seconds\n", (double)(clock() - start_time) / CLOCKS_PER_SEC);
    return ret;
}
#endif
