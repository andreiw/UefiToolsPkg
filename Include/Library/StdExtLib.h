/*
 * Copyright (C) 2017 Andrei Evgenievich Warkentin
 *
 * This program and the accompanying materials
 * are licensed and made available under the terms and conditions of the BSD License
 * which accompanies this distribution.  The full text of the license may be found at
 * http://opensource.org/licenses/bsd-license.php
 *
 * THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 * WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
 */

/*
 * Copyright (c) 1989, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Dieter Baron and Thomas Klausner.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _STD_EXT_LIB_H_
#define _STD_EXT_LIB_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>

/*
 * StdLib/Include/sys/stat.h defines this incorrectly. Sigh.
 */
#undef S_ISCHR
#define S_ISCHR(m) (((m & _S_IFMT) == _S_IFCHR) ||                     \
                    ((m & _S_IFMT) == (_S_IFCHR | S_ITTY)) ||          \
                    ((m & _S_IFMT) == (_S_IFCHR | S_ITTY | S_IWTTY)))  \

#define __arraycount(__x) (sizeof(__x) / sizeof(__x[0]))

void strmode(mode_t mode, char *p);

int toascii(int c);

#define HN_DECIMAL      0x01
#define HN_NOSPACE      0x02
#define HN_B            0x04
#define HN_DIVISOR_1000 0x08

#define HN_GETSCALE     0x10
#define HN_AUTOSCALE    0x20

int humanize_number(char *buf, size_t len, int64_t bytes,
                    const char *suffix, int scale, int flags);

long long
strsuftollx(const char *desc, const char *val,
            long long min, long long max, char *ebuf, size_t ebuflen);

long long
strsuftoll(const char *desc, const char *val,
           long long min, long long max);

void
swab(const void * __restrict from, void * __restrict to, ssize_t len);

int
reallocarr(void *ptr, size_t number, size_t size);

#define _POSIX2_RE_DUP_MAX 255

/*
 * Gnu like getopt_long() and BSD4.4 getsubopt()/optreset extensions
 */
#define no_argument        0
#define required_argument  1
#define optional_argument  2

struct option {
  /* name of long option */
  const char *name;
  /*
   * one of no_argument, required_argument, and optional_argument:
   * whether option takes an argument
   */
  int has_arg;
  /* if not NULL, set *flag to val when option found */
  int *flag;
  /* if flag not NULL, value to set *flag to; else return value */
  int val;
};

int getopt_long(int, char * const *, const char *,
                const struct option *, int *);

ssize_t
getdelim(char **buf, size_t *bufsiz, int delimiter, FILE *fp);

ssize_t
getline(char **buf, size_t *bufsiz, FILE *fp);

#define FNM_NOMATCH 1 /* Match failed. */
#define FNM_NOSYS   2 /* Function not implemented. */
#define FNM_NORES   3 /* Out of resources */

#define FNM_NOESCAPE 0x01    /* Disable backslash escaping. */
#define FNM_PATHNAME 0x02    /* Slash must be matched by slash. */
#define FNM_PERIOD 0x04      /* Period must be matched by period. */
#define FNM_CASEFOLD 0x08    /* Pattern is matched case-insensitive */
#define FNM_LEADING_DIR 0x10 /* Ignore /<tail> after Imatch. */

int fnmatch(const char *, const char *, int);

#endif /* _STD_EXT_LIB_H_ */
