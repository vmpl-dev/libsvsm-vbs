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

    return ioctl(xom_fd, VMPL_XOM_IOC_UNPROTECT, &req);
}
