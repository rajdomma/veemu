#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../include/peripheral.h"
#include "../include/veemu.h"

#define GPIO_MODER   0x00
#define GPIO_OTYPER  0x04
#define GPIO_OSPEEDR 0x08
#define GPIO_PUPDR   0x0C
#define GPIO_IDR     0x10
#define GPIO_ODR     0x14
#define GPIO_BSRR    0x18
#define GPIO_LCKR    0x1C
#define GPIO_AFRL    0x20
#define GPIO_AFRH    0x24

typedef struct {
    uint32_t moder,otyper,ospeedr,pupdr,idr,odr,lckr,afrl,afrh;
    uint32_t moder_reset,ospeedr_reset,pupdr_reset;
    veemu_gpio_cb_t cb; void *cb_ctx;
    char port_name[8]; uint8_t port_idx;
} gpio_state_t;

static void notify(gpio_state_t *s, uint32_t old, uint32_t nw) {
    if(!s->cb) return;
    uint32_t ch=old^nw;
    for(int i=0;i<16;i++)
        if(ch&(1u<<i)) s->cb(s->port_name,(uint8_t)i,(nw>>i)&1,s->cb_ctx);
}

static uint32_t gpio_read(peripheral_t *p, uint32_t o) {
    gpio_state_t *s=p->state;
    switch(o){
    case GPIO_MODER:   return s->moder;
    case GPIO_OTYPER:  return s->otyper;
    case GPIO_OSPEEDR: return s->ospeedr;
    case GPIO_PUPDR:   return s->pupdr;
    case GPIO_IDR:     return s->idr;
    case GPIO_ODR:     return s->odr;
    case GPIO_LCKR:    return s->lckr;
    case GPIO_AFRL:    return s->afrl;
    case GPIO_AFRH:    return s->afrh;
    default: return 0;
    }
}

static void gpio_write(peripheral_t *p, uint32_t o, uint32_t v) {
    gpio_state_t *s=p->state; uint32_t old=s->odr;
    switch(o){
    case GPIO_MODER:   s->moder=v;   break;
    case GPIO_OTYPER:  s->otyper=v;  break;
    case GPIO_OSPEEDR: s->ospeedr=v; break;
    case GPIO_PUPDR:   s->pupdr=v;   break;
    case GPIO_ODR:     s->odr=v&0xFFFF; notify(s,old,s->odr); break;
    case GPIO_BSRR: {
        uint32_t set=v&0xFFFF, rst=(v>>16)&0xFFFF;
        s->odr=(s->odr|set)&~rst; notify(s,old,s->odr); break;
    }
    case GPIO_LCKR:    s->lckr=v;  break;
    case GPIO_AFRL:    s->afrl=v;  break;
    case GPIO_AFRH:    s->afrh=v;  break;
    default: break;
    }
}

static void gpio_reset(peripheral_t *p) {
    gpio_state_t *s=p->state;
    s->moder=s->moder_reset; s->ospeedr=s->ospeedr_reset;
    s->pupdr=s->pupdr_reset; s->otyper=s->odr=s->lckr=s->afrl=s->afrh=0;
    s->idr=0xFFFF;
}
static void gpio_destroy(peripheral_t *p){free(p->state);p->state=NULL;}

peripheral_t *gpio_create(uint8_t idx) {
    peripheral_t *p=calloc(1,sizeof(peripheral_t));
    gpio_state_t *s=calloc(1,sizeof(gpio_state_t));
    if(!p||!s){free(p);free(s);return NULL;}
    s->port_idx=idx;
    snprintf(s->port_name,sizeof(s->port_name),"gpio%c",'a'+idx);
    switch(idx){
    case 0: s->moder_reset=0xA8000000;s->ospeedr_reset=0x0C000000;s->pupdr_reset=0x64000000;break;
    case 1: s->moder_reset=0x00000280;s->ospeedr_reset=0x000000C0;s->pupdr_reset=0x00000100;break;
    default:s->moder_reset=0;s->ospeedr_reset=0;s->pupdr_reset=0;break;
    }
    snprintf(p->name,sizeof(p->name),"gpio%c",'a'+idx);
    p->type=PERIPH_TYPE_GPIO; p->instance=idx;
    p->state=s; p->read=gpio_read; p->write=gpio_write;
    p->reset=gpio_reset; p->destroy=gpio_destroy;
    gpio_reset(p); return p;
}
void gpio_set_input_pin(peripheral_t *p,uint8_t pin,bool state){
    gpio_state_t *s=p->state;
    if(state) s->idr|=(1u<<pin); else s->idr&=~(1u<<pin);
}
void gpio_set_cb(peripheral_t *p,veemu_gpio_cb_t cb,void *ctx){
    gpio_state_t *s=p->state; s->cb=cb; s->cb_ctx=ctx;
}
bool gpio_get_output_pin(peripheral_t *p,uint8_t pin){
    return (((gpio_state_t*)p->state)->odr>>pin)&1;
}
