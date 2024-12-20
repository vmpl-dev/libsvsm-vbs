#define _GNU_SOURCE
#include <dlfcn.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <string.h>

#include "libxom.h"

// Original function pointers
static int (*original_mprotect)(void *addr, size_t len, int prot);
static void* (*original_mmap)(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
static int (*original_munmap)(void *addr, size_t length);

// Initialize function pointers and open XOM device
__attribute__((constructor))
static void init_xom(void) {
    // Load original functions
    original_mprotect = dlsym(RTLD_NEXT, "mprotect");
    original_mmap = dlsym(RTLD_NEXT, "mmap");
    original_munmap = dlsym(RTLD_NEXT, "munmap");

    // Open XOM device
    xom_init();

    // Protect all memory regions
    xom_protect_all();
}

// Cleanup when library is unloaded
__attribute__((destructor))
static void cleanup_xom(void) {
    // Unprotect all memory regions
    xom_unprotect_all();

    // Finalize XOM
    xom_fini();
}

// Hook mprotect
int mprotect(void *addr, size_t len, int prot) {
    int ret;

    // First call original mprotect
    ret = original_mprotect(addr, len, prot);
    if (ret < 0) {
        return ret;
    }

    // Then apply XOM protection if execute permission is set
    if (prot & PROT_EXEC) {
        ret = xom_protect(addr, len);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

// Hook mmap
void* mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    int ret;

    // First call original mmap
    void *result = original_mmap(addr, length, prot, flags, fd, offset);
    
    // If mapping failed, return result
    if (result == MAP_FAILED) {
        return result;
    }

    // Then apply XOM protection if executable memory is requested
    if (prot & PROT_EXEC) {
        ret = xom_protect(result, length);
        if (ret < 0) {
            return MAP_FAILED;
        }
    }

    return result;
}

// Hook munmap
int munmap(void *addr, size_t length) {
    int ret;

    // If XOM is initialized, try to remove XOM protection first
    ret = xom_unprotect(addr, length);
    if (ret < 0) {
        return ret;
    }

    return original_munmap(addr, length);
}
