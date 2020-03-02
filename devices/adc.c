/*
 * adc.c
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

static void adc_task(int dummy) {
    int client, chan, result;
    message m;

    // Initialise the ADC: compare 1/3 of the input with 1/3 of Vdd
    ADC_CONFIG = FIELD(ADC_CONFIG_RES, ADC_CONFIG_RES_10bit)
        | FIELD(ADC_CONFIG_INPSEL, ADC_CONFIG_INPSEL_AIn_1_3)
        | FIELD(ADC_CONFIG_REFSEL, ADC_CONFIG_REFSEL_Vdd_1_3);
    ADC_ENABLE = 1;

    ADC_INTEN = BIT(ADC_INT_END);
    connect(ADC_IRQ);
    
    while (1) {
        receive(ANY, &m);
        assert(m.m_type == REQUEST);
        client = m.m_sender;
        chan = m.m_i1;

        SET_FIELD(ADC_CONFIG, ADC_CONFIG_PSEL, BIT(chan));
        ADC_START = 1;
        receive(HARDWARE, &m);
        assert(ADC_END);
        result = ADC_RESULT;
        SET_FIELD(ADC_CONFIG, ADC_CONFIG_PSEL, 0);
        ADC_END = 0;
        reconnect(ADC_IRQ);
        
        m.m_type = OK;
        m.m_i1 = result;
        send(client, &m);
    }
}

int adc_reading(int chan) {
    message m;
    m.m_type = REQUEST;
    m.m_i1 = chan;
    sendrec(ADC, &m);
    assert(m.m_type == OK);
    return m.m_i1;
}

void adc_init(void) {
    start(ADC, "Adc", adc_task, 0, 256);
}
