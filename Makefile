#
# Makefile for Phos
#
# This file is part of the Phos operating system for microcontrollers
# Copyright (c) 2018 J. M. Spivey
# All rights reserved
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

all: phos.a

ex-%.elf:

DEVICES = adc.o i2c.o radio.o random.o serial.o temp.o timer.o 

phos.a: $(DEVICES:%=devices/%) phos.o mpx-m0.o lib.o startup.o
	$(AR) cr $@ $^

%.o: %.c hardware.h phos.h
	$(CC) $(CPU) $(CFLAGS) -I . -c $< -o $@

%.o: %.s
	$(AS) $(CPU) $< -o $@

%.hex: %.elf
	arm-none-eabi-objcopy -O ihex $< $@

ex-%.elf: examples/%.c phos.a
	$(CC) $(CPU) $(CFLAGS) -I . -T NRF51822.ld -nostdlib \
	    $^ -lc -lgcc --specs=nano.specs -o $@ -Wl,-Map,ex-$*.map
	arm-none-eabi-size $@

startup.o: CFLAGS += -DPHOS

clean: force
	rm -f *.hex *.elf *.map *.o devices/*.o phos.a

# Don't delete intermediate files
.SECONDARY:

force:

CC = arm-none-eabi-gcc
AS = arm-none-eabi-as
AR = arm-none-eabi-ar

CPU = -mcpu=cortex-m0 -mthumb
CFLAGS = -O -g -Wall -ffreestanding
# The -ffreestanding suppresses warnings that functions like exit()
# don't have types that match the built-in definitions.
