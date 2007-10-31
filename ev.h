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

/* shared by all watchers */
#define EV_WATCHER(type)			\
  int active; /* private */			\
  int pending; /* private */			\
  void *data; /* rw */				\
  void (*cb)(struct type *, int revents) /* rw */ /* gets invoked with an eventmask */

#define EV_WATCHER_LIST(type)			\
  EV_WATCHER (type);				\
  struct type *next /* private */

/* invoked at a specific time or after a specific time, repeatable */
struct ev_timer
{
  EV_WATCHER_LIST (ev_timer);

  ev_tstamp at;     /* private */
  ev_tstamp repeat; /* rw */
  unsigned char is_abs; /* ro */
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
extern int ev_loop_done; /* set to 1 to break out of event loop */

/* these may evaluate ev multiple times, and the other arguments at most once */
#define evw_init(ev,cb_,data_)             do { (ev)->active = 0; (ev)->cb = (cb_); (ev)->data = (void *)data_; } while (0)

#define evio_set(ev,fd_,events_)           do { (ev)->fd = (fd_); (ev)->events = (events_); } while (0)
#define evtimer_set_rel(ev,after_,repeat_) do { (ev)->at = (after_); (ev)->repeat = (repeat_); (ev)->is_abs = 0; } while (0)
#define evtimer_set_abs(ev,at_,repeat_)    do { (ev)->at = (at_); (ev)->repeat = (repeat_); (ev)->is_abs = 1; } while (0)
#define evsignal_set(ev,signum_)           do { (ev)->signum = (signum_); } while (0)

#define ev_is_active(ev) (0 + (ev)->active) /* true when the watcher has been started */

/* stopping (enabling, adding) a watcher does nothing if it is already running */
/* stopping (disabling, deleting) a watcher does nothing unless its already running */
void evio_start     (struct ev_io *w);
void evio_stop      (struct ev_io *w);

void evtimer_start  (struct ev_timer *w);
void evtimer_stop   (struct ev_timer *w);

void evsignal_start (struct ev_signal *w);
void evsignal_stop  (struct ev_signal *w);

void evidle_start   (struct ev_idle *w);
void evidle_stop    (struct ev_idle *w);

void evcheck_start  (struct ev_check *w);
void evcheck_stop   (struct ev_check *w);

#endif

