/*
 * timer.c
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
#include "lib.h"

#define TICK 5                  /* Interval between updates (ms) */

#define MAX_TIMERS 4

/* Millis will overflow in about 46 days, but that's long enough. */

static unsigned millis = 0;

struct {
    int client;        // Process that receives message, or -1 if free
    unsigned period;   // Interval between messages, or 0 for one-shot
    unsigned next;     // Next time to send a message
} timer[MAX_TIMERS];

/* check_timers -- send any messages that are due */
static void check_timers(void) {
    int i;
    message m;

    for (i = 0; i < MAX_TIMERS; i++) {
        if (timer[i].client >= 0 && millis >= timer[i].next) {
            m.m_type = PING;
            m.m_i1 = timer[i].next;
            send(timer[i].client, &m);

            if (timer[i].period > 0)
                timer[i].next += timer[i].period;
            else
                timer[i].client = -1;

        }
    }
}

/* create -- create a new timer */
static void create(int client, int delay, int repeat) {
    int i = 0;

    while (i < MAX_TIMERS && timer[i].client >= 0)
        i++;

    if (i == MAX_TIMERS)
        panic("Too many timers");

    timer[i].client = client;
    timer[i].next = millis + delay;
    if (repeat)
        timer[i].period = delay;
    else
        timer[i].period = 0;
}

/* We use Timer 1 because its 16-bit mode is adequate for a clock with
   up to 1us resulution and 1ms period, leaving the 32-bit Timer 0 for
   other purposes. */

void timer_task(int n) {
    message m;

    TIMER1_STOP = 1;
    TIMER1_MODE = TIMER_Mode_Timer;
    TIMER1_BITMODE = TIMER_16Bit;
    TIMER1_PRESCALER = 4;      // 1MHz = 16MHz / 2^4
    TIMER1_CLEAR = 1;
    TIMER1_CC[0] = 1000 * TICK;
    TIMER1_SHORTS = BIT(TIMER_COMPARE0_CLEAR);
    TIMER1_INTENSET = BIT(TIMER_INT_COMPARE0);
    TIMER1_START = 1;

    connect(TIMER1_IRQ);

    while (1) {
        receive(ANY, &m);

        switch (m.m_type) {
        case INTERRUPT:
            if (TIMER1_COMPARE[0]) {
                millis += TICK;
                TIMER1_COMPARE[0] = 0;
                tick(TICK);         // Check for OS timeouts
                check_timers();
            }
            reconnect(TIMER1_IRQ);
            break;

        case REGISTER:
            create(m.m_sender, m.m_i1, m.m_i2);
            break;

        default:
            badmesg(m.m_type);
        }
    }
}

void timer_init(void) {
    int i;

    for (i = 0; i < MAX_TIMERS; i++)
        timer[i].client = -1;

    start(TIMER, "Timer", timer_task, 0, 256);
}

/* delay -- one-shot delay */
void timer_delay(int msec) {
    message m;
    m.m_type = REGISTER;
    m.m_i1 = msec;
    m.m_i2 = 0;                 /* Don't repeat */
    sendrec(TIMER, &m);
    assert(m.m_type == PING);
}

/* pulse -- regular pulse */
void timer_pulse(int msec) {
     message m;
     m.m_type = REGISTER;
     m.m_i1 = msec;
     m.m_i2 = 1;                /* Repetitive */
     send(TIMER, &m);
}
