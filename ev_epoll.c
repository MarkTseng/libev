#include <sys/epoll.h>

static int epoll_fd = -1;

static void
epoll_reify_fd (int fd)
{
  ANFD *anfd = anfds + fd;
  struct ev_io *w;

  int wev = 0;

  for (w = anfd->head; w; w = w->next)
    wev |= w->events;

  if (anfd->wev != wev)
    {
      int mode = wev ? anfd->wev ? EPOLL_CTL_MOD : EPOLL_CTL_ADD : EPOLL_CTL_DEL;
      struct epoll_event ev;
      ev.events = wev;
      ev.data.fd = fd;
      fprintf (stderr, "reify %d,%d,%d m%d (r=%d)\n", fd, anfd->wev, wev, mode,//D
      epoll_ctl (epoll_fd, mode, fd, &ev)
      );//D
      anfd->wev = wev;
    }
}

void epoll_postfork_child (void)
{
  int i;

  epoll_fd = epoll_create (256);

  for (i = 0; i < anfdmax; ++i)
    epoll_reify_fd (i);
}

static void epoll_reify (void)
{
  int i;
  for (i = 0; i < fdchangecnt; ++i)
    epoll_reify_fd (fdchanges [i]);
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
      (events [i].events & (EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLPRI) ? EV_WRITE : 0)
      | (events [i].events & (EPOLLIN | EPOLLERR | EPOLLHUP)           ? EV_READ  : 0)
    );

  /* if the receive array was full, increase its size */
  if (eventcnt == eventmax)
    {
      free (events);
      eventmax += eventmax >> 1;
      events = malloc (sizeof (struct epoll_event) * eventmax);
    }
}

int epoll_init (int flags)
{
  epoll_fd = epoll_create (256);

  if (epoll_fd < 0)
    return 0;

  ev_method = EVMETHOD_EPOLL;
  method_fudge = 1e-3; /* needed to compensate fro epoll returning early */
  method_reify = epoll_reify;
  method_poll  = epoll_poll;

  eventmax = 64; /* intiial number of events receivable per poll */
  events = malloc (sizeof (struct epoll_event) * eventmax);

  return 1;
}
