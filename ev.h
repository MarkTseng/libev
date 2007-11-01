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

#ifndef EV_H
#define EV_H

#ifdef __cplusplus
extern "C" {
#endif

typedef double ev_tstamp;

/* eventmask, revents, events... */
#define EV_UNDEF          -1 /* guaranteed to be invalid */
#define EV_NONE         0x00
#define EV_READ         0x01
#define EV_WRITE        0x02
#define EV_TIMEOUT  0x000100
#define EV_PERIODIC 0x000200
#define EV_SIGNAL   0x000400
#define EV_IDLE     0x000800
#define EV_CHECK    0x001000
#define EV_PREPARE  0x002000
#define EV_CHILD    0x004000
#define EV_ERROR    0x800000 /* sent when an error occurs */

/* can be used to add custom fields to all watchers */
#ifndef EV_COMMON
# define EV_COMMON void *data
#endif
#ifndef EV_PROTOTYPES
# define EV_PROTOTYPES 1
#endif

#define EV_VERSION_MAJOR 1
#define EV_VERSION_MINOR 1

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
/* revent EV_TIMEOUT */
struct ev_timer
{
  EV_WATCHER_TIME (ev_timer);

  ev_tstamp repeat; /* rw */
};

/* invoked at some specific time, possibly repeating at regular intervals (based on UTC) */
/* revent EV_PERIODIC */
struct ev_periodic
{
  EV_WATCHER_TIME (ev_periodic);

  ev_tstamp interval; /* rw */
};

/* invoked when fd is either EV_READable or EV_WRITEable */
/* revent EV_READ, EV_WRITE */
struct ev_io
{
  EV_WATCHER_LIST (ev_io);

  int fd;     /* ro */
  int events; /* ro */
};

/* invoked when the given signal has been received */
/* revent EV_SIGNAL */
struct ev_signal
{
  EV_WATCHER_LIST (ev_signal);

  int signum; /* ro */
};

/* invoked when the nothing else needs to be done, keeps the process from blocking */
/* revent EV_IDLE */
struct ev_idle
{
  EV_WATCHER (ev_idle);
};

/* invoked for each run of the mainloop, just before the blocking call */
/* you can still change events in any way you like */
/* revent EV_PREPARE */
struct ev_prepare
{
  EV_WATCHER (ev_prepare);
};

/* invoked for each run of the mainloop, just after the blocking call */
/* revent EV_CHECK */
struct ev_check
{
  EV_WATCHER (ev_check);
};

/* invoked when sigchld is received and waitpid indicates the givne pid */
/* revent EV_CHILD */
struct ev_child
{
  EV_WATCHER_LIST (ev_child);

  int pid;    /* ro */
  int status; /* rw, holds the exit status, use the macros from sys/wait.h */
};

#define EVMETHOD_NONE   0
#define EVMETHOD_SELECT 1
#define EVMETHOD_EPOLL  2
#if EV_PROTOTYPES
extern int ev_method;
int ev_init (int flags); /* returns ev_method */
int ev_version_major (void);
int ev_version_minor (void);

/* these three calls are suitable for plugging into pthread_atfork */
void ev_fork_prepare (void);
void ev_fork_parent (void);
void ev_fork_child (void);

extern ev_tstamp ev_now; /* time w.r.t. timers and the eventloop, updated after each poll */
ev_tstamp ev_time (void);
#endif

#define EVLOOP_NONBLOCK	1 /* do not block/wait */
#define EVLOOP_ONESHOT	2 /* block *once* only */
#if EV_PROTOTYPES
void ev_loop (int flags);
extern int ev_loop_done; /* set to 1 to break out of event loop, set to 2 to break out of all event loops */

/* convinience function, wait for a single event, without registering an event watcher */
/* if timeout is < 0, do wait indefinitely */
void ev_once (int fd, int events, ev_tstamp timeout, void (*cb)(int revents, void *arg), void *arg);
#endif

/* these may evaluate ev multiple times, and the other arguments at most once */
/* either use ev_watcher_init + ev_TYPE_set, or the ev_TYPE_init macro, below, to first initialise a watcher */
#define ev_watcher_init(ev,cb_)             do { (ev)->active = 0; (ev)->pending = 0; (ev)->cb = (cb_); } while (0)

#define ev_io_set(ev,fd_,events_)           do { (ev)->fd = (fd_); (ev)->events = (events_); } while (0)
#define ev_timer_set(ev,after_,repeat_)     do { (ev)->at = (after_); (ev)->repeat = (repeat_); } while (0)
#define ev_periodic_set(ev,at_,interval_)   do { (ev)->at = (at_); (ev)->interval = (interval_); } while (0)
#define ev_signal_set(ev,signum_)           do { (ev)->signum = (signum_); } while (0)
#define ev_idle_set(ev)                     /* nop, yes, this is a serious in-joke */
#define ev_prepare_set(ev)                  /* nop, yes, this is a serious in-joke */
#define ev_check_set(ev)                    /* nop, yes, this is a serious in-joke */
#define ev_child_set(ev,pid_)               do { (ev)->pid = (pid_); } while (0)

#define ev_io_init(ev,cb,fd,events)         do { ev_watcher_init ((ev), (cb)); ev_io_set ((ev),(fd),(events)); } while (0)
#define ev_timer_init(ev,cb,after,repeat)   do { ev_watcher_init ((ev), (cb)); ev_timer_set ((ev),(after),(repeat)); } while (0)
#define ev_periodic_init(ev,cb,at,interval) do { ev_watcher_init ((ev), (cb)); ev_periodic_set ((ev),(at),(interval)); } while (0)
#define ev_signal_init(ev,cb,signum)        do { ev_watcher_init ((ev), (cb)); ev_signal_set ((ev), (signum)); } while (0)
#define ev_idle_init(ev,cb)                 do { ev_watcher_init ((ev), (cb)); ev_idle_set ((ev)); } while (0)
#define ev_prepare_init(ev,cb)              do { ev_watcher_init ((ev), (cb)); ev_prepare_set ((ev)); } while (0)
#define ev_check_init(ev,cb)                do { ev_watcher_init ((ev), (cb)); ev_check_set ((ev)); } while (0)
#define ev_child_init(ev,cb,pid)            do { ev_watcher_init ((ev), (cb)); ev_child_set ((ev),(pid)); } while (0)

#define ev_is_active(ev) (0 + (ev)->active) /* true when the watcher has been started */

/* stopping (enabling, adding) a watcher does nothing if it is already running */
/* stopping (disabling, deleting) a watcher does nothing unless its already running */
#if EV_PROTOTYPES
void ev_io_start       (struct ev_io *w);
void ev_io_stop        (struct ev_io *w);

void ev_timer_start    (struct ev_timer *w);
void ev_timer_stop     (struct ev_timer *w);
void ev_timer_again    (struct ev_timer *w); /* stops if active and no repeat, restarts if active and repeating, starts if inactive and repeating */

void ev_periodic_start (struct ev_periodic *w);
void ev_periodic_stop  (struct ev_periodic *w);

void ev_signal_start   (struct ev_signal *w);
void ev_signal_stop    (struct ev_signal *w);

void ev_idle_start     (struct ev_idle *w);
void ev_idle_stop      (struct ev_idle *w);

void ev_prepare_start  (struct ev_prepare *w);
void ev_prepare_stop   (struct ev_prepare *w);

void ev_check_start    (struct ev_check *w);
void ev_check_stop     (struct ev_check *w);

void ev_child_start    (struct ev_child *w);
void ev_child_stop     (struct ev_child *w);
#endif

#ifdef __cplusplus
}
#endif

#endif

