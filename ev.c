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
#ifndef EV_STANDALONE
# include "config.h"

# if HAVE_CLOCK_GETTIME
#  define EV_USE_MONOTONIC 1
#  define EV_USE_REALTIME  1
# endif

# if HAVE_SELECT && HAVE_SYS_SELECT_H
#  define EV_USE_SELECT 1
# endif

# if HAVE_POLL && HAVE_POLL_H
#  define EV_USE_POLL 1
# endif

# if HAVE_EPOLL && HAVE_EPOLL_CTL && HAVE_SYS_EPOLL_H
#  define EV_USE_EPOLL 1
# endif

# if HAVE_KQUEUE && HAVE_WORKING_KQUEUE && HAVE_SYS_EVENT_H && HAVE_SYS_QUEUE_H
#  define EV_USE_KQUEUE 1
# endif

#endif

#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>

#include <stdio.h>

#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#ifndef WIN32
# include <sys/wait.h>
#endif
#include <sys/time.h>
#include <time.h>

/**/

#ifndef EV_USE_MONOTONIC
# define EV_USE_MONOTONIC 1
#endif

#ifndef EV_USE_SELECT
# define EV_USE_SELECT 1
#endif

#ifndef EV_USE_POLL
# define EV_USE_POLL 0 /* poll is usually slower than select, and not as well tested */
#endif

#ifndef EV_USE_EPOLL
# define EV_USE_EPOLL 0
#endif

#ifndef EV_USE_KQUEUE
# define EV_USE_KQUEUE 0
#endif

#ifndef EV_USE_REALTIME
# define EV_USE_REALTIME 1
#endif

/**/

#ifndef CLOCK_MONOTONIC
# undef EV_USE_MONOTONIC
# define EV_USE_MONOTONIC 0
#endif

#ifndef CLOCK_REALTIME
# undef EV_USE_REALTIME
# define EV_USE_REALTIME 0
#endif

/**/

#define MIN_TIMEJUMP  1. /* minimum timejump that gets detected (if monotonic clock available) */
#define MAX_BLOCKTIME 59.731 /* never wait longer than this time (to detect time jumps) */
#define PID_HASHSIZE  16 /* size of pid hash table, must be power of two */
/*#define CLEANUP_INTERVAL 300. /* how often to try to free memory and re-check fds */

#include "ev.h"

#if __GNUC__ >= 3
# define expect(expr,value)         __builtin_expect ((expr),(value))
# define inline                     inline
#else
# define expect(expr,value)         (expr)
# define inline                     static
#endif

#define expect_false(expr) expect ((expr) != 0, 0)
#define expect_true(expr)  expect ((expr) != 0, 1)

#define NUMPRI    (EV_MAXPRI - EV_MINPRI + 1)
#define ABSPRI(w) ((w)->priority - EV_MINPRI)

typedef struct ev_watcher *W;
typedef struct ev_watcher_list *WL;
typedef struct ev_watcher_time *WT;

static int have_monotonic; /* did clock_gettime (CLOCK_MONOTONIC) work? */

/*****************************************************************************/

typedef struct
{
  struct ev_watcher_list *head;
  unsigned char events;
  unsigned char reify;
} ANFD;

typedef struct
{
  W w;
  int events;
} ANPENDING;

#if EV_MULTIPLICITY

struct ev_loop
{
# define VAR(name,decl) decl;
# include "ev_vars.h"
};
# undef VAR
# include "ev_wrap.h"

#else

# define VAR(name,decl) static decl;
# include "ev_vars.h"
# undef VAR

#endif

/*****************************************************************************/

inline ev_tstamp
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

ev_tstamp
ev_now (EV_P)
{
  return rt_now;
}

#define array_roundsize(base,n) ((n) | 4 & ~3)

#define array_needsize(base,cur,cnt,init)		\
  if (expect_false ((cnt) > cur))			\
    {							\
      int newcnt = cur;					\
      do						\
        {						\
          newcnt = array_roundsize (base, newcnt << 1);	\
        }						\
      while ((cnt) > newcnt);				\
      							\
      base = realloc (base, sizeof (*base) * (newcnt));	\
      init (base + cur, newcnt - cur);			\
      cur = newcnt;					\
    }

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

static void
event (EV_P_ W w, int events)
{
  if (w->pending)
    {
      pendings [ABSPRI (w)][w->pending - 1].events |= events;
      return;
    }

  w->pending = ++pendingcnt [ABSPRI (w)];
  array_needsize (pendings [ABSPRI (w)], pendingmax [ABSPRI (w)], pendingcnt [ABSPRI (w)], );
  pendings [ABSPRI (w)][w->pending - 1].w      = w;
  pendings [ABSPRI (w)][w->pending - 1].events = events;
}

static void
queue_events (EV_P_ W *events, int eventcnt, int type)
{
  int i;

  for (i = 0; i < eventcnt; ++i)
    event (EV_A_ events [i], type);
}

static void
fd_event (EV_P_ int fd, int events)
{
  ANFD *anfd = anfds + fd;
  struct ev_io *w;

  for (w = (struct ev_io *)anfd->head; w; w = (struct ev_io *)((WL)w)->next)
    {
      int ev = w->events & events;

      if (ev)
        event (EV_A_ (W)w, ev);
    }
}

/*****************************************************************************/

static void
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

      anfd->reify = 0;

      if (anfd->events != events)
        {
          method_modify (EV_A_ fd, anfd->events, events);
          anfd->events = events;
        }
    }

  fdchangecnt = 0;
}

static void
fd_change (EV_P_ int fd)
{
  if (anfds [fd].reify || fdchangecnt < 0)
    return;

  anfds [fd].reify = 1;

  ++fdchangecnt;
  array_needsize (fdchanges, fdchangemax, fdchangecnt, );
  fdchanges [fdchangecnt - 1] = fd;
}

static void
fd_kill (EV_P_ int fd)
{
  struct ev_io *w;

  while ((w = (struct ev_io *)anfds [fd].head))
    {
      ev_io_stop (EV_A_ w);
      event (EV_A_ (W)w, EV_ERROR | EV_READ | EV_WRITE);
    }
}

/* called on EBADF to verify fds */
static void
fd_ebadf (EV_P)
{
  int fd;

  for (fd = 0; fd < anfdmax; ++fd)
    if (anfds [fd].events)
      if (fcntl (fd, F_GETFD) == -1 && errno == EBADF)
        fd_kill (EV_A_ fd);
}

/* called on ENOMEM in select/poll to kill some fds and retry */
static void
fd_enomem (EV_P)
{
  int fd = anfdmax;

  while (fd--)
    if (anfds [fd].events)
      {
        close (fd);
        fd_kill (EV_A_ fd);
        return;
      }
}

/* susually called after fork if method needs to re-arm all fds from scratch */
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
      heap [k]->active = k + 1;
      k >>= 1;
    }

  heap [k] = w;
  heap [k]->active = k + 1;

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
      heap [k]->active = k + 1;
      k = j;
    }

  heap [k] = w;
  heap [k]->active = k + 1;
}

/*****************************************************************************/

typedef struct
{
  struct ev_watcher_list *head;
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
  signals [signum - 1].gotsig = 1;

  if (!gotsig)
    {
      int old_errno = errno;
      gotsig = 1;
      write (sigpipe [1], &signum, 1);
      errno = old_errno;
    }
}

static void
sigcb (EV_P_ struct ev_io *iow, int revents)
{
  struct ev_watcher_list *w;
  int signum;

  read (sigpipe [0], &revents, 1);
  gotsig = 0;

  for (signum = signalmax; signum--; )
    if (signals [signum].gotsig)
      {
        signals [signum].gotsig = 0;

        for (w = signals [signum].head; w; w = w->next)
          event (EV_A_ (W)w, EV_SIGNAL);
      }
}

static void
siginit (EV_P)
{
#ifndef WIN32
  fcntl (sigpipe [0], F_SETFD, FD_CLOEXEC);
  fcntl (sigpipe [1], F_SETFD, FD_CLOEXEC);

  /* rather than sort out wether we really need nb, set it */
  fcntl (sigpipe [0], F_SETFL, O_NONBLOCK);
  fcntl (sigpipe [1], F_SETFL, O_NONBLOCK);
#endif

  ev_io_set (&sigev, sigpipe [0], EV_READ);
  ev_io_start (EV_A_ &sigev);
  ev_unref (EV_A); /* child watcher should not keep loop alive */
}

/*****************************************************************************/

#ifndef WIN32

static struct ev_child *childs [PID_HASHSIZE];
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
        w->priority = sw->priority; /* need to do it *now* */
        w->rpid     = pid;
        w->rstatus  = status;
        event (EV_A_ (W)w, EV_CHILD);
      }
}

static void
childcb (EV_P_ struct ev_signal *sw, int revents)
{
  int pid, status;

  if (0 < (pid = waitpid (-1, &status, WNOHANG | WUNTRACED | WCONTINUED)))
    {
      /* make sure we are called again until all childs have been reaped */
      event (EV_A_ (W)sw, EV_SIGNAL);

      child_reap (EV_A_ sw, pid, pid, status);
      child_reap (EV_A_ sw,   0, pid, status); /* this might trigger a watcher twice, but event catches that */
    }
}

#endif

/*****************************************************************************/

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
#ifdef WIN32
  return 0;
#else
  return getuid () != geteuid ()
      || getgid () != getegid ();
#endif
}

int
ev_method (EV_P)
{
  return method;
}

static void
loop_init (EV_P_ int methods)
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

      rt_now    = ev_time ();
      mn_now    = get_clock ();
      now_floor = mn_now;
      rtmn_diff = rt_now - mn_now;

      if (methods == EVMETHOD_AUTO)
        if (!enable_secure () && getenv ("LIBEV_METHODS"))
          methods = atoi (getenv ("LIBEV_METHODS"));
        else
          methods = EVMETHOD_ANY;

      method = 0;
#if EV_USE_KQUEUE
      if (!method && (methods & EVMETHOD_KQUEUE)) method = kqueue_init (EV_A_ methods);
#endif
#if EV_USE_EPOLL
      if (!method && (methods & EVMETHOD_EPOLL )) method = epoll_init  (EV_A_ methods);
#endif
#if EV_USE_POLL
      if (!method && (methods & EVMETHOD_POLL  )) method = poll_init   (EV_A_ methods);
#endif
#if EV_USE_SELECT
      if (!method && (methods & EVMETHOD_SELECT)) method = select_init (EV_A_ methods);
#endif
    }
}

void
loop_destroy (EV_P)
{
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

  method = 0;
  /*TODO*/
}

void
loop_fork (EV_P)
{
  /*TODO*/
#if EV_USE_EPOLL
  if (method == EVMETHOD_EPOLL ) epoll_fork  (EV_A);
#endif
#if EV_USE_KQUEUE
  if (method == EVMETHOD_KQUEUE) kqueue_fork (EV_A);
#endif
}

#if EV_MULTIPLICITY
struct ev_loop *
ev_loop_new (int methods)
{
  struct ev_loop *loop = (struct ev_loop *)calloc (1, sizeof (struct ev_loop));

  loop_init (EV_A_ methods);

  if (ev_method (EV_A))
    return loop;

  return 0;
}

void
ev_loop_destroy (EV_P)
{
  loop_destroy (EV_A);
  free (loop);
}

void
ev_loop_fork (EV_P)
{
  loop_fork (EV_A);
}

#endif

#if EV_MULTIPLICITY
struct ev_loop default_loop_struct;
static struct ev_loop *default_loop;

struct ev_loop *
#else
static int default_loop;

int
#endif
ev_default_loop (int methods)
{
  if (sigpipe [0] == sigpipe [1])
    if (pipe (sigpipe))
      return 0;

  if (!default_loop)
    {
#if EV_MULTIPLICITY
      struct ev_loop *loop = default_loop = &default_loop_struct;
#else
      default_loop = 1;
#endif

      loop_init (EV_A_ methods);

      if (ev_method (EV_A))
        {
          ev_watcher_init (&sigev, sigcb);
          ev_set_priority (&sigev, EV_MAXPRI);
          siginit (EV_A);

#ifndef WIN32
          ev_signal_init (&childev, childcb, SIGCHLD);
          ev_set_priority (&childev, EV_MAXPRI);
          ev_signal_start (EV_A_ &childev);
          ev_unref (EV_A); /* child watcher should not keep loop alive */
#endif
        }
      else
        default_loop = 0;
    }

  return default_loop;
}

void
ev_default_destroy (void)
{
#if EV_MULTIPLICITY
  struct ev_loop *loop = default_loop;
#endif

  ev_ref (EV_A); /* child watcher */
  ev_signal_stop (EV_A_ &childev);

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
  struct ev_loop *loop = default_loop;
#endif

  loop_fork (EV_A);

  ev_io_stop (EV_A_ &sigev);
  close (sigpipe [0]);
  close (sigpipe [1]);
  pipe (sigpipe);

  ev_ref (EV_A); /* signal watcher */
  siginit (EV_A);
}

/*****************************************************************************/

static void
call_pending (EV_P)
{
  int pri;

  for (pri = NUMPRI; pri--; )
    while (pendingcnt [pri])
      {
        ANPENDING *p = pendings [pri] + --pendingcnt [pri];

        if (p->w)
          {
            p->w->pending = 0;
            p->w->cb (EV_A_ p->w, p->events);
          }
      }
}

static void
timers_reify (EV_P)
{
  while (timercnt && timers [0]->at <= mn_now)
    {
      struct ev_timer *w = timers [0];

      assert (("inactive timer on timer heap detected", ev_is_active (w)));

      /* first reschedule or stop timer */
      if (w->repeat)
        {
          assert (("negative ev_timer repeat value found while processing timers", w->repeat > 0.));
          w->at = mn_now + w->repeat;
          downheap ((WT *)timers, timercnt, 0);
        }
      else
        ev_timer_stop (EV_A_ w); /* nonrepeating: stop timer */

      event (EV_A_ (W)w, EV_TIMEOUT);
    }
}

static void
periodics_reify (EV_P)
{
  while (periodiccnt && periodics [0]->at <= rt_now)
    {
      struct ev_periodic *w = periodics [0];

      assert (("inactive timer on periodic heap detected", ev_is_active (w)));

      /* first reschedule or stop timer */
      if (w->interval)
        {
          w->at += floor ((rt_now - w->at) / w->interval + 1.) * w->interval;
          assert (("ev_periodic timeout in the past detected while processing timers, negative interval?", w->at > rt_now));
          downheap ((WT *)periodics, periodiccnt, 0);
        }
      else
        ev_periodic_stop (EV_A_ w); /* nonrepeating: stop timer */

      event (EV_A_ (W)w, EV_PERIODIC);
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

      if (w->interval)
        {
          ev_tstamp diff = ceil ((rt_now - w->at) / w->interval) * w->interval;

          if (fabs (diff) >= 1e-4)
            {
              ev_periodic_stop (EV_A_ w);
              ev_periodic_start (EV_A_ w);

              i = 0; /* restart loop, inefficient, but time jumps should be rare */
            }
        }
    }
}

inline int
time_update_monotonic (EV_P)
{
  mn_now = get_clock ();

  if (expect_true (mn_now - now_floor < MIN_TIMEJUMP * .5))
    {
      rt_now = rtmn_diff + mn_now;
      return 0;
    }
  else
    {
      now_floor = mn_now;
      rt_now = ev_time ();
      return 1;
    }
}

static void
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
              rtmn_diff = rt_now - mn_now;

              if (fabs (odiff - rtmn_diff) < MIN_TIMEJUMP)
                return; /* all is well */

              rt_now    = ev_time ();
              mn_now    = get_clock ();
              now_floor = mn_now;
            }

          periodics_reschedule (EV_A);
          /* no timer adjustment, as the monotonic clock doesn't jump */
          /* timers_reschedule (EV_A_ rtmn_diff - odiff) */
        }
    }
  else
#endif
    {
      rt_now = ev_time ();

      if (expect_false (mn_now > rt_now || mn_now < rt_now - MAX_BLOCKTIME - MIN_TIMEJUMP))
        {
          periodics_reschedule (EV_A);

          /* adjust timers. this is easy, as the offset is the same for all */
          for (i = 0; i < timercnt; ++i)
            timers [i]->at += rt_now - mn_now;
        }

      mn_now = rt_now;
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

  do
    {
      /* queue check watchers (and execute them) */
      if (expect_false (preparecnt))
        {
          queue_events (EV_A_ (W *)prepares, preparecnt, EV_PREPARE);
          call_pending (EV_A);
        }

      /* update fd-related kernel structures */
      fd_reify (EV_A);

      /* calculate blocking time */

      /* we only need this for !monotonic clockor timers, but as we basically
         always have timers, we just calculate it always */
#if EV_USE_MONOTONIC
      if (expect_true (have_monotonic))
        time_update_monotonic (EV_A);
      else
#endif
        {
          rt_now = ev_time ();
          mn_now = rt_now;
        }

      if (flags & EVLOOP_NONBLOCK || idlecnt)
        block = 0.;
      else
        {
          block = MAX_BLOCKTIME;

          if (timercnt)
            {
              ev_tstamp to = timers [0]->at - mn_now + method_fudge;
              if (block > to) block = to;
            }

          if (periodiccnt)
            {
              ev_tstamp to = periodics [0]->at - rt_now + method_fudge;
              if (block > to) block = to;
            }

          if (block < 0.) block = 0.;
        }

      method_poll (EV_A_ block);

      /* update rt_now, do magic */
      time_update (EV_A);

      /* queue pending timers and reschedule them */
      timers_reify (EV_A); /* relative timers called last */
      periodics_reify (EV_A); /* absolute timers called first */

      /* queue idle watchers unless io or timers are pending */
      if (!pendingcnt)
        queue_events (EV_A_ (W *)idles, idlecnt, EV_IDLE);

      /* queue check watchers, to be executed first */
      if (checkcnt)
        queue_events (EV_A_ (W *)checks, checkcnt, EV_CHECK);

      call_pending (EV_A);
    }
  while (activecnt && !loop_done);

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

  if (ev_is_active (w))
    return;

  assert (("ev_io_start called with negative fd", fd >= 0));

  ev_start (EV_A_ (W)w, 1);
  array_needsize (anfds, anfdmax, fd + 1, anfds_init);
  wlist_add ((WL *)&anfds[fd].head, (WL)w);

  fd_change (EV_A_ fd);
}

void
ev_io_stop (EV_P_ struct ev_io *w)
{
  ev_clear_pending (EV_A_ (W)w);
  if (!ev_is_active (w))
    return;

  wlist_del ((WL *)&anfds[w->fd].head, (WL)w);
  ev_stop (EV_A_ (W)w);

  fd_change (EV_A_ w->fd);
}

void
ev_timer_start (EV_P_ struct ev_timer *w)
{
  if (ev_is_active (w))
    return;

  w->at += mn_now;

  assert (("ev_timer_start called with negative timer repeat value", w->repeat >= 0.));

  ev_start (EV_A_ (W)w, ++timercnt);
  array_needsize (timers, timermax, timercnt, );
  timers [timercnt - 1] = w;
  upheap ((WT *)timers, timercnt - 1);
}

void
ev_timer_stop (EV_P_ struct ev_timer *w)
{
  ev_clear_pending (EV_A_ (W)w);
  if (!ev_is_active (w))
    return;

  if (w->active < timercnt--)
    {
      timers [w->active - 1] = timers [timercnt];
      downheap ((WT *)timers, timercnt, w->active - 1);
    }

  w->at = w->repeat;

  ev_stop (EV_A_ (W)w);
}

void
ev_timer_again (EV_P_ struct ev_timer *w)
{
  if (ev_is_active (w))
    {
      if (w->repeat)
        {
          w->at = mn_now + w->repeat;
          downheap ((WT *)timers, timercnt, w->active - 1);
        }
      else
        ev_timer_stop (EV_A_ w);
    }
  else if (w->repeat)
    ev_timer_start (EV_A_ w);
}

void
ev_periodic_start (EV_P_ struct ev_periodic *w)
{
  if (ev_is_active (w))
    return;

  assert (("ev_periodic_start called with negative interval value", w->interval >= 0.));

  /* this formula differs from the one in periodic_reify because we do not always round up */
  if (w->interval)
    w->at += ceil ((rt_now - w->at) / w->interval) * w->interval;

  ev_start (EV_A_ (W)w, ++periodiccnt);
  array_needsize (periodics, periodicmax, periodiccnt, );
  periodics [periodiccnt - 1] = w;
  upheap ((WT *)periodics, periodiccnt - 1);
}

void
ev_periodic_stop (EV_P_ struct ev_periodic *w)
{
  ev_clear_pending (EV_A_ (W)w);
  if (!ev_is_active (w))
    return;

  if (w->active < periodiccnt--)
    {
      periodics [w->active - 1] = periodics [periodiccnt];
      downheap ((WT *)periodics, periodiccnt, w->active - 1);
    }

  ev_stop (EV_A_ (W)w);
}

void
ev_idle_start (EV_P_ struct ev_idle *w)
{
  if (ev_is_active (w))
    return;

  ev_start (EV_A_ (W)w, ++idlecnt);
  array_needsize (idles, idlemax, idlecnt, );
  idles [idlecnt - 1] = w;
}

void
ev_idle_stop (EV_P_ struct ev_idle *w)
{
  ev_clear_pending (EV_A_ (W)w);
  if (ev_is_active (w))
    return;

  idles [w->active - 1] = idles [--idlecnt];
  ev_stop (EV_A_ (W)w);
}

void
ev_prepare_start (EV_P_ struct ev_prepare *w)
{
  if (ev_is_active (w))
    return;

  ev_start (EV_A_ (W)w, ++preparecnt);
  array_needsize (prepares, preparemax, preparecnt, );
  prepares [preparecnt - 1] = w;
}

void
ev_prepare_stop (EV_P_ struct ev_prepare *w)
{
  ev_clear_pending (EV_A_ (W)w);
  if (ev_is_active (w))
    return;

  prepares [w->active - 1] = prepares [--preparecnt];
  ev_stop (EV_A_ (W)w);
}

void
ev_check_start (EV_P_ struct ev_check *w)
{
  if (ev_is_active (w))
    return;

  ev_start (EV_A_ (W)w, ++checkcnt);
  array_needsize (checks, checkmax, checkcnt, );
  checks [checkcnt - 1] = w;
}

void
ev_check_stop (EV_P_ struct ev_check *w)
{
  ev_clear_pending (EV_A_ (W)w);
  if (ev_is_active (w))
    return;

  checks [w->active - 1] = checks [--checkcnt];
  ev_stop (EV_A_ (W)w);
}

#ifndef SA_RESTART
# define SA_RESTART 0
#endif

void
ev_signal_start (EV_P_ struct ev_signal *w)
{
#if EV_MULTIPLICITY
  assert (("signal watchers are only supported in the default loop", loop == default_loop));
#endif
  if (ev_is_active (w))
    return;

  assert (("ev_signal_start called with illegal signal number", w->signum > 0));

  ev_start (EV_A_ (W)w, 1);
  array_needsize (signals, signalmax, w->signum, signals_init);
  wlist_add ((WL *)&signals [w->signum - 1].head, (WL)w);

  if (!w->next)
    {
      struct sigaction sa;
      sa.sa_handler = sighandler;
      sigfillset (&sa.sa_mask);
      sa.sa_flags = SA_RESTART; /* if restarting works we save one iteration */
      sigaction (w->signum, &sa, 0);
    }
}

void
ev_signal_stop (EV_P_ struct ev_signal *w)
{
  ev_clear_pending (EV_A_ (W)w);
  if (!ev_is_active (w))
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
  assert (("child watchers are only supported in the default loop", loop == default_loop));
#endif
  if (ev_is_active (w))
    return;

  ev_start (EV_A_ (W)w, 1);
  wlist_add ((WL *)&childs [w->pid & (PID_HASHSIZE - 1)], (WL)w);
}

void
ev_child_stop (EV_P_ struct ev_child *w)
{
  ev_clear_pending (EV_A_ (W)w);
  if (ev_is_active (w))
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
  free (once);

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
  struct ev_once *once = malloc (sizeof (struct ev_once));

  if (!once)
    cb (EV_ERROR | EV_READ | EV_WRITE | EV_TIMEOUT, arg);
  else
    {
      once->cb  = cb;
      once->arg = arg;

      ev_watcher_init (&once->io, once_cb_io);
      if (fd >= 0)
        {
          ev_io_set (&once->io, fd, events);
          ev_io_start (EV_A_ &once->io);
        }

      ev_watcher_init (&once->to, once_cb_to);
      if (timeout >= 0.)
        {
          ev_timer_set (&once->to, timeout, 0.);
          ev_timer_start (EV_A_ &once->to);
        }
    }
}

