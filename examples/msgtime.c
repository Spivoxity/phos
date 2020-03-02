/*
 * msgtime.c
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
#include "lib.h"
#include "hardware.h"
#include <string.h>

#define PROCA (USER+0)
#define PROCB (USER+1)

#define PULSE 1

#define ROW_MASK 0x0000e000
#define COL_MASK 0x00001ff0

#define ROW0 13
#define COL0 4

#define R 1
#define C 2

#define LIGHT (1<<(ROW0+R)) | (~(1<<(COL0+C)) & COL_MASK)

void procA(int n) {
    message msg;

    while (1) {
        receive(ANY, &msg);
        GPIO_OUT = 0;

        msg.m_type = PULSE;
        send(PROCB, &msg);
    }
}

void procB(int n) {
    message msg;

    while (1) {
        delay(10000);           // Allow time for procA to be ready
        
        GPIO_OUT = LIGHT;
        GPIO_OUT = 0;

        msg.m_type = PULSE;
        GPIO_OUT = LIGHT;
        sendrec(PROCA, &msg);
    }
}

void init(void) {
    GPIO_DIRSET = ROW_MASK | COL_MASK;

    timer_init();
    start(USER+0, "ProcA", procA, 0, STACK);
    start(USER+1, "ProcB", procB, 0, STACK);
}
