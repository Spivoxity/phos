/*
 * echo.c
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

/* read_line -- read a line of input */
void read_line(char *buf) {
    char *p = buf;
    
    for (;;) {
        char ch = serial_getc();
        if (ch == '\n') break;
        *p++ = ch;              // Should check for buffer overflow
    }

    *p = '\0';
}

const char * const decimal[] = { "00", "25", "50", "75" };

/* echo_task -- main process for example */
void echo_task(int n) {
    char line[128];
    int temp;

    serial_printf("Hello\n");
    serial_printf("CPUID = %x\n", SCB_CPUID);
    serial_printf("DEVID = %x:%x\n", FICR_DEVICEID[1], FICR_DEVICEID[0]);

    while (1) {
        // Read a line of input
        read_line(line);

        // Get the CPU die temperature
        temp = temp_reading();

        // Echo the input with the temperature
        serial_printf("--> %s (%d) temp=%d.%s\n",
                      line, strlen(line), temp>>2, decimal[temp&0x3]);
    }
}

/* init -- start the processes */
void init(void) {
    serial_init();
    temp_init();
    start(USER+0, "Echo", echo_task, 0, STACK);
}
