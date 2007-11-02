
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

static int kq_fd;
static struct kevent *kq_changes;
static int kq_changemax, kq_changecnt;
static struct kevent *kq_events;
static int kq_eventmax;

static void
kqueue_change (int fd, int filter, int flags, int fflags)
{
  struct kevent *ke;

  array_needsize (kq_changes, kq_changemax, ++kq_changecnt, );

  ke = &kq_changes [kq_changecnt - 1];
  memset (ke, 0, sizeof (struct kevent));
  ke->ident  = fd;
  ke->filter = filter;
  ke->flags  = flags;
  ke->fflags = fflags;
}

static void
kqueue_modify (int fd, int oev, int nev)
{
  if ((oev ^ new) & EV_READ)
    {
      if (nev & EV_READ)
        kqueue_change (fd, EVFILT_READ, EV_ADD, NOTE_EOF);
      else
        kqueue_change (fd, EVFILT_READ, EV_DELETE, 0);
    }

  if ((oev ^ new) & EV_WRITE)
    {
      if (nev & EV_WRITE)
        kqueue_change (fd, EVFILT_WRITE, EV_ADD, NOTE_EOF);
      else
        kqueue_change (fd, EVFILT_WRITE, EV_DELETE, 0);
    }
}

static void
kqueue_poll (ev_tstamp timeout)
{
  int res, i;
  struct timespec ts;

  ts.tv_sec  = (time_t)timeout;
  ts.tv_nsec = (long)(timeout - (ev_tstamp)ts.tv_sec) * 1e9;
  res = kevent (kq_fd, kq_changes, kq_changecnt, kq_events, kq_eventmax, &ts);
  kq_changecnt = 0;

  if (res < 0)
    return;

  for (i = 0; i < res; ++i)
    {
      if (kq_events [i].flags & EV_ERROR)
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
          if (events [i].data == EBADF)
            fd_kill (events [i].ident);
        }
      else
        event (
          events [i].ident,
          events [i].filter == EVFILT_READ ? EV_READ
          : events [i].filter == EVFILT_WRITE ? EV_WRITE
          : 0
        );
    }

  if (expect_false (res == kq_eventmax))
    {
      free (kq_events);
      kq_eventmax = array_roundsize (kq_events, kq_eventmax << 1);
      kq_events = malloc (sizeof (struct kevent) * kq_eventmax);
    }
}

static void
kqueue_init (struct event_base *base)
{
  /* Initalize the kernel queue */
  if ((kq_fd = kqueue ()) == -1)
    {
      free (kqueueop);
      return;
    }

  /* Initalize fields */
  kq_changes = malloc (NEVENT * sizeof (struct kevent));
  if (!kq_changes)
    return;

  events = malloc (NEVENT * sizeof (struct kevent));
  if (!events)
    {
      free (kq_changes);
      return;
    }

  /* Check for Mac OS X kqueue bug. */
  kq_changes [0].ident  = -1;
  kq_changes [0].filter = EVFILT_READ;
  kq_changes [0].flags  = EV_ADD;
  /* 
   * If kqueue works, then kevent will succeed, and it will
   * stick an error in events[0]. If kqueue is broken, then
   * kevent will fail.
   */
  if (kevent (kq_fd, kq_changes, 1, kq_events, NEVENT, NULL) != 1
      || kq_events[0].ident != -1
      || kq_events[0].flags != EV_ERROR)
    {
      /* detected broken kqueue */
      free (kq_changes);
      free (kq_events);
      close (kq_fd);
      return;
    }

  ev_method     = EVMETHOD_KQUEUE;
  method_fudge  = 1e-3; /* needed to compensate for kevent returning early */
  method_modify = kq_modify;
  method_poll   = kq_poll;

  kq_eventmax = 64; /* intiial number of events receivable per poll */
  kq_events = malloc (sizeof (struct kevent) * kq_eventmax);
}

