/* -*- mode: c; indent-tabs-mode: nil; -*- */
/*
 * This file is part of Neuland.
 *
 * Copyright © 2014 Volker Sobek <reklov@live.com>
 *
 * Neuland is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Neuland is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Neuland.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "neuland-utils.h"

#include <stdio.h>

/* Returns: TRUE if the hex string is valid, FALSE otherwise */
gboolean
neuland_hex_string_to_bin (guchar *hex_string, guint8 *bin, guint bin_size)
{
  /* 2 hex chars correspond to 8 bit */

  guchar *pos = hex_string;
  int i;
  for (i = 0; i < bin_size; i++, pos += 2)
    {
      if (!(isxdigit (*pos) && isxdigit (*(pos+1))))
        {
          return FALSE;
        }
      sscanf (pos, "%2X", &bin[i]);
    }
  return TRUE;
}

void
neuland_bin_to_hex_string (guint8 *bin, guchar *hex_string, guint bin_size)
{
  guchar *pos = hex_string;
  int i;
  for (i = 0; i < bin_size; i++, pos += 2)
    snprintf (pos, 3, "%02hhX", bin[i]);
}
