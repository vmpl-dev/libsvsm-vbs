#ifndef __LIBXOM_H__
#define __LIBXOM_H__

#include <sys/types.h>

// Initialize XOM protection
int xom_init(void);

// Finalize XOM protection
int xom_fini(void);

// Protect a memory region with XOM
int xom_protect(void *addr, size_t len);

// Unprotect a memory region from XOM
int xom_unprotect(void *addr, size_t len);

#endif
