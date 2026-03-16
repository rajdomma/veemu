#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "../include/peripheral.h"

#define SYST_CSR    0x00
#define SYST_RVR    0x04
#define SYST_CVR    0x08
#define SYST_CALIB  0x0C

#define CSR_ENABLE    (1u<<0)
#define CSR_TICKINT   (1u<<1)
#define CSR_CLKSOURCE (1u<<2)
#define CSR_COUNTFLAG (1u<<16)

typedef struct {
    uint32_t csr, rvr, cvr, calib;
    bool     pending;
} systick_state_t;

static uint32_t systick_read(peripheral_t *p, uint32_t offset) {
    systick_state_t *s = p->state;
    switch (offset) {
    case SYST_CSR: { uint32_t v=s->csr; s->csr&=~CSR_COUNTFLAG; return v; }
    case SYST_RVR:   return s->rvr & 0x00FFFFFF;
    case SYST_CVR:   return s->cvr & 0x00FFFFFF;
    case SYST_CALIB: return s->calib;
    default:         return 0;
    }
}

static void systick_write(peripheral_t *p, uint32_t offset, uint32_t value) {
    systick_state_t *s = p->state;
    switch (offset) {
    case SYST_CSR:
        s->csr = (s->csr & CSR_COUNTFLAG) | (value & 0x7);
        fprintf(stderr,"[systick] CSR=0x%08X EN=%d TICKINT=%d\n",
                s->csr,(s->csr&CSR_ENABLE)?1:0,(s->csr&CSR_TICKINT)?1:0);
        break;
    case SYST_RVR:
        s->rvr = value & 0x00FFFFFF;
        fprintf(stderr,"[systick] LOAD=%u (%.3fms @16MHz)\n",
                s->rvr,(s->rvr+1)/16000.0);
        break;
    case SYST_CVR:
        s->cvr=0; s->csr&=~CSR_COUNTFLAG;
        break;
    case SYST_CALIB: break;
    default: break;
    }
}

static void systick_reset(peripheral_t *p) {
    systick_state_t *s=p->state;
    s->csr=s->rvr=s->cvr=s->calib=0; s->pending=false;
}
static void systick_destroy(peripheral_t *p){free(p->state);p->state=NULL;}

peripheral_t *systick_create(void) {
    peripheral_t    *p=calloc(1,sizeof(peripheral_t));
    systick_state_t *s=calloc(1,sizeof(systick_state_t));
    if(!p||!s){free(p);free(s);return NULL;}
    snprintf(p->name,sizeof(p->name),"systick");
    p->type=PERIPH_TYPE_SYSTICK; p->instance=0;
    p->state=s; p->read=systick_read; p->write=systick_write;
    p->reset=systick_reset; p->destroy=systick_destroy;
    systick_reset(p); return p;
}

bool systick_tick(peripheral_t *p, uint32_t insns_elapsed) {
    systick_state_t *s = p->state;
    if (!(s->csr & CSR_ENABLE) || s->rvr == 0) return false;

    /* First tick after enable — load CVR from RVR */
    if (s->cvr == 0) s->cvr = s->rvr + 1;

    if (insns_elapsed >= s->cvr) {
        /* Timer expired — reload and fire */
        uint32_t period    = s->rvr + 1;
        uint32_t remaining = insns_elapsed - s->cvr;
        s->cvr = period - (remaining % period);
        if (s->cvr == 0) s->cvr = period;   /* never leave CVR=0 after reload */
        s->csr |= CSR_COUNTFLAG;
        if (s->csr & CSR_TICKINT) { s->pending = true; return true; }
    } else {
        s->cvr -= insns_elapsed;
        if (s->cvr == 0) s->cvr = 1;        /* never leave CVR=0 mid-count */
    }
    return false;
}

bool systick_is_pending(peripheral_t *p){
    systick_state_t *s=p->state;
    if(s->pending){s->pending=false;return true;}
    return false;
}
