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

#include <poll.h>

static struct pollfd *polls;
static int pollmax, pollcnt;
static int *pollidxs; /* maps fds into structure indices */
static int pollidxmax;

static void
pollidx_init (int *base, int count)
{
  while (count--)
    *base++ = -1;
}

static void
poll_modify (int fd, int oev, int nev)
{
  int idx;
  array_needsize (pollidxs, pollidxmax, fd + 1, pollidx_init);

  idx = pollidxs [fd];

  if (idx < 0) /* need to allocate a new pollfd */
    {
      idx = pollcnt++;
      array_needsize (polls, pollmax, pollcnt, );
      polls [idx].fd = fd;
    }

  if (nev)
    polls [idx].events =
        (nev & EV_READ ? POLLIN : 0)
        | (nev & EV_WRITE ? POLLOUT : 0);
  else /* remove pollfd */
    {
      if (idx < pollcnt--)
        {
          pollidxs [fd] = -1;
          polls [idx] = polls [pollcnt];
          pollidxs [polls [idx].fd] = idx;
        }
    }
}

static void
poll_poll (ev_tstamp timeout)
{
  int res = poll (polls, pollcnt, ceil (timeout * 1000.));

  if (res > 0)
    {
      int i;

      for (i = 0; i < pollcnt; ++i)
        fd_event (
          polls [i].fd,
          (polls [i].revents & (POLLOUT | POLLERR | POLLHUP) ? EV_WRITE : 0)
          | (polls [i].revents & (POLLIN | POLLERR | POLLHUP) ? EV_READ : 0)
        );
    }
  else if (res < 0)
    {
      if (errno == EBADF)
        fd_ebadf ();
      else if (errno == ENOMEM)
        fd_enomem ();
    }
}

static void
poll_init (int flags)
{
  ev_method     = EVMETHOD_POLL;
  method_fudge  = 1e-3; /* needed to compensate for select returning early, very conservative */
  method_modify = poll_modify;
  method_poll   = poll_poll;
}
