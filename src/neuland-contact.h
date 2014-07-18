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

#ifndef __NEULAND_CONTACT_H__
#define __NEULAND_CONTACT_H__

#include <glib-object.h>

#define NEULAND_TYPE_CONTACT            (neuland_contact_get_type ())
#define NEULAND_CONTACT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEULAND_TYPE_CONTACT, NeulandContact))
#define NEULAND_CONTACT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NEULAND_TYPE_CONTACT, NeulandContactClass))
#define NEULAND_IS_CONTACT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEULAND_TYPE_CONTACT))
#define NEULAND_IS_CONTACT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NEULAND_TYPE_CONTACT))
#define NEULAND_CONTACT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEULAND_TYPE_CONTACT, NeulandContactClass))

typedef enum {
  NEULAND_CONTACT_STATUS_NONE, //TOX_USERSTATUS_NONE,
  NEULAND_CONTACT_STATUS_AWAY, //TOX_USERSTATUS_AWAY,
  NEULAND_CONTACT_STATUS_BUSY  //TOX_USERSTATUS_BUSY
} NeulandContactStatus;

typedef struct _NeulandContact        NeulandContact;
typedef struct _NeulandContactPrivate NeulandContactPrivate;
typedef struct _NeulandContactClass   NeulandContactClass;

struct _NeulandContact
{
  GObject parent;

  NeulandContactPrivate *priv;
};

struct _NeulandContactClass
{
  GObjectClass parent;

  void (* incoming_action_or_message) (NeulandContact *contact,
                                       gchar *message,
                                       gpointer user_data);
};

NeulandContact *
neuland_contact_new (gint64 number, const gchar *tox_id, const gchar *contact_name,
                     const gchar *status_message, guint64 last_connected_change);

void
neuland_contact_increase_unread_messages (NeulandContact *self);

void
neuland_contact_reset_unread_messages (NeulandContact *self);

void
neuland_contact_send_message (NeulandContact *self, gchar *outgoing_message);

void
neuland_contact_signal_incoming_message (NeulandContact *self, gchar *incoming_message);

void
neuland_contact_signal_incoming_action (NeulandContact *self, gchar *incoming_message);

void
neuland_contact_set_name (NeulandContact *contact, const gchar *name);

const gchar *
neuland_contact_get_name (NeulandContact *contact);

gboolean
neuland_contact_get_has_chat_widget (NeulandContact *contact);

void
neuland_contact_set_has_chat_widget (NeulandContact *contact, gboolean has_chat_widget);

void
neuland_contact_set_last_online (NeulandContact *contact, guint64 last_online);

guint64
neuland_contact_get_last_online (NeulandContact *contact);

void
neuland_contact_set_connected (NeulandContact *contact, gboolean connected);

gboolean
neuland_contact_get_connected (NeulandContact *contact);

void
neuland_contact_set_is_typing (NeulandContact *self, gboolean is_typing);

gboolean
neuland_contact_get_is_typing (NeulandContact *self);

void
neuland_contact_set_show_typing (NeulandContact *self, gboolean show_typing);

gboolean
neuland_contact_get_show_typing (NeulandContact *self);

const gchar *
neuland_contact_get_tox_id (NeulandContact *self);

const gchar *
neuland_contact_get_status_message (NeulandContact *self);

#endif /* __NEULAND_CONTACT_H__ */
