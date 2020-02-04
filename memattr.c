#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/queue.h>
#include "libcpeg_memattr.h"
#include "libcpeg_terms.h"
#ifdef LIBCPEG_TESTING
#include "cqc.h"
#endif

#ifdef LIBCPEG_TESTING

typedef cpeg_term *cpeg_term_ptr;

static unsigned test_mem_attr_count;

static void *test_mem_attr_init(void *v)
{
    test_mem_attr_count++;
    return v;
}

static void test_mem_attr_destroy(__attribute__((unused)) void *v)
{
    assert(test_mem_attr_count > 0);
    test_mem_attr_count--;
}

static const cpeg_term_type test_mem_attr_type = {
    .id = "memattr",
    .init = test_mem_attr_init,
    .destroy = test_mem_attr_destroy
};

#define cqc_generate_cpeg_term_ptr(_var, _scale)                        \
    (*(_var) = cpeg_term_newl(&test_mem_attr_type,                      \
                              (void *)(uintptr_t)random(),              \
                              NULL))

#define cqc_release_cpeg_term_ptr(_var) cpeg_term_free(*(_var))

#define cqc_typefmt_cpeg_term_ptr "%p"
#define cqc_typeargs_cpeg_term_ptr(_t) (_t)

#define cqc_equal_cpeg_term_ptr(_v1, _v2) ((_v1) == (_v2))

#endif


void *
cpeg_mem_alloc(size_t size)
{
    void *obj = malloc(size);
    assert(obj != NULL);
    return obj;
}

struct cpeg_term;

#define ATTR_HASH_TABLE_SIZE 65537

typedef struct mem_attr_value {
    const void *attr;
    cpeg_term *term;
    SLIST_ENTRY(mem_attr_value) entry;
} mem_attr_value;

typedef struct mem_attr_cell {
    const void *addr;
    SLIST_HEAD(, mem_attr_value) values;
    LIST_ENTRY(mem_attr_cell) entry;
} mem_attr_cell;

static LIST_HEAD(, mem_attr_cell) addr_hash_table[ATTR_HASH_TABLE_SIZE];

static unsigned
mem_addr_hash(const void *addr)
{
    uintptr_t val = (uintptr_t)addr;

    return (val / sizeof(double)) % ATTR_HASH_TABLE_SIZE;
}

static LIST_HEAD(, mem_attr_cell) cell_free_list =
    LIST_HEAD_INITIALIZER(cell_free_list);
static SLIST_HEAD(, mem_attr_value) attr_val_free_list =
    SLIST_HEAD_INITIALIZER(attr_val_free_list);

static mem_attr_value *
mem_attr_value_alloc(const void *attr)
{
    mem_attr_value *v = NULL;

    if (SLIST_EMPTY(&attr_val_free_list))
        v = malloc(sizeof(*v));
    else
    {
        v = SLIST_FIRST(&attr_val_free_list);
        SLIST_REMOVE_HEAD(&attr_val_free_list, entry);
    }
    assert(v != NULL);

    v->attr = attr;
    v->term = NULL;
    return v;
}

static mem_attr_cell *
mem_attr_find_cell(const void *addr, bool add)
{
    unsigned h = mem_addr_hash(addr);
    mem_attr_cell *cell;

    LIST_FOREACH(cell, &addr_hash_table[h], entry)
    {
        if (cell->addr == addr)
            return cell;
    }
    if (!add)
        return NULL;

    if (LIST_EMPTY(&cell_free_list))
        cell = malloc(sizeof(*cell));
    else
    {
        cell = LIST_FIRST(&cell_free_list);
        LIST_REMOVE(cell, entry);
    }
    assert(cell != NULL);
    cell->addr = addr;
    SLIST_INIT(&cell->values);
    LIST_INSERT_HEAD(&addr_hash_table[h], cell, entry);

    return cell;
}

static mem_attr_value *
mem_attr_access(const void *addr, const void *attr, bool add)
{
    mem_attr_cell *cell = mem_attr_find_cell(addr, add);
    mem_attr_value *v;

    /* this may only happen if `add` is false */
    if (cell == NULL)
        return NULL;

    SLIST_FOREACH(v, &cell->values, entry)
    {
        if (v->attr == attr)
            return v;
    }
    if (add)
    {
        v = mem_attr_value_alloc(attr);
        SLIST_INSERT_HEAD(&cell->values, v, entry);
    }
    return v;
}

cpeg_term *
cpeg_mem_attr_get(const void *addr, const void *attr)
{
    mem_attr_value *v = mem_attr_access(addr, attr, false);

    return v == NULL ? NULL : v->term;
}

void
cpeg_mem_attr_set(const void *addr, const void *attr, cpeg_term *val)
{
    mem_attr_value *v = mem_attr_access(addr, attr, true);

    assert(v != NULL);
    cpeg_term_free(v->term);
    v->term = val;
}

#ifdef LIBCPEG_TESTING
CQC_TESTCASE(test_set_get,
             "If an attribute is set, it can be retrieved",
             CQC_NO_CLASSES,
             cqc_forall
             (uintptr_t, addr,
              cqc_forall
              (uintptr_t, attr,
               cqc_forall
               (cpeg_term_ptr, t,
                cqc_expect
                (cpeg_term_ptr t0;
                 cpeg_mem_attr_set((const void *)addr, (const void *)attr, t);
                 t0 = cpeg_mem_attr_get((const void *)addr,
                                        (const void *)attr);
                 cqc_assert_neq(cpeg_term_ptr, t0, NULL);
                 cqc_assert_eq(cpeg_term_ptr, t, t0);
                 cqc_assert_eq(unsigned, t->refcnt, 1))))));

CQC_TESTCASE(test_set_release,
             "If an attribute is replaced, its previous value is released",
             CQC_NO_CLASSES,
             cqc_forall
             (uintptr_t, addr,
              cqc_forall
              (uintptr_t, attr,
               cqc_forall
               (cpeg_term_ptr, t,
                cqc_forall
                (cpeg_term_ptr, t1,
                 cqc_expect
                 (unsigned saved_cnt = test_mem_attr_count;
                  cpeg_mem_attr_set((const void *)addr, (const void *)attr, t);
                  cpeg_mem_attr_set((const void *)addr, (const void *)attr, t1);
                  cqc_assert_eq(unsigned, test_mem_attr_count, saved_cnt - 1)))))));

CQC_TESTCASE(test_set_get_unaligned,
             "Attributes for unaligned addresses work just the same "
             "as for aligned ones",
             CQC_CLASS_LIST("even", "odd"),
             cqc_forall
             (uintptr_t, addr,
              cqc_forall
              (uintptr_t, attr,
               cqc_forall
               (cpeg_term_ptr, t,
                cqc_forall
                (cpeg_term_ptr, t1,
                 cqc_condition_neq
                 (cpeg_term_ptr, t, t1,
                  cqc_classify
                  (addr % 2,
                   cqc_expect
                   (cpeg_mem_attr_set((const void *)addr,
                                      (const void *)attr, t);
                    cpeg_mem_attr_set((const void *)(addr + 1),
                                      (const void *)attr, t1);
                    cqc_assert_eq(cpeg_term_ptr,
                                  cpeg_mem_attr_get((const void *)addr,
                                                    (const void *)attr),
                                  t);
                    cqc_assert_eq(cpeg_term_ptr,
                                  cpeg_mem_attr_get((const void *)(addr + 1),
                                                    (const void *)attr),
                                  t1)))))))));

#endif

static void
mem_release_attrs(mem_attr_cell *cell)
{
    while (!SLIST_EMPTY(&cell->values))
    {
        mem_attr_value *v = SLIST_FIRST(&cell->values);
        cpeg_term_free(v->term);
        SLIST_REMOVE_HEAD(&cell->values, entry);
        SLIST_INSERT_HEAD(&attr_val_free_list, v, entry);
    }
}

void
cpeg_mem_release_attrs(const void *addr)
{
    mem_attr_cell *cell = mem_attr_find_cell(addr, false);

    if (cell != NULL)
        mem_release_attrs(cell);
}

#ifdef LIBCPEG_TESTING
CQC_TESTCASE(test_release_attr,
             "No attributes are available after being released",
             CQC_NO_CLASSES,
             cqc_forall
             (uintptr_t, addr,
              cqc_forall
              (uintptr_t, attr1,
               cqc_forall
               (uintptr_t, attr2,
                cqc_forall
                (cpeg_term_ptr, t,
                 cqc_forall
                 (cpeg_term_ptr, t1,
                  cqc_expect
                  (unsigned attr_cnt = test_mem_attr_count;
                   cpeg_mem_attr_set((const void *)addr,
                                     (const void *)attr1, t);
                   cpeg_mem_attr_set((const void *)addr,
                                     (const void *)attr2, t1);
                   cpeg_mem_release_attrs((const void *)addr);
                   cqc_assert_eq(cpeg_term_ptr,
                                 cpeg_mem_attr_get((const void *)addr,
                                                   (const void *)attr1),
                                 NULL);
                   cqc_assert_eq(cpeg_term_ptr,
                                 cpeg_mem_attr_get((const void *)addr,
                                                   (const void *)attr2),
                                 NULL);
                   cqc_assert_eq(unsigned, test_mem_attr_count, attr_cnt - 2))))))));

#endif

void
cpeg_mem_free(void *addr)
{
    mem_attr_cell *cell = mem_attr_find_cell(addr, false);

    if (cell != NULL)
    {
        mem_release_attrs(cell);
        LIST_REMOVE(cell, entry);
    }
    free(addr);
}

void *
cpeg_mem_realloc(void *oldaddr, size_t newsize)
{
    void *newaddr = realloc(oldaddr, newsize);

    assert(newaddr != NULL);
    if (oldaddr != newaddr && oldaddr != NULL)
    {
        mem_attr_cell *cell = mem_attr_find_cell(oldaddr, false);

        if (cell != NULL)
        {
            unsigned newh = mem_addr_hash(newaddr);

            LIST_REMOVE(cell, entry);
            LIST_INSERT_HEAD(&addr_hash_table[newh], cell, entry);
        }
    }

    return newaddr;
}

#if 0
#ifdef LIBCPEG_TESTING
CQC_TESTCASE(realloc_keeps_attrs,
             "All attributes are accessible after realloc",
             cqc_forall
             (uintptr_t, attr1,
              cqc_forall
              (uintptr_t, attr2,
               cqc_forall
               (uint16_t, initial_size,
                cqc_forall
                (uint16_t, next_size,
                 cqc_expect
                 (void *ptr = cpeg_mem_alloc((unsigned)initial_size + 1);
                  cpeg_term *v1;
                  cpeg_term *v2;
                  cpeg_
#endif
#endif

#ifdef LIBCPEG_TESTING

CQC_TESTCASE_SINGLE(smoke_test,
                    "Set and free a lot of attributes",
                    cqc_forall
                    (uintptr_t, attr,
                     cqc_forall
                     (uint16_t, chunk_size,
                      cqc_forall
                      (uint8_t, qsize,
                       cqc_expect
                       (struct { void *addr; cpeg_term *value; }    \
                        vector[(unsigned)qsize + 1];
                        unsigned i;

                        for (i = 0; i < (unsigned)qsize + 1; i++)
                        {
                            vector[i].addr = cpeg_mem_alloc((unsigned)chunk_size + 1);
                            cqc_generate_cpeg_term_ptr(&vector[i].value, cqc_scale);
                            cpeg_mem_attr_set(vector[i].addr, (const void *)attr,
                                              vector[i].value);
                        }
                        for (i = 0; i < cqc_scale * 10000; i++)
                        {
                            unsigned j;
                            cpeg_mem_free(vector[0].addr);
                            for (j = 0; j < qsize; j++)
                            {
                                vector[j] = vector[j + 1];
                                cqc_assert_eq(cpeg_term_ptr,
                                              cpeg_mem_attr_get(vector[j].addr,
                                                                (const void *)attr),
                                              vector[j].value);
                            }
                            vector[qsize].addr = cpeg_mem_alloc((unsigned)chunk_size + 1);
                            cqc_generate_cpeg_term_ptr(&vector[qsize].value,
                                                       cqc_scale);
                            cpeg_mem_attr_set(vector[qsize].addr, (const void *)attr,
                                              vector[qsize].value);
                        })))));
#endif
