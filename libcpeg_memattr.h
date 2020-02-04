/**
 * Copyright (c) 2020, Artem V. Andreev
 *
 * SPDX-License-Identifier: MIT
 */


#ifndef LIBCPEG_MEMATTR_H
#define LIBCPEG_MEMATTR_H 1

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

extern void *cpeg_mem_alloc(size_t size);

struct cpeg_term;

extern struct cpeg_term *cpeg_mem_attr_get(const void *addr, const void *attr);

extern void cpeg_mem_attr_set(const void *addr, const void *attr,
                              struct cpeg_term *val);

extern void cpeg_mem_release_attrs(const void *addr);

extern void cpeg_mem_free(void *addr);

extern void *cpeg_mem_realloc(void *oldaddr, size_t newsize);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* LIBCPEG_MEMATTR_H */
