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

static void
epoll_modify (EV_P_ int fd, int oev, int nev)
{
  int mode = nev ? oev ? EPOLL_CTL_MOD : EPOLL_CTL_ADD : EPOLL_CTL_DEL;

  struct epoll_event ev;
  ev.data.u64 = fd; /* use u64 to fully initialise the struct, for nicer strace etc. */
  ev.events =
      (nev & EV_READ ? EPOLLIN : 0)
      | (nev & EV_WRITE ? EPOLLOUT : 0);

  if (epoll_ctl (backend_fd, mode, fd, &ev))
    if (errno != ENOENT /* on ENOENT the fd went away, so try to do the right thing */
        || (nev && epoll_ctl (backend_fd, EPOLL_CTL_ADD, fd, &ev)))
      fd_kill (EV_A_ fd);
}

static void
epoll_poll (EV_P_ ev_tstamp timeout)
{
  int i;
  int eventcnt = epoll_wait (backend_fd, epoll_events, epoll_eventmax, (int)ceil (timeout * 1000.));

  if (eventcnt < 0)
    {
      if (errno != EINTR)
        syserr ("(libev) epoll_wait");

      return;
    }

  for (i = 0; i < eventcnt; ++i)
    fd_event (
      EV_A_
      epoll_events [i].data.u64,
      (epoll_events [i].events & (EPOLLOUT | EPOLLERR | EPOLLHUP) ? EV_WRITE : 0)
      | (epoll_events [i].events & (EPOLLIN | EPOLLERR | EPOLLHUP) ? EV_READ : 0)
    );

  /* if the receive array was full, increase its size */
  if (expect_false (eventcnt == epoll_eventmax))
    {
      ev_free (epoll_events);
      epoll_eventmax = array_nextsize (sizeof (struct epoll_event), epoll_eventmax, epoll_eventmax + 1);
      epoll_events = (struct epoll_event *)ev_malloc (sizeof (struct epoll_event) * epoll_eventmax);
    }
}

int inline_size
epoll_init (EV_P_ int flags)
{
  backend_fd = epoll_create (256);

  if (backend_fd < 0)
    return 0;

  fcntl (backend_fd, F_SETFD, FD_CLOEXEC);

  backend_fudge  = 1e-3; /* needed to compensate for epoll returning early */
  backend_modify = epoll_modify;
  backend_poll   = epoll_poll;

  epoll_eventmax = 64; /* intiial number of events receivable per poll */
  epoll_events = (struct epoll_event *)ev_malloc (sizeof (struct epoll_event) * epoll_eventmax);

  return EVBACKEND_EPOLL;
}

void inline_size
epoll_destroy (EV_P)
{
  ev_free (epoll_events);
}

void inline_size
epoll_fork (EV_P)
{
  close (backend_fd);

  while ((backend_fd = epoll_create (256)) < 0)
    syserr ("(libev) epoll_create");

  fcntl (backend_fd, F_SETFD, FD_CLOEXEC);

  fd_rearm_all (EV_A);
}

