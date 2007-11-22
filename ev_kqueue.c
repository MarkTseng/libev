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

  ++kqueue_changecnt;
  array_needsize (struct kevent, kqueue_changes, kqueue_changemax, kqueue_changecnt, EMPTY2);

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
  /* to detect close/reopen reliably, we have to remove and re-add */
  /* event requests even when oev == nev */

  if (oev & EV_READ)
    kqueue_change (EV_A_ fd, EVFILT_READ, EV_DELETE, 0);

  if (oev & EV_WRITE)
    kqueue_change (EV_A_ fd, EVFILT_WRITE, EV_DELETE, 0);

  if (nev & EV_READ)
    kqueue_change (EV_A_ fd, EVFILT_READ, EV_ADD, NOTE_EOF);

  if (nev & EV_WRITE)
    kqueue_change (EV_A_ fd, EVFILT_WRITE, EV_ADD, NOTE_EOF);
}

static void
kqueue_poll (EV_P_ ev_tstamp timeout)
{
  int res, i;
  struct timespec ts;

  /* need to resize so there is enough space for errors */
  if (kqueue_changecnt > kqueue_eventmax)
    {
      ev_free (kqueue_events);
      kqueue_eventmax = array_roundsize (struct kevent, kqueue_changecnt);
      kqueue_events = (struct kevent *)ev_malloc (sizeof (struct kevent) * kqueue_eventmax);
    }

  ts.tv_sec  = (time_t)timeout;
  ts.tv_nsec = (long)((timeout - (ev_tstamp)ts.tv_sec) * 1e9);
  res = kevent (kqueue_fd, kqueue_changes, kqueue_changecnt, kqueue_events, kqueue_eventmax, &ts);
  kqueue_changecnt = 0;

  if (res < 0)
    { 
      if (errno != EINTR)
        syserr ("(libev) kevent");

      return;
    } 

  for (i = 0; i < res; ++i)
    {
      int fd = kqueue_events [i].ident;

      if (kqueue_events [i].flags & EV_ERROR)
        {
	  int err = kqueue_events [i].data;

          /* 
           * errors that may happen
           *   EBADF happens when the file discriptor has been
           *   closed,
           *   ENOENT when the file descriptor was closed and
           *   then reopened.
           *   EINVAL for some reasons not understood; EINVAL
           *   should not be returned ever; but FreeBSD does :-\
           */

          /* we are only interested in errors for fds that we are interested in :) */
          if (anfds [fd].events)
	    {
              if (err == ENOENT) /* resubmit changes on ENOENT */
                kqueue_modify (EV_A_ fd, 0, anfds [fd].events);
              else if (err == EBADF) /* on EBADF, we re-check the fd */
                {
                  if (fd_valid (fd))
                    kqueue_modify (EV_A_ fd, 0, anfds [fd].events);
                  else
                    fd_kill (EV_A_ fd);
                }
              else /* on all other errors, we error out on the fd */
                fd_kill (EV_A_ fd);
	    }
        }
      else
        fd_event (
          EV_A_
          fd,
          kqueue_events [i].filter == EVFILT_READ ? EV_READ
          : kqueue_events [i].filter == EVFILT_WRITE ? EV_WRITE
          : 0
        );
    }

  if (expect_false (res == kqueue_eventmax))
    {
      ev_free (kqueue_events);
      kqueue_eventmax = array_roundsize (struct kevent, kqueue_eventmax << 1);
      kqueue_events = (struct kevent *)ev_malloc (sizeof (struct kevent) * kqueue_eventmax);
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
  kqueue_events = (struct kevent *)ev_malloc (sizeof (struct kevent) * kqueue_eventmax);

  kqueue_changes   = 0;
  kqueue_changemax = 0;
  kqueue_changecnt = 0;

  return EVMETHOD_KQUEUE;
}

static void
kqueue_destroy (EV_P)
{
  close (kqueue_fd);

  ev_free (kqueue_events);
  ev_free (kqueue_changes);
}

static void
kqueue_fork (EV_P)
{
  close (kqueue_fd);

  while ((kqueue_fd = kqueue ()) < 0)
    syserr ("(libev) kqueue");

  fcntl (kqueue_fd, F_SETFD, FD_CLOEXEC);

  /* re-register interest in fds */
  fd_rearm_all (EV_A);
}

