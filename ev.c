#include <math.h>
#include <stdlib.h>

#include <stdio.h>

#include <errno.h>
#include <sys/time.h>
#include <time.h>

#ifdef CLOCK_MONOTONIC
# define HAVE_MONOTONIC 1
#endif

#define HAVE_EPOLL 1
#define HAVE_REALTIME 1
#define HAVE_SELECT 0

#define MAX_BLOCKTIME 60.

#include "ev.h"

struct ev_watcher {
  EV_WATCHER (ev_watcher);
};

struct ev_watcher_list {
  EV_WATCHER_LIST (ev_watcher_list);
};

ev_tstamp ev_now;
int ev_method;

static int have_monotonic; /* runtime */

static ev_tstamp method_fudge; /* stupid epoll-returns-early bug */
static void (*method_reify)(void);
static void (*method_poll)(ev_tstamp timeout);

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
          newcnt += (newcnt >> 1) + 16;			\
        }						\
      while ((cnt) > newcnt);				\
      fprintf (stderr, "resize(" # base ") from %d to %d\n", cur, newcnt);\
      base = realloc (base, sizeof (*base) * (newcnt));	\
      init (base + cur, newcnt - cur);			\
      cur = newcnt;					\
    }

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

static struct ev_timer **timers;
static int timermax, timercnt;

static void
upheap (int k)
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
downheap (int k)
{
  struct ev_timer *w = timers [k];

  while (k <= (timercnt >> 1))
    {
      int j = k << 1;

      if (j + 1 < timercnt && timers [j]->at > timers [j + 1]->at)
        ++j;

      if (w->at <= timers [j]->at)
        break;

      timers [k] = timers [j];
      timers [k]->active = k;
      k = j;
    }

  timers [k] = w;
  timers [k]->active = k + 1;
}

static struct ev_signal **signals;
static int signalmax, signalcnt;

static void
signals_init (struct ev_signal **base, int count)
{
  while (count--)
    *base++ = 0;
}

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

#if HAVE_EPOLL
  if (epoll_init (flags))
    return ev_method;
#endif
#if HAVE_SELECT
  if (select_init (flags))
    return ev_method;
#endif

  ev_method = EVMETHOD_NONE;
  return ev_method;
}

void ev_prefork (void)
{
}

void ev_postfork_parent (void)
{
}

void ev_postfork_child (void)
{
#if HAVE_EPOLL
  epoll_postfork_child ();
#endif
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
timer_reify (void)
{
  while (timercnt && timers [0]->at <= ev_now)
    {
      struct ev_timer *w = timers [0];

      /* first reschedule timer */
      if (w->repeat)
        {
          fprintf (stderr, "a %f now %f repeat %f, %f\n", w->at, ev_now, w->repeat, w->repeat *1e30);//D
          if (w->is_abs)
            w->at += floor ((ev_now - w->at) / w->repeat + 1.) * w->repeat;
          else
            w->at = ev_now + w->repeat;

          fprintf (stderr, "b %f\n", w->at);//D

          downheap (0);
        }
      else
        evtimer_stop (w); /* nonrepeating: stop timer */

      event ((struct ev_watcher *)w, EV_TIMEOUT);
    }
}

int ev_loop_done;

int ev_loop (int flags)
{
  double block;
  ev_loop_done = flags & EVLOOP_ONESHOT;

  do
    {
      /* update fd-related kernel structures */
      method_reify (); fdchangecnt = 0;

      /* calculate blocking time */
      ev_now = ev_time ();

      if (flags & EVLOOP_NONBLOCK)
        block = 0.;
      else if (!timercnt)
        block = MAX_BLOCKTIME;
      else
        {
          block = timers [0]->at - ev_now + method_fudge;
          if (block < 0.) block = 0.;
          else if (block > MAX_BLOCKTIME) block = MAX_BLOCKTIME;
        }

      fprintf (stderr, "block %f\n", block);//D
      method_poll (block);

      /* put pending timers into pendign queue and reschedule them */
      timer_reify ();

      ev_now = ev_time ();
      call_pending ();
    }
  while (!ev_loop_done);
}

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

  fprintf (stderr, "t1 %f a %d\n", w->at, w->is_abs);//D
  if (w->is_abs)
    {
      if (w->repeat)
        w->at += ceil ((ev_now - w->at) / w->repeat) * w->repeat;
    }
  else
    w->at += ev_now;
  fprintf (stderr, "t2 %f a %d\n", w->at, w->is_abs);//D

  ev_start ((struct ev_watcher *)w, ++timercnt);
  array_needsize (timers, timermax, timercnt, );
  timers [timercnt - 1] = w;
  upheap (timercnt - 1);
}

void
evtimer_stop (struct ev_timer *w)
{
  if (!ev_is_active (w))
    return;

  timers [w->active - 1] = timers [--timercnt];
  downheap (w->active - 1);
  ev_stop ((struct ev_watcher *)w);
}

void
evsignal_start (struct ev_signal *w)
{
  if (ev_is_active (w))
    return;

  ev_start ((struct ev_watcher *)w, 1);
  array_needsize (signals, signalmax, w->signum, signals_init);
  wlist_add ((struct ev_watcher_list **)&signals [w->signum - 1], (struct ev_watcher_list *)w);
}

void
evsignal_stop (struct ev_signal *w)
{
  if (!ev_is_active (w))
    return;

  wlist_del ((struct ev_watcher_list **)&signals [w->signum - 1], (struct ev_watcher_list *)w);
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
  fprintf (stderr, "timer %f,%f (%x) (%f) d%p\n", w->at, w->repeat, revents, w->at - ev_time (), w->data);
}

int main (void)
{
  struct ev_io sin;

  ev_init (0);

  evw_init (&sin, sin_cb, 55);
  evio_set (&sin, 0, EV_READ);
  evio_start (&sin);

  struct ev_timer t1;
  evw_init (&t1, ocb, 1);
  evtimer_set_rel (&t1, 1, 0);
  evtimer_start (&t1);

  struct ev_timer t2;
  evw_init (&t2, ocb, 2);
  evtimer_set_abs (&t2, ev_time () + 2, 0);
  evtimer_start (&t2);

  ev_loop (0);

  return 0;
}

#endif




