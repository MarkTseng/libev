#define VARx(type,name) VAR(name, type name)

VARx(ev_tstamp, now_floor); /* last time we refreshed rt_time */
VARx(ev_tstamp, mn_now);    /* monotonic clock "now" */
VARx(ev_tstamp, rt_now);
VARx(ev_tstamp, diff);      /* difference realtime - monotonic time */
VARx(int, method);

VARx(int, have_monotonic); /* runtime */

VARx(ev_tstamp, method_fudge); /* assumed typical timer resolution */
VAR (method_modify, void (*method_modify)(EV_P_ int fd, int oev, int nev));
VAR (method_poll  , void (*method_poll)(EV_P_ ev_tstamp timeout));

VARx(int, activecnt); /* number of active events */

#if EV_USE_SELECT
VARx(unsigned char *, vec_ri);
VARx(unsigned char *, vec_ro);
VARx(unsigned char *, vec_wi);
VARx(unsigned char *, vec_wo);
VARx(int, vec_max);
#endif

#if EV_USEV_POLL
VARx(struct pollfd *, polls);
VARx(int, pollmax);
VARx(int, pollcnt);
VARx(int *, pollidxs); /* maps fds into structure indices */
VARx(int, pollidxmax);
#endif

#if EV_USE_EPOLL
VARx(int, epoll_fd);

VARx(struct epoll_event *, events);
VARx(int, eventmax);
#endif

#if EV_USE_KQUEUE
VARx(int, kqueue_fd);
VARx(struct kevent *, kqueue_changes);
VARx(int, kqueue_changemax);
VARx(int, kqueue_changecnt);
VARx(struct kevent *, kqueue_events);
VARx(int, kqueue_eventmax);
#endif

VARx(ANFD *, anfds);
VARx(int, anfdmax);

VAR (pendings, ANPENDING *pendings [NUMPRI]);
VAR (pendingmax, int pendingmax [NUMPRI]);
VAR (pendingcnt, int pendingcnt [NUMPRI]);

VARx(int *, fdchanges);
VARx(int, fdchangemax);
VARx(int, fdchangecnt);

VARx(struct ev_timer **, timers);
VARx(int, timermax);
VARx(int, timercnt);

VARx(struct ev_periodic **, periodics);
VARx(int, periodicmax);
VARx(int, periodiccnt);

VARx(struct ev_idle **, idles);
VARx(int, idlemax);
VARx(int, idlecnt);

VARx(struct ev_prepare **, prepares);
VARx(int, preparemax);
VARx(int, preparecnt);

VARx(struct ev_check **, checks);
VARx(int, checkmax);
VARx(int, checkcnt);

VAR (childs, struct ev_child *childs [PID_HASHSIZE]);
VARx(struct ev_signal, childev);

VARx(struct ev_io, sigev);

#undef VARx

