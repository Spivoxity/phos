/*
 * random.c
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

#include "phos.h"
#include "hardware.h"
#include <string.h>

#define NRAND 64                // Number of bytes to buffer

static unsigned char randoms[NRAND];
static unsigned nrand = 0;

/* rng_interrupt -- handle an interrupt */
static void rng_interrupt(void) {
    unsigned char val;

    if (RNG_VALRDY) {
        val = RNG_VALUE;
        if (nrand < NRAND) randoms[nrand++] = val;

        // Clear and re-enable the interrupt
        RNG_VALRDY = 0;
        clear_pending(RNG_IRQ);
        enable_irq(RNG_IRQ);
    }
}

/* rng_await -- wait until at least n bytes are available */
static void rng_await(int n) {
    message m;

    while (nrand < n) {
        receive(HARDWARE, &m);
        assert(m.m_type == INTERRUPT);
        rng_interrupt();
    }
}

/* random_task -- driver process for RNG */
static void random_task(int n) {
    message m;
    int client;

    // Configure and start the hardware
    SET_BIT(RNG_CONFIG, RNG_DERCEN); // Correct for bias
    RNG_VALRDY = 0;
    RNG_START = 1;

    // Enable and connect to the interrupt
    RNG_INTENSET = BIT(RNG_INT_VALRDY);
    connect(RNG_IRQ);

    while (1) {
        receive(ANY, &m);
        switch (m.m_type) {
        case INTERRUPT:
            rng_interrupt();
            break;

        case REQUEST:
             client = m.m_sender;
             int count = m.m_i1;
             unsigned char *buf = m.m_p2;
             assert (count <= NRAND);
             rng_await(count);
             memcpy(buf, &randoms[nrand-count], count);
             nrand -= count;
             m.m_type = OK;
             send(client, &m);

        default:
            badmesg(m.m_type);
        }
    }
}

unsigned randint(void) {
    message m;
    unsigned result;
    m.m_type = REQUEST;
    m.m_i1 = 4;
    m.m_p2 = (unsigned char *) &result;
    sendrec(RANDOM, &m);
    assert(m.m_type == OK);
    return result;
}

unsigned randbyte(void) {
    message m;
    unsigned char result;
    m.m_type = REQUEST;
    m.m_i1 = 1;
    m.m_p2 = &result;
    sendrec(RANDOM, &m);
    assert(m.m_type == OK);
    return result;
}

/* random_init -- start the driver task */
void random_init(void) {
    start(RANDOM, "Random", random_task, 0, 256);
}
