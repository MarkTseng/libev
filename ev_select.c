/*
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

/* for broken bsd's */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

/* for unix systems */
#include <sys/select.h>

#include <string.h>
#include <inttypes.h>

static unsigned char *vec_ri, *vec_ro, *vec_wi, *vec_wo;
static int vec_max;

static void
select_modify (int fd, int oev, int nev)
{
  int offs = fd >> 3;
  int mask = 1 << (fd & 7);

  if (vec_max < (fd >> 5) + 1)
    {
      int new_max = (fd >> 5) + 1;

      vec_ri = (unsigned char *)realloc (vec_ri, new_max * 4);
      vec_ro = (unsigned char *)realloc (vec_ro, new_max * 4); /* could free/malloc */
      vec_wi = (unsigned char *)realloc (vec_wi, new_max * 4);
      vec_wo = (unsigned char *)realloc (vec_wo, new_max * 4); /* could free/malloc */

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

static void select_poll (ev_tstamp timeout)
{
  struct timeval tv;
  int res;

  memcpy (vec_ro, vec_ri, vec_max * 4);
  memcpy (vec_wo, vec_wi, vec_max * 4);

  tv.tv_sec  = (long)timeout;
  tv.tv_usec = (long)((timeout - (ev_tstamp)tv.tv_sec) * 1e6);

  res = select (vec_max * 32, (fd_set *)vec_ro, (fd_set *)vec_wo, 0, &tv);

  if (res > 0)
    {
      int word, offs;

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
                        fd_event (idx * 8 + bit, events);
                    }
              }
        }
    }
  else if (res < 0)
    {
      if (errno == EBADF)
        fd_recheck ();
    }
}

void select_init (int flags)
{
  ev_method     = EVMETHOD_SELECT;
  method_fudge  = 1e-2; /* needed to compensate for select returning early, very conservative */
  method_modify = select_modify;
  method_poll   = select_poll;
}


