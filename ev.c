/*
 * libev event processing core, watcher management
 *
 * Copyright (c) 2007 Marc Alexander Lehmann <libev@schmorp.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EV_STANDALONE
# include "config.h"

# if HAVE_CLOCK_GETTIME
#  ifndef EV_USE_MONOTONIC
#   define EV_USE_MONOTONIC 1
#  endif
#  ifndef EV_USE_REALTIME
#   define EV_USE_REALTIME  1
#  endif
# endif

# if HAVE_SELECT && HAVE_SYS_SELECT_H && !defined (EV_USE_SELECT)
#  define EV_USE_SELECT 1
# endif

# if HAVE_POLL && HAVE_POLL_H && !defined (EV_USE_POLL)
#  define EV_USE_POLL 1
# endif

# if HAVE_EPOLL_CTL && HAVE_SYS_EPOLL_H && !defined (EV_USE_EPOLL)
#  define EV_USE_EPOLL 1
# endif

# if HAVE_KQUEUE && HAVE_SYS_EVENT_H && HAVE_SYS_QUEUE_H && !defined (EV_USE_KQUEUE)
#  define EV_USE_KQUEUE 1
# endif

# if HAVE_PORT_H && HAVE_PORT_CREATE && !defined (EV_USE_PORT)
#  define EV_USE_PORT 1
# endif

#endif

#include <math.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stddef.h>

#include <stdio.h>

#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>

#include <signal.h>

#ifndef _WIN32
# include <unistd.h>
# include <sys/time.h>
# include <sys/wait.h>
#else
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# ifndef EV_SELECT_IS_WINSOCKET
#  define EV_SELECT_IS_WINSOCKET 1
# endif
#endif

/**/

#ifndef EV_USE_MONOTONIC
# define EV_USE_MONOTONIC 0
#endif

#ifndef EV_USE_REALTIME
# define EV_USE_REALTIME 0
#endif

#ifndef EV_USE_SELECT
# define EV_USE_SELECT 1
#endif

#ifndef EV_USE_POLL
# ifdef _WIN32
#  define EV_USE_POLL 0
# else
#  define EV_USE_POLL 1
# endif
#endif

#ifndef EV_USE_EPOLL
# define EV_USE_EPOLL 0
#endif

#ifndef EV_USE_KQUEUE
# define EV_USE_KQUEUE 0
#endif

#ifndef EV_USE_PORT
# define EV_USE_PORT 0
#endif

/**/

/* darwin simply cannot be helped */
#ifdef __APPLE__
# undef EV_USE_POLL
# undef EV_USE_KQUEUE
#endif

#ifndef CLOCK_MONOTONIC
# undef EV_USE_MONOTONIC
# define EV_USE_MONOTONIC 0
#endif

#ifndef CLOCK_REALTIME
# undef EV_USE_REALTIME
# define EV_USE_REALTIME 0
#endif

#if EV_SELECT_IS_WINSOCKET
# include <winsock.h>
#endif

/**/

#define MIN_TIMEJUMP  1. /* minimum timejump that gets detected (if monotonic clock available) */
#define MAX_BLOCKTIME 59.743 /* never wait longer than this time (to detect time jumps) */
#define PID_HASHSIZE  16 /* size of pid hash table, must be power of two */
/*#define CLEANUP_INTERVAL (MAX_BLOCKTIME * 5.) /* how often to try to free memory and re-check fds */

#ifdef EV_H
# include EV_H
#else
# include "ev.h"
#endif

#if __GNUC__ >= 3
# define expect(expr,value)         __builtin_expect ((expr),(value))
# define inline                     static inline
#else
# define expect(expr,value)         (expr)
# define inline                     static
#endif

#define expect_false(expr) expect ((expr) != 0, 0)
#define expect_true(expr)  expect ((expr) != 0, 1)

#define NUMPRI    (EV_MAXPRI - EV_MINPRI + 1)
#define ABSPRI(w) ((w)->priority - EV_MINPRI)

#define EMPTY0      /* required for microsofts broken pseudo-c compiler */
#define EMPTY2(a,b) /* used to suppress some warnings */

typedef struct ev_watcher *W;
typedef struct ev_watcher_list *WL;
typedef struct ev_watcher_time *WT;

static int have_monotonic; /* did clock_gettime (CLOCK_MONOTONIC) work? */

#ifdef _WIN32
# include "ev_win32.c"
#endif

/*****************************************************************************/

static void (*syserr_cb)(const char *msg);

void ev_set_syserr_cb (void (*cb)(const char *msg))
{
  syserr_cb = cb;
}

static void
syserr (const char *msg)
{
  if (!msg)
    msg = "(libev) system error";

  if (syserr_cb)
    syserr_cb (msg);
  else
    {
      perror (msg);
      abort ();
    }
}

static void *(*alloc)(void *ptr, long size);

void ev_set_allocator (void *(*cb)(void *ptr, long size))
{
  alloc = cb;
}

static void *
ev_realloc (void *ptr, long size)
{
  ptr = alloc ? alloc (ptr, size) : realloc (ptr, size);

  if (!ptr && size)
    {
      fprintf (stderr, "libev: cannot allocate %ld bytes, aborting.", size);
      abort ();
    }

  return ptr;
}

#define ev_malloc(size) ev_realloc (0, (size))
#define ev_free(ptr)    ev_realloc ((ptr), 0)

/*****************************************************************************/

typedef struct
{
  WL head;
  unsigned char events;
  unsigned char reify;
#if EV_SELECT_IS_WINSOCKET
  SOCKET handle;
#endif
} ANFD;

typedef struct
{
  W w;
  int events;
} ANPENDING;

#if EV_MULTIPLICITY

  struct ev_loop
  {
    ev_tstamp ev_rt_now;
    #define ev_rt_now ((loop)->ev_rt_now)
    #define VAR(name,decl) decl;
      #include "ev_vars.h"
    #undef VAR
  };
  #include "ev_wrap.h"

  static struct ev_loop default_loop_struct;
  struct ev_loop *ev_default_loop_ptr;

#else

  ev_tstamp ev_rt_now;
  #define VAR(name,decl) static decl;
    #include "ev_vars.h"
  #undef VAR

  static int ev_default_loop_ptr;

#endif

/*****************************************************************************/

ev_tstamp
ev_time (void)
{
#if EV_USE_REALTIME
  struct timespec ts;
  clock_gettime (CLOCK_REALTIME, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
#else
  struct timeval tv;
  gettimeofday (&tv, 0);
  return tv.tv_sec + tv.tv_usec * 1e-6;
#endif
}

inline ev_tstamp
get_clock (void)
{
#if EV_USE_MONOTONIC
  if (expect_true (have_monotonic))
    {
      struct timespec ts;
      clock_gettime (CLOCK_MONOTONIC, &ts);
      return ts.tv_sec + ts.tv_nsec * 1e-9;
    }
#endif

  return ev_time ();
}

#if EV_MULTIPLICITY
ev_tstamp
ev_now (EV_P)
{
  return ev_rt_now;
}
#endif

#define array_roundsize(type,n) (((n) | 4) & ~3)

#define array_needsize(type,base,cur,cnt,init)			\
  if (expect_false ((cnt) > cur))				\
    {								\
      int newcnt = cur;						\
      do							\
        {							\
          newcnt = array_roundsize (type, newcnt << 1);		\
        }							\
      while ((cnt) > newcnt);					\
      								\
      base = (type *)ev_realloc (base, sizeof (type) * (newcnt));\
      init (base + cur, newcnt - cur);				\
      cur = newcnt;						\
    }

#define array_slim(type,stem)					\
  if (stem ## max < array_roundsize (stem ## cnt >> 2))		\
    {								\
      stem ## max = array_roundsize (stem ## cnt >> 1);		\
      base = (type *)ev_realloc (base, sizeof (type) * (stem ## max));\
      fprintf (stderr, "slimmed down " # stem " to %d\n", stem ## max);/*D*/\
    }

#define array_free(stem, idx) \
  ev_free (stem ## s idx); stem ## cnt idx = stem ## max idx = 0;

/*****************************************************************************/

static void
anfds_init (ANFD *base, int count)
{
  while (count--)
    {
      base->head   = 0;
      base->events = EV_NONE;
      base->reify  = 0;

      ++base;
    }
}

void
ev_feed_event (EV_P_ void *w, int revents)
{
  W w_ = (W)w;

  if (expect_false (w_->pending))
    {
      pendings [ABSPRI (w_)][w_->pending - 1].events |= revents;
      return;
    }

  w_->pending = ++pendingcnt [ABSPRI (w_)];
  array_needsize (ANPENDING, pendings [ABSPRI (w_)], pendingmax [ABSPRI (w_)], pendingcnt [ABSPRI (w_)], EMPTY2);
  pendings [ABSPRI (w_)][w_->pending - 1].w      = w_;
  pendings [ABSPRI (w_)][w_->pending - 1].events = revents;
}

static void
queue_events (EV_P_ W *events, int eventcnt, int type)
{
  int i;

  for (i = 0; i < eventcnt; ++i)
    ev_feed_event (EV_A_ events [i], type);
}

inline void
fd_event (EV_P_ int fd, int revents)
{
  ANFD *anfd = anfds + fd;
  struct ev_io *w;

  for (w = (struct ev_io *)anfd->head; w; w = (struct ev_io *)((WL)w)->next)
    {
      int ev = w->events & revents;

      if (ev)
        ev_feed_event (EV_A_ (W)w, ev);
    }
}

void
ev_feed_fd_event (EV_P_ int fd, int revents)
{
  fd_event (EV_A_ fd, revents);
}

/*****************************************************************************/

inline void
fd_reify (EV_P)
{
  int i;

  for (i = 0; i < fdchangecnt; ++i)
    {
      int fd = fdchanges [i];
      ANFD *anfd = anfds + fd;
      struct ev_io *w;

      int events = 0;

      for (w = (struct ev_io *)anfd->head; w; w = (struct ev_io *)((WL)w)->next)
        events |= w->events;

#if EV_SELECT_IS_WINSOCKET
      if (events)
        {
          unsigned long argp;
          anfd->handle = _get_osfhandle (fd);
          assert (("libev only supports socket fds in this configuration", ioctlsocket (anfd->handle, FIONREAD, &argp) == 0));
        }
#endif

      anfd->reify = 0;

      method_modify (EV_A_ fd, anfd->events, events);
      anfd->events = events;
    }

  fdchangecnt = 0;
}

static void
fd_change (EV_P_ int fd)
{
  if (expect_false (anfds [fd].reify))
    return;

  anfds [fd].reify = 1;

  ++fdchangecnt;
  array_needsize (int, fdchanges, fdchangemax, fdchangecnt, EMPTY2);
  fdchanges [fdchangecnt - 1] = fd;
}

static void
fd_kill (EV_P_ int fd)
{
  struct ev_io *w;

  while ((w = (struct ev_io *)anfds [fd].head))
    {
      ev_io_stop (EV_A_ w);
      ev_feed_event (EV_A_ (W)w, EV_ERROR | EV_READ | EV_WRITE);
    }
}

inline int
fd_valid (int fd)
{
#ifdef _WIN32
  return _get_osfhandle (fd) != -1;
#else
  return fcntl (fd, F_GETFD) != -1;
#endif
}

/* called on EBADF to verify fds */
static void
fd_ebadf (EV_P)
{
  int fd;

  for (fd = 0; fd < anfdmax; ++fd)
    if (anfds [fd].events)
      if (!fd_valid (fd) == -1 && errno == EBADF)
        fd_kill (EV_A_ fd);
}

/* called on ENOMEM in select/poll to kill some fds and retry */
static void
fd_enomem (EV_P)
{
  int fd;

  for (fd = anfdmax; fd--; )
    if (anfds [fd].events)
      {
        fd_kill (EV_A_ fd);
        return;
      }
}

/* usually called after fork if method needs to re-arm all fds from scratch */
static void
fd_rearm_all (EV_P)
{
  int fd;

  /* this should be highly optimised to not do anything but set a flag */
  for (fd = 0; fd < anfdmax; ++fd)
    if (anfds [fd].events)
      {
        anfds [fd].events = 0;
        fd_change (EV_A_ fd);
      }
}

/*****************************************************************************/

static void
upheap (WT *heap, int k)
{
  WT w = heap [k];

  while (k && heap [k >> 1]->at > w->at)
    {
      heap [k] = heap [k >> 1];
      ((W)heap [k])->active = k + 1;
      k >>= 1;
    }

  heap [k] = w;
  ((W)heap [k])->active = k + 1;

}

static void
downheap (WT *heap, int N, int k)
{
  WT w = heap [k];

  while (k < (N >> 1))
    {
      int j = k << 1;

      if (j + 1 < N && heap [j]->at > heap [j + 1]->at)
        ++j;

      if (w->at <= heap [j]->at)
        break;

      heap [k] = heap [j];
      ((W)heap [k])->active = k + 1;
      k = j;
    }

  heap [k] = w;
  ((W)heap [k])->active = k + 1;
}

inline void
adjustheap (WT *heap, int N, int k)
{
  upheap (heap, k);
  downheap (heap, N, k);
}

/*****************************************************************************/

typedef struct
{
  WL head;
  sig_atomic_t volatile gotsig;
} ANSIG;

static ANSIG *signals;
static int signalmax;

static int sigpipe [2];
static sig_atomic_t volatile gotsig;
static struct ev_io sigev;

static void
signals_init (ANSIG *base, int count)
{
  while (count--)
    {
      base->head   = 0;
      base->gotsig = 0;

      ++base;
    }
}

static void
sighandler (int signum)
{
#if _WIN32
  signal (signum, sighandler);
#endif

  signals [signum - 1].gotsig = 1;

  if (!gotsig)
    {
      int old_errno = errno;
      gotsig = 1;
      write (sigpipe [1], &signum, 1);
      errno = old_errno;
    }
}

void
ev_feed_signal_event (EV_P_ int signum)
{
  WL w;

#if EV_MULTIPLICITY
  assert (("feeding signal events is only supported in the default loop", loop == ev_default_loop_ptr));
#endif

  --signum;

  if (signum < 0 || signum >= signalmax)
    return;

  signals [signum].gotsig = 0;

  for (w = signals [signum].head; w; w = w->next)
    ev_feed_event (EV_A_ (W)w, EV_SIGNAL);
}

static void
sigcb (EV_P_ struct ev_io *iow, int revents)
{
  int signum;

  read (sigpipe [0], &revents, 1);
  gotsig = 0;

  for (signum = signalmax; signum--; )
    if (signals [signum].gotsig)
      ev_feed_signal_event (EV_A_ signum + 1);
}

static void
fd_intern (int fd)
{
#ifdef _WIN32
  int arg = 1;
  ioctlsocket (_get_osfhandle (fd), FIONBIO, &arg);
#else
  fcntl (fd, F_SETFD, FD_CLOEXEC);
  fcntl (fd, F_SETFL, O_NONBLOCK);
#endif
}

static void
siginit (EV_P)
{
  fd_intern (sigpipe [0]);
  fd_intern (sigpipe [1]);

  ev_io_set (&sigev, sigpipe [0], EV_READ);
  ev_io_start (EV_A_ &sigev);
  ev_unref (EV_A); /* child watcher should not keep loop alive */
}

/*****************************************************************************/

static struct ev_child *childs [PID_HASHSIZE];

#ifndef _WIN32

static struct ev_signal childev;

#ifndef WCONTINUED
# define WCONTINUED 0
#endif

static void
child_reap (EV_P_ struct ev_signal *sw, int chain, int pid, int status)
{
  struct ev_child *w;

  for (w = (struct ev_child *)childs [chain & (PID_HASHSIZE - 1)]; w; w = (struct ev_child *)((WL)w)->next)
    if (w->pid == pid || !w->pid)
      {
        ev_priority (w) = ev_priority (sw); /* need to do it *now* */
        w->rpid         = pid;
        w->rstatus      = status;
        ev_feed_event (EV_A_ (W)w, EV_CHILD);
      }
}

static void
childcb (EV_P_ struct ev_signal *sw, int revents)
{
  int pid, status;

  if (0 < (pid = waitpid (-1, &status, WNOHANG | WUNTRACED | WCONTINUED)))
    {
      /* make sure we are called again until all childs have been reaped */
      ev_feed_event (EV_A_ (W)sw, EV_SIGNAL);

      child_reap (EV_A_ sw, pid, pid, status);
      child_reap (EV_A_ sw,   0, pid, status); /* this might trigger a watcher twice, but event catches that */
    }
}

#endif

/*****************************************************************************/

#if EV_USE_PORT
# include "ev_port.c"
#endif
#if EV_USE_KQUEUE
# include "ev_kqueue.c"
#endif
#if EV_USE_EPOLL
# include "ev_epoll.c"
#endif
#if EV_USE_POLL
# include "ev_poll.c"
#endif
#if EV_USE_SELECT
# include "ev_select.c"
#endif

int
ev_version_major (void)
{
  return EV_VERSION_MAJOR;
}

int
ev_version_minor (void)
{
  return EV_VERSION_MINOR;
}

/* return true if we are running with elevated privileges and should ignore env variables */
static int
enable_secure (void)
{
#ifdef _WIN32
  return 0;
#else
  return getuid () != geteuid ()
      || getgid () != getegid ();
#endif
}

unsigned int
ev_method (EV_P)
{
  return method;
}

static void
loop_init (EV_P_ unsigned int flags)
{
  if (!method)
    {
#if EV_USE_MONOTONIC
      {
        struct timespec ts;
        if (!clock_gettime (CLOCK_MONOTONIC, &ts))
          have_monotonic = 1;
      }
#endif

      ev_rt_now = ev_time ();
      mn_now    = get_clock ();
      now_floor = mn_now;
      rtmn_diff = ev_rt_now - mn_now;

      if (!(flags & EVFLAG_NOENV) && !enable_secure () && getenv ("LIBEV_FLAGS"))
        flags = atoi (getenv ("LIBEV_FLAGS"));

      if (!(flags & 0x0000ffff))
        flags |= 0x0000ffff;

      method = 0;
#if EV_USE_PORT
      if (!method && (flags & EVMETHOD_PORT  )) method = port_init   (EV_A_ flags);
#endif
#if EV_USE_KQUEUE
      if (!method && (flags & EVMETHOD_KQUEUE)) method = kqueue_init (EV_A_ flags);
#endif
#if EV_USE_EPOLL
      if (!method && (flags & EVMETHOD_EPOLL )) method = epoll_init  (EV_A_ flags);
#endif
#if EV_USE_POLL
      if (!method && (flags & EVMETHOD_POLL  )) method = poll_init   (EV_A_ flags);
#endif
#if EV_USE_SELECT
      if (!method && (flags & EVMETHOD_SELECT)) method = select_init (EV_A_ flags);
#endif

      ev_init (&sigev, sigcb);
      ev_set_priority (&sigev, EV_MAXPRI);
    }
}

void
loop_destroy (EV_P)
{
  int i;

#if EV_USE_PORT
  if (method == EVMETHOD_PORT  ) port_destroy   (EV_A);
#endif
#if EV_USE_KQUEUE
  if (method == EVMETHOD_KQUEUE) kqueue_destroy (EV_A);
#endif
#if EV_USE_EPOLL
  if (method == EVMETHOD_EPOLL ) epoll_destroy  (EV_A);
#endif
#if EV_USE_POLL
  if (method == EVMETHOD_POLL  ) poll_destroy   (EV_A);
#endif
#if EV_USE_SELECT
  if (method == EVMETHOD_SELECT) select_destroy (EV_A);
#endif

  for (i = NUMPRI; i--; )
    array_free (pending, [i]);

  /* have to use the microsoft-never-gets-it-right macro */
  array_free (fdchange, EMPTY0);
  array_free (timer, EMPTY0);
#if EV_PERIODICS
  array_free (periodic, EMPTY0);
#endif
  array_free (idle, EMPTY0);
  array_free (prepare, EMPTY0);
  array_free (check, EMPTY0);

  method = 0;
}

static void
loop_fork (EV_P)
{
#if EV_USE_PORT
  if (method == EVMETHOD_PORT  ) port_fork   (EV_A);
#endif
#if EV_USE_KQUEUE
  if (method == EVMETHOD_KQUEUE) kqueue_fork (EV_A);
#endif
#if EV_USE_EPOLL
  if (method == EVMETHOD_EPOLL ) epoll_fork  (EV_A);
#endif

  if (ev_is_active (&sigev))
    {
      /* default loop */

      ev_ref (EV_A);
      ev_io_stop (EV_A_ &sigev);
      close (sigpipe [0]);
      close (sigpipe [1]);

      while (pipe (sigpipe))
        syserr ("(libev) error creating pipe");

      siginit (EV_A);
    }

  postfork = 0;
}

#if EV_MULTIPLICITY
struct ev_loop *
ev_loop_new (unsigned int flags)
{
  struct ev_loop *loop = (struct ev_loop *)ev_malloc (sizeof (struct ev_loop));

  memset (loop, 0, sizeof (struct ev_loop));

  loop_init (EV_A_ flags);

  if (ev_method (EV_A))
    return loop;

  return 0;
}

void
ev_loop_destroy (EV_P)
{
  loop_destroy (EV_A);
  ev_free (loop);
}

void
ev_loop_fork (EV_P)
{
  postfork = 1;
}

#endif

#if EV_MULTIPLICITY
struct ev_loop *
ev_default_loop_ (unsigned int flags)
#else
int
ev_default_loop (unsigned int flags)
#endif
{
  if (sigpipe [0] == sigpipe [1])
    if (pipe (sigpipe))
      return 0;

  if (!ev_default_loop_ptr)
    {
#if EV_MULTIPLICITY
      struct ev_loop *loop = ev_default_loop_ptr = &default_loop_struct;
#else
      ev_default_loop_ptr = 1;
#endif

      loop_init (EV_A_ flags);

      if (ev_method (EV_A))
        {
          siginit (EV_A);

#ifndef _WIN32
          ev_signal_init (&childev, childcb, SIGCHLD);
          ev_set_priority (&childev, EV_MAXPRI);
          ev_signal_start (EV_A_ &childev);
          ev_unref (EV_A); /* child watcher should not keep loop alive */
#endif
        }
      else
        ev_default_loop_ptr = 0;
    }

  return ev_default_loop_ptr;
}

void
ev_default_destroy (void)
{
#if EV_MULTIPLICITY
  struct ev_loop *loop = ev_default_loop_ptr;
#endif

#ifndef _WIN32
  ev_ref (EV_A); /* child watcher */
  ev_signal_stop (EV_A_ &childev);
#endif

  ev_ref (EV_A); /* signal watcher */
  ev_io_stop (EV_A_ &sigev);

  close (sigpipe [0]); sigpipe [0] = 0;
  close (sigpipe [1]); sigpipe [1] = 0;

  loop_destroy (EV_A);
}

void
ev_default_fork (void)
{
#if EV_MULTIPLICITY
  struct ev_loop *loop = ev_default_loop_ptr;
#endif

  if (method)
    postfork = 1;
}

/*****************************************************************************/

static int
any_pending (EV_P)
{
  int pri;

  for (pri = NUMPRI; pri--; )
    if (pendingcnt [pri])
      return 1;

  return 0;
}

inline void
call_pending (EV_P)
{
  int pri;

  for (pri = NUMPRI; pri--; )
    while (pendingcnt [pri])
      {
        ANPENDING *p = pendings [pri] + --pendingcnt [pri];

        if (expect_true (p->w))
          {
            p->w->pending = 0;
            EV_CB_INVOKE (p->w, p->events);
          }
      }
}

inline void
timers_reify (EV_P)
{
  while (timercnt && ((WT)timers [0])->at <= mn_now)
    {
      struct ev_timer *w = timers [0];

      assert (("inactive timer on timer heap detected", ev_is_active (w)));

      /* first reschedule or stop timer */
      if (w->repeat)
        {
          assert (("negative ev_timer repeat value found while processing timers", w->repeat > 0.));

          ((WT)w)->at += w->repeat;
          if (((WT)w)->at < mn_now)
            ((WT)w)->at = mn_now;

          downheap ((WT *)timers, timercnt, 0);
        }
      else
        ev_timer_stop (EV_A_ w); /* nonrepeating: stop timer */

      ev_feed_event (EV_A_ (W)w, EV_TIMEOUT);
    }
}

#if EV_PERIODICS
inline void
periodics_reify (EV_P)
{
  while (periodiccnt && ((WT)periodics [0])->at <= ev_rt_now)
    {
      struct ev_periodic *w = periodics [0];

      assert (("inactive timer on periodic heap detected", ev_is_active (w)));

      /* first reschedule or stop timer */
      if (w->reschedule_cb)
        {
          ((WT)w)->at = w->reschedule_cb (w, ev_rt_now + 0.0001);
          assert (("ev_periodic reschedule callback returned time in the past", ((WT)w)->at > ev_rt_now));
          downheap ((WT *)periodics, periodiccnt, 0);
        }
      else if (w->interval)
        {
          ((WT)w)->at += floor ((ev_rt_now - ((WT)w)->at) / w->interval + 1.) * w->interval;
          assert (("ev_periodic timeout in the past detected while processing timers, negative interval?", ((WT)w)->at > ev_rt_now));
          downheap ((WT *)periodics, periodiccnt, 0);
        }
      else
        ev_periodic_stop (EV_A_ w); /* nonrepeating: stop timer */

      ev_feed_event (EV_A_ (W)w, EV_PERIODIC);
    }
}

static void
periodics_reschedule (EV_P)
{
  int i;

  /* adjust periodics after time jump */
  for (i = 0; i < periodiccnt; ++i)
    {
      struct ev_periodic *w = periodics [i];

      if (w->reschedule_cb)
        ((WT)w)->at = w->reschedule_cb (w, ev_rt_now);
      else if (w->interval)
        ((WT)w)->at += ceil ((ev_rt_now - ((WT)w)->at) / w->interval) * w->interval;
    }

  /* now rebuild the heap */
  for (i = periodiccnt >> 1; i--; )
    downheap ((WT *)periodics, periodiccnt, i);
}
#endif

inline int
time_update_monotonic (EV_P)
{
  mn_now = get_clock ();

  if (expect_true (mn_now - now_floor < MIN_TIMEJUMP * .5))
    {
      ev_rt_now = rtmn_diff + mn_now;
      return 0;
    }
  else
    {
      now_floor = mn_now;
      ev_rt_now = ev_time ();
      return 1;
    }
}

inline void
time_update (EV_P)
{
  int i;

#if EV_USE_MONOTONIC
  if (expect_true (have_monotonic))
    {
      if (time_update_monotonic (EV_A))
        {
          ev_tstamp odiff = rtmn_diff;

          for (i = 4; --i; ) /* loop a few times, before making important decisions */
            {
              rtmn_diff = ev_rt_now - mn_now;

              if (fabs (odiff - rtmn_diff) < MIN_TIMEJUMP)
                return; /* all is well */

              ev_rt_now = ev_time ();
              mn_now    = get_clock ();
              now_floor = mn_now;
            }

# if EV_PERIODICS
          periodics_reschedule (EV_A);
# endif
          /* no timer adjustment, as the monotonic clock doesn't jump */
          /* timers_reschedule (EV_A_ rtmn_diff - odiff) */
        }
    }
  else
#endif
    {
      ev_rt_now = ev_time ();

      if (expect_false (mn_now > ev_rt_now || mn_now < ev_rt_now - MAX_BLOCKTIME - MIN_TIMEJUMP))
        {
#if EV_PERIODICS
          periodics_reschedule (EV_A);
#endif

          /* adjust timers. this is easy, as the offset is the same for all */
          for (i = 0; i < timercnt; ++i)
            ((WT)timers [i])->at += ev_rt_now - mn_now;
        }

      mn_now = ev_rt_now;
    }
}

void
ev_ref (EV_P)
{
  ++activecnt;
}

void
ev_unref (EV_P)
{
  --activecnt;
}

static int loop_done;

void
ev_loop (EV_P_ int flags)
{
  double block;
  loop_done = flags & (EVLOOP_ONESHOT | EVLOOP_NONBLOCK) ? 1 : 0;

  while (activecnt)
    {
      /* queue check watchers (and execute them) */
      if (expect_false (preparecnt))
        {
          queue_events (EV_A_ (W *)prepares, preparecnt, EV_PREPARE);
          call_pending (EV_A);
        }

      /* we might have forked, so reify kernel state if necessary */
      if (expect_false (postfork))
        loop_fork (EV_A);

      /* update fd-related kernel structures */
      fd_reify (EV_A);

      /* calculate blocking time */

      /* we only need this for !monotonic clock or timers, but as we basically
         always have timers, we just calculate it always */
#if EV_USE_MONOTONIC
      if (expect_true (have_monotonic))
        time_update_monotonic (EV_A);
      else
#endif
        {
          ev_rt_now = ev_time ();
          mn_now    = ev_rt_now;
        }

      if (flags & EVLOOP_NONBLOCK || idlecnt)
        block = 0.;
      else
        {
          block = MAX_BLOCKTIME;

          if (timercnt)
            {
              ev_tstamp to = ((WT)timers [0])->at - mn_now + method_fudge;
              if (block > to) block = to;
            }

#if EV_PERIODICS
          if (periodiccnt)
            {
              ev_tstamp to = ((WT)periodics [0])->at - ev_rt_now + method_fudge;
              if (block > to) block = to;
            }
#endif

          if (expect_false (block < 0.)) block = 0.;
        }

      method_poll (EV_A_ block);

      /* update ev_rt_now, do magic */
      time_update (EV_A);

      /* queue pending timers and reschedule them */
      timers_reify (EV_A); /* relative timers called last */
#if EV_PERIODICS
      periodics_reify (EV_A); /* absolute timers called first */
#endif

      /* queue idle watchers unless io or timers are pending */
      if (idlecnt && !any_pending (EV_A))
        queue_events (EV_A_ (W *)idles, idlecnt, EV_IDLE);

      /* queue check watchers, to be executed first */
      if (expect_false (checkcnt))
        queue_events (EV_A_ (W *)checks, checkcnt, EV_CHECK);

      call_pending (EV_A);

      if (expect_false (loop_done))
        break;
    }

  if (loop_done != 2)
    loop_done = 0;
}

void
ev_unloop (EV_P_ int how)
{
  loop_done = how;
}

/*****************************************************************************/

inline void
wlist_add (WL *head, WL elem)
{
  elem->next = *head;
  *head = elem;
}

inline void
wlist_del (WL *head, WL elem)
{
  while (*head)
    {
      if (*head == elem)
        {
          *head = elem->next;
          return;
        }

      head = &(*head)->next;
    }
}

inline void
ev_clear_pending (EV_P_ W w)
{
  if (w->pending)
    {
      pendings [ABSPRI (w)][w->pending - 1].w = 0;
      w->pending = 0;
    }
}

inline void
ev_start (EV_P_ W w, int active)
{
  if (w->priority < EV_MINPRI) w->priority = EV_MINPRI;
  if (w->priority > EV_MAXPRI) w->priority = EV_MAXPRI;

  w->active = active;
  ev_ref (EV_A);
}

inline void
ev_stop (EV_P_ W w)
{
  ev_unref (EV_A);
  w->active = 0;
}

/*****************************************************************************/

void
ev_io_start (EV_P_ struct ev_io *w)
{
  int fd = w->fd;

  if (expect_false (ev_is_active (w)))
    return;

  assert (("ev_io_start called with negative fd", fd >= 0));

  ev_start (EV_A_ (W)w, 1);
  array_needsize (ANFD, anfds, anfdmax, fd + 1, anfds_init);
  wlist_add ((WL *)&anfds[fd].head, (WL)w);

  fd_change (EV_A_ fd);
}

void
ev_io_stop (EV_P_ struct ev_io *w)
{
  ev_clear_pending (EV_A_ (W)w);
  if (expect_false (!ev_is_active (w)))
    return;

  assert (("ev_io_start called with illegal fd (must stay constant after start!)", w->fd >= 0 && w->fd < anfdmax));

  wlist_del ((WL *)&anfds[w->fd].head, (WL)w);
  ev_stop (EV_A_ (W)w);

  fd_change (EV_A_ w->fd);
}

void
ev_timer_start (EV_P_ struct ev_timer *w)
{
  if (expect_false (ev_is_active (w)))
    return;

  ((WT)w)->at += mn_now;

  assert (("ev_timer_start called with negative timer repeat value", w->repeat >= 0.));

  ev_start (EV_A_ (W)w, ++timercnt);
  array_needsize (struct ev_timer *, timers, timermax, timercnt, EMPTY2);
  timers [timercnt - 1] = w;
  upheap ((WT *)timers, timercnt - 1);

  assert (("internal timer heap corruption", timers [((W)w)->active - 1] == w));
}

void
ev_timer_stop (EV_P_ struct ev_timer *w)
{
  ev_clear_pending (EV_A_ (W)w);
  if (expect_false (!ev_is_active (w)))
    return;

  assert (("internal timer heap corruption", timers [((W)w)->active - 1] == w));

  if (expect_true (((W)w)->active < timercnt--))
    {
      timers [((W)w)->active - 1] = timers [timercnt];
      adjustheap ((WT *)timers, timercnt, ((W)w)->active - 1);
    }

  ((WT)w)->at -= mn_now;

  ev_stop (EV_A_ (W)w);
}

void
ev_timer_again (EV_P_ struct ev_timer *w)
{
  if (ev_is_active (w))
    {
      if (w->repeat)
        {
          ((WT)w)->at = mn_now + w->repeat;
          adjustheap ((WT *)timers, timercnt, ((W)w)->active - 1);
        }
      else
        ev_timer_stop (EV_A_ w);
    }
  else if (w->repeat)
    {
      w->at = w->repeat;
      ev_timer_start (EV_A_ w);
    }
}

#if EV_PERIODICS
void
ev_periodic_start (EV_P_ struct ev_periodic *w)
{
  if (expect_false (ev_is_active (w)))
    return;

  if (w->reschedule_cb)
    ((WT)w)->at = w->reschedule_cb (w, ev_rt_now);
  else if (w->interval)
    {
      assert (("ev_periodic_start called with negative interval value", w->interval >= 0.));
      /* this formula differs from the one in periodic_reify because we do not always round up */
      ((WT)w)->at += ceil ((ev_rt_now - ((WT)w)->at) / w->interval) * w->interval;
    }

  ev_start (EV_A_ (W)w, ++periodiccnt);
  array_needsize (struct ev_periodic *, periodics, periodicmax, periodiccnt, EMPTY2);
  periodics [periodiccnt - 1] = w;
  upheap ((WT *)periodics, periodiccnt - 1);

  assert (("internal periodic heap corruption", periodics [((W)w)->active - 1] == w));
}

void
ev_periodic_stop (EV_P_ struct ev_periodic *w)
{
  ev_clear_pending (EV_A_ (W)w);
  if (expect_false (!ev_is_active (w)))
    return;

  assert (("internal periodic heap corruption", periodics [((W)w)->active - 1] == w));

  if (expect_true (((W)w)->active < periodiccnt--))
    {
      periodics [((W)w)->active - 1] = periodics [periodiccnt];
      adjustheap ((WT *)periodics, periodiccnt, ((W)w)->active - 1);
    }

  ev_stop (EV_A_ (W)w);
}

void
ev_periodic_again (EV_P_ struct ev_periodic *w)
{
  /* TODO: use adjustheap and recalculation */
  ev_periodic_stop (EV_A_ w);
  ev_periodic_start (EV_A_ w);
}
#endif

void
ev_idle_start (EV_P_ struct ev_idle *w)
{
  if (expect_false (ev_is_active (w)))
    return;

  ev_start (EV_A_ (W)w, ++idlecnt);
  array_needsize (struct ev_idle *, idles, idlemax, idlecnt, EMPTY2);
  idles [idlecnt - 1] = w;
}

void
ev_idle_stop (EV_P_ struct ev_idle *w)
{
  ev_clear_pending (EV_A_ (W)w);
  if (expect_false (!ev_is_active (w)))
    return;

  idles [((W)w)->active - 1] = idles [--idlecnt];
  ev_stop (EV_A_ (W)w);
}

void
ev_prepare_start (EV_P_ struct ev_prepare *w)
{
  if (expect_false (ev_is_active (w)))
    return;

  ev_start (EV_A_ (W)w, ++preparecnt);
  array_needsize (struct ev_prepare *, prepares, preparemax, preparecnt, EMPTY2);
  prepares [preparecnt - 1] = w;
}

void
ev_prepare_stop (EV_P_ struct ev_prepare *w)
{
  ev_clear_pending (EV_A_ (W)w);
  if (expect_false (!ev_is_active (w)))
    return;

  prepares [((W)w)->active - 1] = prepares [--preparecnt];
  ev_stop (EV_A_ (W)w);
}

void
ev_check_start (EV_P_ struct ev_check *w)
{
  if (expect_false (ev_is_active (w)))
    return;

  ev_start (EV_A_ (W)w, ++checkcnt);
  array_needsize (struct ev_check *, checks, checkmax, checkcnt, EMPTY2);
  checks [checkcnt - 1] = w;
}

void
ev_check_stop (EV_P_ struct ev_check *w)
{
  ev_clear_pending (EV_A_ (W)w);
  if (expect_false (!ev_is_active (w)))
    return;

  checks [((W)w)->active - 1] = checks [--checkcnt];
  ev_stop (EV_A_ (W)w);
}

#ifndef SA_RESTART
# define SA_RESTART 0
#endif

void
ev_signal_start (EV_P_ struct ev_signal *w)
{
#if EV_MULTIPLICITY
  assert (("signal watchers are only supported in the default loop", loop == ev_default_loop_ptr));
#endif
  if (expect_false (ev_is_active (w)))
    return;

  assert (("ev_signal_start called with illegal signal number", w->signum > 0));

  ev_start (EV_A_ (W)w, 1);
  array_needsize (ANSIG, signals, signalmax, w->signum, signals_init);
  wlist_add ((WL *)&signals [w->signum - 1].head, (WL)w);

  if (!((WL)w)->next)
    {
#if _WIN32
      signal (w->signum, sighandler);
#else
      struct sigaction sa;
      sa.sa_handler = sighandler;
      sigfillset (&sa.sa_mask);
      sa.sa_flags = SA_RESTART; /* if restarting works we save one iteration */
      sigaction (w->signum, &sa, 0);
#endif
    }
}

void
ev_signal_stop (EV_P_ struct ev_signal *w)
{
  ev_clear_pending (EV_A_ (W)w);
  if (expect_false (!ev_is_active (w)))
    return;

  wlist_del ((WL *)&signals [w->signum - 1].head, (WL)w);
  ev_stop (EV_A_ (W)w);

  if (!signals [w->signum - 1].head)
    signal (w->signum, SIG_DFL);
}

void
ev_child_start (EV_P_ struct ev_child *w)
{
#if EV_MULTIPLICITY
  assert (("child watchers are only supported in the default loop", loop == ev_default_loop_ptr));
#endif
  if (expect_false (ev_is_active (w)))
    return;

  ev_start (EV_A_ (W)w, 1);
  wlist_add ((WL *)&childs [w->pid & (PID_HASHSIZE - 1)], (WL)w);
}

void
ev_child_stop (EV_P_ struct ev_child *w)
{
  ev_clear_pending (EV_A_ (W)w);
  if (expect_false (!ev_is_active (w)))
    return;

  wlist_del ((WL *)&childs [w->pid & (PID_HASHSIZE - 1)], (WL)w);
  ev_stop (EV_A_ (W)w);
}

/*****************************************************************************/

struct ev_once
{
  struct ev_io io;
  struct ev_timer to;
  void (*cb)(int revents, void *arg);
  void *arg;
};

static void
once_cb (EV_P_ struct ev_once *once, int revents)
{
  void (*cb)(int revents, void *arg) = once->cb;
  void *arg = once->arg;

  ev_io_stop (EV_A_ &once->io);
  ev_timer_stop (EV_A_ &once->to);
  ev_free (once);

  cb (revents, arg);
}

static void
once_cb_io (EV_P_ struct ev_io *w, int revents)
{
  once_cb (EV_A_ (struct ev_once *)(((char *)w) - offsetof (struct ev_once, io)), revents);
}

static void
once_cb_to (EV_P_ struct ev_timer *w, int revents)
{
  once_cb (EV_A_ (struct ev_once *)(((char *)w) - offsetof (struct ev_once, to)), revents);
}

void
ev_once (EV_P_ int fd, int events, ev_tstamp timeout, void (*cb)(int revents, void *arg), void *arg)
{
  struct ev_once *once = (struct ev_once *)ev_malloc (sizeof (struct ev_once));

  if (expect_false (!once))
    {
      cb (EV_ERROR | EV_READ | EV_WRITE | EV_TIMEOUT, arg);
      return;
    }

  once->cb  = cb;
  once->arg = arg;

  ev_init (&once->io, once_cb_io);
  if (fd >= 0)
    {
      ev_io_set (&once->io, fd, events);
      ev_io_start (EV_A_ &once->io);
    }

  ev_init (&once->to, once_cb_to);
  if (timeout >= 0.)
    {
      ev_timer_set (&once->to, timeout, 0.);
      ev_timer_start (EV_A_ &once->to);
    }
}

#ifdef __cplusplus
}
#endif

