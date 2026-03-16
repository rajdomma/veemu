#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../include/peripheral.h"

#define RCC_CR         0x00
#define RCC_PLLCFGR    0x04
#define RCC_CFGR       0x08
#define RCC_CIR        0x0C
#define RCC_AHB1RSTR   0x10
#define RCC_AHB2RSTR   0x14
#define RCC_APB1RSTR   0x20
#define RCC_APB2RSTR   0x24
#define RCC_AHB1ENR    0x30
#define RCC_AHB2ENR    0x34
#define RCC_APB1ENR    0x40
#define RCC_APB2ENR    0x44
#define RCC_AHB1LPENR  0x50
#define RCC_AHB2LPENR  0x54
#define RCC_APB1LPENR  0x60
#define RCC_APB2LPENR  0x64
#define RCC_BDCR       0x70
#define RCC_CSR        0x74
#define RCC_SSCGR      0x80
#define RCC_PLLI2SCFGR 0x84
#define RCC_DCKCFGR    0x8C

typedef struct {
    uint32_t cr,pllcfgr,cfgr,cir;
    uint32_t ahb1rstr,ahb2rstr,apb1rstr,apb2rstr;
    uint32_t ahb1enr,ahb2enr,apb1enr,apb2enr;
    uint32_t ahb1lpenr,ahb2lpenr,apb1lpenr,apb2lpenr;
    uint32_t bdcr,csr,sscgr,plli2scfgr,dckcfgr;
} rcc_state_t;

static uint32_t rcc_read(peripheral_t *p, uint32_t o) {
    rcc_state_t *s=p->state;
    switch(o){
    case RCC_CR:         return s->cr;
    case RCC_PLLCFGR:    return s->pllcfgr;
    case RCC_CFGR:       return s->cfgr;
    case RCC_CIR:        return s->cir;
    case RCC_AHB1RSTR:   return s->ahb1rstr;
    case RCC_AHB2RSTR:   return s->ahb2rstr;
    case RCC_APB1RSTR:   return s->apb1rstr;
    case RCC_APB2RSTR:   return s->apb2rstr;
    case RCC_AHB1ENR:    return s->ahb1enr;
    case RCC_AHB2ENR:    return s->ahb2enr;
    case RCC_APB1ENR:    return s->apb1enr;
    case RCC_APB2ENR:    return s->apb2enr;
    case RCC_AHB1LPENR:  return s->ahb1lpenr;
    case RCC_AHB2LPENR:  return s->ahb2lpenr;
    case RCC_APB1LPENR:  return s->apb1lpenr;
    case RCC_APB2LPENR:  return s->apb2lpenr;
    case RCC_BDCR:       return s->bdcr;
    case RCC_CSR:        return s->csr;
    case RCC_SSCGR:      return s->sscgr;
    case RCC_PLLI2SCFGR: return s->plli2scfgr;
    case RCC_DCKCFGR:    return s->dckcfgr;
    default: return 0;
    }
}

static void rcc_write(peripheral_t *p, uint32_t o, uint32_t v) {
    fprintf(stderr,"[rcc_write] off=0x%02X val=0x%08X\n",o,v); fflush(stderr);
    rcc_state_t *s=p->state;
    switch(o){
    case RCC_CR:
        s->cr=v;
        if(v&(1u<<0))  s->cr|=(1u<<1);   /* HSIRDY */
        if(v&(1u<<16)) s->cr|=(1u<<17);  /* HSERDY */
        if(v&(1u<<24)) s->cr|=(1u<<25);  /* PLLRDY */
        break;
    case RCC_CFGR:
        s->cfgr=v;
        /* SWS mirrors SW so firmware clock-switch poll succeeds */
        s->cfgr=(s->cfgr&~(3u<<2))|((v&3u)<<2);
        break;
    case RCC_PLLCFGR:    s->pllcfgr=v;    break;
    case RCC_CIR:        s->cir=v;        break;
    case RCC_AHB1RSTR:   s->ahb1rstr=v;   break;
    case RCC_AHB2RSTR:   s->ahb2rstr=v;   break;
    case RCC_APB1RSTR:   s->apb1rstr=v;   break;
    case RCC_APB2RSTR:   s->apb2rstr=v;   break;
    case RCC_AHB1ENR:    s->ahb1enr=v; fprintf(stderr,"[rcc] AHB1ENR=0x%08X\n",v); fflush(stderr); break;
    case RCC_AHB2ENR:    s->ahb2enr=v;    break;
    case RCC_APB1ENR:    s->apb1enr=v; fprintf(stderr,"[rcc] APB1ENR=0x%08X\n",v); fflush(stderr); break;
    case RCC_APB2ENR:    s->apb2enr=v;    break;
    case RCC_AHB1LPENR:  s->ahb1lpenr=v;  break;
    case RCC_AHB2LPENR:  s->ahb2lpenr=v;  break;
    case RCC_APB1LPENR:  s->apb1lpenr=v;  break;
    case RCC_APB2LPENR:  s->apb2lpenr=v;  break;
    case RCC_BDCR:       s->bdcr=v;       break;
    case RCC_CSR:        s->csr=v;        break;
    case RCC_SSCGR:      s->sscgr=v;      break;
    case RCC_PLLI2SCFGR: s->plli2scfgr=v; break;
    case RCC_DCKCFGR:    s->dckcfgr=v;    break;
    default: break;
    }
}

static void rcc_reset(peripheral_t *p) {
    rcc_state_t *s=p->state;
    memset(s,0,sizeof(*s));
    s->cr=0x00000083; s->csr=0x0E000001;
}
static void rcc_destroy(peripheral_t *p){free(p->state);p->state=NULL;}

peripheral_t *rcc_create(void) {
    peripheral_t *p=calloc(1,sizeof(peripheral_t));
    rcc_state_t  *s=calloc(1,sizeof(rcc_state_t));
    if(!p||!s){free(p);free(s);return NULL;}
    snprintf(p->name,sizeof(p->name),"rcc");
    p->type=PERIPH_TYPE_RCC; p->instance=0;
    p->state=s; p->read=rcc_read; p->write=rcc_write;
    p->reset=rcc_reset; p->destroy=rcc_destroy;
    rcc_reset(p); return p;
}
