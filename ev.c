/*
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

#ifndef HAVE_MONOTONIC
# ifdef CLOCK_MONOTONIC
#  define HAVE_MONOTONIC 1
# endif
#endif

#ifndef HAVE_SELECT
# define HAVE_SELECT 1
#endif

#ifndef HAVE_EPOLL
# define HAVE_EPOLL 0
#endif

#ifndef HAVE_REALTIME
# define HAVE_REALTIME 1 /* posix requirement, but might be slower */
#endif

#define MIN_TIMEJUMP  1. /* minimum timejump that gets detected (if monotonic clock available) */
#define MAX_BLOCKTIME 60.
#define PID_HASHSIZE  16 /* size of pid hahs table, must be power of two */

#include "ev.h"

typedef struct ev_watcher *W;
typedef struct ev_watcher_list *WL;
typedef struct ev_watcher_time *WT;

static ev_tstamp now, diff; /* monotonic clock */
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
#if HAVE_REALTIME
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
#if HAVE_MONOTONIC
  if (have_monotonic)
    {
      struct timespec ts;
      clock_gettime (CLOCK_MONOTONIC, &ts);
      return ts.tv_sec + ts.tv_nsec * 1e-9;
    }
#endif

  return ev_time ();
}

#define array_needsize(base,cur,cnt,init)		\
  if ((cnt) > cur)					\
    {							\
      int newcnt = cur;					\
      do						\
        {						\
          newcnt = (newcnt << 1) | 4 & ~3;		\
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
  unsigned char wev, rev; /* want, received event set */
} ANFD;

static ANFD *anfds;
static int anfdmax;

static int *fdchanges;
static int fdchangemax, fdchangecnt;

static void
anfds_init (ANFD *base, int count)
{
  while (count--)
    {
      base->head = 0;
      base->wev = base->rev = EV_NONE;
      ++base;
    }
}

typedef struct
{
  W w;
  int events;
} ANPENDING;

static ANPENDING *pendings;
static int pendingmax, pendingcnt;

static void
event (W w, int events)
{
  if (w->active)
    {
      w->pending = ++pendingcnt;
      array_needsize (pendings, pendingmax, pendingcnt, );
      pendings [pendingcnt - 1].w      = w;
      pendings [pendingcnt - 1].events = events;
    }
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

static void
queue_events (W *events, int eventcnt, int type)
{
  int i;

  for (i = 0; i < eventcnt; ++i)
    event (events [i], type);
}

/* called on EBADF to verify fds */
static void
fd_recheck ()
{
  int fd;

  for (fd = 0; fd < anfdmax; ++fd)
    if (anfds [fd].wev)
      if (fcntl (fd, F_GETFD) == -1 && errno == EBADF)
        while (anfds [fd].head)
          evio_stop (anfds [fd].head);
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
  sig_atomic_t gotsig;
} ANSIG;

static ANSIG *signals;
static int signalmax;

static int sigpipe [2];
static sig_atomic_t gotsig;
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
      write (sigpipe [1], &gotsig, 1);
    }
}

static void
sigcb (struct ev_io *iow, int revents)
{
  struct ev_signal *w;
  int sig;

  gotsig = 0;
  read (sigpipe [0], &revents, 1);

  for (sig = signalmax; sig--; )
    if (signals [sig].gotsig)
      {
        signals [sig].gotsig = 0;

        for (w = signals [sig].head; w; w = w->next)
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

  evio_set (&sigev, sigpipe [0], EV_READ);
  evio_start (&sigev);
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
      if (w->pid == pid || w->pid == -1)
        {
          w->status = status;
          event ((W)w, EV_CHILD);
        }
}

/*****************************************************************************/

#if HAVE_EPOLL
# include "ev_epoll.c"
#endif
#if HAVE_SELECT
# include "ev_select.c"
#endif

int ev_init (int flags)
{
  if (!ev_method)
    {
#if HAVE_MONOTONIC
      {
        struct timespec ts;
        if (!clock_gettime (CLOCK_MONOTONIC, &ts))
          have_monotonic = 1;
      }
#endif

      ev_now = ev_time ();
      now    = get_clock ();
      diff   = ev_now - now;

      if (pipe (sigpipe))
        return 0;

      ev_method = EVMETHOD_NONE;
#if HAVE_EPOLL
      if (ev_method == EVMETHOD_NONE) epoll_init (flags);
#endif
#if HAVE_SELECT
      if (ev_method == EVMETHOD_NONE) select_init (flags);
#endif

      if (ev_method)
        {
          evw_init (&sigev, sigcb);
          siginit ();

          evsignal_init (&childev, childcb, SIGCHLD);
          evsignal_start (&childev);
        }
    }

  return ev_method;
}

/*****************************************************************************/

void ev_prefork (void)
{
  /* nop */
}

void ev_postfork_parent (void)
{
  /* nop */
}

void ev_postfork_child (void)
{
#if HAVE_EPOLL
  if (ev_method == EVMETHOD_EPOLL)
    epoll_postfork_child ();
#endif

  evio_stop (&sigev);
  close (sigpipe [0]);
  close (sigpipe [1]);
  pipe (sigpipe);
  siginit ();
}

/*****************************************************************************/

static void
fd_reify (void)
{
  int i;

  for (i = 0; i < fdchangecnt; ++i)
    {
      int fd = fdchanges [i];
      ANFD *anfd = anfds + fd;
      struct ev_io *w;

      int wev = 0;

      for (w = anfd->head; w; w = w->next)
        wev |= w->events;

      if (anfd->wev != wev)
        {
          method_modify (fd, anfd->wev, wev);
          anfd->wev = wev;
        }
    }

  fdchangecnt = 0;
}

static void
call_pending ()
{
  while (pendingcnt)
    {
      ANPENDING *p = pendings + --pendingcnt;

      if (p->w)
        {
          p->w->pending = 0;
          p->w->cb (p->w, p->events);
        }
    }
}

static void
timers_reify ()
{
  while (timercnt && timers [0]->at <= now)
    {
      struct ev_timer *w = timers [0];

      event ((W)w, EV_TIMEOUT);

      /* first reschedule or stop timer */
      if (w->repeat)
        {
          w->at = now + w->repeat;
          assert (("timer timeout in the past, negative repeat?", w->at > now));
          downheap ((WT *)timers, timercnt, 0);
        }
      else
        evtimer_stop (w); /* nonrepeating: stop timer */
    }
}

static void
periodics_reify ()
{
  while (periodiccnt && periodics [0]->at <= ev_now)
    {
      struct ev_periodic *w = periodics [0];

      /* first reschedule or stop timer */
      if (w->interval)
        {
          w->at += floor ((ev_now - w->at) / w->interval + 1.) * w->interval;
          assert (("periodic timeout in the past, negative interval?", w->at > ev_now));
          downheap ((WT *)periodics, periodiccnt, 0);
        }
      else
        evperiodic_stop (w); /* nonrepeating: stop timer */

      event ((W)w, EV_TIMEOUT);
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
              evperiodic_stop (w);
              evperiodic_start (w);

              i = 0; /* restart loop, inefficient, but time jumps should be rare */
            }
        }
    }
}

static void
time_update ()
{
  int i;

  ev_now = ev_time ();

  if (have_monotonic)
    {
      ev_tstamp odiff = diff;

      for (i = 4; --i; ) /* loop a few times, before making important decisions */
        {
          now = get_clock ();
          diff = ev_now - now;

          if (fabs (odiff - diff) < MIN_TIMEJUMP)
            return; /* all is well */

          ev_now = ev_time ();
        }

      periodics_reschedule (diff - odiff);
      /* no timer adjustment, as the monotonic clock doesn't jump */
    }
  else
    {
      if (now > ev_now || now < ev_now - MAX_BLOCKTIME - MIN_TIMEJUMP)
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
  ev_loop_done = flags & EVLOOP_ONESHOT ? 1 : 0;

  do
    {
      /* queue check watchers (and execute them) */
      if (preparecnt)
        {
          queue_events ((W *)prepares, preparecnt, EV_PREPARE);
          call_pending ();
        }

      /* update fd-related kernel structures */
      fd_reify ();

      /* calculate blocking time */

      /* we only need this for !monotonic clockor timers, but as we basically
         always have timers, we just calculate it always */
      ev_now = ev_time ();

      if (flags & EVLOOP_NONBLOCK || idlecnt)
        block = 0.;
      else
        {
          block = MAX_BLOCKTIME;

          if (timercnt)
            {
              ev_tstamp to = timers [0]->at - (have_monotonic ? get_clock () : ev_now) + method_fudge;
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
ev_clear (W w)
{
  if (w->pending)
    {
      pendings [w->pending - 1].w = 0;
      w->pending = 0;
    }
}

static void
ev_start (W w, int active)
{
  w->active = active;
}

static void
ev_stop (W w)
{
  w->active = 0;
}

/*****************************************************************************/

void
evio_start (struct ev_io *w)
{
  if (ev_is_active (w))
    return;

  int fd = w->fd;

  ev_start ((W)w, 1);
  array_needsize (anfds, anfdmax, fd + 1, anfds_init);
  wlist_add ((WL *)&anfds[fd].head, (WL)w);

  ++fdchangecnt;
  array_needsize (fdchanges, fdchangemax, fdchangecnt, );
  fdchanges [fdchangecnt - 1] = fd;
}

void
evio_stop (struct ev_io *w)
{
  ev_clear ((W)w);
  if (!ev_is_active (w))
    return;

  wlist_del ((WL *)&anfds[w->fd].head, (WL)w);
  ev_stop ((W)w);

  ++fdchangecnt;
  array_needsize (fdchanges, fdchangemax, fdchangecnt, );
  fdchanges [fdchangecnt - 1] = w->fd;
}

void
evtimer_start (struct ev_timer *w)
{
  if (ev_is_active (w))
    return;

  w->at += now;

  assert (("timer repeat value less than zero not allowed", w->repeat >= 0.));

  ev_start ((W)w, ++timercnt);
  array_needsize (timers, timermax, timercnt, );
  timers [timercnt - 1] = w;
  upheap ((WT *)timers, timercnt - 1);
}

void
evtimer_stop (struct ev_timer *w)
{
  ev_clear ((W)w);
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
evtimer_again (struct ev_timer *w)
{
  if (ev_is_active (w))
    {
      if (w->repeat)
        {
          w->at = now + w->repeat;
          downheap ((WT *)timers, timercnt, w->active - 1);
        }
      else
        evtimer_stop (w);
    }
  else if (w->repeat)
    evtimer_start (w);
}

void
evperiodic_start (struct ev_periodic *w)
{
  if (ev_is_active (w))
    return;

  assert (("periodic interval value less than zero not allowed", w->interval >= 0.));

  /* this formula differs from the one in periodic_reify because we do not always round up */
  if (w->interval)
    w->at += ceil ((ev_now - w->at) / w->interval) * w->interval;

  ev_start ((W)w, ++periodiccnt);
  array_needsize (periodics, periodicmax, periodiccnt, );
  periodics [periodiccnt - 1] = w;
  upheap ((WT *)periodics, periodiccnt - 1);
}

void
evperiodic_stop (struct ev_periodic *w)
{
  ev_clear ((W)w);
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
evsignal_start (struct ev_signal *w)
{
  if (ev_is_active (w))
    return;

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
evsignal_stop (struct ev_signal *w)
{
  ev_clear ((W)w);
  if (!ev_is_active (w))
    return;

  wlist_del ((WL *)&signals [w->signum - 1].head, (WL)w);
  ev_stop ((W)w);

  if (!signals [w->signum - 1].head)
    signal (w->signum, SIG_DFL);
}

void evidle_start (struct ev_idle *w)
{
  if (ev_is_active (w))
    return;

  ev_start ((W)w, ++idlecnt);
  array_needsize (idles, idlemax, idlecnt, );
  idles [idlecnt - 1] = w;
}

void evidle_stop (struct ev_idle *w)
{
  ev_clear ((W)w);
  if (ev_is_active (w))
    return;

  idles [w->active - 1] = idles [--idlecnt];
  ev_stop ((W)w);
}

void evprepare_start (struct ev_prepare *w)
{
  if (ev_is_active (w))
    return;

  ev_start ((W)w, ++preparecnt);
  array_needsize (prepares, preparemax, preparecnt, );
  prepares [preparecnt - 1] = w;
}

void evprepare_stop (struct ev_prepare *w)
{
  ev_clear ((W)w);
  if (ev_is_active (w))
    return;

  prepares [w->active - 1] = prepares [--preparecnt];
  ev_stop ((W)w);
}

void evcheck_start (struct ev_check *w)
{
  if (ev_is_active (w))
    return;

  ev_start ((W)w, ++checkcnt);
  array_needsize (checks, checkmax, checkcnt, );
  checks [checkcnt - 1] = w;
}

void evcheck_stop (struct ev_check *w)
{
  ev_clear ((W)w);
  if (ev_is_active (w))
    return;

  checks [w->active - 1] = checks [--checkcnt];
  ev_stop ((W)w);
}

void evchild_start (struct ev_child *w)
{
  if (ev_is_active (w))
    return;

  ev_start ((W)w, 1);
  wlist_add ((WL *)&childs [w->pid & (PID_HASHSIZE - 1)], (WL)w);
}

void evchild_stop (struct ev_child *w)
{
  ev_clear ((W)w);
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

  evio_stop (&once->io);
  evtimer_stop (&once->to);
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
    cb (EV_ERROR, arg);
  else
    {
      once->cb  = cb;
      once->arg = arg;

      evw_init (&once->io, once_cb_io);

      if (fd >= 0)
        {
          evio_set (&once->io, fd, events);
          evio_start (&once->io);
        }

      evw_init (&once->to, once_cb_to);

      if (timeout >= 0.)
        {
          evtimer_set (&once->to, timeout, 0.);
          evtimer_start (&once->to);
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
  evtimer_stop (w);
  evtimer_start (w);
}

static void
scb (struct ev_signal *w, int revents)
{
  fprintf (stderr, "signal %x,%d\n", revents, w->signum);
  evio_stop (&wio);
  evio_start (&wio);
}

static void
gcb (struct ev_signal *w, int revents)
{
  fprintf (stderr, "generic %x\n", revents);

}

int main (void)
{
  ev_init (0);

  evio_init (&wio, sin_cb, 0, EV_READ);
  evio_start (&wio);

  struct ev_timer t[10000];

#if 0
  int i;
  for (i = 0; i < 10000; ++i)
    {
      struct ev_timer *w = t + i;
      evw_init (w, ocb, i);
      evtimer_init_abs (w, ocb, drand48 (), 0.99775533);
      evtimer_start (w);
      if (drand48 () < 0.5)
        evtimer_stop (w);
    }
#endif

  struct ev_timer t1;
  evtimer_init (&t1, ocb, 5, 10);
  evtimer_start (&t1);

  struct ev_signal sig;
  evsignal_init (&sig, scb, SIGQUIT);
  evsignal_start (&sig);

  struct ev_check cw;
  evcheck_init (&cw, gcb);
  evcheck_start (&cw);

  struct ev_idle iw;
  evidle_init (&iw, gcb);
  evidle_start (&iw);

  ev_loop (0);

  return 0;
}

#endif




