/*
 * Copyright 2007      Marc Alexander Lehmann <libev@schmorp.de>
 * Copyright 2000-2002 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include <sys/queue.h>
#include <sys/event.h>
#include <string.h>
#include <errno.h>

static void
kqueue_change (EV_P_ int fd, int filter, int flags, int fflags)
{
  struct kevent *ke;

  array_needsize (kqueue_changes, kqueue_changemax, ++kqueue_changecnt, );

  ke = &kqueue_changes [kqueue_changecnt - 1];
  memset (ke, 0, sizeof (struct kevent));
  ke->ident  = fd;
  ke->filter = filter;
  ke->flags  = flags;
  ke->fflags = fflags;
}

#ifndef NOTE_EOF
# define NOTE_EOF 0
#endif

static void
kqueue_modify (EV_P_ int fd, int oev, int nev)
{
  if ((oev ^ nev) & EV_READ)
    {
      if (nev & EV_READ)
        kqueue_change (fd, EVFILT_READ, EV_ADD, NOTE_EOF);
      else
        kqueue_change (fd, EVFILT_READ, EV_DELETE, 0);
    }

  if ((oev ^ nev) & EV_WRITE)
    {
      if (nev & EV_WRITE)
        kqueue_change (fd, EVFILT_WRITE, EV_ADD, NOTE_EOF);
      else
        kqueue_change (fd, EVFILT_WRITE, EV_DELETE, 0);
    }
}

static void
kqueue_poll (EV_P_ ev_tstamp timeout)
{
  int res, i;
  struct timespec ts;

  ts.tv_sec  = (time_t)timeout;
  ts.tv_nsec = (long)(timeout - (ev_tstamp)ts.tv_sec) * 1e9;
  res = kevent (kqueue_fd, kqueue_changes, kqueue_changecnt, kqueue_events, kqueue_eventmax, &ts);
  kqueue_changecnt = 0;

  if (res < 0)
    return;

  for (i = 0; i < res; ++i)
    {
      if (kqueue_events [i].flags & EV_ERROR)
        {
          /* 
           * Error messages that can happen, when a delete fails.
           *   EBADF happens when the file discriptor has been
           *   closed,
           *   ENOENT when the file discriptor was closed and
           *   then reopened.
           *   EINVAL for some reasons not understood; EINVAL
           *   should not be returned ever; but FreeBSD does :-\
           * An error is also indicated when a callback deletes
           * an event we are still processing.  In that case
           * the data field is set to ENOENT.
           */
          if (kqueue_events [i].data == EBADF)
            fd_kill (EV_A_ kqueue_events [i].ident);
        }
      else
        fd_event (
          EV_A_
          kqueue_events [i].ident,
          kqueue_events [i].filter == EVFILT_READ ? EV_READ
          : kqueue_events [i].filter == EVFILT_WRITE ? EV_WRITE
          : 0
        );
    }

  if (expect_false (res == kqueue_eventmax))
    {
      free (kqueue_events);
      kqueue_eventmax = array_roundsize (kqueue_events, kqueue_eventmax << 1);
      kqueue_events = malloc (sizeof (struct kevent) * kqueue_eventmax);
    }
}

static int
kqueue_init (EV_P_ int flags)
{
  struct kevent ch, ev;

  /* Initalize the kernel queue */
  if ((kqueue_fd = kqueue ()) < 0)
    return 0;

  fcntl (kqueue_fd, F_SETFD, FD_CLOEXEC); /* not sure if necessary, hopefully doesn't hurt */

  /* Check for Mac OS X kqueue bug. */
  ch.ident  = -1;
  ch.filter = EVFILT_READ;
  ch.flags  = EV_ADD;

  /* 
   * If kqueue works, then kevent will succeed, and it will
   * stick an error in ev. If kqueue is broken, then
   * kevent will fail.
   */
  if (kevent (kqueue_fd, &ch, 1, &ev, 1, 0) != 1
      || ev.ident != -1
      || ev.flags != EV_ERROR)
    {
      /* detected broken kqueue */
      close (kqueue_fd);
      return 0;
    }

  method_fudge  = 1e-3; /* needed to compensate for kevent returning early */
  method_modify = kqueue_modify;
  method_poll   = kqueue_poll;

  kqueue_eventmax = 64; /* intiial number of events receivable per poll */
  kqueue_events = malloc (sizeof (struct kevent) * kqueue_eventmax);

  kqueue_changes   = 0;
  kqueue_changemax = 0;
  kqueue_changecnt = 0;

  return EVMETHOD_KQUEUE;
}

static void
kqueue_destroy (EV_P)
{
  close (kqueue_fd);

  free (kqueue_events);
  free (kqueue_changes);
}

static void
kqueue_fork (EV_P)
{
  kqueue_fd = kqueue ();
  fcntl (kqueue_fd, F_SETFD, FD_CLOEXEC);

  /* re-register interest in fds */
  fd_rearm_all ();
}

