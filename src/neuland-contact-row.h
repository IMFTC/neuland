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

#ifndef __NEULAND_CONTACT_ROW__
#define __NEULAND_CONTACT_ROW__

#include <gtk/gtk.h>

#include "neuland-contact.h"

#define NEULAND_TYPE_CONTACT_ROW            (neuland_contact_row_get_type ())
#define NEULAND_CONTACT_ROW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEULAND_TYPE_CONTACT_ROW, NeulandContactRow))
#define NEULAND_CONTACT_ROW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NEULAND_TYPE_CONTACT_ROW, NeulandContactRowClass))
#define NEULAND_IS_CONTACT_ROW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEULAND_TYPE_CONTACT_ROW))
#define NEULAND_IS_CONTACT_ROW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NEULAND_TYPE_CONTACT_ROW))
#define NEULAND_CONTACT_ROW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEULAND_TYPE_CONTACT_ROW, NeulandContactRowClass))

typedef struct _NeulandContactRow        NeulandContactRow;
typedef struct _NeulandContactRowPrivate NeulandContactRowPrivate;
typedef struct _NeulandContactRowClass   NeulandContactRowClass;

struct _NeulandContactRow
{
  GtkListBoxRow parent;

  NeulandContactRowPrivate *priv;
};

struct _NeulandContactRowClass
{
  GtkListBoxRowClass parent;
};

GType
neuland_contact_row_get_type (void);

void
neuland_contact_row_set_unread_messages_count (NeulandContactRow *contact_row, gint count);

gint
neuland_contact_row_get_private_number (NeulandContactRow *contact_row);

NeulandContact*
neuland_contact_row_get_contact (NeulandContactRow *contact_row);

void
neuland_contact_row_set_status (NeulandContactRow *contact_row, NeulandContactStatus new_status);

void
neuland_contact_row_set_name (NeulandContactRow *contact_row, const gchar *name);

void
neuland_contact_row_set_message (NeulandContactRow *contact_row, const gchar *new_message);

void
neuland_contact_row_show_selection (NeulandContactRow *contact_row, gboolean *show_selection);

GtkWidget*
neuland_contact_row_new (NeulandContact *contact);

#endif /* __NEULAND_CONTACT_ROW__ */
