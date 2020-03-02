/*
 * temp.c
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

/* temp_task -- driver process for temperature sensor */
static void temp_task(int n) {
    message m;
    int temp;
    int client;

    TEMP_INTEN = BIT(TEMP_INT_DATARDY);
    connect(TEMP_IRQ);

    while (1) {
        receive(ANY, &m);

        switch (m.m_type) {
        case REQUEST:
            client = m.m_sender;

            TEMP_START = 1;
            receive(HARDWARE, &m);
            assert(TEMP_DATARDY);
            temp = TEMP_TEMP;
            TEMP_DATARDY = 0;
            reconnect(TEMP_IRQ);

            m.m_type = OK;
            m.m_i1 = temp;
            send(client, &m);
            break;

        default:
            badmesg(m.m_type);
        }
    }
}

/* temp_reading -- get die temperature in 1/4 degree units */
int temp_reading(void) {
     message m;
     m.m_type = REQUEST;
     sendrec(TEMP, &m);
     assert(m.m_type == OK);
     return m.m_i1;
}

/* temp_init -- start the driver task */
void temp_init(void) {
    start(TEMP, "Temp", temp_task, 0, 256);
}
