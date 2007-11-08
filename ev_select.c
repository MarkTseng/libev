/*
 * libev select fd activity backend
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

/* for unix systems */
#if WIN32
typedef unsigned int uint32_t;
# ifndef EV_SELECT_USE_FD_SET
#  define EV_SELECT_USE_FD_SET 1
# endif
#else
# include <sys/select.h>
# include <inttypes.h>
#endif

#if EV_SELECT_USE_WIN32_HANDLES
# undef EV_SELECT_USE_FD_SET
# define EV_SELECT_USE_FD_SET 1
#endif

#include <string.h>

static void
select_modify (EV_P_ int fd, int oev, int nev)
{
  if (oev == nev)
    return;

#if EV_SELECT_USE_FD_SET
# if EV_SELECT_USE_WIN32_HANDLES
  fd = _get_osfhandle (fd);
  if (fd < 0)
    return;
# endif

  if (nev & EV_READ)
    FD_SET (fd, (struct fd_set *)vec_ri);
  else
    FD_CLR (fd, (struct fd_set *)vec_ri);

  if (nev & EV_WRITE)
    FD_SET (fd, (struct fd_set *)vec_wi);
  else
    FD_CLR (fd, (struct fd_set *)vec_wi);
#else
  {
    int offs = fd >> 3;
    int mask = 1 << (fd & 7);

    if (vec_max < (fd >> 5) + 1)
      {
        int new_max = (fd >> 5) + 1;

        vec_ri = (unsigned char *)ev_realloc (vec_ri, new_max * 4);
        vec_ro = (unsigned char *)ev_realloc (vec_ro, new_max * 4); /* could free/malloc */
        vec_wi = (unsigned char *)ev_realloc (vec_wi, new_max * 4);
        vec_wo = (unsigned char *)ev_realloc (vec_wo, new_max * 4); /* could free/malloc */

        for (; vec_max < new_max; ++vec_max)
          ((uint32_t *)vec_ri)[vec_max] =
          ((uint32_t *)vec_wi)[vec_max] = 0;
      }

    vec_ri [offs] |= mask;
    if (!(nev & EV_READ))
      vec_ri [offs] &= ~mask;

    vec_wi [offs] |= mask;
    if (!(nev & EV_WRITE))
      vec_wi [offs] &= ~mask;
  }
#endif
}

static void
select_poll (EV_P_ ev_tstamp timeout)
{
  int word, offs;
  struct timeval tv;
  int res;

#if EV_SELECT_USE_FD_SET
  memcpy (vec_ro, vec_ri, sizeof (struct fd_set));
  memcpy (vec_wo, vec_wi, sizeof (struct fd_set));
#else
  memcpy (vec_ro, vec_ri, vec_max * 4);
  memcpy (vec_wo, vec_wi, vec_max * 4);
#endif

  tv.tv_sec  = (long)timeout;
  tv.tv_usec = (long)((timeout - (ev_tstamp)tv.tv_sec) * 1e6);

  res = select (vec_max * 32, (fd_set *)vec_ro, (fd_set *)vec_wo, 0, &tv);

  if (res < 0)
    {
#ifdef WIN32
      if (errno == WSAEINTR   ) errno = EINTR;
      if (errno == WSAENOTSOCK) errno = EBADF;
#endif

      if (errno == EBADF)
        fd_ebadf (EV_A);
      else if (errno == ENOMEM && !syserr_cb)
        fd_enomem (EV_A);
      else if (errno != EINTR)
        syserr ("(libev) select");

      return;
    }

#if EV_SELECT_USE_FD_SET
# if EV_SELECT_USE_WIN32_HANDLES
  for (word = 0; word < anfdmax; ++word)
    {
      if (!anfd [word].events)
        {
          int fd = _get_osfhandle (word);

          if (fd >= 0)
            {
              int events = 0;

              if (FD_ISSET (fd, (struct fd_set *)vec_ro)) events |= EV_READ;
              if (FD_ISSET (fd, (struct fd_set *)vec_wo)) events |= EV_WRITE;

              if (events)
                fd_event (EV_A_ word, events);
            }
        }
    }
# else
  for (word = 0; word < FD_SETSIZE; ++word)
    {
      int events = 0;
      if (FD_ISSET (word, (struct fd_set *)vec_ro)) events |= EV_READ;
      if (FD_ISSET (word, (struct fd_set *)vec_wo)) events |= EV_WRITE;

      if (events)
        fd_event (EV_A_ word, events);
    }
# endif
#else
  for (word = vec_max; word--; )
    {
      if (((uint32_t *)vec_ro) [word] | ((uint32_t *)vec_wo) [word])
        for (offs = 4; offs--; )
          {
            int idx = word * 4 + offs;
            unsigned char byte_r = vec_ro [idx];
            unsigned char byte_w = vec_wo [idx];
            int bit;

            if (byte_r | byte_w)
              for (bit = 8; bit--; )
                {
                  int events = 0;
                  events |= byte_r & (1 << bit) ? EV_READ  : 0;
                  events |= byte_w & (1 << bit) ? EV_WRITE : 0;

                  if (events)
                    fd_event (EV_A_ idx * 8 + bit, events);
                }
          }
    }
#endif
}

static int
select_init (EV_P_ int flags)
{
  method_fudge  = 1e-2; /* needed to compensate for select returning early, very conservative */
  method_modify = select_modify;
  method_poll   = select_poll;

#if EV_SELECT_USE_FD_SET
  vec_max = FD_SETSIZE / 32;
  vec_ri  = ev_malloc (sizeof (struct fd_set)); FD_ZERO ((struct fd_set *)vec_ri);
  vec_ro  = ev_malloc (sizeof (struct fd_set));
  vec_wi  = ev_malloc (sizeof (struct fd_set)); FD_ZERO ((struct fd_set *)vec_wi);
  vec_wo  = ev_malloc (sizeof (struct fd_set));
#else
  vec_max = 0;
  vec_ri  = 0; 
  vec_ri  = 0;   
  vec_wo  = 0; 
  vec_wo  = 0; 
#endif

  return EVMETHOD_SELECT;
}

static void
select_destroy (EV_P)
{
  ev_free (vec_ri);
  ev_free (vec_ro);
  ev_free (vec_wi);
  ev_free (vec_wo);
}


