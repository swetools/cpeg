#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include "libcpeg_terms.h"

static cpeg_term *term_free_list;

static cpeg_term *
alloc_term(const cpeg_term_type *type)
{
    cpeg_term *term = term_free_list;

    if (term != NULL)
        term_free_list = term_free_list->value;
    else
    {
        term = malloc(sizeof(*term));
        assert(term != NULL);
    }
    term->type   = type;
    term->refcnt = 1;
    return term;
}

static void
alloc_children(cpeg_term *term, unsigned n_children,
               cpeg_term *children[], bool share)
{
    unsigned i;

    term->n_children = n_children;
    if (n_children == 0)
        term->children = NULL;
    else
        term->children = malloc(n_children * sizeof(*term->children));

    for (i = 0; i < n_children; i++)
        term->children[i] = share ? cpeg_term_use(children[i]) : children[i];
}

cpeg_term *
cpeg_term_new(const cpeg_term_type *type, void *value,
              unsigned n_children, cpeg_term *children[])
{
    cpeg_term *term = alloc_term(type);

    term->value  = type->init ? type->init(value) : value;
    alloc_children(term, n_children, children, false);

    return term;
}

#define MAX_INLINE_CHILDREN 64
#define SETUP_INLINE_CHILDREN(_children, _n, _lastarg)          \
    cpeg_term *_children[MAX_INLINE_CHILDREN];                  \
    va_list _args;                                              \
    unsigned _n;                                                \
                                                                \
    va_start(_args, _lastarg);                                  \
    for (_n = 0;; _n++)                                         \
    {                                                           \
        _children[_n] = va_arg(_args, cpeg_term *);             \
        if (_children[_n] == NULL)                              \
            break;                                              \
        assert(_n < sizeof(_children) / sizeof(*_children));    \
    }                                                           \
    va_end(_args)

cpeg_term *
cpeg_term_newl(const cpeg_term_type *type, void *value, ...)
{
    SETUP_INLINE_CHILDREN(children, n, value);

    return cpeg_term_new(type, value, n, children);
}

cpeg_term *
cpeg_term_fromstr(const cpeg_term_type *type,
                  const char *value, unsigned n_children,
                  cpeg_term *children[])
{
    cpeg_term *term = alloc_term(type);

    assert(type->fromstr != NULL);
    term->value = type->fromstr(value);
    alloc_children(term, n_children, children, false);

    return term;
}

cpeg_term *
cpeg_term_fromstrl(const cpeg_term_type *type, const char *value, ...)
{
    SETUP_INLINE_CHILDREN(children, n, value);

    return cpeg_term_fromstr(type, value, n, children);
}

void
cpeg_term_reclaim(cpeg_term *term)
{
    unsigned i;

    assert(term->refcnt == 0);
    if (term->type->destroy)
        term->type->destroy(term->value);
    for (i = 0; i < term->n_children; i++)
        cpeg_term_free(term->children[i]);
    free(term->children);
    term->value = term_free_list;
    term_free_list = term;
}

cpeg_term *
cpeg_term_copy(const cpeg_term *term)
{
    if (term == NULL)
        return NULL;
    else
    {
        cpeg_term *copy = alloc_term(term->type);

        copy->value = term->type->init ?
            term->type->init(term->value) :
            term->value;
        alloc_children(copy, term->n_children, term->children, true);

        return copy;
    }
}


cpeg_term *
cpeg_term_deep_copy(const cpeg_term *term)
{
    if (term == NULL)
        return NULL;
    else
    {
        cpeg_term *children[term->n_children + 1];
        unsigned i;

        for (i = 0; i < term->n_children; i++)
            children[i] = cpeg_term_deep_copy(term->children[i]);

        return cpeg_term_new(term->type, term->value,
                             term->n_children, children);
    }
}


cpeg_term *
cpeg_term_graft(cpeg_term *term, unsigned pos,
                cpeg_term *child)
{
    cpeg_term **new_children;

    if (pos == UINT_MAX)
        pos = term->n_children;
    assert(pos <= term->n_children);

    new_children = malloc(sizeof(*new_children) * (term->n_children + 1));
    assert(new_children != NULL);

    memcpy(new_children, term->children, sizeof(*term->children) * pos);
    new_children[pos] = child;
    memcpy(&new_children[pos + 1], &term->children[pos],
           sizeof(*term->children) * (term->n_children - pos));
    term->n_children++;
    free(term->children);
    term->children = new_children;

    return term;
}

cpeg_term *
cpeg_term_glue(cpeg_term *term,
               const cpeg_term *side)
{
    unsigned i;

    if (side == NULL || side->n_children == 0)
        return term;

    term->children = realloc(term->children,
                             sizeof(*term->children) *
                             (term->n_children + side->n_children));
    for (i = 0; i < side->n_children; i++)
        term->children[i + term->n_children] = cpeg_term_use(side->children[i]);
    term->n_children += side->n_children;

    return term;
}

cpeg_term *
cpeg_term_prune(cpeg_term *term, unsigned pos)
{
    cpeg_term *pruned;

    if (pos >= term->n_children)
        return NULL;

    pruned = term->children[pos];
    memmove(&term->children[pos], &term->children[pos + 1],
            sizeof(*term->children) * (term->n_children - pos - 1));
    term->n_children--;

    return pruned;
}


int
cpeg_term_traverse_preorder(cpeg_term_traverse_fn fn,
                            const cpeg_term *term,
                            void *data)
{
    unsigned i;
    int rc;

    rc = fn(term, data);
    if (rc != 0)
        return rc;

    for (i = 0; i < term->n_children; i++)
    {
        rc = cpeg_term_traverse_preorder(fn, term->children[i], data);
        if (rc != 0)
            return rc;
    }
    return 0;
}

int
cpeg_term_traverse_postorder(cpeg_term_traverse_fn fn,
                             const cpeg_term *term,
                             void *data)
{
    unsigned i;
    int rc;

    for (i = 0; i < term->n_children; i++)
    {
        rc = cpeg_term_traverse_postorder(fn, term->children[i], data);
        if (rc != 0)
            return rc;
    }

    return fn(term, data);
}

cpeg_term *
cpeg_term_map(cpeg_term_map_fn map, const cpeg_term *term, void *data)
{
    cpeg_term *children[term->n_children + 1];
    cpeg_term *mapped;
    unsigned i;

    for (i = 0; i < term->n_children; i++)
        children[i] = cpeg_term_map(map, term->children[i], data);

    mapped = map(term, data);
    if (mapped == NULL)
    {
        return cpeg_term_new(term->type, term->value,
                             term->n_children, children);
    }
    assert(mapped->n_children == 0);
    alloc_children(mapped, term->n_children, children, false);

    return mapped;
}

void *
cpeg_term_reduce(cpeg_term_reduce_fn reduce, const cpeg_term *term,
                 void *data)
{
    void *results[term->n_children + 1];
    unsigned i;

    for (i = 0; i < term->n_children; i++)
        results[i] = cpeg_term_reduce(reduce, term->children[i], data);

    return reduce(term, results, data);
}
