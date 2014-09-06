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
#include <ctype.h>
#include <gio/gio.h>

/* Returns: TRUE if the hex string is valid, FALSE otherwise */
gboolean
neuland_hex_string_to_bin (const gchar *hex_string, guint8 *bin, guint bin_size)
{
  /* 2 hex chars correspond to 8 bit */

  const gchar *pos = hex_string;
  int i;
  for (i = 0; i < bin_size; i++, pos += 2)
    {
      if (!(isxdigit (*pos) && isxdigit (*(pos+1))))
        {
          return FALSE;
        }
      sscanf (pos, "%2hhX", &bin[i]);
    }
  return TRUE;
}

void
neuland_bin_to_hex_string (guint8 *bin, gchar *hex_string, guint bin_size)
{
  gchar *pos = hex_string;
  int i;
  for (i = 0; i < bin_size; i++, pos += 2)
    snprintf (pos, 3, "%02hhX", bin[i]);
}


gboolean
neuland_use_24h_time_format (void)
{
  static GSettings *interface_settings = NULL;
  static gboolean locale_has_am_pm;

  if (interface_settings == NULL)
    {
      interface_settings = g_settings_new ("org.gnome.desktop.interface");
      GDateTime *some_time = g_date_time_new_now_local ();
      gchar *am_pm = g_date_time_format (some_time, "%p");
      locale_has_am_pm = g_strcmp0 (am_pm, "") != 0;

      g_date_time_unref (some_time);
      g_free (am_pm);
    }

  gchar *clock_format = g_settings_get_string (interface_settings, "clock-format");
  gboolean use_24h_time_format = g_strcmp0 (clock_format, "24h") == 0 || !locale_has_am_pm;

  g_free (clock_format);

  g_debug ("use_24h_time_format: %s", use_24h_time_format ? "YES" : "NO");

  return use_24h_time_format;
}
