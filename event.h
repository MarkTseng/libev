/*
 * libevent compatibility header, only core events supported
 *
 * Copyright (c) 2007      Marc Alexander Lehmann <libev@schmorp.de>
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
#ifndef _EVENT_H_
#define _EVENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ev.h"

struct event
{
  /* lib watchers we map to */
  union {
    struct ev_io io;
    struct ev_signal sig;
  } iosig;
  struct ev_timer to;

  /* compatibility slots */
  struct event_base *ev_base;
  void (*ev_callback)(int, short, void *arg);
  void *ev_arg;
  int ev_fd;
  int ev_pri;
  int ev_res;
  short ev_events;
};

#define EV_PERSIST                 0x10

#define EVENT_SIGNAL(ev)           ((int) (ev)->ev_fd)
#define EVENT_FD(ev)               ((int) (ev)->ev_fd)

#define event_initialized(ev)      1

#define evtimer_add(ev,tv)         event_add (ev, tv)
#define evtimer_set(ev,cb,data)    event_set (ev, -1, 0, cb, data)
#define evtimer_del(ev)            event_del (ev)
#define evtimer_pending(ev,tv)     event_pending (ev, EV_TIMEOUT, tv)
#define evtimer_initialized(ev)    event_initialized (ev)

#define timeout_add(ev,tv)         evtimer_add (ev, tv)
#define timeout_set(ev,cb,data)    evtimer_set (ev, cb, data)
#define timeout_del(ev)            evtimer_del (ev)
#define timeout_pending(ev,tv)     evtimer_pending (ev, tv)
#define timeout_initialized(ev)    evtimer_initialized (ev)

#define signal_add(ev,tv)          event_add (ev, tv)
#define signal_set(ev,sig,cb,data) event_set (ev, sig, EV_SIGNAL | EV_PERSIST, cb, data)
#define signal_del(ev)             event_del (ev)
#define signal_pending(ev,tv)      event_pending (ev, EV_SIGNAL, tv)
#define signal_initialized(ev)     event_initialized (ev)

const char *event_get_version (void);
const char *event_get_method (void);

void *event_init (void);
void event_base_free (struct event_base *base);

#define EVLOOP_ONCE      EVLOOP_ONESHOT
int event_loop (int);
int event_loopexit (struct timeval *tv);
int event_dispatch (void);

#define _EVENT_LOG_DEBUG 0
#define _EVENT_LOG_MSG   1
#define _EVENT_LOG_WARN  2
#define _EVENT_LOG_ERR   3
typedef void (*event_log_cb)(int severity, const char *msg);
void event_set_log_callback(event_log_cb cb);

void event_set (struct event *ev, int fd, short events, void (*cb)(int, short, void *), void *arg);
int event_once (int fd, short events, void (*cb)(int, short, void *), void *arg, struct timeval *tv);

int event_add (struct event *ev, struct timeval *tv);
int event_del (struct event *ev);

int event_pending (struct event *ev, short, struct timeval *tv);

int event_priority_init (int npri);
int event_priority_set (struct event *ev, int pri);

struct event_base;

int event_base_set (struct event_base *base, struct event *ev);
int event_base_loop (struct event_base *base, int);
int event_base_loopexit (struct event_base *base, struct timeval *tv);
int event_base_dispatch (struct event_base *base);
int event_base_once (struct event_base *base, int fd, short events, void (*cb)(int, short, void *), void *arg, struct timeval *tv);
int event_base_priority_init (struct event_base *base, int fd);

#endif
