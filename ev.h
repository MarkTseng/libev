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

#ifndef EV_H__
#define EV_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef double ev_tstamp;

/* these priorities are inclusive, higher priorities will be called earlier */
#ifndef EV_MINPRI
# define EV_MINPRI -2
#endif
#ifndef EV_MAXPRI
# define EV_MAXPRI +2
#endif

#ifndef EV_MULTIPLICITY
# define EV_MULTIPLICITY 1
#endif

#ifndef EV_PERIODICS
# define EV_PERIODICS 1
#endif

/* support multiple event loops? */
#if EV_MULTIPLICITY
struct ev_loop;
# define EV_P struct ev_loop *loop
# define EV_P_ EV_P,
# define EV_A loop
# define EV_A_ EV_A,
# define EV_DEFAULT_A ev_default_loop (0)
# define EV_DEFAULT_A_ EV_DEFAULT_A,
#else
# define EV_P void
# define EV_P_
# define EV_A
# define EV_A_
# define EV_DEFAULT_A
# define EV_DEFAULT_A_
#endif

/* eventmask, revents, events... */
#define EV_UNDEF          -1 /* guaranteed to be invalid */
#define EV_NONE         0x00
#define EV_READ         0x01 /* io only */
#define EV_WRITE        0x02 /* io only */
#define EV_TIMEOUT  0x000100 /* timer only */
#define EV_PERIODIC 0x000200 /* periodic timer only */
#define EV_SIGNAL   0x000400 /* signal only */
#define EV_IDLE     0x000800 /* idle only */
#define EV_CHECK    0x001000 /* check only */
#define EV_PREPARE  0x002000 /* prepare only */
#define EV_CHILD    0x004000 /* child/pid only */
#define EV_ERROR    0x800000 /* sent when an error occurs */

/* can be used to add custom fields to all watchers, while losing binary compatibility */
#ifndef EV_COMMON
# define EV_COMMON void *data;
#endif
#ifndef EV_PROTOTYPES
# define EV_PROTOTYPES 1
#endif

#define EV_VERSION_MAJOR 1
#define EV_VERSION_MINOR 1

#ifndef EV_CB_DECLARE
# define EV_CB_DECLARE(type) void (*cb)(EV_P_ struct type *w, int revents);
#endif
#ifndef EV_CB_INVOKE
# define EV_CB_INVOKE(watcher,revents) (watcher)->cb (EV_A_ (watcher), (revents))
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
  int priority; /* private */			\
  EV_COMMON /* rw */				\
  EV_CB_DECLARE (type) /* private */

#define EV_WATCHER_LIST(type)			\
  EV_WATCHER (type)				\
  struct ev_watcher_list *next; /* private */

#define EV_WATCHER_TIME(type)			\
  EV_WATCHER (type)				\
  ev_tstamp at;     /* private */

/* base class, nothing to see here unless you subclass */
struct ev_watcher
{
  EV_WATCHER (ev_watcher)
};

/* base class, nothing to see here unless you subclass */
struct ev_watcher_list
{
  EV_WATCHER_LIST (ev_watcher_list)
};

/* base class, nothing to see here unless you subclass */
struct ev_watcher_time
{
  EV_WATCHER_TIME (ev_watcher_time)
};

/* invoked after a specific time, repeatable (based on monotonic clock) */
/* revent EV_TIMEOUT */
struct ev_timer
{
  EV_WATCHER_TIME (ev_timer)

  ev_tstamp repeat; /* rw */
};

/* invoked at some specific time, possibly repeating at regular intervals (based on UTC) */
/* revent EV_PERIODIC */
struct ev_periodic
{
  EV_WATCHER_TIME (ev_periodic)

  ev_tstamp interval; /* rw */
  ev_tstamp (*reschedule_cb)(struct ev_periodic *w, ev_tstamp now); /* rw */
};

/* invoked when fd is either EV_READable or EV_WRITEable */
/* revent EV_READ, EV_WRITE */
struct ev_io
{
  EV_WATCHER_LIST (ev_io)

  int fd;     /* ro */
  int events; /* ro */
};

/* invoked when the given signal has been received */
/* revent EV_SIGNAL */
struct ev_signal
{
  EV_WATCHER_LIST (ev_signal)

  int signum; /* ro */
};

/* invoked when the nothing else needs to be done, keeps the process from blocking */
/* revent EV_IDLE */
struct ev_idle
{
  EV_WATCHER (ev_idle)
};

/* invoked for each run of the mainloop, just before the blocking call */
/* you can still change events in any way you like */
/* revent EV_PREPARE */
struct ev_prepare
{
  EV_WATCHER (ev_prepare)
};

/* invoked for each run of the mainloop, just after the blocking call */
/* revent EV_CHECK */
struct ev_check
{
  EV_WATCHER (ev_check)
};

/* invoked when sigchld is received and waitpid indicates the given pid */
/* revent EV_CHILD */
/* does not support priorities */
struct ev_child
{
  EV_WATCHER_LIST (ev_child)

  int pid;     /* ro */
  int rpid;    /* rw, holds the received pid */
  int rstatus; /* rw, holds the exit status, use the macros from sys/wait.h */
};

/* the presence of this union forces similar struct layout */
union ev_any_watcher
{
  struct ev_watcher w;
  struct ev_watcher_list wl;

  struct ev_io io;
  struct ev_timer timer;
  struct ev_periodic periodic;
  struct ev_idle idle;
  struct ev_prepare prepare;
  struct ev_check check;
  struct ev_signal signal;
  struct ev_child child;
};

/* bits for ev_default_loop and ev_loop_new */
/* the default */
#define EVFLAG_AUTO      0x00000000 /* not quite a mask */

/* method bits to be ored together */
#define EVMETHOD_SELECT  0x00000001 /* about anywhere */
#define EVMETHOD_POLL    0x00000002 /* !win */
#define EVMETHOD_EPOLL   0x00000004 /* linux */
#define EVMETHOD_KQUEUE  0x00000008 /* bsd */
#define EVMETHOD_DEVPOLL 0x00000010 /* solaris 8 */ /* NYI */
#define EVMETHOD_PORT    0x00000020 /* solaris 10 */

/* flag bits */
#define EVFLAG_NOENV     0x01000000 /* do NOT consult environment */

#if EV_PROTOTYPES
int ev_version_major (void);
int ev_version_minor (void);

ev_tstamp ev_time (void);

/* Sets the allocation function to use, works like realloc.
 * It is used to allocate and free memory.
 * If it returns zero when memory needs to be allocated, the library might abort
 * or take some potentially destructive action.
 * The default is your system realloc function.
 */
void ev_set_allocator (void *(*cb)(void *ptr, long size));

/* set the callback function to call on a
 * retryable syscall error
 * (such as failed select, poll, epoll_wait)
 */
void ev_set_syserr_cb (void (*cb)(const char *msg));

# if EV_MULTIPLICITY
/* the default loop is the only one that handles signals and child watchers */
/* you can call this as often as you like */
static struct ev_loop *
ev_default_loop (unsigned int flags)
{
  extern struct ev_loop *ev_default_loop_ptr;
  extern struct ev_loop *ev_default_loop_ (unsigned int flags);

  if (!ev_default_loop_ptr)
    ev_default_loop_ (flags);

  return ev_default_loop_ptr;
}

/* create and destroy alternative loops that don't handle signals */
struct ev_loop *ev_loop_new (unsigned int flags);
void ev_loop_destroy (EV_P);
void ev_loop_fork (EV_P);

ev_tstamp ev_now (EV_P); /* time w.r.t. timers and the eventloop, updated after each poll */

# else

int ev_default_loop (unsigned int flags); /* returns true when successful */

static ev_tstamp
ev_now (void)
{
  extern ev_tstamp ev_rt_now;

  return ev_rt_now;
}
# endif

void ev_default_destroy (void); /* destroy the default loop */
/* this needs to be called after fork, to duplicate the default loop */
/* if you create alternative loops you have to call ev_loop_fork on them */
/* you can call it in either the parent or the child */
/* you can actually call it at any time, anywhere :) */
void ev_default_fork (void);

unsigned int ev_method (EV_P);
#endif

#define EVLOOP_NONBLOCK	1 /* do not block/wait */
#define EVLOOP_ONESHOT	2 /* block *once* only */
#define EVUNLOOP_ONE    1 /* unloop once */
#define EVUNLOOP_ALL    2 /* unloop all loops */

#if EV_PROTOTYPES
void ev_loop (EV_P_ int flags);
void ev_unloop (EV_P_ int how); /* set to 1 to break out of event loop, set to 2 to break out of all event loops */

/*
 * ref/unref can be used to add or remove a refcount on the mainloop. every watcher
 * keeps one reference. if you have a long-runing watcher you never unregister that
 * should not keep ev_loop from running, unref() after starting, and ref() before stopping.
 */
void ev_ref   (EV_P);
void ev_unref (EV_P);

/* convinience function, wait for a single event, without registering an event watcher */
/* if timeout is < 0, do wait indefinitely */
void ev_once (EV_P_ int fd, int events, ev_tstamp timeout, void (*cb)(int revents, void *arg), void *arg);
#endif

/* these may evaluate ev multiple times, and the other arguments at most once */
/* either use ev_init + ev_TYPE_set, or the ev_TYPE_init macro, below, to first initialise a watcher */
#define ev_init(ev,cb_) do {				\
  ((struct ev_watcher *)(void *)(ev))->active   =	\
  ((struct ev_watcher *)(void *)(ev))->pending  =	\
  ((struct ev_watcher *)(void *)(ev))->priority = 0;	\
  ev_set_cb ((ev), cb_);					\
} while (0)

#define ev_io_set(ev,fd_,events_)           do { (ev)->fd = (fd_); (ev)->events = (events_); } while (0)
#define ev_timer_set(ev,after_,repeat_)     do { (ev)->at = (after_); (ev)->repeat = (repeat_); } while (0)
#define ev_periodic_set(ev,at_,ival_,res_)  do { (ev)->at = (at_); (ev)->interval = (ival_); (ev)->reschedule_cb= (res_); } while (0)
#define ev_signal_set(ev,signum_)           do { (ev)->signum = (signum_); } while (0)
#define ev_idle_set(ev)                     /* nop, yes, this is a serious in-joke */
#define ev_prepare_set(ev)                  /* nop, yes, this is a serious in-joke */
#define ev_check_set(ev)                    /* nop, yes, this is a serious in-joke */
#define ev_child_set(ev,pid_)               do { (ev)->pid = (pid_); } while (0)

#define ev_io_init(ev,cb,fd,events)         do { ev_init ((ev), (cb)); ev_io_set ((ev),(fd),(events)); } while (0)
#define ev_timer_init(ev,cb,after,repeat)   do { ev_init ((ev), (cb)); ev_timer_set ((ev),(after),(repeat)); } while (0)
#define ev_periodic_init(ev,cb,at,ival,res) do { ev_init ((ev), (cb)); ev_periodic_set ((ev),(at),(ival),(res)); } while (0)
#define ev_signal_init(ev,cb,signum)        do { ev_init ((ev), (cb)); ev_signal_set ((ev), (signum)); } while (0)
#define ev_idle_init(ev,cb)                 do { ev_init ((ev), (cb)); ev_idle_set ((ev)); } while (0)
#define ev_prepare_init(ev,cb)              do { ev_init ((ev), (cb)); ev_prepare_set ((ev)); } while (0)
#define ev_check_init(ev,cb)                do { ev_init ((ev), (cb)); ev_check_set ((ev)); } while (0)
#define ev_child_init(ev,cb,pid)            do { ev_init ((ev), (cb)); ev_child_set ((ev),(pid)); } while (0)

#define ev_is_pending(ev)                   (0 + ((struct ev_watcher *)(void *)(ev))->pending) /* ro, true when watcher is waiting for callback invocation */
#define ev_is_active(ev)                    (0 + ((struct ev_watcher *)(void *)(ev))->active) /* ro, true when the watcher has been started */

#define ev_priority(ev)                     ((struct ev_watcher *)(void *)(ev))->priority /* rw */
#define ev_cb(ev)                           (ev)->cb /* rw */
#define ev_set_priority(ev,pri)             ev_priority (ev) = (pri)

#ifndef ev_set_cb
# define ev_set_cb(ev,cb_)                  ev_cb (ev) = (cb_)
#endif

/* stopping (enabling, adding) a watcher does nothing if it is already running */
/* stopping (disabling, deleting) a watcher does nothing unless its already running */
#if EV_PROTOTYPES

/* feeds an event into a watcher as if the event actually occured */
/* accepts any ev_watcher type */
void ev_feed_event     (EV_P_ void *w, int revents);
void ev_feed_fd_event  (EV_P_ int fd, int revents);
void ev_feed_signal_event (EV_P_ int signum);

void ev_io_start       (EV_P_ struct ev_io *w);
void ev_io_stop        (EV_P_ struct ev_io *w);

void ev_timer_start    (EV_P_ struct ev_timer *w);
void ev_timer_stop     (EV_P_ struct ev_timer *w);
/* stops if active and no repeat, restarts if active and repeating, starts if inactive and repeating */
void ev_timer_again    (EV_P_ struct ev_timer *w);

#if EV_PERIODICS
void ev_periodic_start (EV_P_ struct ev_periodic *w);
void ev_periodic_stop  (EV_P_ struct ev_periodic *w);
void ev_periodic_again (EV_P_ struct ev_periodic *w);
#endif

void ev_idle_start     (EV_P_ struct ev_idle *w);
void ev_idle_stop      (EV_P_ struct ev_idle *w);

void ev_prepare_start  (EV_P_ struct ev_prepare *w);
void ev_prepare_stop   (EV_P_ struct ev_prepare *w);

void ev_check_start    (EV_P_ struct ev_check *w);
void ev_check_stop     (EV_P_ struct ev_check *w);

/* only supported in the default loop */
void ev_signal_start   (EV_P_ struct ev_signal *w);
void ev_signal_stop    (EV_P_ struct ev_signal *w);

/* only supported in the default loop */
void ev_child_start    (EV_P_ struct ev_child *w);
void ev_child_stop     (EV_P_ struct ev_child *w);
#endif

#ifdef __cplusplus
}
#endif

#endif

