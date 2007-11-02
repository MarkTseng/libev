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
#if EV_USE_CONFIG_H
# include "config.h"
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
#include <sys/wait.h>
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

static ev_tstamp now_floor, now, diff; /* monotonic clock */
ev_tstamp ev_now;
int ev_method;

static int have_monotonic; /* runtime */

static ev_tstamp method_fudge; /* stupid epoll-returns-early bug */
static void (*method_modify)(int fd, int oev, int nev);
static void (*method_poll)(ev_tstamp timeout);

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

static ev_tstamp
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

typedef struct
{
  struct ev_io *head;
  unsigned char events;
  unsigned char reify;
} ANFD;

static ANFD *anfds;
static int anfdmax;

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

typedef struct
{
  W w;
  int events;
} ANPENDING;

static ANPENDING *pendings [NUMPRI];
static int pendingmax [NUMPRI], pendingcnt [NUMPRI];

static void
event (W w, int events)
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
queue_events (W *events, int eventcnt, int type)
{
  int i;

  for (i = 0; i < eventcnt; ++i)
    event (events [i], type);
}

static void
fd_event (int fd, int events)
{
  ANFD *anfd = anfds + fd;
  struct ev_io *w;

  for (w = anfd->head; w; w = w->next)
    {
      int ev = w->events & events;

      if (ev)
        event ((W)w, ev);
    }
}

/*****************************************************************************/

static int *fdchanges;
static int fdchangemax, fdchangecnt;

static void
fd_reify (void)
{
  int i;

  for (i = 0; i < fdchangecnt; ++i)
    {
      int fd = fdchanges [i];
      ANFD *anfd = anfds + fd;
      struct ev_io *w;

      int events = 0;

      for (w = anfd->head; w; w = w->next)
        events |= w->events;

      anfd->reify = 0;

      if (anfd->events != events)
        {
          method_modify (fd, anfd->events, events);
          anfd->events = events;
        }
    }

  fdchangecnt = 0;
}

static void
fd_change (int fd)
{
  if (anfds [fd].reify || fdchangecnt < 0)
    return;

  anfds [fd].reify = 1;

  ++fdchangecnt;
  array_needsize (fdchanges, fdchangemax, fdchangecnt, );
  fdchanges [fdchangecnt - 1] = fd;
}

static void
fd_kill (int fd)
{
  struct ev_io *w;

  printf ("killing fd %d\n", fd);//D
  while ((w = anfds [fd].head))
    {
      ev_io_stop (w);
      event ((W)w, EV_ERROR | EV_READ | EV_WRITE);
    }
}

/* called on EBADF to verify fds */
static void
fd_ebadf (void)
{
  int fd;

  for (fd = 0; fd < anfdmax; ++fd)
    if (anfds [fd].events)
      if (fcntl (fd, F_GETFD) == -1 && errno == EBADF)
        fd_kill (fd);
}

/* called on ENOMEM in select/poll to kill some fds and retry */
static void
fd_enomem (void)
{
  int fd = anfdmax;

  while (fd--)
    if (anfds [fd].events)
      {
        close (fd);
        fd_kill (fd);
        return;
      }
}

/*****************************************************************************/

static struct ev_timer **timers;
static int timermax, timercnt;

static struct ev_periodic **periodics;
static int periodicmax, periodiccnt;

static void
upheap (WT *timers, int k)
{
  WT w = timers [k];

  while (k && timers [k >> 1]->at > w->at)
    {
      timers [k] = timers [k >> 1];
      timers [k]->active = k + 1;
      k >>= 1;
    }

  timers [k] = w;
  timers [k]->active = k + 1;

}

static void
downheap (WT *timers, int N, int k)
{
  WT w = timers [k];

  while (k < (N >> 1))
    {
      int j = k << 1;

      if (j + 1 < N && timers [j]->at > timers [j + 1]->at)
        ++j;

      if (w->at <= timers [j]->at)
        break;

      timers [k] = timers [j];
      timers [k]->active = k + 1;
      k = j;
    }

  timers [k] = w;
  timers [k]->active = k + 1;
}

/*****************************************************************************/

typedef struct
{
  struct ev_signal *head;
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
      gotsig = 1;
      write (sigpipe [1], &signum, 1);
    }
}

static void
sigcb (struct ev_io *iow, int revents)
{
  struct ev_signal *w;
  int signum;

  read (sigpipe [0], &revents, 1);
  gotsig = 0;

  for (signum = signalmax; signum--; )
    if (signals [signum].gotsig)
      {
        signals [signum].gotsig = 0;

        for (w = signals [signum].head; w; w = w->next)
          event ((W)w, EV_SIGNAL);
      }
}

static void
siginit (void)
{
  fcntl (sigpipe [0], F_SETFD, FD_CLOEXEC);
  fcntl (sigpipe [1], F_SETFD, FD_CLOEXEC);

  /* rather than sort out wether we really need nb, set it */
  fcntl (sigpipe [0], F_SETFL, O_NONBLOCK);
  fcntl (sigpipe [1], F_SETFL, O_NONBLOCK);

  ev_io_set (&sigev, sigpipe [0], EV_READ);
  ev_io_start (&sigev);
}

/*****************************************************************************/

static struct ev_idle **idles;
static int idlemax, idlecnt;

static struct ev_prepare **prepares;
static int preparemax, preparecnt;

static struct ev_check **checks;
static int checkmax, checkcnt;

/*****************************************************************************/

static struct ev_child *childs [PID_HASHSIZE];
static struct ev_signal childev;

#ifndef WCONTINUED
# define WCONTINUED 0
#endif

static void
childcb (struct ev_signal *sw, int revents)
{
  struct ev_child *w;
  int pid, status;

  while ((pid = waitpid (-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) != -1)
    for (w = childs [pid & (PID_HASHSIZE - 1)]; w; w = w->next)
      if (w->pid == pid || !w->pid)
        {
          w->status = status;
          event ((W)w, EV_CHILD);
        }
}

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

/* return true if we are running with elevated privileges and ignore env variables */
static int
enable_secure ()
{
  return getuid () != geteuid ()
      || getgid () != getegid ();
}

int ev_init (int methods)
{
  if (!ev_method)
    {
#if EV_USE_MONOTONIC
      {
        struct timespec ts;
        if (!clock_gettime (CLOCK_MONOTONIC, &ts))
          have_monotonic = 1;
      }
#endif

      ev_now    = ev_time ();
      now       = get_clock ();
      now_floor = now;
      diff      = ev_now - now;

      if (pipe (sigpipe))
        return 0;

      if (methods == EVMETHOD_AUTO)
          if (!enable_secure () && getenv ("LIBEV_METHODS"))
            methods = atoi (getenv ("LIBEV_METHODS"));
          else
            methods = EVMETHOD_ANY;

      ev_method = 0;
#if EV_USE_KQUEUE
      if (!ev_method && (methods & EVMETHOD_KQUEUE)) kqueue_init (methods);
#endif
#if EV_USE_EPOLL
      if (!ev_method && (methods & EVMETHOD_EPOLL )) epoll_init  (methods);
#endif
#if EV_USE_POLL
      if (!ev_method && (methods & EVMETHOD_POLL  )) poll_init   (methods);
#endif
#if EV_USE_SELECT
      if (!ev_method && (methods & EVMETHOD_SELECT)) select_init (methods);
#endif

      if (ev_method)
        {
          ev_watcher_init (&sigev, sigcb);
          siginit ();

          ev_signal_init (&childev, childcb, SIGCHLD);
          ev_signal_start (&childev);
        }
    }

  return ev_method;
}

/*****************************************************************************/

void
ev_fork_prepare (void)
{
  /* nop */
}

void
ev_fork_parent (void)
{
  /* nop */
}

void
ev_fork_child (void)
{
#if EV_USE_EPOLL
  if (ev_method == EVMETHOD_EPOLL)
    epoll_postfork_child ();
#endif

  ev_io_stop (&sigev);
  close (sigpipe [0]);
  close (sigpipe [1]);
  pipe (sigpipe);
  siginit ();
}

/*****************************************************************************/

static void
call_pending (void)
{
  int pri;

  for (pri = NUMPRI; pri--; )
    while (pendingcnt [pri])
      {
        ANPENDING *p = pendings [pri] + --pendingcnt [pri];

        if (p->w)
          {
            p->w->pending = 0;
            p->w->cb (p->w, p->events);
          }
      }
}

static void
timers_reify (void)
{
  while (timercnt && timers [0]->at <= now)
    {
      struct ev_timer *w = timers [0];

      /* first reschedule or stop timer */
      if (w->repeat)
        {
          assert (("negative ev_timer repeat value found while processing timers", w->repeat > 0.));
          w->at = now + w->repeat;
          downheap ((WT *)timers, timercnt, 0);
        }
      else
        ev_timer_stop (w); /* nonrepeating: stop timer */

      event ((W)w, EV_TIMEOUT);
    }
}

static void
periodics_reify (void)
{
  while (periodiccnt && periodics [0]->at <= ev_now)
    {
      struct ev_periodic *w = periodics [0];

      /* first reschedule or stop timer */
      if (w->interval)
        {
          w->at += floor ((ev_now - w->at) / w->interval + 1.) * w->interval;
          assert (("ev_periodic timeout in the past detected while processing timers, negative interval?", w->at > ev_now));
          downheap ((WT *)periodics, periodiccnt, 0);
        }
      else
        ev_periodic_stop (w); /* nonrepeating: stop timer */

      event ((W)w, EV_PERIODIC);
    }
}

static void
periodics_reschedule (ev_tstamp diff)
{
  int i;

  /* adjust periodics after time jump */
  for (i = 0; i < periodiccnt; ++i)
    {
      struct ev_periodic *w = periodics [i];

      if (w->interval)
        {
          ev_tstamp diff = ceil ((ev_now - w->at) / w->interval) * w->interval;

          if (fabs (diff) >= 1e-4)
            {
              ev_periodic_stop (w);
              ev_periodic_start (w);

              i = 0; /* restart loop, inefficient, but time jumps should be rare */
            }
        }
    }
}

static int
time_update_monotonic (void)
{
  now = get_clock ();

  if (expect_true (now - now_floor < MIN_TIMEJUMP * .5))
    {
      ev_now = now + diff;
      return 0;
    }
  else
    {
      now_floor = now;
      ev_now = ev_time ();
      return 1;
    }
}

static void
time_update (void)
{
  int i;

#if EV_USE_MONOTONIC
  if (expect_true (have_monotonic))
    {
      if (time_update_monotonic ())
        {
          ev_tstamp odiff = diff;

          for (i = 4; --i; ) /* loop a few times, before making important decisions */
            {
              diff = ev_now - now;

              if (fabs (odiff - diff) < MIN_TIMEJUMP)
                return; /* all is well */

              ev_now    = ev_time ();
              now       = get_clock ();
              now_floor = now;
            }

          periodics_reschedule (diff - odiff);
          /* no timer adjustment, as the monotonic clock doesn't jump */
        }
    }
  else
#endif
    {
      ev_now = ev_time ();

      if (expect_false (now > ev_now || now < ev_now - MAX_BLOCKTIME - MIN_TIMEJUMP))
        {
          periodics_reschedule (ev_now - now);

          /* adjust timers. this is easy, as the offset is the same for all */
          for (i = 0; i < timercnt; ++i)
            timers [i]->at += diff;
        }

      now = ev_now;
    }
}

int ev_loop_done;

void ev_loop (int flags)
{
  double block;
  ev_loop_done = flags & (EVLOOP_ONESHOT | EVLOOP_NONBLOCK) ? 1 : 0;

  do
    {
      /* queue check watchers (and execute them) */
      if (expect_false (preparecnt))
        {
          queue_events ((W *)prepares, preparecnt, EV_PREPARE);
          call_pending ();
        }

      /* update fd-related kernel structures */
      fd_reify ();

      /* calculate blocking time */

      /* we only need this for !monotonic clockor timers, but as we basically
         always have timers, we just calculate it always */
#if EV_USE_MONOTONIC
      if (expect_true (have_monotonic))
        time_update_monotonic ();
      else
#endif
        {
          ev_now = ev_time ();
          now    = ev_now;
        }

      if (flags & EVLOOP_NONBLOCK || idlecnt)
        block = 0.;
      else
        {
          block = MAX_BLOCKTIME;

          if (timercnt)
            {
              ev_tstamp to = timers [0]->at - now + method_fudge;
              if (block > to) block = to;
            }

          if (periodiccnt)
            {
              ev_tstamp to = periodics [0]->at - ev_now + method_fudge;
              if (block > to) block = to;
            }

          if (block < 0.) block = 0.;
        }

      method_poll (block);

      /* update ev_now, do magic */
      time_update ();

      /* queue pending timers and reschedule them */
      timers_reify (); /* relative timers called last */
      periodics_reify (); /* absolute timers called first */

      /* queue idle watchers unless io or timers are pending */
      if (!pendingcnt)
        queue_events ((W *)idles, idlecnt, EV_IDLE);

      /* queue check watchers, to be executed first */
      if (checkcnt)
        queue_events ((W *)checks, checkcnt, EV_CHECK);

      call_pending ();
    }
  while (!ev_loop_done);

  if (ev_loop_done != 2)
    ev_loop_done = 0;
}

/*****************************************************************************/

static void
wlist_add (WL *head, WL elem)
{
  elem->next = *head;
  *head = elem;
}

static void
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

static void
ev_clear_pending (W w)
{
  if (w->pending)
    {
      pendings [ABSPRI (w)][w->pending - 1].w = 0;
      w->pending = 0;
    }
}

static void
ev_start (W w, int active)
{
  if (w->priority < EV_MINPRI) w->priority = EV_MINPRI;
  if (w->priority > EV_MAXPRI) w->priority = EV_MAXPRI;

  w->active = active;
}

static void
ev_stop (W w)
{
  w->active = 0;
}

/*****************************************************************************/

void
ev_io_start (struct ev_io *w)
{
  int fd = w->fd;

  if (ev_is_active (w))
    return;

  assert (("ev_io_start called with negative fd", fd >= 0));

  ev_start ((W)w, 1);
  array_needsize (anfds, anfdmax, fd + 1, anfds_init);
  wlist_add ((WL *)&anfds[fd].head, (WL)w);

  fd_change (fd);
}

void
ev_io_stop (struct ev_io *w)
{
  ev_clear_pending ((W)w);
  if (!ev_is_active (w))
    return;

  wlist_del ((WL *)&anfds[w->fd].head, (WL)w);
  ev_stop ((W)w);

  fd_change (w->fd);
}

void
ev_timer_start (struct ev_timer *w)
{
  if (ev_is_active (w))
    return;

  w->at += now;

  assert (("ev_timer_start called with negative timer repeat value", w->repeat >= 0.));

  ev_start ((W)w, ++timercnt);
  array_needsize (timers, timermax, timercnt, );
  timers [timercnt - 1] = w;
  upheap ((WT *)timers, timercnt - 1);
}

void
ev_timer_stop (struct ev_timer *w)
{
  ev_clear_pending ((W)w);
  if (!ev_is_active (w))
    return;

  if (w->active < timercnt--)
    {
      timers [w->active - 1] = timers [timercnt];
      downheap ((WT *)timers, timercnt, w->active - 1);
    }

  w->at = w->repeat;

  ev_stop ((W)w);
}

void
ev_timer_again (struct ev_timer *w)
{
  if (ev_is_active (w))
    {
      if (w->repeat)
        {
          w->at = now + w->repeat;
          downheap ((WT *)timers, timercnt, w->active - 1);
        }
      else
        ev_timer_stop (w);
    }
  else if (w->repeat)
    ev_timer_start (w);
}

void
ev_periodic_start (struct ev_periodic *w)
{
  if (ev_is_active (w))
    return;

  assert (("ev_periodic_start called with negative interval value", w->interval >= 0.));

  /* this formula differs from the one in periodic_reify because we do not always round up */
  if (w->interval)
    w->at += ceil ((ev_now - w->at) / w->interval) * w->interval;

  ev_start ((W)w, ++periodiccnt);
  array_needsize (periodics, periodicmax, periodiccnt, );
  periodics [periodiccnt - 1] = w;
  upheap ((WT *)periodics, periodiccnt - 1);
}

void
ev_periodic_stop (struct ev_periodic *w)
{
  ev_clear_pending ((W)w);
  if (!ev_is_active (w))
    return;

  if (w->active < periodiccnt--)
    {
      periodics [w->active - 1] = periodics [periodiccnt];
      downheap ((WT *)periodics, periodiccnt, w->active - 1);
    }

  ev_stop ((W)w);
}

void
ev_signal_start (struct ev_signal *w)
{
  if (ev_is_active (w))
    return;

  assert (("ev_signal_start called with illegal signal number", w->signum > 0));

  ev_start ((W)w, 1);
  array_needsize (signals, signalmax, w->signum, signals_init);
  wlist_add ((WL *)&signals [w->signum - 1].head, (WL)w);

  if (!w->next)
    {
      struct sigaction sa;
      sa.sa_handler = sighandler;
      sigfillset (&sa.sa_mask);
      sa.sa_flags = 0;
      sigaction (w->signum, &sa, 0);
    }
}

void
ev_signal_stop (struct ev_signal *w)
{
  ev_clear_pending ((W)w);
  if (!ev_is_active (w))
    return;

  wlist_del ((WL *)&signals [w->signum - 1].head, (WL)w);
  ev_stop ((W)w);

  if (!signals [w->signum - 1].head)
    signal (w->signum, SIG_DFL);
}

void
ev_idle_start (struct ev_idle *w)
{
  if (ev_is_active (w))
    return;

  ev_start ((W)w, ++idlecnt);
  array_needsize (idles, idlemax, idlecnt, );
  idles [idlecnt - 1] = w;
}

void
ev_idle_stop (struct ev_idle *w)
{
  ev_clear_pending ((W)w);
  if (ev_is_active (w))
    return;

  idles [w->active - 1] = idles [--idlecnt];
  ev_stop ((W)w);
}

void
ev_prepare_start (struct ev_prepare *w)
{
  if (ev_is_active (w))
    return;

  ev_start ((W)w, ++preparecnt);
  array_needsize (prepares, preparemax, preparecnt, );
  prepares [preparecnt - 1] = w;
}

void
ev_prepare_stop (struct ev_prepare *w)
{
  ev_clear_pending ((W)w);
  if (ev_is_active (w))
    return;

  prepares [w->active - 1] = prepares [--preparecnt];
  ev_stop ((W)w);
}

void
ev_check_start (struct ev_check *w)
{
  if (ev_is_active (w))
    return;

  ev_start ((W)w, ++checkcnt);
  array_needsize (checks, checkmax, checkcnt, );
  checks [checkcnt - 1] = w;
}

void
ev_check_stop (struct ev_check *w)
{
  ev_clear_pending ((W)w);
  if (ev_is_active (w))
    return;

  checks [w->active - 1] = checks [--checkcnt];
  ev_stop ((W)w);
}

void
ev_child_start (struct ev_child *w)
{
  if (ev_is_active (w))
    return;

  ev_start ((W)w, 1);
  wlist_add ((WL *)&childs [w->pid & (PID_HASHSIZE - 1)], (WL)w);
}

void
ev_child_stop (struct ev_child *w)
{
  ev_clear_pending ((W)w);
  if (ev_is_active (w))
    return;

  wlist_del ((WL *)&childs [w->pid & (PID_HASHSIZE - 1)], (WL)w);
  ev_stop ((W)w);
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
once_cb (struct ev_once *once, int revents)
{
  void (*cb)(int revents, void *arg) = once->cb;
  void *arg = once->arg;

  ev_io_stop (&once->io);
  ev_timer_stop (&once->to);
  free (once);

  cb (revents, arg);
}

static void
once_cb_io (struct ev_io *w, int revents)
{
  once_cb ((struct ev_once *)(((char *)w) - offsetof (struct ev_once, io)), revents);
}

static void
once_cb_to (struct ev_timer *w, int revents)
{
  once_cb ((struct ev_once *)(((char *)w) - offsetof (struct ev_once, to)), revents);
}

void
ev_once (int fd, int events, ev_tstamp timeout, void (*cb)(int revents, void *arg), void *arg)
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
          ev_io_start (&once->io);
        }

      ev_watcher_init (&once->to, once_cb_to);
      if (timeout >= 0.)
        {
          ev_timer_set (&once->to, timeout, 0.);
          ev_timer_start (&once->to);
        }
    }
}

/*****************************************************************************/

#if 0

struct ev_io wio;

static void
sin_cb (struct ev_io *w, int revents)
{
  fprintf (stderr, "sin %d, revents %d\n", w->fd, revents);
}

static void
ocb (struct ev_timer *w, int revents)
{
  //fprintf (stderr, "timer %f,%f (%x) (%f) d%p\n", w->at, w->repeat, revents, w->at - ev_time (), w->data);
  ev_timer_stop (w);
  ev_timer_start (w);
}

static void
scb (struct ev_signal *w, int revents)
{
  fprintf (stderr, "signal %x,%d\n", revents, w->signum);
  ev_io_stop (&wio);
  ev_io_start (&wio);
}

static void
gcb (struct ev_signal *w, int revents)
{
  fprintf (stderr, "generic %x\n", revents);

}

int main (void)
{
  ev_init (0);

  ev_io_init (&wio, sin_cb, 0, EV_READ);
  ev_io_start (&wio);

  struct ev_timer t[10000];

#if 0
  int i;
  for (i = 0; i < 10000; ++i)
    {
      struct ev_timer *w = t + i;
      ev_watcher_init (w, ocb, i);
      ev_timer_init_abs (w, ocb, drand48 (), 0.99775533);
      ev_timer_start (w);
      if (drand48 () < 0.5)
        ev_timer_stop (w);
    }
#endif

  struct ev_timer t1;
  ev_timer_init (&t1, ocb, 5, 10);
  ev_timer_start (&t1);

  struct ev_signal sig;
  ev_signal_init (&sig, scb, SIGQUIT);
  ev_signal_start (&sig);

  struct ev_check cw;
  ev_check_init (&cw, gcb);
  ev_check_start (&cw);

  struct ev_idle iw;
  ev_idle_init (&iw, gcb);
  ev_idle_start (&iw);

  ev_loop (0);

  return 0;
}

#endif




