/*
 * timeout.c
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

#define MAIN USER+0
#define BUTTON USER+1


/* Polling button driver */

#define NBUT 2

const unsigned butpin[NBUT] = { BUTTON_A, BUTTON_B };

static int but_client;

static void button_task(int n) {
     int state[NBUT];           // Current button states
     unsigned char filter[NBUT]; // Filter for button readings
     message m;

     for (int i = 0; i < NBUT; i++) {
          GPIO_PINCNF[butpin[i]] = 0;
          state[i] = 0;
          filter[i] = 0;
     }

     timer_pulse(5);

     while (1) {
          receive(TIMER, &m);

          for (int i = 0; i < NBUT; i++) {
               // Use a FIR filter to debounce the switch.
               int next = state[i];
               int val = filter[i];
               val >>= 1;
               if ((GPIO_IN >> butpin[i]) & 1) val += 0x80;
               filter[i] = val;

               if (val < 0x20) next = 1;
               else if (val >= 0xe0) next = 0;
               
               if (next && !state[i]) {
                    // The button has been pressed
                    m.m_type = PING;
                    m.m_i1 = i;
                    send(but_client, &m);
               }

               state[i] = next;
          }
     }
}

void init_buttons(int client) {
     but_client = client;
     start(BUTTON, "Button", button_task, 0, STACK);
}


/* Main program */

const char *butname[] = { "A", "B" };

static void main_task(int n) {
     message m;

     while (1) {
          // Receive with timeout
          receive_t(BUTTON, &m, 1000);

          switch (m.m_type) {
          case PING:
               serial_printf("You pressed button %s\n", butname[m.m_i1]);
               break;

          case TIMEOUT:
               serial_printf("You didn't press any button\n");
               break;

          default:
               badmesg(m.m_type);
          }
     }
}

void init(void) {
     serial_init();
     timer_init();
     init_buttons(MAIN);
     start(MAIN, "Main", main_task, 0, STACK);
}
