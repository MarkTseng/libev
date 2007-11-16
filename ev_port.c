/*
 * Copyright 2007      Marc Alexander Lehmann <libev@schmorp.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <poll.h>
#include <port.h>
#include <string.h>
#include <errno.h>

static void
port_modify (EV_P_ int fd, int oev, int nev)
{
  /* we need to reassociate no matter what, as closes are
   * once more silently being discarded.
   */
  if (!nev)
    {
      if (oev)
        port_dissociate (port_fd, PORT_SOURCE_FD, fd);
    }
  else if (0 >
      port_associate (
         port_fd, PORT_SOURCE_FD, fd,
         (nev & EV_READ ? POLLIN : 0)
         | (nev & EV_WRITE ? POLLOUT : 0),
         0
      )
  )
    {
      if (errno == EBADFD)
        fd_kill (EV_A_ fd);
      else
        syserr ("(libev) port_associate");
    } 
}

static void
port_poll (EV_P_ ev_tstamp timeout)
{
  int res, i;
  struct timespec ts;
  uint_t nget = 1;

  ts.tv_sec  = (time_t)timeout;
  ts.tv_nsec = (long)(timeout - (ev_tstamp)ts.tv_sec) * 1e9;
  res = port_getn (port_fd, port_events, port_eventmax, &nget, &ts);

  if (res < 0)
    { 
      if (errno != EINTR && errno != ETIME)
        syserr ("(libev) port_getn");

      return;
    } 

  for (i = 0; i < nget; ++i)
    {
      if (port_events [i].portev_source == PORT_SOURCE_FD)
        {
          int fd = port_events [i].portev_object;

          fd_event (
            EV_A_
            fd,
            (port_events [i].portev_events & (POLLOUT | POLLERR | POLLHUP) ? EV_WRITE : 0)
            | (port_events [i].portev_events & (POLLIN | POLLERR | POLLHUP) ? EV_READ : 0)
          );

          anfds [fd].events = 0; /* event received == disassociated */
          fd_change (EV_A_ fd); /* need to reify later */
        }
    }

  if (expect_false (nget == port_eventmax))
    {
      ev_free (port_events);
      port_eventmax = array_roundsize (port_event_t, port_eventmax << 1);
      port_events = (port_event_t *)ev_malloc (sizeof (port_event_t) * port_eventmax);
    }
}

static int
port_init (EV_P_ int flags)
{
  /* Initalize the kernel queue */
  if ((port_fd = port_create ()) < 0)
    return 0;

  fcntl (port_fd, F_SETFD, FD_CLOEXEC); /* not sure if necessary, hopefully doesn't hurt */

  method_fudge  = 1e-3; /* needed to compensate for port_getn returning early */
  method_modify = port_modify;
  method_poll   = port_poll;

  port_eventmax = 64; /* intiial number of events receivable per poll */
  port_events = (port_event_t *)ev_malloc (sizeof (port_event_t) * port_eventmax);

  return EVMETHOD_PORT;
}

static void
port_destroy (EV_P)
{
  close (port_fd);

  ev_free (port_events);
}

static void
port_fork (EV_P)
{
  close (port_fd);

  while ((port_fd = port_create ()) < 0)
    syserr ("(libev) port");

  fcntl (port_fd, F_SETFD, FD_CLOEXEC);

  /* re-register interest in fds */
  fd_rearm_all (EV_A);
}

