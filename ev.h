#ifndef EV_H
#define EV_H

typedef double ev_tstamp;

/* eventmask, revents, events... */
#define EV_UNDEF   -1 /* guaranteed to be invalid */
#define EV_NONE    0x00
#define EV_READ    0x01
#define EV_WRITE   0x02
#define EV_TIMEOUT 0x04
#define EV_SIGNAL  0x08
#define EV_IDLE    0x10
#define EV_CHECK   0x20

/* can be used to add custom fields to all watchers */
#ifndef EV_COMMON
# define EV_COMMON void *data
#endif

/*
 * struct member types:
 * private: you can look at them, but not change them, and they might not mean anything to you.
 * ro: can be read anytime, but only changed when the watcher isn't active
 * rw: can be read and modified anytime, even when the watcher is active
 */

/* shared by all watchers */
#define EV_WATCHER(type)			\
  int active; /* private */			\
  int pending; /* private */			\
  EV_COMMON; /* rw */				\
  void (*cb)(struct type *, int revents); /* rw */ /* gets invoked with an eventmask */

#define EV_WATCHER_LIST(type)			\
  EV_WATCHER (type);				\
  struct type *next /* private */

#define EV_WATCHER_TIME(type)			\
  EV_WATCHER (type);				\
  ev_tstamp at     /* private */

/* base class, nothing to see here unless you subclass */
struct ev_watcher {
  EV_WATCHER (ev_watcher);
};

/* base class, nothing to see here unless you subclass */
struct ev_watcher_list {
  EV_WATCHER_LIST (ev_watcher_list);
};

/* base class, nothing to see here unless you subclass */
struct ev_watcher_time {
  EV_WATCHER_TIME (ev_watcher_time);
};

/* invoked after a specific time, repeatable (based on monotonic clock) */
struct ev_timer
{
  EV_WATCHER_TIME (ev_timer);

  ev_tstamp repeat; /* rw */
};

/* invoked at some specific time, possibly repeating at regular intervals (based on UTC) */
struct ev_periodic
{
  EV_WATCHER_TIME (ev_periodic);

  ev_tstamp interval; /* rw */
};

/* invoked when fd is either EV_READable or EV_WRITEable */
struct ev_io
{
  EV_WATCHER_LIST (ev_io);

  int fd;     /* ro */
  int events; /* ro */
};

/* invoked when the given signal has been received */
struct ev_signal
{
  EV_WATCHER_LIST (ev_signal);

  int signum; /* ro */
};

/* invoked when the nothing else needs to be done, keeps the process from blocking */
struct ev_idle
{
  EV_WATCHER (ev_idle);
};

/* invoked for each run of the mainloop, just before the next blocking vall is initiated */
struct ev_check
{
  EV_WATCHER (ev_check);
};

#define EVMETHOD_NONE   0
#define EVMETHOD_SELECT 1
#define EVMETHOD_EPOLL  2
int ev_init (int flags); /* returns ev_method */
extern int ev_method;

/* these three calls are suitable for plugging into pthread_atfork */
void ev_prefork (void);
void ev_postfork_parent (void);
void ev_postfork_child (void);

extern ev_tstamp ev_now; /* time w.r.t. timers and the eventloop, updated after each poll */
ev_tstamp ev_time (void);

#define EVLOOP_NONBLOCK	1 /* do not block/wait */
#define EVLOOP_ONESHOT	2 /* block *once* only */
void ev_loop (int flags);
extern int ev_loop_done; /* set to 1 to break out of event loop, set to 2 to break out of all event loops */

/* these may evaluate ev multiple times, and the other arguments at most once */
/* either use evw_init + evXXX_set, or the evXXX_init macro, below, to first initialise a watcher */
#define evw_init(ev,cb_)                   do { (ev)->active = 0; (ev)->cb = (cb_); } while (0)

#define evio_set(ev,fd_,events_)           do { (ev)->fd = (fd_); (ev)->events = (events_); } while (0)
#define evtimer_set(ev,after_,repeat_)     do { (ev)->at = (after_); (ev)->repeat = (repeat_); } while (0)
#define evperiodic_set(ev,at_,interval_)   do { (ev)->at = (at_); (ev)->interval = (interval_); } while (0)
#define evsignal_set(ev,signum_)           do { (ev)->signum = (signum_); } while (0)
#define evcheck_set(ev)                    /* nop, yes this is a serious in-joke */
#define evidle_set(ev)                     /* nop, yes this is a serious in-joke */

#define evio_init(ev,cb,fd,events)         do { evw_init ((ev), (cb)); evio_set ((ev),(fd),(events)); } while (0)
#define evtimer_init(ev,cb,after,repeat)   do { evw_init ((ev), (cb)); evtimer_set ((ev),(after),(repeat)); } while (0)
#define evperiodic_init(ev,cb,at,interval) do { evw_init ((ev), (cb)); evperiodic_set ((ev),(at),(interval)); } while (0)
#define evsignal_init(ev,cb,signum)        do { evw_init ((ev), (cb)); evsignal_set ((ev), (signum)); } while (0)
#define evcheck_init(ev,cb)                do { evw_init ((ev), (cb)); evcheck_set ((ev)); } while (0)
#define evidle_init(ev,cb)                 do { evw_init ((ev), (cb)); evidle_set ((ev)); } while (0)

#define ev_is_active(ev) (0 + (ev)->active) /* true when the watcher has been started */

/* stopping (enabling, adding) a watcher does nothing if it is already running */
/* stopping (disabling, deleting) a watcher does nothing unless its already running */
void evio_start       (struct ev_io *w);
void evio_stop        (struct ev_io *w);

void evtimer_start    (struct ev_timer *w);
void evtimer_stop     (struct ev_timer *w);
void evtimer_again    (struct ev_timer *w); /* stops if active and no repeat, restarts if active and repeating, starts if inactive and repeating */

void evperiodic_start (struct ev_periodic *w);
void evperiodic_stop  (struct ev_periodic *w);

void evsignal_start   (struct ev_signal *w);
void evsignal_stop    (struct ev_signal *w);

void evidle_start     (struct ev_idle *w);
void evidle_stop      (struct ev_idle *w);

void evcheck_start    (struct ev_check *w);
void evcheck_stop     (struct ev_check *w);

#endif

