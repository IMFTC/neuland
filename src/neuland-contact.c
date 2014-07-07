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

#include <tox/tox.h>

#include "neuland-enums.h"
#include "neuland-contact.h"

struct _NeulandContactPrivate
{
  gint64 number;
  gchar *name;
  gchar *status_message;

  NeulandContactStatus status;
  gboolean connected;
  gboolean is_typing;
  guint unread_messages;
  guint64 last_connected_change;

  gboolean has_chat_widget;
};

G_DEFINE_TYPE_WITH_PRIVATE (NeulandContact, neuland_contact, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_NUMBER,
  PROP_NAME,
  PROP_CONNECTED,
  PROP_STATUS,
  PROP_STATUS_MESSAGE,
  PROP_UNREAD_MESSAGES,
  PROP_IS_TYPING,
  PROP_LAST_CONNECTED_CHANGE,
  PROP_N
};

enum {
  ENSURE_CHAT_WIDGET,
  INCOMING_MESSAGE,
  INCOMING_ACTION,
  OUTGOING_MESSAGE,
  OUTGOING_ACTION,
  LAST_SIGNAL
};

static GParamSpec *properties[PROP_N] = {NULL, };
static guint signals[LAST_SIGNAL] = { 0 };


void
neuland_contact_set_name (NeulandContact *contact,
                          const gchar *name)
{
  g_return_if_fail (NEULAND_IS_CONTACT (contact));

  g_free (contact->priv->name);
  contact->priv->name = g_strdup (name);

  g_object_notify_by_pspec (G_OBJECT (contact), properties[PROP_NAME]);
}

const gchar *
neuland_contact_get_name (NeulandContact *contact)
{
  g_return_val_if_fail (NEULAND_IS_CONTACT (contact), NULL);
  return contact->priv->name;
}

/* [*0] Before emitting a "incoming-message" or "incoming-action"
   signal, we must ensure that there is a receiver (chat widget) which
   is connected to these signals.  When there isn't one yet
   (priv->has_chat_widget == FALSE) we emit a "ensure-chat-widget"
   signal first. The application is then responsible for creating and
   connecting an appropriate receiver widget to handle the following
   message/action signal correctly and to set (optionally)
   has_chat_widget to TRUE [currently this is done in
   neuland_chat_widget_new (ntox, contact)] to stop emission of the
   "ensure-chat-widget" signal before every message/action.
*/
void
neuland_contact_set_has_chat_widget (NeulandContact *contact,
                                     gboolean has_chat_widget)
{
  contact->priv->has_chat_widget = has_chat_widget;
}

gboolean
neuland_contact_get_has_chat_widget (NeulandContact *contact)
{
  return contact->priv->has_chat_widget;
}

gboolean
neuland_contact_get_connected (NeulandContact *contact)
{
  return contact->priv->connected;
}

guint64
neuland_contact_get_last_connected_change (NeulandContact *contact)
{
  return contact->priv->last_connected_change;
}

gint64
neuland_contact_get_number (NeulandContact *contact)
{
  return contact->priv->number;
}

/* construct-only property, so this is not public */
static void
neuland_contact_set_last_connected_change (NeulandContact *contact,
                                           guint64 last_connected_change)
{
  contact->priv->last_connected_change = last_connected_change;
  g_object_notify_by_pspec (G_OBJECT (contact),
                            properties[PROP_LAST_CONNECTED_CHANGE]);
  g_debug ("setting last_connected_change to %llus since epoche",
           last_connected_change);
}

void
neuland_contact_set_connected (NeulandContact *contact,
                               gboolean connected)
{
  NeulandContactPrivate *priv = contact->priv;
  if (priv->connected == connected)
    return;

  g_object_freeze_notify (G_OBJECT (contact));

  neuland_contact_set_last_connected_change (contact,
                                             (guint64)(g_get_real_time () / 1000000LL));
  priv->connected = connected;
  g_object_notify_by_pspec (G_OBJECT (contact), properties[PROP_CONNECTED]);

  g_object_thaw_notify (G_OBJECT (contact));
}

static void
neuland_contact_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
  NeulandContact *contact = NEULAND_CONTACT (object);
  switch (property_id)
    {
    case PROP_NUMBER:
      contact->priv->number = g_value_get_int64 (value);
      break;
    case PROP_NAME:
      neuland_contact_set_name (contact, g_value_get_string (value));
      break;
    case PROP_CONNECTED:
      neuland_contact_set_connected (contact, g_value_get_boolean (value));
      break;
    case PROP_IS_TYPING:
      contact->priv->is_typing = g_value_get_boolean (value);
      break;
    case PROP_STATUS:
      contact->priv->status = g_value_get_enum (value);
      break;
    case PROP_STATUS_MESSAGE:
      contact->priv->status_message = g_value_dup_string (value);
      break;
    case PROP_UNREAD_MESSAGES:
      contact->priv->unread_messages = g_value_get_uint (value);
      break;
    case PROP_LAST_CONNECTED_CHANGE:
      neuland_contact_set_last_connected_change (contact, g_value_get_uint64 (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
neuland_contact_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
  NeulandContact *contact = NEULAND_CONTACT (object);

  switch (property_id)
    {
    case PROP_NUMBER:
      g_value_set_int64 (value, contact->priv->number);
      break;
    case PROP_NAME:
      g_value_set_string (value, contact->priv->name);
      break;
    case PROP_CONNECTED:
      g_value_set_boolean (value, contact->priv->connected);
      break;
    case PROP_IS_TYPING:
      g_value_set_boolean (value, contact->priv->is_typing);
      break;
    case PROP_STATUS:
      g_value_set_enum (value, contact->priv->status);
      break;
    case PROP_STATUS_MESSAGE:
      g_value_set_string (value, contact->priv->status_message);
      break;
    case PROP_UNREAD_MESSAGES:
      g_value_set_uint (value, contact->priv->unread_messages);
      break;
    case PROP_LAST_CONNECTED_CHANGE:
      g_value_set_uint64 (value, neuland_contact_get_last_connected_change (contact));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

void
neuland_contact_increase_unread_messages (NeulandContact *self)
{
  self->priv->unread_messages++;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_UNREAD_MESSAGES]);
}

void
neuland_contact_reset_unread_messages (NeulandContact *self)
{
  self->priv->unread_messages = 0;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_UNREAD_MESSAGES]);
}

void
neuland_contact_send_message (NeulandContact *self, gchar *outgoing_message)
{
  g_signal_emit (self,
                 signals[OUTGOING_MESSAGE],
                 0,
                 outgoing_message);
}

void
neuland_contact_send_action (NeulandContact *self, gchar *outgoing_action)
{
  g_signal_emit (self,
                 signals[OUTGOING_ACTION],
                 0,
                 outgoing_action);
}

void
neuland_contact_signal_incoming_message (NeulandContact *self,
                                         gchar *incoming_message)
{
  /* See note [*0] */
  if (!self->priv->has_chat_widget)
    g_signal_emit (self, signals[ENSURE_CHAT_WIDGET], 0);
  if (!self->priv->has_chat_widget)
    g_warning ("Apparently there is no chat widget for contact %s, even though "
               "its creation has been requested. The following message might go lost:\n%s",
               neuland_contact_get_name (self), incoming_message);

  g_signal_emit (self,
                 signals[INCOMING_MESSAGE],
                 0,
                 incoming_message);
}

void
neuland_contact_signal_incoming_action (NeulandContact *self,
                                        gchar *incoming_action)
{
  /* See note [*0] */
  if (!self->priv->has_chat_widget)
    g_signal_emit (self, signals[ENSURE_CHAT_WIDGET], 0);
  if (!self->priv->has_chat_widget)
    g_warning ("Apparently there is no chat widget for contact %s, even though "
               "its creation has been requested. The following action might go lost:\n%s",
               neuland_contact_get_name (self), incoming_action);

  g_signal_emit (self,
                 signals[INCOMING_ACTION],
                 0,
                 incoming_action);
}

static void
incoming_action_or_message (NeulandContact *contact,
                            gchar *message,
                            gpointer user_data)
{
  g_debug ("incoming_action_or_message");
}

static void
neuland_contact_finalize (GObject *object)
{

  NeulandContact *contact = NEULAND_CONTACT (object);
  NeulandContactPrivate *priv = contact->priv;

  g_free (priv->name);
  g_free (priv->status_message);

  G_OBJECT_CLASS (neuland_contact_parent_class)->finalize (object);
}

static void
neuland_contact_class_init (NeulandContactClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = neuland_contact_set_property;
  gobject_class->get_property = neuland_contact_get_property;
  gobject_class->finalize = neuland_contact_finalize;
  klass->incoming_action_or_message = incoming_action_or_message;

  /* This differs from int32_t that tox uses, since there is no
     g_param_spec_int32 in glib! */
  properties[PROP_NUMBER] =
    g_param_spec_int64 ("number",
                        "Number",
                        "Number of the contact",
                        -1,
                        G_MAXINT64,
                        -1,
                        G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY);

  properties[PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "The contact's name",
                         "Unnamed Contact",
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT);

  properties[PROP_CONNECTED] =
    g_param_spec_boolean ("connected",
                          "Connection status",
                          "TRUE if the user is connected, FALSE if not",
                          FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT);

  properties[PROP_IS_TYPING] =
    g_param_spec_boolean ("is-typing",
                          "Is typing",
                          "TRUE if the user is currently typing, FALSE if not",
                          FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT);

  properties[PROP_STATUS] =
    g_param_spec_enum ("status",
                       "Status",
                       "The user status",
                       NEULAND_TYPE_CONTACT_STATUS,
                       NEULAND_CONTACT_STATUS_NONE,
                       G_PARAM_READWRITE |
                       G_PARAM_CONSTRUCT);

  properties[PROP_STATUS_MESSAGE] =
    g_param_spec_string ("status-message",
                         "Status message",
                         "The contact's status message",
                         "",
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT);

  properties[PROP_UNREAD_MESSAGES] =
    g_param_spec_uint ("unread-messages",
                       "Unread messages",
                       "Number of unread messages of contact",
                       0,
                       G_MAXUINT,
                       0,
                       G_PARAM_READWRITE |
                       G_PARAM_CONSTRUCT);

  properties[PROP_LAST_CONNECTED_CHANGE] =
    g_param_spec_uint64 ("last-connected-change",
                         "Last connected change",
                         "Timestamp of the last time when the contact's connected property"
                         "changed (contact went offline or came online)",
                         0,
                         G_MAXUINT64,
                         0,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (gobject_class,
                                     PROP_N,
                                     properties);
  signals[INCOMING_MESSAGE] =
    g_signal_new ("incoming-message",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (NeulandContactClass, incoming_action_or_message),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRING);

  signals[INCOMING_ACTION] =
    g_signal_new ("incoming-action",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (NeulandContactClass, incoming_action_or_message),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRING);

  signals[OUTGOING_MESSAGE] =
    g_signal_new ("outgoing-message",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRING);

  signals[OUTGOING_ACTION] =
    g_signal_new ("outgoing-action",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_STRING);

  signals[ENSURE_CHAT_WIDGET] =
    g_signal_new ("ensure-chat-widget",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);
};

static void
neuland_contact_init (NeulandContact *self)
{
  g_debug ("neuland_contact_init");
  NEULAND_IS_CONTACT (self);
  self->priv = neuland_contact_get_instance_private (self);
}

NeulandContact *
neuland_contact_new (gint64 number, gchar *contact_name, guint64 last_connected_change)
{
  g_debug ("neuland_contact_new (%i, %s)", number, contact_name);

  NeulandContact *neuland_contact;
  NeulandContactPrivate *priv = neuland_contact->priv;

  neuland_contact = NEULAND_CONTACT (g_object_new (NEULAND_TYPE_CONTACT,
                                                   "number", number,
                                                   "name", contact_name,
                                                   "last-connected-change", last_connected_change,
                                                   NULL));

  return neuland_contact;
}
