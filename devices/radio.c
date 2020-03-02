/*
 * radio.c
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

/* 
Modes:
   DISABLED -- doing nothing
   IDLE -- initialised for reception
   LISTENING -- waiting for a packet, DMA already set up
*/
#define DISABLED 0
#define READY 1
#define LISTENING 2

#define FREQ 7 // Frequency 2407 MHz

/* We use a packet format that agrees with the standard micro:bit
   runtime.  That means prefixing the packet with three bytes
   (version, group, protocol) and counting these three in the length:
   the STATLEN feature of the radio does not do this. */

static struct radio_frame {
    byte length;
    byte version;
    byte group;
    byte protocol;
    byte data[RADIO_PACKET];
} packet_buffer;

static void init_radio() {
    RADIO_TXPOWER = 0; // Default transmit power
    RADIO_FREQUENCY = FREQ;
    RADIO_MODE = RADIO_MODE_NRF_1Mbit;
    RADIO_BASE0 = 0x75626974; // That spells 'uBit'.
    RADIO_PREFIX0 = 0; // Group 0
    RADIO_TXADDRESS = 0;
    RADIO_RXADDRESSES = BIT(0);

    RADIO_PCNF0 = 0x8; // 8 bit length field; no S0 or S1
    RADIO_PCNF1 =
        BIT(RADIO_PCNF1_WHITEEN) | FIELD(RADIO_PCNF1_BALEN, 4)
        | FIELD(RADIO_PCNF1_MAXLEN, RADIO_PACKET+3);

    // CRC settings -- matches micro_bit runtime
    RADIO_CRCCNF = 2;
    RADIO_CRCINIT = 0xffff;
    RADIO_CRCPOLY = 0x11021;

    // Whitening -- ditto
    RADIO_DATAWHITEIV = 0x18;

    // Trim override?  Probably not needed on micro:bit
    if ((FICR_OVERRIDEEN & BIT(FICR_OVERRIDEEN_NRF)) == 0) {
        kprintf("Setting radio override values\r\n");
        RADIO_OVERRIDE[0] = FICR_NRF_1MBIT[0];
        RADIO_OVERRIDE[1] = FICR_NRF_1MBIT[1];
        RADIO_OVERRIDE[2] = FICR_NRF_1MBIT[2];
        RADIO_OVERRIDE[3] = FICR_NRF_1MBIT[3];
        RADIO_OVERRIDE[4] = FICR_NRF_1MBIT[4];
    }

    // Configure interrupts
    RADIO_INTENSET =
        BIT(RADIO_INT_READY) | BIT(RADIO_INT_END) | BIT(RADIO_INT_DISABLED);

    // Set packet buffer
    RADIO_PACKETPTR = (unsigned) &packet_buffer;    
}

static void await(unsigned volatile *event) {
    message m;
    receive(HARDWARE, &m);
    assert(*event);
    *event = 0;
    reconnect(RADIO_IRQ);
}

static void radio_task(int dummy) {
    int mode = DISABLED;
    int listener = 0, n;
    void *buffer = NULL;
    message m;

    init_radio();
    connect(RADIO_IRQ);

    while (1) {
        receive(ANY, &m);

        switch (m.m_type) {
        case INTERRUPT:
            // A packet has been received
            if (!RADIO_END || mode != LISTENING)
                panic("unexpected radio interrrupt");
            RADIO_END = 0;
            reconnect(RADIO_IRQ);
            if (RADIO_CRCSTATUS == 0)
                // Ignore corrupted packets
                break;

            n = packet_buffer.length-3;
            memcpy(buffer, packet_buffer.data, n);

            m.m_type = PACKET;
            m.m_i1 = n;
            send(listener, &m);
            mode = READY;
            break;

        case RECEIVE:
            if (mode == LISTENING)
                panic("radio supports only one listener at a time");
            listener = m.m_sender;
            buffer = m.m_p1;

            if (mode == DISABLED) {
                RADIO_RXEN = 1;
                await(&RADIO_READY);
            }

            RADIO_START = 1;
            mode = LISTENING;
            break;

        case SEND:
            if (mode != DISABLED) {
                // The radio was set up for receiving: disable it
                RADIO_DISABLE = 1;
                await(&RADIO_DISABLED);
            }

            // Assemble the packet
            n = m.m_i2;
            packet_buffer.length = n+3;
            packet_buffer.version = 1;
            packet_buffer.group = 0;
            packet_buffer.protocol = 1; // Agrees with uBit datagrams
            memcpy(packet_buffer.data, m.m_p1, n);

            // Enable for sending and transmit the packet
            RADIO_TXEN = 1;
            await(&RADIO_READY);
            RADIO_START = 1;
            await(&RADIO_END);

            // Disable the transmitter -- otherwise it jams the airwaves
            RADIO_DISABLE = 1;
            await(&RADIO_DISABLED);

            if (mode != LISTENING)
                mode = DISABLED;
            else {
                // Go back to listening
                RADIO_RXEN = 1;
                await(&RADIO_READY);
                RADIO_START = 1;
            }

            m.m_type = OK;
            send(m.m_sender, &m);
            break;

        default:
            badmesg(m.m_type);
        }
    }
}

void radio_send(void *buf, int n) {
    message m;
    m.m_type = SEND;
    m.m_p1 = buf;
    m.m_i2 = n;
    sendrec(RADIO, &m);
}

int radio_receive(void *buf) {
    message m;
    m.m_type = RECEIVE;
    m.m_p1 = buf;
    sendrec(RADIO, &m);
    assert(m.m_type == PACKET);
    return m.m_i1;
}
    
void radio_init(void) {
    start(RADIO, "Radio", radio_task, 0, 256);
}
