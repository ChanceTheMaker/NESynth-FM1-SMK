/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>

/* Single-core ISR queue: one scanner producer, one audio consumer. Volatile
 * publication is ordered by the compiler barrier; this is not a multicore
 * queue. Discard requires the caller to mask the producer interrupt. */
#define SMK37_EVENTS 128u
typedef struct { uint32_t key, duration; } smk37_event_t;
typedef struct {
    volatile smk37_event_t items[SMK37_EVENTS];
    volatile uint32_t write, read, overflow;
} smk37_events_t;
#define SMK37_EVENT_BARRIER() __asm__ volatile ("" ::: "memory")

static inline int smk37_events_push(smk37_events_t *q, unsigned key, uint32_t duration)
{
    uint32_t w = q->write;
    if (key >= 37u) return 0;
    /* Latch loss: no later events enter until the consumer releases its notes
     * and discards the incomplete history. */
    if (q->overflow) return 0;
    if (w - q->read >= SMK37_EVENTS) { q->overflow = 1; return 0; }
    q->items[w % SMK37_EVENTS].key = key;
    q->items[w % SMK37_EVENTS].duration = duration;
    SMK37_EVENT_BARRIER();
    q->write = w + 1u;
    return 1;
}
static inline int smk37_events_pop(smk37_events_t *q, smk37_event_t *event)
{
    uint32_t r = q->read;
    if (q->overflow || r == q->write) return 0;
    SMK37_EVENT_BARRIER();
    event->key = q->items[r % SMK37_EVENTS].key;
    event->duration = q->items[r % SMK37_EVENTS].duration;
    SMK37_EVENT_BARRIER();
    q->read = r + 1u;
    return 1;
}
static inline void smk37_events_discard(smk37_events_t *q)
{
    q->read = q->write;
    SMK37_EVENT_BARRIER();
    q->overflow = 0;
}
