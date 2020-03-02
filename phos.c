/*
 * phos.c
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

/* STORAGE ALLOCATION */

/* Stack space for processes is allocated from the space between the
   static data and the main stack. */

extern unsigned char __stack_limit[], __end[];

static unsigned char *__break = __end;

#define ROUNDUP(x, n)  (((x)+(n)-1) & ~((n)-1))

static void *sbrk(int inc) {
    inc = ROUNDUP(inc, 4);
    __break = (unsigned char *) ROUNDUP((unsigned) __break, 8);
    if (inc > __stack_limit - __break)
        panic("Phos is out of memory");
    void *result = __break;
    __break += inc;
    return result;
}


/* PROCESS TABLE */

#define NPROCS 16

static struct proc {
     int p_pid;                  /* Process ID (equal to index) */
     char p_name[16];            /* Name for debugging */
     unsigned p_state;           /* SENDING, RECEIVING, etc. */
     unsigned *p_sp;             /* Saved stack pointer */
     void *p_stack;              /* Stack area */
     unsigned p_stksize;         /* Stack size (bytes) */
     int p_priority;             /* Priority: 0 is highest */

     struct proc *p_waiting;     /* Processes waiting to send */
     int p_pending;              /* Whether HARDWARE message pending */
     int p_accept;               /* Processes who may send: ANY or pid */
     message *p_message;         /* Pointer to message buffer */
     int p_timeout;              /* Timeout for recieve (ms) or NO_TIME */

     struct proc *p_next;        /* Next process in ready or send queue */
} ptable[NPROCS];

/* Possible p_state values */
#define DEAD 0
#define READY 1
#define SENDING 2
#define RECEIVING 3
#define BOTH 4
#define IDLING 5

#define NO_TIME 0x80000000

static struct proc *current;
static struct proc *idle_proc;

#define BLANK 0xdeadbeef        /* Filler for initial stack */

static void kprintf_setup(void);
static void kprintf_internal(char *fmt, ...);

/* phos_dump -- display process states */
static void phos_dump(void) {
    char *status = "ZASRBI";
    char buf1[16], buf2[16];

    kprintf_setup();
    kprintf_internal("\r\nPROCESS DUMP\r\n");

    // Our version of printf is a bit feeble, so the following is
    // rather painful.

    for (int pid = 0; pid < NPROCS; pid++) {
        struct proc *p = &ptable[pid];

        if (*p->p_name != '\0') {
            unsigned *z = (unsigned *) p->p_stack;
            while (*z == BLANK) z++;
            unsigned free = (char *) z - (char *) p->p_stack;

            if (pid < 10)
                sprintf(buf1, " %d", pid);
            else
                sprintf(buf1, "%d", pid);

            sprintf(buf2, "%u/%u", p->p_stksize-free, p->p_stksize);
            int w = strlen(buf2);
            if (w < 9) {
                memset(buf2+w, ' ', 9-w);
                buf2[9] = '\0';
            }

            kprintf_internal("%s: [%c] %x stk=%s %s\r\n",
                             buf1, status[p->p_state],
                             (unsigned) p->p_stack,
                             buf2, p->p_name);
        }
    }
}


/* PROCESS QUEUES */

/* ready_q -- one queue for each priority */
static struct queue {
    struct proc *q_head, *q_tail;
} ready_q[3];

/* make_ready -- add process to end of appropriate queue */
static void make_ready(struct proc *p) {
    if (p == idle_proc) return;

    p->p_state = READY;
    p->p_next = NULL;

    struct queue *q = &ready_q[p->p_priority];

    if (q->q_head == NULL)
        q->q_head = p;
    else
        q->q_tail->p_next = p;
     
    q->q_tail = p;
}

/* choose_proc -- pick a new process as current */
static void choose_proc(void) {
    for (int p = 0; p < 3; p++) {
        struct queue *q = &ready_q[p];
        if (q->q_head != NULL) {
            current = q->q_head;
            q->q_head = current->p_next;
            return;
        }
    }

    current = idle_proc;
}


/* TIMEOUTS */

/* A process p has a timeout set if p->p_timeout != NO_TIME.  Those
   processes are also in the array timeout, so we can find them
   quickly on each tick. */

static struct proc *timeout[NPROCS];
static int n_timeouts = 0;

static int time_clock = 0;      // Accumulated ticks
static int time_next = NO_TIME; // Earliest timeout due (could be -ve)

static void set_timeout(int ms) {
     int due = time_clock + ms;

     assert(n_timeouts < NPROCS);
     assert(current->p_timeout == NO_TIME);

     current->p_timeout = due;
     timeout[n_timeouts++] = current;

     if (time_next == NO_TIME || due < time_next)
          time_next = due;
}

static void cancel_timeout(struct proc *p) {
     p->p_timeout = NO_TIME;

     // Delete p from the timeout array
     for (int i = 0; i < n_timeouts; i++) {
          if (timeout[i] == p) {
               timeout[i] = timeout[--n_timeouts];
               return;
          }
     }

     panic("Cancelling a non-existent timeout");
}

void mini_tick(int delta) {
     /* We delay delivery of timeouts until the clock tick after the
        one where they expire. That way, there is no risk of the
        timeout coming too soon if a tick happens just after the
        timeout is set.  Note that timeouts are delivered as if from
        HARDWARE even if the receive call specified another process. */

     int n = 0;                 // Number of surviving timeouts

     if (time_next == NO_TIME)
          return;
     else if (time_clock <= time_next) {
          time_clock += delta;
          return;
     }
     
     time_next = NO_TIME;

     for (int j = 0; j < n_timeouts; j++) {
          struct proc *pdst = timeout[j];
          assert(pdst->p_timeout != NO_TIME);

          if (pdst->p_timeout >= time_clock) {
               pdst->p_timeout -= time_clock + delta;
               if (time_next == NO_TIME || pdst->p_timeout < time_next)
                    time_next = pdst->p_timeout;
               timeout[n++] = pdst;
          } else {
               // Timeout already due.
               pdst->p_timeout = NO_TIME;
               pdst->p_message->m_sender = HARDWARE;
               pdst->p_message->m_type = TIMEOUT;
               make_ready(pdst);
          }
     }               

     time_clock = 0;
     n_timeouts = n;
}


/* SEND AND RECEIVE */

/* These versions of send and receive are invoked indirectly from user
   processes via the system calls send() and receive(). */

/* mini_send -- send a message */
static void mini_send(int dst, message *msg) {
    int src = current->p_pid;
    struct proc *pdst = &ptable[dst];

    if (dst < 0 || dst >= NPROCS || pdst->p_state == DEAD)
        panic("Sending to a non-existent process %d", dst);

    if (pdst->p_state == RECEIVING
        && (pdst->p_accept == ANY || pdst->p_accept == src)) {
        // Receiver is waiting for us
        *(pdst->p_message) = *msg;
        pdst->p_message->m_sender = src;
        if (pdst->p_timeout != NO_TIME)
             cancel_timeout(pdst);
        make_ready(pdst);
    } else {
        // Sender must wait by joining the receiver's queue
        current->p_state = SENDING;
        current->p_message = msg;
        current->p_next = NULL;

        if (pdst->p_waiting == NULL)
            pdst->p_waiting = current;
        else {
            struct proc *r = pdst->p_waiting;
            while (r->p_next != NULL)
                r = r->p_next;
            r->p_next = current;
        }

        choose_proc();
    }
}

/* mini_receive -- receive a message */
static void mini_receive(int accept, message *msg, int timeout) {
    // First see if an interrupt is pending
    if (current->p_pending && (accept == ANY || accept == HARDWARE)) {
        current->p_pending = 0;
        msg->m_sender = HARDWARE;
        msg->m_type = INTERRUPT;
        return;
    }

    if (accept != HARDWARE) {
        // Now look to see if an acceptable process is waiting
        struct proc *prev = NULL;

        for (struct proc *psrc = current->p_waiting; psrc != NULL;
             psrc = psrc->p_next) {
            if (accept == ANY || accept == psrc->p_pid) {
                if (prev == NULL)
                    current->p_waiting = psrc->p_next;
                else
                    prev->p_next = psrc->p_next;

                *msg = *(psrc->p_message);
                msg->m_sender = psrc->p_pid;

                if (psrc->p_state == BOTH)
                    psrc->p_state = RECEIVING;
                else {
                    /* Non-preemptive: the receiver continues to run
                       even if the sender has lower priority */
                    make_ready(psrc);
                }
                
                return;
            }
            prev = psrc;
        }
    }
     
    if (accept != ANY && accept != HARDWARE &&
        (accept < 0 || accept >= NPROCS || ptable[accept].p_state == DEAD))
        panic("Trying to receive from non-existent process %d", accept);

    if (timeout == 0) {
         // No message, and we are not prepared to wait
         msg->m_type = TIMEOUT;
         msg->m_sender = HARDWARE;
         return;
    }
    
    // No luck: we must wait.
    current->p_state = RECEIVING;
    current->p_accept = accept;
    current->p_message = msg;
    if (timeout > 0) set_timeout(timeout);
    choose_proc();
}

static void mini_sendrec(int dst, message *msg) {
    int src = current->p_pid;
    struct proc *pdst = &ptable[dst];

    if (dst < 0 || dst >= NPROCS || pdst->p_state == DEAD)
        panic("Sending to a non-existent process %d", dst);

    msg->m_sender = src;
    current->p_accept = dst;
    current->p_message = msg;

    if (pdst->p_state == RECEIVING
        && (pdst->p_accept == ANY || pdst->p_accept == src)) {
        // Receiver is waiting for us
        *(pdst->p_message) = *msg;
        pdst->p_message->m_sender = src;
        current->p_state = RECEIVING;
        make_ready(pdst);
    } else {
        // Sender must wait by joining the receiver's queue
        current->p_state = BOTH;
        current->p_next = NULL;
        if (pdst->p_waiting == NULL)
            pdst->p_waiting = current;
        else {
            struct proc *r = pdst->p_waiting;
            while (r->p_next != NULL)
                r = r->p_next;
            r->p_next = current;
        }
    }

    choose_proc();
}    

/* INTERRUPT HANDLING */

/* Interrupts send an INTERRUPT message (from HARDWARE) to a
   registered handler process.  The default beheviour is to disable
   the relevant IRQ in the interrupt handler, so that it can be
   re-enabled in the handler once it has reacted to the interrupt.

   We only deal with the 32 interrupts >= 0, not the 16 exceptions
   that are < 0 this way. */

static int handler[32];

/* connect -- connect the current process to an IRQ */
void connect(int irq) {
    if (irq < 0) panic("Can't connect to CPU exceptions");
    current->p_priority = 0;
    handler[irq] = current->p_pid;
    enable_irq(irq);
}

void reconnect(int irq) {
     /* When a task has dealt with the cause of an interrupt, we want
        to enable it again.  But the device has probably set the
        pending bit of the interrupt a second time after the interrupt
        handler returned.  So we must clear it here.  Any extra
        interrupts that arrive before the task runs are lost. */
     clear_pending(irq);
     enable_irq(irq);
}

/* priority -- set process priority */
void priority(int p) {
    if (p > 2) panic("Bad priority %u\n", p);
    current->p_priority = p;
}

/* interrupt -- send interrupt message */
void interrupt(int dst) {
    struct proc *pdst = &ptable[dst];

    if (pdst->p_state == RECEIVING
        && (pdst->p_accept == ANY || pdst->p_accept == HARDWARE)) {
        // Receiver is waiting for an interrupt
        pdst->p_message->m_sender = HARDWARE;
        pdst->p_message->m_type = INTERRUPT;
        make_ready(pdst);
        if (current->p_priority > 0)
             // Preempt lower-priority process
             reschedule();
    } else {
        // Let's hope it's not urgent!
        pdst->p_pending = 1;
    }
}

/* All interrupts are handled by this common handler, which disables the
   interrupt temporarily, then sends or queues a message to the registered
   handler task.  Normally the handler task will deal with the cause of the
   interrupt, then re-enable it using reconnect(). */

/* phos_interrupt -- handle an interrupt */
void phos_interrupt(int irq) {
    int task;

    if (irq < 0 || (task = handler[irq]) == 0)
        panic("Unexpected interrupt %d", irq);
    disable_irq(irq);
    interrupt(task);
}

/* hardfault_handler -- substitutes for the definition in startup.c */
void hardfault_handler(void) {
    panic("HardFault");
}


/* INITIALISATION */

#define IDLE_STACK 128

static void init_ptable(struct proc *p, int pid, char *name, unsigned stksize) {
    unsigned char *stack = sbrk(stksize);
    unsigned *sp = (unsigned *) &stack[stksize];

    /* Blank out the stack space to help detect overflow */
    for (unsigned *p = (unsigned *) stack; p < sp; p++) *p = BLANK;

    p->p_pid = pid;
    strncpy(p->p_name, name, 15);
    p->p_name[15] = '\0';
    p->p_sp = sp;
    p->p_stack = stack;
    p->p_stksize = stksize;
    p->p_state = READY;
    p->p_priority = P_LOW;
    p->p_waiting = 0;
    p->p_pending = 0;
    p->p_accept = ANY;
    p->p_timeout = NO_TIME;
    p->p_message = NULL;
    p->p_next = NULL;
}

/* phos_init -- set up initial values */
void phos_init(void) {
    // Create idle task as process 0
    idle_proc = &ptable[IDLE];
    init_ptable(idle_proc, IDLE, "IDLE", IDLE_STACK);
    idle_proc->p_state = IDLING;
    idle_proc->p_priority = 3;
}

#define INIT_PSR 0x01000000     /* Thumb bit is set */

// These match the frame layout in mpx.s, and the hardware
#define R0_SAVE 8
#define R1_SAVE 9
#define R2_SAVE 10
#define LR_SAVE 13
#define PC_SAVE 14
#define PSR_SAVE 15

#define roundup(x, n) (((x) + ((n)-1)) & ~((n)-1))

/* start -- initialise process to run later */
void start(int pid, char *name, void (*body)(int), int arg, int stksize) {
    struct proc *p = &ptable[pid];

    if (current != NULL)
         panic("start() called after scheduler startup");

    if (p->p_state != DEAD)
         panic("pid for process %s is already taken", name);

    init_ptable(p, pid, name, roundup(stksize, 4));

    /* Fake an exception frame */
    unsigned *sp = p->p_sp - 16;
    memset(sp, 0, 64);
    sp[PSR_SAVE] = INIT_PSR;
    sp[PC_SAVE] = (unsigned) body & ~0x1; // Activate the process body
    sp[LR_SAVE] = (unsigned) exit; // Make it return to exit()
    sp[R0_SAVE] = (unsigned) arg;  // Pass the supplied argument in R0
    p->p_sp = sp;

    make_ready(p);
}

/* setstack -- enter thread mode with specified stack */
void setstack(unsigned *sp);

/* phos_start -- start up the process scheduler */
void phos_start(void) {
    // The main program morphs into the idle process.  The intial stack
    // becomes the kernel stack, and the idle process gets its own small
    // stack.

    current = idle_proc;
    setstack(current->p_sp);
    yield();                    // Pick a real process to run

    // Idle only runs again when there's nothing to do.
    while (1) {
        pause();                // Wait for an interrupt
    }
}


/* SYSTEM CALL INTERFACE */

// System call numbers
#define SYS_YIELD 0
#define SYS_SEND 1
#define SYS_RECEIVE 2
#define SYS_SENDREC 3
#define SYS_EXIT 4
#define SYS_DUMP 5
#define SYS_TICK 6

/* system_call -- entry from system call traps */
unsigned *system_call(unsigned *psp) {
    short *pc = (short *) psp[PC_SAVE]; // Program counter
    int op = pc[-1] & 0xff;      // Syscall number from svc instruction
    int x = psp[R0_SAVE];        // PID or IRQ or prio from r0
    message *m = (message *) psp[R1_SAVE];  // Message pointer from r1
    int t = psp[R2_SAVE];        // Timeout from r2

    // Save sp of the current process
    current->p_sp = psp;

    switch (op) {
    case SYS_YIELD:
         make_ready(current);
         choose_proc();
         break;

    case SYS_SEND:
         mini_send(x, m);
         break;

    case SYS_RECEIVE:
         mini_receive(x, m, t);
         break;

    case SYS_SENDREC:
         mini_sendrec(x, m);
         break;

    case SYS_TICK:
         mini_tick(x);
         break;

    case SYS_EXIT:
        current->p_state = DEAD;
        choose_proc();
        break;

    case SYS_DUMP:
        /* Invoking phos_dump as a system call means that its own
           stack space is taken from the system stack rather than the
           stack of the current process. */
        phos_dump();
        break;

    default:
        panic("Unknown syscall %d", op);
    }

    // Return sp for next process to run
    return current->p_sp;
}

/* cxtswitch -- handler for PendSV trap */
unsigned *cxtswitch(unsigned *psp) {
     current->p_sp = psp;
     make_ready(current);
     choose_proc();
     return current->p_sp;
}


/* SYSTEM CALL STUBS */

/* Each function defined here leaves its arguments in r0 and r1 and
   executes an svc instruction with operand equal to the system call
   number.  After state has been saved, system_call() is invoked and
   retrieves the call number and arguments from the exception frame.
   Calls to these functions must not be inlined, or the arguments will
   not be found in the right places. */

#define NOINLINE __attribute((noinline))

void NOINLINE yield(void) {
     syscall(SYS_YIELD);
}

void NOINLINE send(int dst, message *msg) {
     syscall(SYS_SEND);
}

void NOINLINE receive_t(int src, message *msg, int timeout) {
     syscall(SYS_RECEIVE);
}

void NOINLINE sendrec(int dst, message *msg) {
     syscall(SYS_SENDREC);
}

void NOINLINE exit(void) {
     syscall(SYS_EXIT);
}

void NOINLINE dump(void) {
     syscall(SYS_DUMP);
}

void NOINLINE tick(int ms) {
     syscall(SYS_TICK);
}


/* DEBUG PRINTING */

/* The routines here work by disabling interrupts and then polling: they
   should be used only for debugging. */

static int txinit;                     // UART not transmitting

/* delay_usec -- delay loop */
static void delay_usec(int usec) {
    int t = usec<<1;
    while (t > 0) {
        // 500nsec per iteration
        nop(); nop(); nop();
        t--;
    }
}
        
/* kprintf_setup -- set up UART connection to host */
static void kprintf_setup(void) {
    // Delay so any UART activity can cease
    delay_usec(2000);

    // Reconfigure the UART just to be sure
    UART_ENABLE = 0;

    GPIO_DIRSET = BIT(USB_TX);
    GPIO_DIRCLR = BIT(USB_RX);
    SET_FIELD(GPIO_PINCNF[USB_TX], GPIO_PINCNF_PULL, GPIO_Pullup);
    SET_FIELD(GPIO_PINCNF[USB_RX], GPIO_PINCNF_PULL, GPIO_Pullup);

    UART_BAUDRATE = UART_BAUD_9600;     // 9600 baud
    UART_CONFIG = 0;                    // format 8N1
    UART_PSELTXD = USB_TX;              // choose pins
    UART_PSELRXD = USB_RX;
    UART_ENABLE = UART_Enabled;
    UART_STARTTX = 1;
    UART_STARTRX = 1;
    UART_RXDRDY = 0;

    txinit = 1;
}

/* kputc -- send output character */
static void kputc(char ch) {
    if (! txinit) {
        while (! UART_TXDRDY) { }
    }
    UART_TXD = ch;
    UART_TXDRDY = 0;
    txinit = 0;
}

/* kprintf_internal -- internal version of kprintf */
static void kprintf_internal(char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    do_print(kputc, fmt, va);
    va_end(va);
}

/* kprintf -- printf variant for debugging (disables interrupts) */
void kprintf(char *fmt, ...) {
    va_list va;

    lock();
    kprintf_setup();

    va_start(va, fmt);
    do_print(kputc, fmt, va);
    va_end(va);

    restore();
    // Caller gets a UART interrupt if enabled.
}

/* panic -- the unusual has happened.  Did you think it impossible? */
void panic(char *fmt, ...) {
    va_list va;
     
    lock();
    kprintf_setup();     

    kprintf_internal("\r\nPanic: ");
    va_start(va, fmt);
    do_print(kputc, fmt, va);
    va_end(va);
    if (current != NULL)
         kprintf_internal(" in process %s", current->p_name);
    kprintf_internal("\r\n");

    spin();
}

/* badmesg -- default case for switches on message type */
void badmesg(int type) {
     panic("Bad message type %d", type);
}
