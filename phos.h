/*
 * phos.h
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

typedef unsigned char byte;

/* Standard pids and message types */
#define HARDWARE -1
#define ANY 999

#define IDLE 0
#define SERIAL 1
#define TIMER 2
#define I2C 3
#define RADIO 4
#define RANDOM 5
#define TEMP 6
#define ADC 7
#define USER 8                  // 8..15 are for user processes

#define INTERRUPT 1
#define TIMEOUT 2
#define REGISTER 3
#define PING 4
#define REQUEST 5
#define OK 6
#define READ 7
#define WRITE 8
#define ERROR 10
#define SEND 11
#define RECEIVE 12
#define PACKET 13

/* Possible priorities */
#define P_HANDLER 0             // Interrupt handler
#define P_HIGH 1                // Responsive
#define P_LOW 2                 // Normal
#define P_IDLE 3                // The idle process

typedef struct {                // 16 bytes
    unsigned short m_type;      // Type of message
    short m_sender;             // PID of sender
    union {                     // Three words of data, each
        int m_i;                // ... an integer
        void *m_p;              // ... or some kind of pointer
        struct {                // ... of four bytes
            byte m_bw, m_bx, m_by, m_bz;
        } m_b;
    } m_x1, m_x2, m_x3;
} message;

#define m_i1 m_x1.m_i
#define m_i2 m_x2.m_i
#define m_i3 m_x3.m_i
#define m_p1 m_x1.m_p
#define m_p2 m_x2.m_p
#define m_p3 m_x3.m_p
#define m_b1 m_x1.m_b.m_bw
#define m_b2 m_x1.m_b.m_bx
#define m_b3 m_x1.m_b.m_by
#define m_b4 m_x1.m_b.m_bz


/* phos.c */

/* start -- create process that will run when init returns */
void start(int pid, char *name, void (*body)(int), int arg, int stksize);

#define STACK 1024              // Default stack size

/* System calls */
void yield(void);
void send(int dst, message *msg);
#ifdef ENABLE_TIMEOUTS
void receive_t(int src, message *msg, int timeout);
#define receive(dst, msg) receive_t(dst, msg, -1)
#else
void receive(int src, message *msg);
#endif
void sendrec(int dst, message *msg);
void tick(int ms);
void connect(int irq);
void reconnect(int irq);
void priority(int p);
void exit(void);
void dump(void);

/* interrupt -- send INTERRUPT message from handler */
void interrupt(int pid);

/* lock -- disable all interrupts */
void lock(void);

/* unlock -- enable interrupts again */
void unlock(void);

/* restore -- restore previous interrupt setting (used by kprintf) */
void restore(void);

/* reschedule -- request context switch */
void reschedule(void);


/* kprintf -- print message on console without using serial task */
void kprintf(char *fmt, ...);

/* panic -- crash with message [and show seven stars] */
void panic(char *fmt, ...);

/* badmesg -- panic after receiving unexpected message */
void badmesg(int type);

#define assert(p) \
    do { if (!(p)) panic("File %s, line %d: assertion %s failed", \
                         __FILE__, __LINE__, #p); } while (0)

/* spin -- flash the seven stars of death forever */
void spin(void);


/* serial.c */
void serial_putc(char ch);
char serial_getc(void);
void serial_printf(char *fmt, ...);
void serial_init(void);

/* timer.c */
void timer_delay(int msec);
void timer_pulse(int msec);
void timer_init(void);

/* i2c.c */

int i2c_read_reg(int addr, int cmd);
int i2c_try_read(int addr, int cmd, byte *buf);
void i2c_write_reg(int addr, int cmd, int val);
int i2c_xfer(int kind, int addr, byte *buf1, int n1, byte *buf2, int n2);
void i2c_init(void);

void accel_start(void);
void accel_reading(int *x, int *y, int *z);

/* temp.c */
int temp_reading(void);
void temp_init(void);

/* random.c */
unsigned randint(void);
unsigned randbyte(void);
void random_init(void);

/* radio.c */
#define RADIO_PACKET 32

void radio_send(void *buf, int n);
int radio_receive(void *buf);
void radio_init(void);

/* adc.c */
int adc_reading(int chan);
void adc_init(void);
