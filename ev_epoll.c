/*
 * libev epoll fd activity backend
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

#include <sys/epoll.h>

static int epoll_fd = -1;

static void
epoll_modify (int fd, int oev, int nev)
{
  int mode = nev ? oev ? EPOLL_CTL_MOD : EPOLL_CTL_ADD : EPOLL_CTL_DEL;

  struct epoll_event ev;
  ev.data.u64 = fd; /* use u64 to fully initialise the struct, for nicer strace etc. */
  ev.events =
      (nev & EV_READ ? EPOLLIN : 0)
      | (nev & EV_WRITE ? EPOLLOUT : 0);

  epoll_ctl (epoll_fd, mode, fd, &ev);
}

static void
epoll_postfork_child (void)
{
  int fd;

  epoll_fd = epoll_create (256);
  fcntl (epoll_fd, F_SETFD, FD_CLOEXEC);

  /* re-register interest in fds */
  for (fd = 0; fd < anfdmax; ++fd)
    if (anfds [fd].events)//D
      epoll_modify (fd, EV_NONE, anfds [fd].events);
}

static struct epoll_event *events;
static int eventmax;

static void
epoll_poll (ev_tstamp timeout)
{
  int eventcnt = epoll_wait (epoll_fd, events, eventmax, ceil (timeout * 1000.));
  int i;

  if (eventcnt < 0)
    return;

  for (i = 0; i < eventcnt; ++i)
    fd_event (
      events [i].data.fd,
      (events [i].events & (EPOLLOUT | EPOLLERR | EPOLLHUP) ? EV_WRITE : 0)
      | (events [i].events & (EPOLLIN | EPOLLERR | EPOLLHUP) ? EV_READ : 0)
    );

  /* if the receive array was full, increase its size */
  if (expect_false (eventcnt == eventmax))
    {
      free (events);
      eventmax = array_roundsize (events, eventmax << 1);
      events = malloc (sizeof (struct epoll_event) * eventmax);
    }
}

static void
epoll_init (int flags)
{
  epoll_fd = epoll_create (256);

  if (epoll_fd < 0)
    return;

  fcntl (epoll_fd, F_SETFD, FD_CLOEXEC);

  ev_method     = EVMETHOD_EPOLL;
  method_fudge  = 1e-3; /* needed to compensate for epoll returning early */
  method_modify = epoll_modify;
  method_poll   = epoll_poll;

  eventmax = 64; /* intiial number of events receivable per poll */
  events = malloc (sizeof (struct epoll_event) * eventmax);
}

