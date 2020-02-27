/*
 * startup.c
 *
 * This file is part of the Phos operating system for microcontrollers
 * Copyright (c) 2018 J. M. Spivey
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "hardware.h"

/* init -- main program, creates application processes */
void init(void);

#ifdef PHOS
/* phos_init -- hook to set up scheduler data structures */
void phos_init(void);

/* phos_start -- hook to start process execution */
void phos_start(void);

/* phos_interrupt -- general interrupt handler */
void phos_interrupt(int irq);
#endif

/* Addresses set by the linker */
extern unsigned __data_start[], __data_end[],
     __bss_start[], __bss_end[], __etext[], __stack[];

/* __reset -- the system starts here */
void __reset(void) {
     unsigned *p, *q;

     // Make sure all RAM banks are powered on.
     POWER_RAMON |= BIT(0) | BIT(1);

     // Activate the crystal clock
     CLOCK_XTALFREQ = CLOCK_XTALFREQ_16MHz;
     CLOCK_HFCLKSTARTED = 0;
     CLOCK_HFCLKSTART = 1;
     while (! CLOCK_HFCLKSTARTED) { }

     // Copy data segment and zero out bss.
     p = __data_start; q = __etext;
     while (p < __data_end) *p++ = *q++;
     p = __bss_start;
     while (p < __bss_end) *p++ = 0;

#ifdef PHOS
     phos_init();               // Initialise the scheduler.
     init();                    // Let the program initialise itself.
     phos_start();              // Start the scheduler -- never returns.
#else
     init();                    // Call the main program.
     while (1) pause();         // Halt if it returns.
#endif
}


/* NVIC SETUP FUNCTIONS */

/* On Cortex-M0, only the top two bits of each interrupt priority are
   implemented, but for portability priorities should be specified
   with integers in the range [0..255]. */

/* irq_priority -- set priority for an IRQ to a value [0..255] */
void irq_priority(int irq, unsigned prio) {
     if (irq < 0)
         SET_BYTE(SCB_SHPR[(irq+8) >> 2], irq & 0x3, prio);
     else
         SET_BYTE(NVIC_IPR[irq >> 2], irq & 0x3, prio);
}
     
/* enable_irq -- enable an interrupt request */
void enable_irq(int irq) {
     NVIC_ISER[0] = BIT(irq);
}

/* disable_irq -- disable and interrupt request */
void disable_irq(int irq) {
     NVIC_ICER[0] = BIT(irq);
}

/* clear_pending -- clear pending interrupt */
void clear_pending(int irq) {
     NVIC_ICPR[0] = BIT(irq);
}

/* reschedule -- request PendSV trap before next user instruction */
void reschedule(void) {
     SCB_ICSR = BIT(SCB_ICSR_PendSVSet);
}


/*  INTERRUPT VECTORS */

/* We use GCC features to define each handler name as an alias for the
   spin() function if it is not defined elsewhere, or for the general
   interrupt handler of Phos.  Applications can subsitute their own
   definitions for individual handler names like uart_handler(). */

/* spin -- show Seven Stars of Death */
void spin(void) {
     int n;

     intr_disable();

     GPIO_DIR = 0xfff0;
     while (1) {
          GPIO_OUT = 0x4000;
          for (n = 1000000; n > 0; n--) {
               nop(); nop(); nop();
          }
          GPIO_OUT = 0;
          for (n = 200000; n > 0; n--) {
               nop(); nop(); nop();
          }
     }          
}

#define stringify(x) #x
#define WEAK_ALIAS(x) __attribute((weak, alias(stringify(x))))

#ifdef PHOS

void default_handler(void) {
     int irq = GET_FIELD(SCB_ICSR, SCB_ICSR_VECTACTIVE);
     phos_interrupt(irq-16);
}

#else

#define default_handler spin

#endif

#define HANDLER(name) \
     void name(void) WEAK_ALIAS(default_handler)

HANDLER(nmi_handler);
HANDLER(hardfault_handler);
HANDLER(svc_handler);
HANDLER(pendsv_handler);
HANDLER(systick_handler);
HANDLER(uart_handler);
HANDLER(timer0_handler);
HANDLER(timer1_handler);
HANDLER(timer2_handler);
HANDLER(power_clock_handler);
HANDLER(radio_handler);
HANDLER(spi0_twi0_handler);
HANDLER(spi1_twi1_handler);
HANDLER(gpiote_handler);
HANDLER(adc_handler);
HANDLER(rtc0_handler);
HANDLER(temp_handler);
HANDLER(rng_handler);
HANDLER(ecb_handler);
HANDLER(ccm_aar_handler);
HANDLER(wdt_handler);
HANDLER(rtc1_handler);
HANDLER(qdec_handler);
HANDLER(lpcomp_handler);
HANDLER(swi0_handler);
HANDLER(swi1_handler);
HANDLER(swi2_handler);
HANDLER(swi3_handler);
HANDLER(swi4_handler);
HANDLER(swi5_handler);

// Entries filled with default_handler are unused by the hardware.  Getting
// such an interrupt would be a real surprise!

void *__vectors[] __attribute((section(".vectors"))) = {
    __stack,                    // -16
    __reset,
    nmi_handler,
    hardfault_handler,
    default_handler,           // -12
    default_handler,
    default_handler,
    default_handler,
    default_handler,           //  -8
    default_handler,
    default_handler,
    svc_handler,
    default_handler,           // -4
    default_handler,
    pendsv_handler,
    systick_handler,
    
    /* external interrupts */
    power_clock_handler,       //  0
    radio_handler,
    uart_handler,
    spi0_twi0_handler,
    spi1_twi1_handler,         //  4
    default_handler,
    gpiote_handler,
    adc_handler,
    timer0_handler,            //  8
    timer1_handler,
    timer2_handler,
    rtc0_handler,
    temp_handler,              // 12
    rng_handler,
    ecb_handler,
    ccm_aar_handler,
    wdt_handler,               // 16
    rtc1_handler,
    qdec_handler,
    lpcomp_handler,
    swi0_handler,              // 20
    swi1_handler,
    swi2_handler,
    swi3_handler,
    swi4_handler,              // 24
    swi5_handler,
    default_handler,
    default_handler,
    default_handler,           // 28
    default_handler,
    default_handler,
    default_handler
};
