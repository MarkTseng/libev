#ifndef EV_H
#define EV_H

typedef double ev_tstamp;

/* eventmask, revents, events... */
#define EV_UNDEF   -1 /* guaranteed to be invalid */
#define EV_NONE    0
#define EV_READ    1
#define EV_WRITE   2
#define EV_TIMEOUT 4
#define EV_SIGNAL  8

/* shared by all watchers */
#define EV_WATCHER(type)			\
  int active; /* private */			\
  int pending; /* private */			\
  void *data; /* rw */				\
  void (*cb)(struct type *, int revents) /* rw */

#define EV_WATCHER_LIST(type)			\
  EV_WATCHER (type);				\
  struct type *next /* private */

struct ev_timer
{
  EV_WATCHER_LIST (ev_timer);

  ev_tstamp at;     /* ro */
  ev_tstamp repeat; /* rw */
  unsigned char is_abs; /* rw */
};

struct ev_io
{
  EV_WATCHER_LIST (ev_io);

  int fd;     /* ro */
  int events; /* ro */
};

struct ev_signal
{
  EV_WATCHER_LIST (ev_signal);

  int signum; /* ro */
};

#define EVMETHOD_NONE   0
#define EVMETHOD_SELECT 1
#define EVMETHOD_EPOLL  2
int ev_init (int flags);
extern int ev_method;

void ev_prefork (void);
void ev_postfork_parent (void);
void ev_postfork_child (void);

extern ev_tstamp ev_now; /* time w.r.t. timers and the eventloop, updated after each poll */
ev_tstamp ev_time (void);

#define EVLOOP_NONBLOCK	1 /* do not block/wait */
#define EVLOOP_ONESHOT	2 /* block *once* only */
int ev_loop (int flags);
extern int ev_loop_done; /* set to 1 to break out of event loop */

/* these may evaluate ev multiple times, and the other arguments at most once */
#define evw_init(ev,cb_,data_)             do { (ev)->active = 0; (ev)->cb = (cb_); (ev)->data = (void *)data_; } while (0)
#define evio_set(ev,fd_,events_)           do { (ev)->fd = (fd_); (ev)->events = (events_); } while (0)
#define evtimer_set_rel(ev,after_,repeat_) do { (ev)->at = (after_); (ev)->repeat = (repeat_); (ev)->is_abs = 0; } while (0)
#define evtimer_set_abs(ev,at_,repeat_)    do { (ev)->at = (at_); (ev)->repeat = (repeat_); (ev)->is_abs = 1; } while (0)
#define evsignal_set(ev,signum_)           do { (ev)->signum = (signum_); } while (0)

#define ev_is_active(ev) (0 + (ev)->active) /* true when the watcher has been started */

void evio_start (struct ev_io *w);
void evio_stop  (struct ev_io *w);

void evtimer_start (struct ev_timer *w);
void evtimer_stop  (struct ev_timer *w);

void evsignal_start (struct ev_signal *w);
void evsignal_stop  (struct ev_signal *w);

#endif

