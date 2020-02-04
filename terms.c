#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include "libcpeg_memattr.h"
#include "libcpeg_terms.h"
#ifdef LIBCPEG_TESTING
#include "cqc.h"
#endif

#ifdef LIBCPEG_TESTING

typedef cpeg_term *cpeg_term_ptr;

static unsigned test_term_object_count;

static void *test_term_type_init(void *v)
{
    test_term_object_count++;
    return v;
}

static void test_term_type_destroy(__attribute__((unused)) void *v)
{
    assert(test_term_object_count > 0);
    test_term_object_count--;
}

static const cpeg_term_type test_term_type = {
    .id = "test",
    .init = test_term_type_init,
    .destroy = test_term_type_destroy
};

static void
cqc_generate_cpeg_term_ptr(cpeg_term_ptr *var, size_t scale)
{
    unsigned n_children = random() % scale;
    cpeg_term_ptr children[n_children + 1];
    unsigned i;

    for (i = 0; i < n_children; i++)
        cqc_generate_cpeg_term_ptr(&children[i], scale / n_children);

    *var = cpeg_term_new(&test_term_type, (void *)(uintptr_t)random(),
                         n_children, children);
}

#define cqc_release_cpeg_term_ptr(_var) cpeg_term_free(*(_var))

#define cqc_typefmt_cpeg_term_ptr "%s:%p[%p,%u]"
#define cqc_typeargs_cpeg_term_ptr(_t) \
    ((_t) ? (_t)->type->id : "???"),   \
        (_t),                          \
        ((_t) ? (_t)->value : "???"),  \
        ((_t) ? (_t)->n_children : 0)

#define cqc_equal_cpeg_term_ptr(_v1, _v2) ((_v1) == (_v2))

static void
cpeg_term_validate(const cpeg_term *term)
{
    unsigned i;

    assert(term->type != NULL);
    assert(term->refcnt > 0);
    for (i = 0; i < term->n_children; i++)
    {
        assert(term->children[i] != NULL);
        cpeg_term_validate(term->children[i]);
    }
}

#endif

static cpeg_term *term_free_list;

static cpeg_term *
alloc_term(const cpeg_term_type *type)
{
    cpeg_term *term = term_free_list;

    if (term != NULL)
        term_free_list = term_free_list->value;
    else
    {
        term = cpeg_mem_alloc(sizeof(*term));
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
        term->children = cpeg_mem_alloc(n_children * sizeof(*term->children));

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

#ifdef LIBCPEG_TESTING
CQC_TESTCASE(term_new, "Any term is valid",  CQC_NO_CLASSES,
             cqc_forall(cpeg_term_ptr, t,
                        cqc_expect(cpeg_term_validate(t))));

CQC_TESTCASE(term_new_no_use,
             "Term construction does not automatically increment refcounters",
             CQC_NO_CLASSES,
             cqc_forall(cpeg_term_ptr, t1,
                        cqc_expect(
                                cpeg_term_ptr t2 =
                                cpeg_term_newl(&test_term_type,
                                               NULL,
                                               t1, NULL);
                                cqc_assert_eq(unsigned, t1->refcnt, 1);
                                cqc_assert_eq(unsigned, t2->n_children, 1);
                                cqc_assert_eq(cpeg_term_ptr, t2->children[0], t1))));

CQC_TESTCASE(terms_reused,
             "A reclaimed term is reused at the next allocation",
             CQC_NO_CLASSES,
             cqc_forall(cpeg_term_ptr, t,
                        cqc_expect(
                                uintptr_t oldv = (uintptr_t)t->value;
                                cpeg_term *nt;
                                cpeg_term_free(t);
                                nt = cpeg_term_newl(&test_term_type,
                                                    (void *)(oldv + 1),
                                                    NULL);
                                cqc_assert_eq(cpeg_term_ptr, t, nt);
                                cqc_assert_eq(cqc_opaque, t->value,
                                              (void *)(oldv + 1));
                                cqc_assert_eq(unsigned, t->n_children, 0);
                                cqc_assert_eq(cqc_opaque, t->children, NULL))));

CQC_TESTCASE_SINGLE(test_static_term,
                    "Statically declared terms are not refcounted",
                    cqc_expect
                    (cpeg_term *t = CPEG_TERM_LEAF(test_term_type, NULL);
                     cpeg_term_use(t);
                     cqc_assert_eq(unsigned, t->refcnt, UINT_MAX);
                     cpeg_term_free(t);
                     cqc_assert_eq(unsigned, t->refcnt, UINT_MAX)));
#endif

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
    cpeg_mem_free(term->children);
    cpeg_mem_release_attrs(term);
    term->value = term_free_list;
    term_free_list = term;
}

#ifdef LIBCPEG_TESTING
CQC_TESTCASE(term_destructor_called,
             "Term destructor is called", CQC_NO_CLASSES,
             unsigned saved_cnt = test_term_object_count;
             cqc_forall(cpeg_term_ptr, t,
                        cqc_expect(cqc_assert(saved_cnt <
                                              test_term_object_count)));
             cqc_expect(cqc_assert_eq(unsigned, saved_cnt,
                                      test_term_object_count)));
#endif

cpeg_term *
cpeg_term_leftmost(cpeg_term *term)
{
    if (term == NULL)
        return NULL;

    while (term->n_children > 0)
        term = term->children[0];

    return term;
}

cpeg_term *
cpeg_term_rightmost(cpeg_term *term)
{
    if (term == NULL)
        return NULL;

    while (term->n_children > 0)
        term = term->children[term->n_children - 1];

    return term;
}

#ifdef LIBCPEG_TESTING
CQC_TESTCASE(term_leftmost_rightmost,
             "If a term has a single child, its leftmost and rightmost "
             "subterms are the same as those of its child",
             CQC_NO_CLASSES,
             cqc_forall
             (cpeg_term_ptr, t,
              cqc_expect
              (cpeg_term *nt = cpeg_term_newl(&test_term_type, NULL,
                                              t, NULL);
               cqc_assert_eq(cpeg_term_ptr,
                             cpeg_term_leftmost(nt),
                             cpeg_term_leftmost(t));
               cqc_assert_eq(cpeg_term_ptr,
                             cpeg_term_rightmost(nt),
                             cpeg_term_rightmost(t)))));
#endif

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

#ifdef LIBCPEG_TESTING
CQC_TESTCASE(term_copy_eq, "Shallow copying terms works", CQC_NO_CLASSES,
             cqc_forall(cpeg_term_ptr, t,
                        cqc_expect(cpeg_term *t1 = cpeg_term_copy(t);
                                   cqc_assert_neq(cqc_opaque, t1, NULL);
                                   cqc_assert_neq(cpeg_term_ptr, t1, t);
                                   cqc_assert_eq(cqc_opaque, t1->type, t->type);
                                   cqc_assert_eq(cqc_opaque, t1->value, t->value);
                                   cqc_assert_eq(unsigned, t1->refcnt, 1);
                                   cqc_assert_eq(unsigned, t->refcnt, 1);
                                   cqc_assert_eqn(cpeg_term_ptr,
                                                  t->n_children, t->children,
                                                  t1->n_children, t1->children);
                                   cqc_assert_eqfn(unsigned, t1->n_children,
                                                   t1->children, ->refcnt, 2))));
#endif

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

#ifdef LIBCPEG_TESTING
static int
equal_but_unshared(const cpeg_term *t1, const cpeg_term *t2,
                   __attribute__ ((unused)) void *data)
{
    if (t1 == t2)
        return 1;

    if (t1->type != t2->type || t1->value != t2->value)
        return 2;

    if (t1->refcnt != 1 || t2->refcnt != 1)
        return 3;

    return 0;
}

CQC_TESTCASE(test_deep_copy,
             "Deep copies do not have shared subterms",
             CQC_NO_CLASSES,
             cqc_forall
             (cpeg_term_ptr, t,
              cqc_expect
              (cpeg_term *t1 = cpeg_term_deep_copy(t);
               cqc_assert_neq(cpeg_term_ptr, t1, NULL);
               cqc_assert_eq(int,
                             cpeg_term_zip(equal_but_unshared,
                                           t, t1, NULL), 0))));
#endif

cpeg_term *
cpeg_term_graft(cpeg_term *term, unsigned pos,
                cpeg_term *child)
{
    cpeg_term **new_children;

    if (pos == UINT_MAX)
        pos = term->n_children;
    assert(pos <= term->n_children);

    new_children = cpeg_mem_realloc(term->children,
                                    sizeof(*new_children) *
                                    (term->n_children + 1));
    assert(new_children != NULL);

    memmove(&new_children[pos + 1], &new_children[pos],
            sizeof(*term->children) * (term->n_children - pos));
    new_children[pos] = child;

    term->n_children++;
    term->children = new_children;

    return term;
}

#ifdef LIBCPEG_TESTING
static int
position_class(unsigned pos, unsigned n_children)
{
    if (pos == 0)
        return 0;
    if (pos >= n_children)
        return 2;
    return 1;
}

CQC_TESTCASE(test_graft,
             "Grafting works",
             CQC_CLASS_LIST("initial", "medial", "final"),
             cqc_forall
             (cpeg_term_ptr, t1,
              cqc_forall
              (cpeg_term_ptr, t2,
               cqc_forall_range
               (unsigned, n,
                0, t1->n_children,
                cqc_classify(position_class(n, t1->n_children),
                             cqc_expect
                             (cpeg_term *tc = cpeg_term_copy(t1);
                              cpeg_term *t3 = cpeg_term_graft(t1, n, t2);
                              cqc_assert_eq(cpeg_term_ptr, t3, t1);
                              cqc_assert_eq(unsigned, t3->n_children,
                                            tc->n_children + 1);
                              cqc_assert_eq(cpeg_term_ptr, t3->children[n], t2);
                              cqc_assert_eqn(cpeg_term_ptr, n, t3->children,
                                             n, tc->children);
                              cqc_assert_eq(unsigned, t2->refcnt, 1);
                              cqc_assert_eqfn(unsigned, n,
                                              t3->children, ->refcnt, 2);
                              cqc_assert_eqn(cpeg_term_ptr,
                                             tc->n_children - n,
                                             &t3->children[n + 1],
                                             tc->n_children - n,
                                             &tc->children[n]);
                              cqc_assert_eqfn(unsigned,
                                              tc->n_children - n,
                                              &t3->children[n + 1],
                                              ->refcnt, 2)))))));
#endif

cpeg_term *
cpeg_term_glue(cpeg_term *term,
               const cpeg_term *side)
{
    unsigned i;

    if (side == NULL || side->n_children == 0)
        return term;

    term->children = cpeg_mem_realloc(term->children,
                                      sizeof(*term->children) *
                                      (term->n_children + side->n_children));
    for (i = 0; i < side->n_children; i++)
        term->children[i + term->n_children] = cpeg_term_use(side->children[i]);
    term->n_children += side->n_children;

    return term;
}

#ifdef LIBCPEG_TESTING

CQC_TESTCASE(test_glue_with_empty,
             "Gluing with a term with no children does not change the term",
             CQC_NO_CLASSES,
             cqc_forall
             (cpeg_term_ptr, t1,
              cqc_expect
              (cpeg_term *tc = cpeg_term_copy(t1);
               cpeg_term *nt = cpeg_term_newl(&test_term_type, NULL, NULL);
               cpeg_term *tg = cpeg_term_glue(t1, nt);
               cqc_assert_eq(cpeg_term_ptr, tg, t1);
               cqc_assert_eqn(cpeg_term_ptr,
                              tc->n_children, tc->children,
                              tg->n_children, tg->children))));


CQC_TESTCASE(test_empty_glue_with_empty,
             "Gluing a term with no children takes all the children "
             "of the second term",
             CQC_NO_CLASSES,
             cqc_forall
             (cpeg_term_ptr, t1,
              cqc_expect
              (cpeg_term *nt = cpeg_term_newl(&test_term_type, NULL, NULL);
               cpeg_term *tg = cpeg_term_glue(nt, t1);
               cqc_assert_eq(cpeg_term_ptr, tg, nt);
               cqc_assert_eq(cqc_opaque, tg->value, NULL);
               cqc_assert_eqn(cpeg_term_ptr,
                              t1->n_children, t1->children,
                              tg->n_children, tg->children))));

CQC_TESTCASE(test_glue,
             "Gluing works",
             CQC_NO_CLASSES,
             cqc_forall
             (cpeg_term_ptr, t1,
              cqc_forall
              (cpeg_term_ptr, t2,
               cqc_expect
               (cpeg_term *tc = cpeg_term_copy(t1);
                cpeg_term *t3 = cpeg_term_glue(t1, t2);
                cqc_assert_eq(cpeg_term_ptr, t3, t1);
                cqc_assert_eq(unsigned, t3->n_children,
                              tc->n_children + t2->n_children);
                cqc_assert_eqn(cpeg_term_ptr,
                               tc->n_children, tc->children,
                               tc->n_children, t3->children);
                cqc_assert_eqn(cpeg_term_ptr,
                               t2->n_children, t2->children,
                               t2->n_children, &t3->children[tc->n_children]);
                cqc_assert_eqfn(unsigned,
                                t3->n_children, t3->children, ->refcnt, 2)))));

CQC_TESTCASE(glue_leftmost_rightmost,
             "If two terms with children are glued, then the leftmost "
             "subterm of the result is the leftmost one of the first term "
             "and the rightmost subterm is the rightmost one of the "
             "second term",
             CQC_NO_CLASSES,
             cqc_forall
             (cpeg_term_ptr, t1,
              cqc_forall
              (cpeg_term_ptr, t2,
               cqc_condition_neq
               (unsigned, t1->n_children, 0,
                cqc_condition_neq
                (unsigned, t2->n_children, 0,
                 cqc_expect
                 (cpeg_term *lm = cpeg_term_leftmost(t1);
                  cpeg_term *rm = cpeg_term_rightmost(t2);
                  cpeg_term *g = cpeg_term_glue(t1, t2);
                  cqc_assert_eq(cpeg_term_ptr,
                                cpeg_term_leftmost(g), lm);
                  cqc_assert_eq(cpeg_term_ptr,
                                cpeg_term_rightmost(g), rm)))))));

#endif

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

#ifdef LIBCPEG_TESTING

CQC_TESTCASE(test_prune,
             "Pruning works",
             CQC_NO_CLASSES,
             cqc_forall
             (cpeg_term_ptr, t,
              cqc_condition_neq
              (unsigned, t->n_children, 0,
               cqc_forall_range
               (unsigned, pos, 0, t->n_children - 1,
                cqc_expect
                (cpeg_term *tc = cpeg_term_copy(t);
                 cpeg_term *p = cpeg_term_prune(t, pos);
                 cqc_assert_eq(cpeg_term_ptr, p, tc->children[pos]);
                 cqc_assert_eq(unsigned, t->n_children, tc->n_children - 1);
                 cqc_assert_eqn(cpeg_term_ptr, pos, tc->children,
                                pos, t->children);
                 cqc_assert_eqn(cpeg_term_ptr,
                                t->n_children - pos, &tc->children[pos + 1],
                                t->n_children - pos, &t->children[pos]);
                 cqc_assert_eq(unsigned, p->refcnt, 2))))));
#endif

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

#ifdef LIBCPEG_TESTING

static int
counting_fn(__attribute__ ((unused))
            const cpeg_term *term, void *data)
{
    (*(unsigned *)data)++;
    return 0;
}

CQC_TESTCASE(preorder_postorder_comm,
             "If a traversing function only depends on the term value and/or "
             "identity, not its structure, preorder and postorder yield "
             "the same result",
             CQC_NO_CLASSES,
             cqc_forall
             (cpeg_term_ptr, t,
              cqc_expect
              (unsigned count1 = 0;
               unsigned count2 = 0;
               cqc_assert_eq(int, cpeg_term_traverse_preorder(counting_fn, t,
                                                              &count1), 0);
               cqc_assert_eq(int, cpeg_term_traverse_postorder(counting_fn, t,
                                                               &count2), 0);
               cqc_assert_eq(unsigned, count1, count2))));

static int
keep_value(const cpeg_term *term, void *data)
{
    *(void **)data = term->value;
    return 0;
}

CQC_TESTCASE(postorder_executes_last,
             "Postorder function is executed after the subtree traversal",
             CQC_NO_CLASSES,
             cqc_forall
             (cpeg_term_ptr, t,
              cqc_expect
              (void *v = NULL;
               cqc_assert_eq(int, cpeg_term_traverse_postorder(keep_value, t,
                                                               &v), 0);
               cqc_assert_eq(cqc_opaque, v, t->value))));

CQC_TESTCASE(preorder_executes_last,
             "Preorder function is executed before the subtree traversal",
             CQC_NO_CLASSES,
             cqc_forall
             (cpeg_term_ptr, t,
              cqc_expect
              (void *v = NULL;
               cqc_assert_eq(int, cpeg_term_traverse_preorder(keep_value, t,
                                                              &v), 0);
               cqc_assert_eq(cqc_opaque, v, cpeg_term_rightmost(t)->value))));

static int
check_and_store_value(const cpeg_term *term, void *data)
{
    if (((void **)data)[0] == term->value)
        return 1;

    ((void **)data)[1] = term->value;
    return 0;
}


CQC_TESTCASE(traverse_nonzero_preorder,
             "Non-zero return from the function stops preorder traversal",
             CQC_NO_CLASSES,
             cqc_forall
             (cpeg_term_ptr, t,
              cqc_forall
              (cpeg_term_ptr, t1,
               cqc_condition_neq
               (cqc_opaque, t1->value, t->value,
                cqc_expect
                (void *v[2];
                 v[0] = t1->value;
                 v[1] = NULL;
                 cpeg_term_graft(t1, 0, t);
                 cqc_assert_eq(int,
                               cpeg_term_traverse_preorder
                               (check_and_store_value, t1, v), 1);
                 cqc_assert_eq(cqc_opaque, v[1], NULL);
                 v[0] = t->value;
                 cqc_assert_eq(int,
                               cpeg_term_traverse_preorder
                               (check_and_store_value, t1, v), 1);
                 cqc_assert_eq(cqc_opaque, v[1], t1->value))))));

CQC_TESTCASE(traverse_nonzero_postorder,
             "Non-zero return from the function stops postorder traversal",
             CQC_NO_CLASSES,
             cqc_forall
             (cpeg_term_ptr, t,
              cqc_forall
              (cpeg_term_ptr, t1,
               cqc_condition_neq
               (cqc_opaque, t1->value, t->value,
                cqc_expect
                (void *v[2];
                 v[0] = t1->value;
                 v[1] = NULL;
                 cpeg_term_graft(t1, UINT_MAX, t);
                 cqc_assert_eq(int,
                               cpeg_term_traverse_postorder
                               (check_and_store_value, t1, v), 1);
                 cqc_assert_eq(cqc_opaque, v[1], t->value);
                 v[0] = t->value;
                 cqc_assert_eq(int,
                               cpeg_term_traverse_postorder
                               (check_and_store_value, t1, v), 1);
                 cqc_condition_neq
                 (unsigned, t->n_children, 0,
                  cqc_assert_eq(cqc_opaque,
                                v[1],
                                t->children[t->n_children - 1]->value)))))));

#endif

bool
cpeg_term_isomorphic(const cpeg_term *term1, const cpeg_term *term2)
{
    unsigned i;

    if (term1 == NULL)
        return term2 == NULL;
    if (term2 == NULL)
        return false;

    if (term1->n_children != term2->n_children)
        return false;

    for (i = 0; i < term1->n_children; i++)
    {
        if (!cpeg_term_isomorphic(term1->children[i], term2->children[i]))
            return false;
    }
    return true;
}

int
cpeg_term_zip(cpeg_term_zip_fn fn,
              const cpeg_term *term1, const cpeg_term *term2,
              void *data)
{
    int rc = fn(term1, term2, data);
    unsigned i;

    if (rc != 0)
        return rc;
    assert(term1->n_children == term2->n_children);
    for (i = 0; i < term1->n_children; i++)
    {
        rc = cpeg_term_zip(fn, term1->children[i],
                           term2->children[i], data);
        if (rc != 0)
            return rc;
    }
    return 0;
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
