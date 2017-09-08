/*	$NetBSD: print.c,v 1.51.2.2 2014/08/19 23:45:11 tls Exp $	*/

/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Michael Fischbein.
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

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)print.c	8.5 (Berkeley) 7/28/94";
#else
__RCSID("$NetBSD: print.c,v 1.51.2.2 2014/08/19 23:45:11 tls Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <Library/FTSLib.h>
/* #include <grp.h> */
/* #include <pwd.h> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
/* #include <tzfile.h> */
#define SECSPERMIN  60
#define MINSPERHOUR 60
#define HOURSPERDAY 24
#define DAYSPERWEEK 7
#define DAYSPERNYEAR  365
#define DAYSPERLYEAR  366
#define SECSPERHOUR (SECSPERMIN * MINSPERHOUR)
#define SECSPERDAY  ((LONG32)(SECSPERHOUR * HOURSPERDAY))
#define MONSPERYEAR 12
#include <unistd.h>
/* #include <util.h> */

#include "ls.h"
#include "extern.h"

extern int termwidth;

static int	printaname(FTSENT *, int, int);
/* static void	printlink(FTSENT *); */
static void	printtime(time_t);
static void	printtotal(DISPLAY *dp);
static int	printtype(u_int);

static time_t	now;

#define	IS_NOPRINT(p)	((p)->fts_number == NO_PRINT)


#define HN_DECIMAL      0x01
#define HN_NOSPACE      0x02
#define HN_B            0x04
#define HN_DIVISOR_1000 0x08

#define HN_GETSCALE     0x10
#define HN_AUTOSCALE    0x20

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

void
strmode(mode, p)
     mode_t mode;
     char *p;
{

  _DIAGASSERT(p != NULL);

  /* print type */
  switch (mode & S_IFMT) {
  case S_IFDIR:/* directory */
    *p++ = 'd';
    break;
  case S_IFCHR | S_ITTY:
  case S_IFCHR | S_ITTY | S_IWTTY:
  case S_IFCHR:/* character special */
    *p++ = 'c';
    break;
  case S_IFBLK:/* block special */
    *p++ = 'b';
    break;
  case S_IFREG:/* regular */
    #ifdef S_ARCH2
    if ((mode & S_ARCH2) != 0) {
      *p++ = 'A';
    } else if ((mode & S_ARCH1) != 0) {
      *p++ = 'a';
    } else {
      #endif
      *p++ = '-';
      #ifdef S_ARCH2
    }
    #endif
    break;
#ifdef S_IFLNK    
  case S_IFLNK:/* symbolic link */
    *p++ = 'l';
    break;
#endif
#ifdef S_IFSOCK
  case S_IFSOCK:/* socket */
    *p++ = 's';
    break;
    #endif
    #ifdef S_IFIFO
  case S_IFIFO:/* fifo */
    *p++ = 'p';
    break;
    #endif
    #ifdef S_IFWHT
  case S_IFWHT:/* whiteout */
    *p++ = 'w';
    break;
    #endif
    #ifdef S_IFDOOR
  case S_IFDOOR:/* door */
    *p++ = 'D';
    break;
    #endif
  default:/* unknown */
    *p++ = '?';
    break;
  }
  /* usr */
  if (mode & S_IRUSR)
    *p++ = 'r';
  else
    *p++ = '-';
  if (mode & S_IWUSR)
    *p++ = 'w';
  else
    *p++ = '-';
  switch (mode & (S_IXUSR | S_ISUID)) {
  case 0:
    *p++ = '-';
    break;
  case S_IXUSR:
    *p++ = 'x';
    break;
  case S_ISUID:
    *p++ = 'S';
    break;
  case S_IXUSR | S_ISUID:
    *p++ = 's';
    break;
  }
  /* group */
  if (mode & S_IRGRP)
    *p++ = 'r';
  else
    *p++ = '-';
  if (mode & S_IWGRP)
    *p++ = 'w';
  else
    *p++ = '-';
  switch (mode & (S_IXGRP | S_ISGID)) {
  case 0:
    *p++ = '-';
    break;
  case S_IXGRP:
    *p++ = 'x';
    break;
  case S_ISGID:
    *p++ = 'S';
    break;
  case S_IXGRP | S_ISGID:
    *p++ = 's';
    break;
  }
  /* other */
  if (mode & S_IROTH)
    *p++ = 'r';
  else
    *p++ = '-';
  if (mode & S_IWOTH)
    *p++ = 'w';
  else
    *p++ = '-';
  switch (mode & (S_IXOTH /* | S_ISVTX */)) {
  case 0:
    *p++ = '-';
    break;
  case S_IXOTH:
    *p++ = 'x';
    break;
  /* case S_ISVTX: */
  /*   *p++ = 'T'; */
  /*   break; */
  /* case S_IXOTH | S_ISVTX: */
  /*   *p++ = 't'; */
  /*   break; */
  }
  *p++ = ' ';/* will be a '+' if ACL's implemented */
  *p = '\0';
}

/* static int */
/* safe_printpath(const FTSENT *p) { */
/* 	int chcnt; */

/* 	if (f_fullpath) { */
/* 		chcnt = safe_print(p->fts_path); */
/* 		chcnt += safe_print("/"); */
/* 	} else */
/* 		chcnt = 0; */
/* 	return chcnt + safe_print(p->fts_name); */
/* } */

/* static int */
/* printescapedpath(const FTSENT *p) { */
/* 	int chcnt; */

/* 	if (f_fullpath) { */
/* 		chcnt = printescaped(p->fts_path); */
/* 		chcnt += printescaped("/"); */
/* 	} else */
/* 		chcnt = 0; */

/* 	return chcnt + printescaped(p->fts_name); */
/* } */

static int
printpath(const FTSENT *p) {
	if (f_fullpath)
		return printf("%s/%s", p->fts_path, p->fts_name);
	else
		return printf("%s", p->fts_name);
}

void
printscol(DISPLAY *dp)
{
	FTSENT *p;

	for (p = dp->list; p; p = p->fts_link) {
		if (IS_NOPRINT(p))
			continue;
		(void)printaname(p, dp->s_inode, dp->s_block);
		(void)putchar('\n');
	}
}

void
printlong(DISPLAY *dp)
{
	struct stat *sp;
	FTSENT *p;
	NAMES *np;
	char buf[20], szbuf[5];

	now = time(NULL);

	if (!f_leafonly)
		printtotal(dp);		/* "total: %u\n" */
	
	for (p = dp->list; p; p = p->fts_link) {
		if (IS_NOPRINT(p))
			continue;
		sp = p->fts_statp;
		/* if (f_inode) */
		/* 	(void)printf("%*"PRIu64" ", dp->s_inode, sp->st_ino); */
		if (f_size) {
			if (f_humanize) {
				if ((humanize_number(szbuf, sizeof(szbuf),
				    sp->st_physsize,
				    "", HN_AUTOSCALE,
				    (HN_DECIMAL | HN_B | HN_NOSPACE))) == -1)
					err(1, "humanize_number");
				(void)printf("%*s ", dp->s_block, szbuf);
			} else {
                                int blocks = howmany(sp->st_physsize, S_BLKSIZE);

				(void)printf(f_commas ? "%'*llu " : "%*llu ",
				    dp->s_block,
                                    (unsigned long long)howmany(blocks,
				    blocksize));
			}
		}
		(void)strmode(sp->st_mode, buf);
		np = p->fts_pointer;
		(void)printf("%s %*lu ", buf, dp->s_nlink,
                             (unsigned long) /* sp->st_nlink */ 1);
		if (!f_grouponly)
			(void)printf("%-*s  ", dp->s_user, np->user);
		(void)printf("%-*s  ", dp->s_group, np->group);
		/* if (f_flags) */
		/* 	(void)printf("%-*s ", dp->s_flags, np->flags); */
		if (S_ISCHR(sp->st_mode) || S_ISBLK(sp->st_mode))
			(void)printf("%*lld, %*lld ",
			    dp->s_major, (long long) 0 /* major(sp->st_rdev) */,
			    dp->s_minor, (long long) 0 /* minor(sp->st_rdev) */);
		else {
			if (f_humanize) {
				if ((humanize_number(szbuf, sizeof(szbuf),
				    sp->st_size, "", HN_AUTOSCALE,
				    (HN_DECIMAL | HN_B | HN_NOSPACE))) == -1)
					err(1, "humanize_number");
				(void)printf("%*s ", dp->s_size, szbuf);
			} else {
				(void)printf(f_commas ? "%'*llu " : "%*llu ", 
				    dp->s_size, (unsigned long long)
				    sp->st_size);
			}
                }
		if (f_accesstime)
			printtime(sp->st_atime);
		/* else if (f_statustime) */
		/* 	printtime(sp->st_ctime); */
		else
			printtime(sp->st_mtime);
		/* if (f_octal || f_octal_escape) */
		/* 	(void)safe_printpath(p); */
		/* else if (f_nonprint) */
		/* 	(void)printescapedpath(p); */
		/* else */
			(void)printpath(p);

		if (f_type || (f_typedir && S_ISDIR(sp->st_mode)))
			(void)printtype(sp->st_mode);
		/* if (S_ISLNK(sp->st_mode)) */
		/* 	printlink(p); */
		(void)putchar('\n');
	}
}

void
printcol(DISPLAY *dp)
{
	static FTSENT **array;
	static int lastentries = -1;
	FTSENT *p;
	int base, chcnt, col, colwidth, num;
	int numcols, numrows, row;

	colwidth = dp->maxlen;
	/* if (f_inode) */
	/* 	colwidth += dp->s_inode + 1; */
	if (f_size) {
		if (f_humanize)
			colwidth += dp->s_size + 1;
		else
			colwidth += dp->s_block + 1;
	}
	if (f_type || f_typedir)
		colwidth += 1;

	colwidth += 1;

	if (termwidth < 2 * colwidth) {
		printscol(dp);
		return;
	}

	/*
	 * Have to do random access in the linked list -- build a table
	 * of pointers.
	 */
	if (dp->entries > lastentries) {
		FTSENT **newarray;

		newarray = realloc(array, dp->entries * sizeof(FTSENT *));
		if (newarray == NULL) {
			warn(NULL);
			printscol(dp);
			return;
		}
		lastentries = dp->entries;
		array = newarray;
	}
	for (p = dp->list, num = 0; p; p = p->fts_link)
		if (p->fts_number != NO_PRINT)
			array[num++] = p;

	numcols = termwidth / colwidth;
	colwidth = termwidth / numcols;		/* spread out if possible */
	numrows = num / numcols;
	if (num % numcols)
		++numrows;

	printtotal(dp);				/* "total: %u\n" */

	for (row = 0; row < numrows; ++row) {
		for (base = row, chcnt = col = 0; col < numcols; ++col) {
			chcnt = printaname(array[base], dp->s_inode,
			    f_humanize ? dp->s_size : dp->s_block);
			if ((base += numrows) >= num)
				break;
			while (chcnt++ < colwidth)
				(void)putchar(' ');
		}
		(void)putchar('\n');
	}
}

void
printacol(DISPLAY *dp)
{
	FTSENT *p;
	int chcnt, col, colwidth;
	int numcols;

	colwidth = dp->maxlen;
	/* if (f_inode) */
	/* 	colwidth += dp->s_inode + 1; */
	if (f_size) {
		if (f_humanize)
			colwidth += dp->s_size + 1;
		else
			colwidth += dp->s_block + 1;
	}
	if (f_type || f_typedir)
		colwidth += 1;

	colwidth += 1;

	if (termwidth < 2 * colwidth) {
		printscol(dp);
		return;
	}

	numcols = termwidth / colwidth;
	colwidth = termwidth / numcols;		/* spread out if possible */

	printtotal(dp);				/* "total: %u\n" */

	chcnt = col = 0;
	for (p = dp->list; p; p = p->fts_link) {
		if (IS_NOPRINT(p))
			continue;
		if (col >= numcols) {
			chcnt = col = 0;
			(void)putchar('\n');
		}
		chcnt = printaname(p, dp->s_inode,
		    f_humanize ? dp->s_size : dp->s_block);
		while (chcnt++ < colwidth)
			(void)putchar(' ');
		col++;
	}
	(void)putchar('\n');
}

void
printstream(DISPLAY *dp)
{
	FTSENT *p;
	int col;
	int extwidth;

	extwidth = 0;
	/* if (f_inode) */
	/* 	extwidth += dp->s_inode + 1; */
	if (f_size) {
		if (f_humanize)
			extwidth += dp->s_size + 1;
		else
			extwidth += dp->s_block + 1;
	}
	if (f_type)
		extwidth += 1;

	for (col = 0, p = dp->list; p != NULL; p = p->fts_link) {
		if (IS_NOPRINT(p))
			continue;
		if (col > 0) {
			(void)putchar(','), col++;
			if (col + 1 + extwidth + (int)p->fts_namelen >= termwidth)
				(void)putchar('\n'), col = 0;
			else
				(void)putchar(' '), col++;
		}
		col += printaname(p, dp->s_inode,
		    f_humanize ? dp->s_size :dp->s_block);
	}
	(void)putchar('\n');
}

/*
 * print [inode] [size] name
 * return # of characters printed, no trailing characters.
 */
static int
printaname(FTSENT *p, int inodefield, int sizefield)
{
	struct stat *sp;
	int chcnt;
	char szbuf[5];

	sp = p->fts_statp;
	chcnt = 0;
	/* if (f_inode) */
	/* 	chcnt += printf("%*"PRIu64" ", inodefield, sp->st_ino); */
	if (f_size) {
		if (f_humanize) {
			if ((humanize_number(szbuf, sizeof(szbuf), sp->st_size,
			    "", HN_AUTOSCALE,
			    (HN_DECIMAL | HN_B | HN_NOSPACE))) == -1)
				err(1, "humanize_number");
			chcnt += printf("%*s ", sizefield, szbuf);
		} else {
                        int blocks = howmany(sp->st_physsize, S_BLKSIZE);

			chcnt += printf(f_commas ? "%'*llu " : "%*llu ",
			    sizefield, (unsigned long long)
			    howmany(blocks, blocksize));
		}
	}
	/* if (f_octal || f_octal_escape) */
	/* 	chcnt += safe_printpath(p); */
	/* else if (f_nonprint) */
	/* 	chcnt += printescapedpath(p); */
	/* else */
		chcnt += printpath(p);
	if (f_type || (f_typedir && S_ISDIR(sp->st_mode)))
		chcnt += printtype(sp->st_mode);
	return (chcnt);
}

static void
printtime(time_t ftime)
{
	int i;
	const char *longstring;

	if ((longstring = ctime(&ftime)) == NULL) {
			   /* 012345678901234567890123 */
		longstring = "????????????????????????";
	}
	for (i = 4; i < 11; ++i)
		(void)putchar(longstring[i]);

#define	SIXMONTHS	((DAYSPERNYEAR / 2) * SECSPERDAY)
	if (f_sectime)
		for (i = 11; i < 24; i++)
			(void)putchar(longstring[i]);
	else if (ftime + SIXMONTHS > now && ftime - SIXMONTHS < now)
		for (i = 11; i < 16; ++i)
			(void)putchar(longstring[i]);
	else {
		(void)putchar(' ');
		for (i = 20; i < 24; ++i)
			(void)putchar(longstring[i]);
	}
	(void)putchar(' ');
}

/*
 * Display total used disk space in the form "total: %u\n".
 * Note: POSIX (IEEE Std 1003.1-2001) says this should be always in 512 blocks,
 * but we humanise it with -h, or separate it with commas with -M, and use 1024
 * with -k.
 */
static void
printtotal(DISPLAY *dp)
{
	char szbuf[5];
	
	if (dp->list->fts_level != FTS_ROOTLEVEL && (f_longform || f_size)) {
		if (f_humanize) {
			if ((humanize_number(szbuf, sizeof(szbuf), (int64_t)dp->stotal,
			    "", HN_AUTOSCALE,
			    (HN_DECIMAL | HN_B | HN_NOSPACE))) == -1)
				err(1, "humanize_number");
			(void)printf("total %s\n", szbuf);
		} else {
			(void)printf(f_commas ? "total %'llu\n" :
			    "total %llu\n", (unsigned long long)
			    howmany(dp->btotal, blocksize));
		}
	}
}

static int
printtype(u_int mode)
{
	switch (mode & S_IFMT) {
	case S_IFDIR:
		(void)putchar('/');
		return (1);
	case S_IFIFO:
		(void)putchar('|');
		return (1);
	/* case S_IFLNK: */
	/* 	(void)putchar('@'); */
	/* 	return (1); */
	case S_IFSOCK:
		(void)putchar('=');
		return (1);
	/* case S_IFWHT: */
	/* 	(void)putchar('%'); */
	/* 	return (1); */
	}
	if (mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
		(void)putchar('*');
		return (1);
	}
	return (0);
}

/* static void */
/* printlink(FTSENT *p) */
/* { */
/* 	int lnklen; */
/* 	char name[MAXPATHLEN + 1], path[MAXPATHLEN + 1]; */

/* 	if (p->fts_level == FTS_ROOTLEVEL) */
/* 		(void)snprintf(name, sizeof(name), "%s", p->fts_name); */
/* 	else */
/* 		(void)snprintf(name, sizeof(name), */
/* 		    "%s/%s", p->fts_parent->fts_accpath, p->fts_name); */
/* 	if ((lnklen = readlink(name, path, sizeof(path) - 1)) == -1) { */
/* 		(void)fprintf(stderr, "\nls: %s: %s\n", name, strerror(errno)); */
/* 		return; */
/* 	} */
/* 	path[lnklen] = '\0'; */
/* 	(void)printf(" -> "); */
/* 	if (f_octal || f_octal_escape) */
/* 		(void)safe_print(path); */
/* 	else if (f_nonprint) */
/* 		(void)printescaped(path); */
/* 	else */
/* 		(void)printf("%s", path); */
/* } */
