/*
 * i2c.c
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
#include <stddef.h>

/* i2c_wait -- wait for an expected interrupt event and detect error */
static int i2c_wait(unsigned volatile *event) {
     message m;
     
     receive(HARDWARE, &m);

     if (I2C_ERROR) {
          I2C_ERROR = 0; 
          reconnect(I2C_IRQ);
          return ERROR;
     }      

     assert(*event);
     *event = 0;                                 
     reconnect(I2C_IRQ);
     return OK;
}

/* i2c_stop -- signal stop condition */
static void i2c_stop(void) {
     I2C_STOP = 1;
     i2c_wait(&I2C_STOPPED);
}

/* i2c_read -- read one or more bytes */
static int i2c_read(char *buf, int n) {
     int status = OK;

     for (int i = 0; i < n; i++) {
          if (i == n-1)
               I2C_SHORTS = BIT(I2C_BB_STOP);
          else
               I2C_SHORTS = BIT(I2C_BB_SUSPEND);

          if (i == 0)
               I2C_STARTRX = 1;
          else
               I2C_RESUME = 1;

          status = i2c_wait(&I2C_RXDREADY);
          if (status != OK) return status;
          buf[i] = I2C_RXD;
     }

     i2c_wait(&I2C_STOPPED);
     return OK;
}          
          
/* i2c_start_write -- start write transaction */
static void i2c_start_write(void) {
     I2C_SHORTS = 0;
     I2C_STARTTX = 1;
}

/* i2c_write_bytes -- send one or more bytes */
static int i2c_write_bytes(char *buf, int n) {
     int status = OK;

     /* The I2C hardware makes zero-length writes impossible, because
        there is no event generated when the address has been sent. */

     for (int i = 0; i < n; i++) {
          I2C_TXD = buf[i];
          status = i2c_wait(&I2C_TXDSENT);
          if (status != OK) return status;
     }

     return OK;
}

/* i2c_task -- driver process for I2C hardware */
static void i2c_task(int dummy) {
     message m;
     int client, addr, n1, n2, status;
     char *buf1, *buf2;

     I2C_ENABLE = 0;

     // Connect pins as inputs and select open-drain mode
     SET_FIELD(GPIO_PINCNF[I2C_SCL], GPIO_PINCNF_CONNECT, GPIO_Connect);
     SET_FIELD(GPIO_PINCNF[I2C_SCL], GPIO_PINCNF_DRIVE, GPIO_S0D1);
     SET_FIELD(GPIO_PINCNF[I2C_SDA], GPIO_PINCNF_CONNECT, GPIO_Connect);
     SET_FIELD(GPIO_PINCNF[I2C_SDA], GPIO_PINCNF_DRIVE, GPIO_S0D1);

     // Configure I2C hardware
     I2C_PSELSCL = I2C_SCL;
     I2C_PSELSDA = I2C_SDA;
     I2C_FREQUENCY = I2C_FREQ_100kHz;
     I2C_ENABLE = I2C_Enabled;

     // Enable interrupts
     I2C_INTEN = BIT(I2C_INT_RXDREADY) | BIT(I2C_INT_TXDSENT)
          | BIT(I2C_INT_STOPPED) | BIT(I2C_INT_ERROR);
     connect(I2C_IRQ);

     while (1) {
          receive(ANY, &m);
          client = m.m_sender;
          addr = m.m_b1;        // Address [0..127] without R/W flag
          n1 = m.m_b2;          // Number of bytes in command
          n2 = m.m_b3;          // Number of bytes to transfer (R/W)
          buf1 = m.m_p2;        // Buffer for command
          buf2 = m.m_p3;        // Buffer for transfer

          switch (m.m_type) {
          case READ:
               I2C_ADDRESS = addr;
               status = OK;

               // Write followed by read, with repeated start
               if (n1 > 0) {
                    i2c_start_write();
                    status = i2c_write_bytes(buf1, n1);
               }
               if (status == OK)
                    status = i2c_read(buf2, n2);

               if (status == OK)
                    m.m_type = OK;
               else {
                    i2c_stop();
                    m.m_type = ERROR;
                    m.m_i1 = I2C_ERRORSRC;
                    I2C_ERRORSRC = I2C_ERROR_ALL;
               }
               
               send(client, &m);
               break;

          case WRITE:
               I2C_ADDRESS = addr;
               status = OK;

               // A single write transaction
               i2c_start_write();
               if (n1 > 0)
                    status = i2c_write_bytes(buf1, n1);
               if (status == OK)
                    status = i2c_write_bytes(buf2, n2);
               i2c_stop();

               if (status == OK)
                    m.m_type = OK;
               else {
                    m.m_type = ERROR;
                    m.m_i1 = I2C_ERRORSRC;
                    I2C_ERRORSRC = I2C_ERROR_ALL;
               }
               
               send(client, &m);
               break;

          default:
               badmesg(m.m_type);
          }
     }
}

/* i2c_init -- start I2C driver process */
void i2c_init(void) {
     start(I2C, "I2C", i2c_task, 0, 256);
}

/* i2c_xfer -- i2c transaction with command write then data read or write */
int i2c_xfer(int kind, int addr, byte *buf1, int n1, byte *buf2, int n2) {
     message m;
     m.m_type = kind;
     m.m_b1 = addr;
     m.m_b2 = n1;
     m.m_b3 = n2;
     m.m_p2 = buf1;
     m.m_p3 = buf2;
     sendrec(I2C, &m);
     return m.m_type;
}

/* i2c_try_read -- try to read from I2C device */
int i2c_try_read(int addr, int cmd, byte *buf2) {
     byte buf1 = cmd;
     return i2c_xfer(READ, addr, &buf1, 1, buf2, 1);
}
     
/* i2c_read_reg -- send command and read result */
int i2c_read_reg(int addr, int cmd) {
     byte buf;
     int status = i2c_try_read(addr, cmd, &buf);
     assert(status == OK);
     return buf;
}

/* i2c_write_reg -- send command and write data */
void i2c_write_reg(int addr, int cmd, int val) {
     byte buf1 = cmd, buf2 = val;
     int status = i2c_xfer(WRITE, addr, &buf1, 1, &buf2, 1);
     assert(status == OK);
}


/* Accelerometer */

/* Different revisions of the board have different accelerometer
   chips: a MMA8653FC on the first revision, and a LSM303AGR on the
   second revision (also including the magnetometer in the same chip).
   Each chip has its own I2C address and its own internal
   structure. */

#define ACC1 0x1d               // I2C address of accelerometer
#define ACC1_CTRL_REG1 0x2a     // Control register
#define ACC1_X_DATA 0x01        // Acceleration data

#define ACC2 0x19               // I2C address of mk2 accelerometer
#define ACC2_CTRL_REG1 0x20     // Control register
#define ACC2_OUT 0x28           // Acceleration data (different format)

static int acc_addr = 0;

/* acc1_read -- read acceleration data (v1) */
static void acc1_read(int *x, int *y, int *z) {
     byte addr = ACC1_X_DATA;
     signed char buf[3];
     i2c_xfer(READ, ACC1, &addr, 1, (byte *) buf, 3);
     *x = -buf[0]; *y = buf[1]; *z = -buf[2];
}

/* acc2_read -- read acceleration data (v2) */
static void acc2_read(int *x, int *y, int *z) {
     byte addr = ACC2_OUT | 0x80;
     signed char buf[6];
     i2c_xfer(READ, ACC2, &addr, 1, (byte *) buf, 6);
     *x = buf[1]; *y = buf[3]; *z = -buf[5];
}

/* accel_start -- initialise accelerometer */
void accel_start(void) {
     byte buf;

     if (i2c_try_read(ACC1, 0x0d, &buf) == OK) {
          i2c_write_reg(ACC1, ACC1_CTRL_REG1, 0x23); // 50Hz, 8 bit, Active
          acc_addr = ACC1;
     } else if (i2c_try_read(ACC2, 0x0f, &buf) == OK) {
          i2c_write_reg(ACC2, ACC2_CTRL_REG1, 0x4f); // 50Hz, 8 bit, Active
          acc_addr = ACC2;
     } else {
          panic("Can't find accelerometer");
     }
}

/* accel_reading -- obtain accelerometer reading */
void accel_reading(int *x, int *y, int *z) {
    switch (acc_addr) {
    case ACC1:
        acc1_read(x, y, z);
        break;
    case ACC2:
        acc2_read(x, y, z);
        break;
    default:
        panic("Unknown accelerometer");
    }
}
