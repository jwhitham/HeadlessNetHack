/*	SCCS Id: @(#)ioctl.c	3.4	1990/22/02 */
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/* This cannot be part of hack.tty.c (as it was earlier) since on some
   systems (e.g. MUNIX) the include files <termio.h> and <sgtty.h>
   define the same constants, and the C preprocessor complains. */

#include "hack.h"

#if defined(BSD_JOB_CONTROL) || defined(_BULL_SOURCE)
# ifdef HPUX
#include <bsdtty.h>
# else
#  if defined(AIX_31) && !defined(_ALL_SOURCE)
#   define _ALL_SOURCE	/* causes struct winsize to be present */
#   ifdef _AIX32
#    include <sys/ioctl.h>
#   endif
#  endif
#  if defined(_BULL_SOURCE)
#   include <termios.h>
struct termios termio;
#   undef TIMEOUT		/* defined in you.h and sys/tty.h */
#   include <sys/tty.h>		/* define winsize */
#   include <sys/ttold.h>	/* define struct ltchars */
#   include <sys/bsdioctl.h>	/* define TIOGWINSZ */
#  else
#   ifdef LINUX
#    include <bsd/sgtty.h>
#   else
#    include <sgtty.h>
#   endif
#  endif
# endif
struct ltchars ltchars;
struct ltchars ltchars0 = { -1, -1, -1, -1, -1, -1 }; /* turn all off */
#else

# ifdef POSIX_TYPES
#include <termios.h>
struct termios termio;
#  if defined(BSD) || defined(_AIX32)
#   if defined(_AIX32) && !defined(_ALL_SOURCE)
#    define _ALL_SOURCE
#   endif
#include <sys/ioctl.h>
#  endif
# else
#include <termio.h>	/* also includes part of <sgtty.h> */
#  if defined(TCSETS) && !defined(AIX_31)
struct termios termio;
#  else
struct termio termio;
#  endif
# endif
# ifdef AMIX
#include <sys/ioctl.h>
# endif /* AMIX */
#endif

#ifdef SUSPEND	/* BSD isn't alone anymore... */
#include	<signal.h>
#endif

#if defined(TIOCGWINSZ) && (defined(BSD) || defined(ULTRIX) || defined(AIX_31) || defined(_BULL_SOURCE) || defined(SVR4))
#define USE_WIN_IOCTL
#include "tcap.h"	/* for LI and CO */
#endif

#ifdef _M_UNIX
extern void NDECL(sco_mapon);
extern void NDECL(sco_mapoff);
#endif
#ifdef __linux__
extern void NDECL(linux_mapon);
extern void NDECL(linux_mapoff);
#endif

#ifdef AUX
void
catch_stp()
{
    signal(SIGTSTP, SIG_DFL);
    dosuspend();
}
#endif /* AUX */

void
getwindowsz()
{
    LI = 25;
    CO = 80;
}

void
getioctls()
{
	getwindowsz();
}

void
setioctls()
{
}

#ifdef SUSPEND		/* No longer implies BSD */
int
dosuspend()
{
	return(0);
}
#endif /* SUSPEND */
