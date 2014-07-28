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

#ifndef __NEULAND_TOX_H__
#define __NEULAND_TOX_H__

#include <glib-object.h>
#include <tox/tox.h>

#include "neuland-contact.h"

#define NEULAND_TYPE_TOX            (neuland_tox_get_type ())
#define NEULAND_TOX(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEULAND_TYPE_TOX, NeulandTox))
#define NEULAND_TOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NEULAND_TYPE_TOX, NeulandToxClass))
#define NEULAND_IS_TOX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEULAND_TYPE_TOX))
#define NEULAND_IS_TOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NEULAND_TYPE_TOX))
#define NEULAND_TOX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEULAND_TYPE_TOX, NeulandToxClass))

typedef struct _NeulandTox        NeulandTox;
typedef struct _NeulandToxPrivate NeulandToxPrivate;
typedef struct _NeulandToxClass   NeulandToxClass;

struct _NeulandTox
{
  GObject parent;

  NeulandToxPrivate *priv;
};

struct _NeulandToxClass
{
  GObjectClass parent;

  /* Signals */

  void (* connection_status_changed) (NeulandTox *tox, NeulandContact *contact);
  void (* remove_contacts) (NeulandTox *tox, GSList *contacts, gpointer user_data);
};

NeulandTox *
neuland_tox_new (gchar *data_file);

GList *
neuland_tox_get_contacts (NeulandTox *tox);

void
neuland_tox_save_and_kill (NeulandTox *tox);

void
neuland_tox_set_status (NeulandTox *tox, NeulandContactStatus status);

NeulandContactStatus
neuland_tox_get_status (NeulandTox *tox);

NeulandContact *
neuland_tox_get_contact_by_number (NeulandTox *tox, gint64 number);

void
neuland_tox_set_name (NeulandTox *tox, const gchar *name);

const gchar *
neuland_tox_get_name (NeulandTox *tox);

void
neuland_tox_set_status_message (NeulandTox *tox, const gchar *status_message);

const gchar *
neuland_tox_get_status_message (NeulandTox *tox);

void
neuland_tox_add_contact_from_hex_address (NeulandTox *tox, const gchar *hex_address, const gchar *message);

void
neuland_tox_accept_contact_requests (NeulandTox *tox, GSList *contacts);

gint64
neuland_tox_get_pending_requests (NeulandTox *tox);

const char *
neuland_tox_get_tox_id_hex (NeulandTox *tox);

#endif /* __NEULAND_TOX_H__ */
