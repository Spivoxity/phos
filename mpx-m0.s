@
@ mpx-m0.s
@
@ This file is part of the Phos operating system for microcontrollers
@ Copyright (c) 2018 J. M. Spivey
@ All rights reserved
@ 
@ Redistribution and use in source and binary forms, with or without
@ modification, are permitted provided that the following conditions are met:
@ 
@ 1. Redistributions of source code must retain the above copyright notice,
@    this list of conditions and the following disclaimer.
@ 
@ 2. Redistributions in binary form must reproduce the above copyright notice,
@    this list of conditions and the following disclaimer in the documentation
@    and/or other materials provided with the distribution.
@ 
@ 3. The name of the author may not be used to endorse or promote products
@    derived from this software without specific prior written permission.
@ 
@ THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
@ IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
@ OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
@ IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
@ SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
@ PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
@ OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
@ WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
@ OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
@ ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
@

@@@ Hardware multiplexing for the ARM Cortex-M0

        .syntax unified
        .text

@@@ setstack -- enter process mode
        .global setstack
        .thumb_func
setstack:
        msr psp, r0             @ Set up the stack
	movs r0, #2             @ Use psp for stack pointer
        msr control, r0
	isb                     @ Drain the pipeline
	bx lr
        
@@@ Stack layout for interrupt frames (16 words, 64 bytes)
@@@ --------------------------------------
@@@ 15  PSR  Status register
@@@ 14  PC   Program counter
@@@ 13  LR   Link register
@@@ 12  R12
@@@ 11  R3
@@@ 10  R2           (Saved by hardware)
@@@  9  R1
@@@  8  R0
@@@ --------------------------------------
@@@  7  R7   
@@@  6  R6
@@@  5  R5
@@@  4  R4           (Saved manually)
@@@  3  R11
@@@  2  R10
@@@  1  R9
@@@  0  R8  <-- Stack pointer
@@@ --------------------------------------

@@@ isave -- save context for system call
isave:   
	mrs r0, psp             @ Get thread stack pointer
        subs r0, #16
        stm r0!, {r4-r7}        @ Save low regs on thread stack
	mov r4, r8              @ Copy from high to low
        mov r5, r9
        mov r6, r10
        mov r7, r11
        subs r0, #32
        stm r0!, {r4-r7}        @ Save high regs in thread stack
        subs r0, #16
        bx lr                   @ Return process sp

@@@ irestore -- restore context after system call
irestore:                       @ Expect process sp in r0
        ldm r0!, {r4-r7}        @ Restore high registers
        mov r8, r4              @ Copy from low to high
        mov r9, r5
        mov r10, r6
        mov r11, r7
        ldm r0!, {r4-r7}        @ Restore low registers
        msr psp, r0             @ Set stack pointer for thread
        bx lr

@@@ svc_handler -- handler for SVC interrupt (system call)
        .global svc_handler
        .thumb_func
svc_handler:
	push {lr}               @ Push lr on main stack
	bl isave                @ Complete saving of state
        bl system_call          @ Perform system call
        bl irestore             @ Restore manually saved state
        pop {pc}                @ Return to new thread

@@@ pendsv_handler -- handler for PendSV interupt (context switch)
        .global pendsv_handler
        .thumb_func
pendsv_handler:
        push {lr}
        bl isave
        bl cxtswitch
        bl irestore
        pop {pc}

@@@ lock -- disable interrupts
	.global lock
        .thumb_func
lock:                           
        mrs r0, primask         @ Save current state for restore
	ldr r1, =prev_mask
        str r0, [r1]
        cpsid i
        bx lr

@@@ unlock -- enable interrupts
	.global unlock
        .thumb_func
unlock:
        cpsie i
        bx lr

@@@ restore -- restore previous interrupt setting (used by kprintf)
	.global restore
        .thumb_func
restore:
        ldr r1, =prev_mask      @ Get previously saved state
        ldr r0, [r1]
        msr primask, r0         @ Restore it
        bx lr
        
        .bss
	.align 2
prev_mask:
        .space 4                @ Previous interrupt setting

