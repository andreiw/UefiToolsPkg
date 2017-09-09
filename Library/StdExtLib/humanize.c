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

#include <stdio.h>
#include <Library/StdExtLib.h>

int
humanize_number(char *buf, size_t len, int64_t bytes,
                const char *suffix, int scale, int flags)
{
  const char *prefixes, *sep;
  int b, s1, s2, sign, r = 0;
  int64_t divisor, max, post = 1;
  size_t i, baselen, maxscale;
  int     getscale = 0;

  _DIAGASSERT(scale >= 0);

  if (flags & HN_DIVISOR_1000) {
    /* SI for decimal multiplies */
    divisor = 1000;
    if (flags & HN_B)
      prefixes = "B\0k\0M\0G\0T\0P\0E";
    else
      prefixes = "\0\0k\0M\0G\0T\0P\0E";
  } else {
    /*
     * binary multiplies
     * XXX IEC 60027-2 recommends Ki, Mi, Gi...
     */
    divisor = 1024;
    if (flags & HN_B)
      prefixes = "B\0K\0M\0G\0T\0P\0E";
    else
      prefixes = "\0\0K\0M\0G\0T\0P\0E";
  }

  if (scale & HN_GETSCALE)
    getscale = 1;

  if ((getscale) && (suffix == NULL))
    return (-1);

  if ((!getscale) && (buf == NULL))
    return (-1);

#define SCALE2PREFIX(scale) (&prefixes[(scale) << 1])
  maxscale = 7;

  if ((size_t)scale >= maxscale &&
      (scale & (HN_AUTOSCALE | HN_GETSCALE)) == 0)
    return (-1);

  if (buf == NULL || suffix == NULL)
    return (-1);

  if ((buf != NULL) && (len > 0))
    buf[0] = '\0';
  if (bytes < 0) {
    sign = -1;
    baselen = 3;/* sign, digit, prefix */
    if (-bytes < INT64_MAX / 100)
      bytes *= -100;
    else {
      bytes = -bytes;
      post = 100;
      baselen += 2;
    }
  } else {
    sign = 1;
    baselen = 2;/* digit, prefix */
    if (bytes < INT64_MAX / 100)
      bytes *= 100;
    else {
      post = 100;
      baselen += 2;
    }
  }
  if (flags & HN_NOSPACE)
    sep = "";
  else {
    sep = " ";
    baselen++;
  }
  if (suffix != NULL)
    baselen += strlen(suffix);

  /* Check if enough room for `x y' + suffix + `\0' */
  if (len < baselen + 1)
    return (-1);

  if (scale & (HN_AUTOSCALE | HN_GETSCALE)) {
    /* See if there is additional columns can be used. */
    for (max = 100, i = len - baselen; i-- > 0;)
      max *= 10;

    /*
     * Divide the number until it fits the given column.
     * If there will be an overflow by the rounding below,
     * divide once more.
     */
    for (i = 0; bytes >= max - 50 && i < maxscale; i++)
      bytes /= divisor;
  } else
    for (i = 0; i < (size_t)scale && i < maxscale; i++)
      bytes /= divisor;
  bytes *= post;

  /* If a value <= 9.9 after rounding and ... */
  if (bytes < 995 && i > 0 && flags & HN_DECIMAL) {
    /* baselen + \0 + .N */
    if (len < baselen + 1 + 2)
      return (-1);
    b = ((int)bytes + 5) / 10;
    s1 = b / 10;
    s2 = b % 10;
    if (suffix != NULL)
      r = snprintf(buf, len, "%d.%d%s%s%s",
                   sign * s1, s2,
                   sep, SCALE2PREFIX(i), suffix);
    else
      r = snprintf(buf, len, "%d.%d%s%s",
                   sign * s1, s2,
                   sep, SCALE2PREFIX(i));
  } else {
    if (suffix != NULL)
      r = snprintf(buf, len, "%lld%s%s%s",
                   (long long) (sign * ((bytes + 50) / 100)),
                   sep, SCALE2PREFIX(i), suffix);
    else
      r = snprintf(buf, len, "%lld%s%s",
                   (long long) (sign * ((bytes + 50) / 100)),
                   sep, SCALE2PREFIX(i));
  }

  if (getscale)
    return (int)i;
  else
    return (r);
}
