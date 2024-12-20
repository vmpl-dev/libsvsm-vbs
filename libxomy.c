#define _GNU_SOURCE
#include <dlfcn.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdarg.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "libxom.h"

// Original function pointers
static int (*original_mprotect)(void *addr, size_t len, int prot);
static void* (*original_mmap)(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
static int (*original_munmap)(void *addr, size_t length);
static void* (*original_mremap)(void *old_address, size_t old_size, size_t new_size, int flags, ...);
static int (*original_remap_file_pages)(void *addr, size_t size, int prot, size_t pgoff, int flags);
static int (*original_madvise)(void *addr, size_t length, int advice);
static void* (*original_shmat)(int shmid, const void *shmaddr, int shmflg);
static int (*original_shmdt)(const void *shmaddr);
static int (*original_mlock)(const void *addr, size_t len);
static int (*original_mlock2)(const void *addr, size_t len, unsigned int flags);
static int (*original_munlock)(const void *addr, size_t len);
static int (*original_mlockall)(int flags);
static int (*original_munlockall)(void);

static int (*original_pkey_mprotect)(void *addr, size_t len, int prot, int pkey);

// Initialize function pointers and open XOM device
__attribute__((constructor))
static void init_xom(void) {
    // Load original functions
    original_mprotect = dlsym(RTLD_NEXT, "mprotect");
    original_mmap = dlsym(RTLD_NEXT, "mmap");
    original_munmap = dlsym(RTLD_NEXT, "munmap");
    original_mremap = dlsym(RTLD_NEXT, "mremap");
    original_remap_file_pages = dlsym(RTLD_NEXT, "remap_file_pages");
    original_mlock = dlsym(RTLD_NEXT, "mlock");
    original_munlock = dlsym(RTLD_NEXT, "munlock");
    original_madvise = dlsym(RTLD_NEXT, "madvise");
    original_shmat = dlsym(RTLD_NEXT, "shmat");
    original_shmdt = dlsym(RTLD_NEXT, "shmdt");
    original_mlockall = dlsym(RTLD_NEXT, "mlockall");
    original_mlock2 = dlsym(RTLD_NEXT, "mlock2");
    original_pkey_mprotect = dlsym(RTLD_NEXT, "pkey_mprotect");

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

// Hook remap_file_pages
int remap_file_pages(void *addr, size_t size, int prot, size_t pgoff, int flags) {
    // First call original remap_file_pages
    int ret = original_remap_file_pages(addr, size, prot, flags, pgoff);
    if (ret < 0) {
        return ret;
    }

    // Then apply XOM protection if executable memory is requested
    if (prot & PROT_EXEC) {
        ret = xom_protect(addr, size);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

// Hook mremap
void* mremap(void *old_address, size_t old_size, size_t new_size, int flags, ...) {
    int ret;
    void *new_address = NULL;
    void *result = NULL;

    // Get new_address from va_list
    va_list args;
    va_start(args, flags);
    new_address = va_arg(args, void *);
    va_end(args);

    // Unprotect old memory region
    ret = xom_unprotect(old_address, old_size);
    if (ret < 0) {
        return MAP_FAILED;
    }

    // If new_address is not NULL, we are remapping to a new address
    if (new_address != NULL) {
        // First call original mremap
        result = original_mremap(old_address, old_size, new_size, flags, new_address);
        if (result != MAP_FAILED) {
            // Protect new memory region
            ret = xom_protect(new_address, new_size);
            if (ret < 0) {
                return MAP_FAILED;
            }
        }
    } else {
        // First call original mremap
        result = original_mremap(old_address, old_size, new_size, flags);
        if (result != MAP_FAILED) {
            // Protect new memory region
            ret = xom_protect(result, new_size);
            if (ret < 0) {
                return MAP_FAILED;
            }
        }
    }

    return result;
}

// Hook madvise
int madvise(void *addr, size_t length, int advice) {
    int ret = original_madvise(addr, length, advice);
    if (ret == 0) {
        xom_protect(addr, length);
    }
    return ret;
}

// Hook shmat
void* shmat(int shmid, const void *shmaddr, int shmflg) {
    void *result = original_shmat(shmid, shmaddr, shmflg);
    if (result != (void *)-1) {
        // Need to get the size of shared memory segment
        struct shmid_ds buf;
        if (shmctl(shmid, IPC_STAT, &buf) == 0) {
            xom_protect(result, buf.shm_segsz);
        }
    }
    return result;
}

// Hook shmdt
int shmdt(const void *shmaddr) {
    xom_unprotect(shmaddr, 0); // Size not needed for unprotect
    return original_shmdt(shmaddr);
}

// Hook mlock
int mlock(const void *addr, size_t len) {
    int ret;

    // First call original mlock
    ret = original_mlock(addr, len);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

// Hook mlock2
int mlock2(const void *addr, size_t len, unsigned int flags) {
    int ret = original_mlock2(addr, len, flags);
    if (ret == 0) {
        xom_protect(addr, len);
    }
    return ret;
}

// Hook munlock
int munlock(const void *addr, size_t len) {
    int ret;

    // First call original munlock
    ret = original_munlock(addr, len);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

// Hook mlockall
int mlockall(int flags) {
    int ret = original_mlockall(flags);
    if (ret == 0) {
        xom_protect_all();
    }
    return ret;
}

// Hook munlockall
int munlockall(void) {
    xom_unprotect_all();
    return original_munlockall();
}

// Hook pkey_mprotect
int pkey_mprotect(void *addr, size_t len, int prot, int pkey) {
    int ret = original_pkey_mprotect(addr, len, prot, pkey);
    if (ret == 0 && (prot & PROT_EXEC)) {
        xom_protect(addr, len);
    }
    return ret;
}
