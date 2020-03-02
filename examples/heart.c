/*
 * heart.c
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
#include "phos.h"

#define HEART (USER+0)
#define PRIME (USER+1)

/* wait -- sleep until next timer ping */
static void wait(void) {
    message m;
    receive(TIMER, &m);
    assert(m.m_type == PING);
}

/* heart -- filled-in heart image */
static const unsigned short heart[] = {
    0x28f0, 0x5e00, 0x8060
};
     
/* small -- small heart image */
static const unsigned short small[] = {
    0x2df0, 0x5fb0, 0x8af0
};

/* show -- display three rows of a picture n times */
static void show(const unsigned short *img, int n) {
    while (n-- > 0) {
        // Takes 15msec per iteration
        for (int p = 0; p < 3; p++) {
            GPIO_OUT = img[p];
            wait();
        }
    }
}

/* heart_task -- show beating heart */
static void heart_task(int n) {
    GPIO_DIRSET = 0xfff0;
    GPIO_PINCNF[BUTTON_A] = 0;
    GPIO_PINCNF[BUTTON_B] = 0;

    priority(P_HIGH);
    timer_pulse(5);

    while (1) {
        show(heart, 70);
        show(small, 10);
        show(heart, 10);
        show(small, 10);
    }
}

/* This is a bit lighter than the lab3 example, because we use GCC's
builtin modulo operation, rather than repeated subtraction.  That
leaves some CPU time over to look after the blinking lights. */

/* prime -- test for primality */
int prime(int n) {
    for (int k = 2; k * k <= n; k++) {
        if (n % k == 0)
            return 0;
    }

    return 1;
}

/* prime_task -- print primes on the serial port */
void prime_task(int arg) {
    int n = 2, count = 0;

    while (1) {
        if (prime(n)) {
            count++;
            serial_printf("prime(%d) = %d\n", count, n);
        }
        n++;
    }
}

/* init -- set the ball rolling */
void init(void) {
    serial_init();
    timer_init();
    start(HEART, "Heart", heart_task, 0, STACK);
    start(PRIME, "Prime", prime_task, 0, STACK);
}
