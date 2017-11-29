/*
 * Copyright (C) 2013-2017 Argonne National Laboratory, Department of Energy,
 *                    UChicago Argonne, LLC and The HDF Group.
 * All rights reserved.
 *
 * The full copyright notice, including terms governing use, modification,
 * and redistribution, is contained in the COPYING file that can be
 * found at the root of the source code distribution tree.
 */

/* Implementation derived from:
 * https://github.com/freebsd/freebsd/blob/master/sys/sys/buf_ring.h
 *
 * -
 * Copyright (c) 2007-2009 Kip Macy <kmacy@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef MERCURY_ATOMIC_QUEUE_H
#define MERCURY_ATOMIC_QUEUE_H

#include "mercury_atomic.h"
#include "mercury_thread.h"

/*************************************/
/* Public Type and Struct Definition */
/*************************************/

#define HG_UTIL_CACHE_ALIGNMENT 64
struct hg_atomic_queue {
    hg_atomic_int32_t prod_head;
    hg_atomic_int32_t prod_tail;
    unsigned int      prod_size;
    unsigned int      prod_mask;
    hg_util_uint64_t  drops;
    hg_atomic_int32_t cons_head __attribute__((aligned(HG_UTIL_CACHE_ALIGNMENT)));
    hg_atomic_int32_t cons_tail;
    unsigned int      cons_size;
    unsigned int      cons_mask;
    hg_atomic_int64_t *ring[1] __attribute__((aligned(HG_UTIL_CACHE_ALIGNMENT)));
};

/*****************/
/* Public Macros */
/*****************/

#define HG_ATOMIC_QUEUE_ELT_SIZE sizeof(hg_atomic_int64_t)

#ifndef cpu_spinwait
#if defined(__x86_64__) || defined(__amd64__)
#define cpu_spinwait() asm volatile("pause\n": : :"memory");
#else
#define cpu_spinwait();
#endif
#endif

/*********************/
/* Public Prototypes */
/*********************/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Allocate a new queue that can hold \count elements.
 *
 * \param count [IN]                maximum number of elements
 *
 * \return pointer to allocated queue or NULL on failure
 */
HG_UTIL_EXPORT struct hg_atomic_queue *
hg_atomic_queue_alloc(unsigned int count);

/**
 * Free an existing queue.
 *
 * \param hg_atomic_queue [IN]      pointer to queue
 */
HG_UTIL_EXPORT void
hg_atomic_queue_free(struct hg_atomic_queue *hg_atomic_queue);

/**
 * Push an entry to the queue.
 *
 * \param hg_atomic_queue [IN/OUT]  pointer to queue
 * \param entry [IN]                pointer to object
 *
 * \return Non-negative on success or negative on failure
 */
static HG_UTIL_INLINE int
hg_atomic_queue_push(struct hg_atomic_queue *hg_atomic_queue, void *entry);

/**
 * Pop an entry from the queue (multi-consumer).
 *
 * \param hg_atomic_queue [IN/OUT]  pointer to queue
 *
 * \return Pointer to popped object or NULL if queue is empty
 */
static HG_UTIL_INLINE void *
hg_atomic_queue_pop_mc(struct hg_atomic_queue *hg_atomic_queue);

/**
 * Pop an entry from the queue (single consumer).
 *
 * \param hg_atomic_queue [IN/OUT]  pointer to queue
 *
 * \return Pointer to popped object or NULL if queue is empty
 */
static HG_UTIL_INLINE void *
hg_atomic_queue_pop_sc(struct hg_atomic_queue *hg_atomic_queue);

/**
 * Determine whether queue is empty.
 *
 * \param hg_atomic_queue [IN/OUT]  pointer to queue
 *
 * \return HG_UTIL_TRUE if empty, HG_UTIL_FALSE if not
 */
static HG_UTIL_INLINE hg_util_bool_t
hg_atomic_queue_is_empty(struct hg_atomic_queue *hg_atomic_queue);

/**
 * Determine number of entries in a queue.
 *
 * \param hg_atomic_queue [IN/OUT]  pointer to queue
 *
 * \return Number of entries queued or 0 if none
 */
static HG_UTIL_INLINE unsigned int
hg_atomic_queue_count(struct hg_atomic_queue *hg_atomic_queue);

/*---------------------------------------------------------------------------*/
static HG_UTIL_INLINE int
hg_atomic_queue_push(struct hg_atomic_queue *hg_atomic_queue, void *entry)
{
    hg_util_int32_t prod_head, prod_next, cons_tail;
    int ret = HG_UTIL_SUCCESS;

    do {
        prod_head = hg_atomic_get32(&hg_atomic_queue->prod_head);
        prod_next = (prod_head + 1) & (int) hg_atomic_queue->prod_mask;
        cons_tail = hg_atomic_get32(&hg_atomic_queue->cons_tail);

        if (prod_next == cons_tail) {
            hg_atomic_fence();
            if (prod_head == hg_atomic_get32(&hg_atomic_queue->prod_head) &&
                cons_tail == hg_atomic_get32(&hg_atomic_queue->cons_tail)) {
                hg_atomic_queue->drops++;
                /* Full */
                ret = HG_UTIL_FAIL;
                goto done;
            }
            continue;
        }
    } while (!hg_atomic_cas32(&hg_atomic_queue->prod_head, prod_head,
        prod_next));

    hg_atomic_set64((hg_atomic_int64_t *) &hg_atomic_queue->ring[prod_head],
        (hg_util_int64_t) entry);

    /*
     * If there are other enqueues in progress
     * that preceded us, we need to wait for them
     * to complete
     */
    while (hg_atomic_get32(&hg_atomic_queue->prod_tail) != prod_head)
        cpu_spinwait();

    hg_atomic_set32(&hg_atomic_queue->prod_tail, prod_next);

done:
    return ret;
}

/*---------------------------------------------------------------------------*/
static HG_UTIL_INLINE void *
hg_atomic_queue_pop_mc(struct hg_atomic_queue *hg_atomic_queue)
{
    hg_util_int32_t cons_head, cons_next;
    void *entry = NULL;

    do {
        cons_head = hg_atomic_get32(&hg_atomic_queue->cons_head);
        cons_next = (cons_head + 1) & (int) hg_atomic_queue->cons_mask;

        if (cons_head == hg_atomic_get32(&hg_atomic_queue->prod_tail))
            goto done;
    } while (!hg_atomic_cas32(&hg_atomic_queue->cons_head, cons_head,
        cons_next));

    entry = (void *) hg_atomic_get64(
        (hg_atomic_int64_t *) &hg_atomic_queue->ring[cons_head]);

    /*
     * If there are other dequeues in progress
     * that preceded us, we need to wait for them
     * to complete
     */
    while (hg_atomic_get32(&hg_atomic_queue->cons_tail) != cons_head)
        cpu_spinwait();

    hg_atomic_set32(&hg_atomic_queue->cons_tail, cons_next);

done:
    return entry;
}

/*---------------------------------------------------------------------------*/
static HG_UTIL_INLINE void *
hg_atomic_queue_pop_sc(struct hg_atomic_queue *hg_atomic_queue)
{
    hg_util_int32_t cons_head, cons_next;
    hg_util_int32_t prod_tail;
    void *entry = NULL;

    cons_head = hg_atomic_get32(&hg_atomic_queue->cons_head);
    prod_tail = hg_atomic_get32(&hg_atomic_queue->prod_tail);
    cons_next = (cons_head + 1) & (int) hg_atomic_queue->cons_mask;

    if (cons_head == prod_tail)
        /* Empty */
        goto done;

    hg_atomic_set32(&hg_atomic_queue->cons_head, cons_next);

    entry = (void *) hg_atomic_get64(
        (hg_atomic_int64_t *) &hg_atomic_queue->ring[cons_head]);

    hg_atomic_set32(&hg_atomic_queue->cons_tail, cons_next);

done:
    return entry;
}

/*---------------------------------------------------------------------------*/
static HG_UTIL_INLINE hg_util_bool_t
hg_atomic_queue_is_empty(struct hg_atomic_queue *hg_atomic_queue)
{
    return (hg_atomic_get32(&hg_atomic_queue->cons_head) ==
        hg_atomic_get32(&hg_atomic_queue->prod_tail));
}

/*---------------------------------------------------------------------------*/
static HG_UTIL_INLINE unsigned int
hg_atomic_queue_count(struct hg_atomic_queue *hg_atomic_queue)
{
    return ((hg_atomic_queue->prod_size
        + (unsigned int) hg_atomic_get32(&hg_atomic_queue->prod_tail)
        - (unsigned int) hg_atomic_get32(&hg_atomic_queue->cons_tail))
        & hg_atomic_queue->prod_mask);
}

#ifdef __cplusplus
}
#endif

#endif /* MERCURY_ATOMIC_QUEUE_H */
