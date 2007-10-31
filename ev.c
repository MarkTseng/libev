#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include <stdio.h>

#include <assert.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>

#ifdef CLOCK_MONOTONIC
# define HAVE_MONOTONIC 1
#endif

#define HAVE_REALTIME 1
#define HAVE_EPOLL 1
#define HAVE_SELECT 1

#define MIN_TIMEJUMP  1. /* minimum timejump that gets detected (if monotonic clock available) */
#define MAX_BLOCKTIME 60.

#include "ev.h"

struct ev_watcher {
  EV_WATCHER (ev_watcher);
};

struct ev_watcher_list {
  EV_WATCHER_LIST (ev_watcher_list);
};

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
      int newcnt = cur ? cur << 1 : 16;			\
      fprintf (stderr, "resize(" # base ") from %d to %d\n", cur, newcnt);\
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
  struct ev_watcher *w;
  int events;
} ANPENDING;

static ANPENDING *pendings;
static int pendingmax, pendingcnt;

static void
event (struct ev_watcher *w, int events)
{
  w->pending = ++pendingcnt;
  array_needsize (pendings, pendingmax, pendingcnt, );
  pendings [pendingcnt - 1].w      = w;
  pendings [pendingcnt - 1].events = events;
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
        event ((struct ev_watcher *)w, ev);
    }
}

static void
queue_events (struct ev_watcher **events, int eventcnt, int type)
{
  int i;

  for (i = 0; i < eventcnt; ++i)
    event (events [i], type);
}

/*****************************************************************************/

static struct ev_timer **atimers;
static int atimermax, atimercnt;

static struct ev_timer **rtimers;
static int rtimermax, rtimercnt;

static void
upheap (struct ev_timer **timers, int k)
{
  struct ev_timer *w = timers [k];

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
downheap (struct ev_timer **timers, int N, int k)
{
  struct ev_timer *w = timers [k];

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
          event ((struct ev_watcher *)w, EV_SIGNAL);
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

static struct ev_check **checks;
static int checkmax, checkcnt;

/*****************************************************************************/

#if HAVE_EPOLL
# include "ev_epoll.c"
#endif
#if HAVE_SELECT
# include "ev_select.c"
#endif

int ev_init (int flags)
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
      evw_init (&sigev, sigcb, 0);
      siginit ();
    }

  return ev_method;
}

/*****************************************************************************/

void ev_prefork (void)
{
}

void ev_postfork_parent (void)
{
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
  int i;

  for (i = 0; i < pendingcnt; ++i)
    {
      ANPENDING *p = pendings + i;

      if (p->w)
        {
          p->w->pending = 0;
          p->w->cb (p->w, p->events);
        }
    }

  pendingcnt = 0;
}

static void
timers_reify (struct ev_timer **timers, int timercnt, ev_tstamp now)
{
  while (timercnt && timers [0]->at <= now)
    {
      struct ev_timer *w = timers [0];

      /* first reschedule or stop timer */
      if (w->repeat)
        {
          if (w->is_abs)
            w->at += floor ((now - w->at) / w->repeat + 1.) * w->repeat;
          else
            w->at = now + w->repeat;

          assert (w->at > now);

          downheap (timers, timercnt, 0);
        }
      else
        {
          evtimer_stop (w); /* nonrepeating: stop timer */
          --timercnt; /* maybe pass by reference instead? */
        }

      event ((struct ev_watcher *)w, EV_TIMEOUT);
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

      /* detecting time jumps is much more difficult */
      for (i = 2; --i; ) /* loop a few times, before making important decisions */
        {
          now = get_clock ();
          diff = ev_now - now;

          if (fabs (odiff - diff) < MIN_TIMEJUMP)
            return; /* all is well */

          ev_now = ev_time ();
        }

      /* time jump detected, reschedule atimers */
      for (i = 0; i < atimercnt; ++i)
        {
          struct ev_timer *w = atimers [i];
          w->at += ceil ((ev_now - w->at) / w->repeat + 1.) * w->repeat;
        }
    }
  else
    {
      if (now > ev_now || now < ev_now - MAX_BLOCKTIME - MIN_TIMEJUMP)
        /* time jump detected, adjust rtimers */
        for (i = 0; i < rtimercnt; ++i)
          rtimers [i]->at += ev_now - now;

      now = ev_now;
    }
}

int ev_loop_done;

void ev_loop (int flags)
{
  double block;
  ev_loop_done = flags & EVLOOP_ONESHOT;

  if (checkcnt)
    {
      queue_events (checks, checkcnt, EV_CHECK);
      call_pending ();
    }

  do
    {
      /* update fd-related kernel structures */
      fd_reify ();

      /* calculate blocking time */
      if (flags & EVLOOP_NONBLOCK || idlecnt)
        block = 0.;
      else
        {
          block = MAX_BLOCKTIME;

          if (rtimercnt)
            {
              ev_tstamp to = rtimers [0]->at - get_clock () + method_fudge;
              if (block > to) block = to;
            }

          if (atimercnt)
            {
              ev_tstamp to = atimers [0]->at - ev_time   () + method_fudge;
              if (block > to) block = to;
            }

          if (block < 0.) block = 0.;
        }

      method_poll (block);

      /* update ev_now, do magic */
      time_update ();

      /* queue pending timers and reschedule them */
      /* absolute timers first */
      timers_reify (atimers, atimercnt, ev_now);
      /* relative timers second */
      timers_reify (rtimers, rtimercnt, now);

      /* queue idle watchers unless io or timers are pending */
      if (!pendingcnt)
        queue_events (idles, idlecnt, EV_IDLE);

      /* queue check and possibly idle watchers */
      queue_events (checks, checkcnt, EV_CHECK);

      call_pending ();
    }
  while (!ev_loop_done);
}

/*****************************************************************************/

static void
wlist_add (struct ev_watcher_list **head, struct ev_watcher_list *elem)
{
  elem->next = *head;
  *head = elem;
}

static void
wlist_del (struct ev_watcher_list **head, struct ev_watcher_list *elem)
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
ev_start (struct ev_watcher *w, int active)
{
  w->pending = 0;
  w->active = active;
}

static void
ev_stop (struct ev_watcher *w)
{
  if (w->pending)
    pendings [w->pending - 1].w = 0;

  w->active = 0;
  /* nop */
}

/*****************************************************************************/

void
evio_start (struct ev_io *w)
{
  if (ev_is_active (w))
    return;

  int fd = w->fd;

  ev_start ((struct ev_watcher *)w, 1);
  array_needsize (anfds, anfdmax, fd + 1, anfds_init);
  wlist_add ((struct ev_watcher_list **)&anfds[fd].head, (struct ev_watcher_list *)w);

  ++fdchangecnt;
  array_needsize (fdchanges, fdchangemax, fdchangecnt, );
  fdchanges [fdchangecnt - 1] = fd;
}

void
evio_stop (struct ev_io *w)
{
  if (!ev_is_active (w))
    return;

  wlist_del ((struct ev_watcher_list **)&anfds[w->fd].head, (struct ev_watcher_list *)w);
  ev_stop ((struct ev_watcher *)w);

  ++fdchangecnt;
  array_needsize (fdchanges, fdchangemax, fdchangecnt, );
  fdchanges [fdchangecnt - 1] = w->fd;
}

void
evtimer_start (struct ev_timer *w)
{
  if (ev_is_active (w))
    return;

  if (w->is_abs)
    {
      /* this formula differs from the one in timer_reify becuse we do not round up */
      if (w->repeat)
        w->at += ceil ((ev_now - w->at) / w->repeat) * w->repeat;

      ev_start ((struct ev_watcher *)w, ++atimercnt);
      array_needsize (atimers, atimermax, atimercnt, );
      atimers [atimercnt - 1] = w;
      upheap (atimers, atimercnt - 1);
    }
  else
    {
      w->at += now;

      ev_start ((struct ev_watcher *)w, ++rtimercnt);
      array_needsize (rtimers, rtimermax, rtimercnt, );
      rtimers [rtimercnt - 1] = w;
      upheap (rtimers, rtimercnt - 1);
    }

}

void
evtimer_stop (struct ev_timer *w)
{
  if (!ev_is_active (w))
    return;

  if (w->is_abs)
    {
      if (w->active < atimercnt--)
        {
          atimers [w->active - 1] = atimers [atimercnt];
          downheap (atimers, atimercnt, w->active - 1);
        }
    }
  else
    {
      if (w->active < rtimercnt--)
        {
          rtimers [w->active - 1] = rtimers [rtimercnt];
          downheap (rtimers, rtimercnt, w->active - 1);
        }
    }

  ev_stop ((struct ev_watcher *)w);
}

void
evsignal_start (struct ev_signal *w)
{
  if (ev_is_active (w))
    return;

  ev_start ((struct ev_watcher *)w, 1);
  array_needsize (signals, signalmax, w->signum, signals_init);
  wlist_add ((struct ev_watcher_list **)&signals [w->signum - 1].head, (struct ev_watcher_list *)w);

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
  if (!ev_is_active (w))
    return;

  wlist_del ((struct ev_watcher_list **)&signals [w->signum - 1].head, (struct ev_watcher_list *)w);
  ev_stop ((struct ev_watcher *)w);

  if (!signals [w->signum - 1].head)
    signal (w->signum, SIG_DFL);
}

void evidle_start (struct ev_idle *w)
{
  if (ev_is_active (w))
    return;

  ev_start ((struct ev_watcher *)w, ++idlecnt);
  array_needsize (idles, idlemax, idlecnt, );
  idles [idlecnt - 1] = w;
}

void evidle_stop (struct ev_idle *w)
{
  idles [w->active - 1] = idles [--idlecnt];
  ev_stop ((struct ev_watcher *)w);
}

void evcheck_start (struct ev_check *w)
{
  if (ev_is_active (w))
    return;

  ev_start ((struct ev_watcher *)w, ++checkcnt);
  array_needsize (checks, checkmax, checkcnt, );
  checks [checkcnt - 1] = w;
}

void evcheck_stop (struct ev_check *w)
{
  checks [w->active - 1] = checks [--checkcnt];
  ev_stop ((struct ev_watcher *)w);
}

/*****************************************************************************/
#if 1

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
}

static void
gcb (struct ev_signal *w, int revents)
{
  fprintf (stderr, "generic %x\n", revents);
}

int main (void)
{
  struct ev_io sin;

  ev_init (0);

  evw_init (&sin, sin_cb, 55);
  evio_set (&sin, 0, EV_READ);
  evio_start (&sin);

  struct ev_timer t[10000];

#if 0
  int i;
  for (i = 0; i < 10000; ++i)
    {
      struct ev_timer *w = t + i;
      evw_init (w, ocb, i);
      evtimer_set_abs (w, drand48 (), 0.99775533);
      evtimer_start (w);
      if (drand48 () < 0.5)
        evtimer_stop (w);
    }
#endif

  struct ev_timer t1;
  evw_init (&t1, ocb, 0);
  evtimer_set_abs (&t1, 5, 10);
  evtimer_start (&t1);

  struct ev_signal sig;
  evw_init (&sig, scb, 65535);
  evsignal_set (&sig, SIGQUIT);
  evsignal_start (&sig);

  struct ev_check cw;
  evw_init (&cw, gcb, 0);
  evcheck_start (&cw);

  struct ev_idle iw;
  evw_init (&iw, gcb, 0);
  evidle_start (&iw);

  ev_loop (0);

  return 0;
}

#endif




