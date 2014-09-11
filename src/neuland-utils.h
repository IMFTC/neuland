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

#include <glib.h>
#include <gtk/gtk.h>

void
neuland_bin_to_hex_string (guint8 *bin, gchar *hex_string, guint bin_size);

gboolean
neuland_hex_string_to_bin (const gchar *hex_string, guint8 *bin, guint bin_size);

gboolean
neuland_use_24h_time_format (void);

void
list_box_header_func (GtkListBoxRow *row, GtkListBoxRow *before, gpointer user_data);
