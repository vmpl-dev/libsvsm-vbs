#ifndef __LIBXOM_H__
#define __LIBXOM_H__

#include <sys/types.h>

// Initialize XOM protection
int xom_init(void);

// Finalize XOM protection
int xom_fini(void);

// Protect a memory region with XOM
int xom_protect(const void *addr, size_t len);

// Unprotect a memory region from XOM
int xom_unprotect(const void *addr, size_t len);

// Protect all memory regions
int xom_protect_all(void);

// Unprotect all memory regions
int xom_unprotect_all(void);

#endif
