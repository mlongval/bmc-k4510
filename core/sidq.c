/* See sidq.h.  Single producer (the CPU's core), single consumer (whichever
 * core is rendering), no lock -- the ring's two indices are the only shared
 * mutable state and each is written by one side only. */
#include "sidq.h"
#include <stdatomic.h>

#define QN 4096                                  /* ~85 ms of the busiest tune at a write a scanline */
typedef struct { uint32_t us; uint8_t chip, reg, val, pad; } ev_t;
static ev_t q[QN];
static _Atomic unsigned q_w, q_r;

static _Atomic uint32_t now_us;
static _Atomic int owner = SIDQ_OWNER_CPU;
static _Atomic int want  = SIDQ_OWNER_CPU;

void     sidq_tick(uint32_t us) { atomic_fetch_add_explicit(&now_us, us, memory_order_release); }
uint32_t sidq_now(void)         { return atomic_load_explicit(&now_us, memory_order_acquire); }
int      sidq_owner(void)       { return atomic_load_explicit(&owner, memory_order_acquire); }
int      sidq_pending(void)     { return atomic_load_explicit(&want, memory_order_acquire) != atomic_load_explicit(&owner, memory_order_acquire); }
void     sidq_accept(void)      { atomic_store_explicit(&owner, atomic_load_explicit(&want, memory_order_acquire), memory_order_release); }

void sidq_reset(void)
{
    atomic_store(&q_w, 0); atomic_store(&q_r, 0);
    atomic_store(&now_us, 0);
}

/* The rendezvous.  The asking side spins -- this is called at a point where
 * the machine is already stopped (a menu close, or the ROM starting the Tube),
 * so a spin of a few milliseconds costs nothing and a lock would cost more.
 * The bound matters: if the other core has died or was never started, the
 * machine must not hang, it must keep the sound itself. */
int sidq_request(int o)
{
    atomic_store_explicit(&want, o, memory_order_release);
    if (atomic_load_explicit(&owner, memory_order_acquire) == o) return 1;
    for (long i = 0; i < 20000000L; i++) {                    /* tens of milliseconds: a pump loop is far shorter */
        if (atomic_load_explicit(&owner, memory_order_acquire) == o) return 1;
#if defined(__aarch64__)
        __asm__ volatile("yield" ::: "memory");
#endif
    }
    atomic_store_explicit(&want, atomic_load_explicit(&owner, memory_order_acquire), memory_order_release);
    return 0;                                                  /* nobody answered: nothing moved */
}

int sidq_push(uint8_t chip, uint8_t reg, uint8_t val)
{
    unsigned w = atomic_load_explicit(&q_w, memory_order_relaxed);
    unsigned n = (w + 1) & (QN - 1);
    if (n == atomic_load_explicit(&q_r, memory_order_acquire)) return 0;      /* full */
    q[w].us = sidq_now(); q[w].chip = chip; q[w].reg = reg; q[w].val = val;
    atomic_store_explicit(&q_w, n, memory_order_release);
    return 1;
}

void sidq_drain(uint32_t us, void (*apply)(int chip, uint8_t reg, uint8_t val))
{
    unsigned r = atomic_load_explicit(&q_r, memory_order_relaxed);
    for (;;) {
        unsigned w = atomic_load_explicit(&q_w, memory_order_acquire);
        if (r == w) break;
        /* Unsigned wrap-safe "is it due yet": the stamp is 32 bits of
         * microseconds and rolls over every 71 minutes. */
        if ((uint32_t)(us - q[r].us) >= 0x80000000u) break;                   /* still in the future */
        apply(q[r].chip, q[r].reg, q[r].val);
        r = (r + 1) & (QN - 1);
        atomic_store_explicit(&q_r, r, memory_order_release);
    }
}
