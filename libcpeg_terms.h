/**
 * Copyright (c) 2019, Artem V. Andreev
 *
 * SPDX-License-Identifier: MIT
 */


#ifndef LIBCPEG_TERMS_H
#define LIBCPEG_TERMS_H 1

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

typedef struct cpeg_term cpeg_term;

typedef struct cpeg_term_type {
    const char *id;
    void *(*init)(void *);
    void *(*fromstr)(const char *);
    void (*destroy)(void *);
} cpeg_term_type;

typedef struct cpeg_term {
    const cpeg_term_type *type;
    unsigned refcnt;
    void *value;
    unsigned n_children;
    cpeg_term **children;
} cpeg_term;

extern cpeg_term *cpeg_term_new(const cpeg_term_type *type,
                                void *value, unsigned n_children,
                                cpeg_term *children[]);

extern cpeg_term *cpeg_term_newl(const cpeg_term_type *type,
                                 void *value, ...);

extern cpeg_term *cpeg_term_fromstr(const cpeg_term_type *type,
                                    const char *value, unsigned n_children,
                                    cpeg_term *children[]);

extern cpeg_term *cpeg_term_fromstrl(const cpeg_term_type *type,
                                     const char *value, ...);


static inline cpeg_term *
cpeg_term_use(cpeg_term *term)
{
    if (term != NULL)
        term->refcnt++;
    return term;
}

extern void cpeg_term_reclaim(cpeg_term *term);

static inline void
cpeg_term_free(cpeg_term *term)
{
    if (term != NULL && term->refcnt-- <= 1)
        cpeg_term_reclaim(term);
}

extern cpeg_term *cpeg_term_copy(const cpeg_term *term);

extern cpeg_term *cpeg_term_deep_copy(const cpeg_term *term);

static inline cpeg_term *
cpeg_term_cow(cpeg_term *term)
{
    if (term == NULL || term->refcnt == 1)
        return term;

    return cpeg_term_copy(term);
}

extern cpeg_term *cpeg_term_graft(cpeg_term *term,
                                  unsigned pos,
                                  cpeg_term *child);

extern cpeg_term *cpeg_term_glue(cpeg_term *term,
                                 const cpeg_term *side);

extern cpeg_term *cpeg_term_prune(cpeg_term *term,
                                  unsigned pos);

typedef int (*cpeg_term_traverse_fn)(const cpeg_term *, void *);

extern int cpeg_term_traverse_preorder(cpeg_term_traverse_fn fn,
                                       const cpeg_term *term,
                                       void *data);

extern int cpeg_term_traverse_postorder(cpeg_term_traverse_fn fn,
                                        const cpeg_term *term,
                                        void *data);

typedef cpeg_term *(*cpeg_term_map_fn)(const cpeg_term *, void *);

extern cpeg_term *cpeg_term_map(cpeg_term_map_fn map, const cpeg_term *term,
                                void *data);


typedef void *(*cpeg_term_reduce_fn)(const cpeg_term *, void *[], void *);

extern void *cpeg_term_reduce(cpeg_term_reduce_fn reduce, const cpeg_term *term,
                              void *data);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* LIBCPEG_TERMS_H */
