#ifndef EVPP_H__
#define EVPP_H__

/* work in progress, don't use unless you know what you are doing */

namespace ev {

  template<class watcher>
  class callback
  {
    struct object { };

    void *obj;
    void (object::*meth)(watcher &, int);

    /* a proxy is a kind of recipe on how to call a specific class method */
    struct proxy_base {
      virtual void call (void *obj, void (object::*meth)(watcher &, int), watcher &w, int) const = 0;
    };
    template<class O1, class O2>
    struct proxy : proxy_base {
      virtual void call (void *obj, void (object::*meth)(watcher &, int), watcher &w, int e) const
      {
        ((reinterpret_cast<O1 *>(obj)) ->* (reinterpret_cast<void (O2::*)(watcher &, int)>(meth)))
          (w, e);
      }
    };

    proxy_base *prxy;

  public:
    template<class O1, class O2>
    explicit callback (O1 *object, void (O2::*method)(watcher &, int))
    {
      static proxy<O1,O2> p;
      obj  = reinterpret_cast<void *>(object);
      meth = reinterpret_cast<void (object::*)(watcher &, int)>(method);
      prxy = &p;
    }

    void call (watcher *w, int e) const
    {
      return prxy->call (obj, meth, *w, e);
    }
  };

  #include "ev.h"

  enum {
    UNDEF    = EV_UNDEF,
    NONE     = EV_NONE,
    READ     = EV_READ,
    WRITE    = EV_WRITE,
    TIMEOUT  = EV_TIMEOUT,
    PERIODIC = EV_PERIODIC,
    SIGNAL   = EV_SIGNAL,
    IDLE     = EV_IDLE,
    CHECK    = EV_CHECK,
    PREPARE  = EV_PREPARE,
    CHILD    = EV_CHILD,
    ERROR    = EV_ERROR,
  };

  typedef ev_tstamp tstamp;

  inline ev_tstamp now (EV_P)
  {
    return ev_now (EV_A);
  }

  #if EV_MULTIPLICITY

    #define EV_CONSTRUCT(cppstem)							\
      EV_P;                                                                             \
                                                                                        \
      void set (EV_P)                                                                   \
      {                                                                                 \
        this->EV_A = EV_A;                                                              \
      }                                                                                 \
                                                                                        \
      template<class O1, class O2>                                                      \
      explicit cppstem (O1 *object, void (O2::*method)(cppstem &, int), EV_P = ev_default_loop (0)) \
      : callback<cppstem> (object, method), EV_A (EV_A)

  #else

    #define EV_CONSTRUCT(cppstem)							\
      template<class O1, class O2>							\
      explicit cppstem (O1 *object, void (O2::*method)(cppstem &, int))                 \
      : callback<cppstem> (object, method)

  #endif

  /* using a template here would require quite a bit more lines,
   * so a macro solution was chosen */
  #define EV_BEGIN_WATCHER(cppstem,cstem)	                                        \
                                                                                        \
  static void cb_ ## cppstem (struct ev_ ## cstem *w, int revents);                     \
                                                                                        \
  struct cppstem : ev_ ## cstem, callback<cppstem>                                      \
  {                                                                                     \
    EV_CONSTRUCT (cppstem)                                                              \
    {                                                                                   \
      ev_init (static_cast<ev_ ## cstem *>(this), cb_ ## cppstem);                      \
    }                                                                                   \
                                                                                        \
    bool is_active () const                                                             \
    {                                                                                   \
      return ev_is_active (static_cast<const ev_ ## cstem *>(this));                    \
    }                                                                                   \
                                                                                        \
    bool is_pending () const                                                            \
    {                                                                                   \
      return ev_is_pending (static_cast<const ev_ ## cstem *>(this));                   \
    }                                                                                   \
                                                                                        \
    void start ()                                                                       \
    {                                                                                   \
      ev_ ## cstem ## _start (EV_A_ static_cast<ev_ ## cstem *>(this));                 \
    }                                                                                   \
                                                                                        \
    void stop ()                                                                        \
    {                                                                                   \
      ev_ ## cstem ## _stop (EV_A_ static_cast<ev_ ## cstem *>(this));                  \
    }                                                                                   \
                                                                                        \
    void operator ()(int events = EV_UNDEF)                                             \
    {                                                                                   \
      return call (this, events);                                                       \
    }                                                                                   \
                                                                                        \
    ~cppstem ()                                                                         \
    {                                                                                   \
      stop ();                                                                          \
    }                                                                                   \
                                                                                        \
  private:                                                                              \
                                                                                        \
    cppstem (const cppstem &o)								\
    : callback<cppstem> (this, (void (cppstem::*)(cppstem &, int))0)                    \
    { /* disabled */ }                                        				\
    void operator =(const cppstem &o) { /* disabled */ }                                \
                                                                                        \
  public:

  #define EV_END_WATCHER(cppstem,cstem)	                                                \
  };                                                                                    \
                                                                                        \
  static void cb_ ## cppstem (struct ev_ ## cstem *w, int revents)                      \
  {                                                                                     \
    (*static_cast<cppstem *>(w))(revents);                                              \
  }

  EV_BEGIN_WATCHER (io, io)
    void set (int fd, int events)
    {
      int active = is_active ();
      if (active) stop ();
      ev_io_set (static_cast<ev_io *>(this), fd, events);
      if (active) start ();
    }

    void set (int events)
    {
      int active = is_active ();
      if (active) stop ();
      ev_io_set (static_cast<ev_io *>(this), fd, events);
      if (active) start ();
    }

    void start (int fd, int events)
    {
      set (fd, events);
      start ();
    }
  EV_END_WATCHER (io, io)

  EV_BEGIN_WATCHER (timer, timer)
    void set (ev_tstamp after, ev_tstamp repeat = 0.)
    {
      int active = is_active ();
      if (active) stop ();
      ev_timer_set (static_cast<ev_timer *>(this), after, repeat);
      if (active) start ();
    }

    void start (ev_tstamp after, ev_tstamp repeat = 0.)
    {
      set (after, repeat);
      start ();
    }

    void again ()
    {
      ev_timer_again (EV_A_ static_cast<ev_timer *>(this));
    }
  EV_END_WATCHER (timer, timer)

  #if EV_PERIODICS
  EV_BEGIN_WATCHER (periodic, periodic)
    void set (ev_tstamp at, ev_tstamp interval = 0.)
    {
      int active = is_active ();
      if (active) stop ();
      ev_periodic_set (static_cast<ev_periodic *>(this), at, interval, 0);
      if (active) start ();
    }

    void start (ev_tstamp at, ev_tstamp interval = 0.)
    {
      set (at, interval);
      start ();
    }

    void again ()
    {
      ev_periodic_again (EV_A_ static_cast<ev_periodic *>(this));
    }
  EV_END_WATCHER (periodic, periodic)
  #endif

  EV_BEGIN_WATCHER (idle, idle)
  EV_END_WATCHER (idle, idle)

  EV_BEGIN_WATCHER (prepare, prepare)
  EV_END_WATCHER (prepare, prepare)

  EV_BEGIN_WATCHER (check, check)
  EV_END_WATCHER (check, check)

  EV_BEGIN_WATCHER (sig, signal)
    void set (int signum)
    {
      int active = is_active ();
      if (active) stop ();
      ev_signal_set (static_cast<ev_signal *>(this), signum);
      if (active) start ();
    }

    void start (int signum)
    {
      set (signum);
      start ();
    }
  EV_END_WATCHER (sig, signal)

  EV_BEGIN_WATCHER (child, child)
    void set (int pid)
    {
      int active = is_active ();
      if (active) stop ();
      ev_child_set (static_cast<ev_child *>(this), pid);
      if (active) start ();
    }

    void start (int pid)
    {
      set (pid);
      start ();
    }
  EV_END_WATCHER (child, child)

  #undef EV_CONSTRUCT
  #undef EV_BEGIN_WATCHER
}

#endif

