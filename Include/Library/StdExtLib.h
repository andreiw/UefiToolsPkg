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

#ifndef _STD_EXT_LIB_H_
#define _STD_EXT_LIB_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <string.h>

/*
 * StdLib/Include/sys/stat.h defines this incorrectly. Sigh.
 */
#undef S_ISCHR
#define S_ISCHR(m) (((m & _S_IFMT) == _S_IFCHR) ||                     \
                    ((m & _S_IFMT) == (_S_IFCHR | S_ITTY)) ||          \
                    ((m & _S_IFMT) == (_S_IFCHR | S_ITTY | S_IWTTY)))  \

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

#endif /* _STD_EXT_LIB_H_ */
