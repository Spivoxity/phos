/*
 * lib.c
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

#include "lib.h"

#define NMAX 16                 // Max digits in a printed number

/* utoa -- convert unsigned to decimal or hex */
static char *utoa(unsigned x, unsigned base, char *nbuf) {
     char *p = &nbuf[NMAX];
     const char *hex = "0123456789abcdef";

     *--p = '\0';
     do {
          *--p = hex[x % base];
          x = x / base;
     } while (x != 0);
     
     return p;
}

/* itoa -- convert signed integer to decimal */
static char *itoa(int v, char *nbuf) {
     if (v >= 0)
          return utoa(v, 10, nbuf);
     else {
          char *p = utoa(-v, 10, nbuf);
          *--p = '-';
          return p;
     }
}

/* atoi -- convert decimal string to integer */
int atoi(const char *p) {
     unsigned x = 0;
     int minus = 0;

     if (*p == '-') {
         minus = 1; p++;
     }

     while (*p >= '0' && *p <= '9')
          x = 10 * x + (*p++ - '0');

     if (minus)
         return -x;
     else
         return x;
}

/* xtou -- convert hex string to unsigned */
unsigned xtou(char *p) {
    unsigned x = 0;

    for (;;) {
        if (*p >= '0' && *p <= '9')
            x = (x << 4) + (*p++ - '0');
        else if (*p >= 'a' && *p <= 'f')
            x = (x << 4) + (*p++ - 'a' + 10);
        else if (*p >= 'A' && *p <= 'F')
            x = (x << 4) + (*p++ - 'A' + 10);
        else
            break;
    }

    return x;
}


/* We need sprintf to be thread-safe, so a trick is needed: in
   _do_print, the parameter q is either a function to call or the
   address of a character pointer used with sprintf. */

/* do_string -- output or buffer each character of a string */
static void do_string(void (*fputc)(void *, char), void *q, char *str) {
     for (char *p = str; *p != '\0'; p++)
          fputc(q, *p);
}

/* _do_print -- the guts of printf */
void _do_print(void (*fputc)(void *, char), void *q,
               const char *fmt, va_list va) {
    int x;
    char nbuf[NMAX];

    for (const char *p = fmt; *p != 0; p++) {
        if (*p == '%' && *(p+1) != '\0') {
            switch (*++p) {
            case 'c':
                fputc(q, va_arg(va, int));
                break;
            case 'd':
                do_string(fputc, q, itoa(va_arg(va, int), nbuf));
                break;
            case 's':
                do_string(fputc, q, va_arg(va, char *));
                break;
            case 'u':
                do_string(fputc, q, utoa(va_arg(va, unsigned), 10, nbuf));
                break;
            case 'x':
                x = va_arg(va, unsigned);
                if (x == 0)
                    fputc(q, '0');
                else {
                    fputc(q, '0'); fputc(q, 'x');
                    do_string(fputc, q, utoa(x, 16, nbuf));
                }
                break;
            default:
                fputc(q, *p);
                break;
            }
        } else {
            fputc(q, *p);
        }
    }
}     

/* f_printc -- call q as a function */
static void f_printc(void *q, char c) {
    void (*f)(char) = q;
    f(c);
}

/* f_storec -- use q as a character pointer */
void f_storec(void *p, char c) {
    char **q = (char **) p;
    *(*q)++ = c;
}

/* do_print -- public skeleton for printf */
void do_print(void (*putc)(char), const char *fmt, va_list va) {
    _do_print(f_printc, (void *) putc, fmt, va);
}

/* sprintf -- print to a character array */
int sprintf(char *buf, const char *fmt, ...) {
     // Note the usual problem with buffer overflow
    char *p = buf;
    va_list va;
    va_start(va, fmt);
    _do_print(f_storec, (void *) &p, fmt, va);
    va_end(va);
    *p++ = '\0';
    return (p - buf);
}
