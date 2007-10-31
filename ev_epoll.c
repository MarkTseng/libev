#include <sys/epoll.h>

static int epoll_fd = -1;

static void
epoll_modify (int fd, int oev, int nev)
{
  int mode = nev ? oev ? EPOLL_CTL_MOD : EPOLL_CTL_ADD : EPOLL_CTL_DEL;

  struct epoll_event ev;
  ev.data.fd = fd;
  ev.events =
      (nev & EV_READ ? EPOLLIN : 0)
      | (nev & EV_WRITE ? EPOLLOUT : 0);

  epoll_ctl (epoll_fd, mode, fd, &ev);
}

void epoll_postfork_child (void)
{
  int fd;

  epoll_fd = epoll_create (256);
  fcntl (epoll_fd, F_SETFD, FD_CLOEXEC);

  /* re-register interest in fds */
  for (fd = 0; fd < anfdmax; ++fd)
    if (anfds [fd].wev)
      epoll_modify (fd, EV_NONE, anfds [fd].wev);
}

static struct epoll_event *events;
static int eventmax;

static void epoll_poll (ev_tstamp timeout)
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
  if (eventcnt == eventmax)
    {
      free (events);
      eventmax += eventmax >> 1;
      events = malloc (sizeof (struct epoll_event) * eventmax);
    }
}

void epoll_init (int flags)
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
