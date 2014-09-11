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
#include <glib/gi18n.h>

#include "neuland-contact.h"
#include "neuland-utils.h"
#include "neuland-enums.h"

#define NEULAND_CONTACT_SHOW_TYPING_TIMEOUT 3 /* Seconds */
#define MAX_PREFERRED_NAME_LENGTH 12          /* UTF-8 chars, not bytes */

struct _NeulandContactPrivate
{
  gchar *tox_id_hex;
  gchar *name;
  gchar *preferred_name;
  gchar *status_message;
  gchar *request_message;
  gchar *last_seen;

  gpointer tox_id;
  guint unread_messages;
  guint show_typing_timeout_id;
  gint64 number;
  guint64 last_connected_change;
  gboolean connected;
  gboolean is_typing;
  gboolean has_chat_widget;
  NeulandContactStatus status;

  GHashTable *file_transfers_send;
  GHashTable *file_transfers_receive;
  GHashTable *file_transfers_all;
};

G_DEFINE_TYPE_WITH_PRIVATE (NeulandContact, neuland_contact, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_TOX_ID,
  PROP_TOX_ID_HEX,
  PROP_NUMBER,
  PROP_NAME,
  PROP_PREFERRED_NAME,
  PROP_REQUEST_MESSAGE,
  PROP_CONNECTED,
  PROP_STATUS,
  PROP_STATUS_MESSAGE,
  PROP_LAST_SEEN,
  PROP_UNREAD_MESSAGES,
  PROP_IS_TYPING,
  PROP_SHOW_TYPING,
  PROP_LAST_CONNECTED_CHANGE,
  PROP_N
};

enum {
  ENSURE_CHAT_WIDGET,
  INCOMING_MESSAGE,
  INCOMING_ACTION,
  OUTGOING_MESSAGE,
  OUTGOING_ACTION,
  NEW_TRANSFER,
  LAST_SIGNAL
};

static GParamSpec *properties[PROP_N] = {NULL, };
static guint signals[LAST_SIGNAL] = { 0 };

static void
neuland_contact_update_preferred_name (NeulandContact *contact)
{
  NeulandContactPrivate *priv = contact->priv;
  const gchar *name = priv->name;
  glong name_length = g_utf8_strlen (name, MAX_PREFERRED_NAME_LENGTH);

  g_free (priv->preferred_name);

  if (name_length > 0)
    priv->preferred_name = g_utf8_substring (name, 0, name_length);
  else
    priv->preferred_name = g_strndup (priv->tox_id_hex, MAX_PREFERRED_NAME_LENGTH);

  g_debug ("Preferred name for contact %p changed to: \"%s\"", contact, priv->preferred_name);
}

void
neuland_contact_set_name (NeulandContact *contact,
                          const gchar *name)
{
  NeulandContactPrivate *priv;

  g_return_if_fail (NEULAND_IS_CONTACT (contact));

  priv = contact->priv;

  if (g_strcmp0 (priv->name, name) == 0)
    return;

  g_free (priv->name);

  priv->name = g_strdup (name ? name : "");

  neuland_contact_update_preferred_name (contact);

  g_object_notify_by_pspec (G_OBJECT (contact), properties[PROP_NAME]);
  g_object_notify_by_pspec (G_OBJECT (contact), properties[PROP_PREFERRED_NAME]);
}


const gchar *
neuland_contact_get_name (NeulandContact *contact)
{
  g_return_val_if_fail (NEULAND_IS_CONTACT (contact), NULL);

  return contact->priv->name;
}

void
neuland_contact_set_request_message (NeulandContact *contact,
                                     const gchar *request_message)
{
  NeulandContactPrivate *priv;

  g_return_if_fail (NEULAND_IS_CONTACT (contact));

  priv = contact->priv;
  g_free (contact->priv->request_message);
  priv->request_message = g_strdup (request_message);

  g_object_notify_by_pspec (G_OBJECT (contact), properties[PROP_REQUEST_MESSAGE]);
}

const gchar *
neuland_contact_get_request_message (NeulandContact *contact)
{
  g_return_val_if_fail (NEULAND_IS_CONTACT (contact), NULL);

  return contact->priv->request_message;
}

/* [*0] Before emitting a "incoming-message" or "incoming-action"
   signal, we must ensure that there is a receiver (chat widget) which
   is connected to these signals.  When there isn't one yet
   (priv->has_chat_widget == FALSE) we emit a "ensure-chat-widget"
   signal first. The application is then responsible for creating and
   connecting an appropriate receiver widget to handle the following
   message/action signal correctly and to set (optionally)
   has_chat_widget to TRUE [currently this is done in
   neuland_chat_widget_new (tox, contact)] to stop emission of the
   "ensure-chat-widget" signal before every message/action.
*/
void
neuland_contact_set_has_chat_widget (NeulandContact *contact,
                                     gboolean has_chat_widget)
{
  g_return_if_fail (NEULAND_IS_CONTACT (contact));

  contact->priv->has_chat_widget = has_chat_widget;
}

gboolean
neuland_contact_get_has_chat_widget (NeulandContact *contact)
{
  g_return_val_if_fail (NEULAND_IS_CONTACT (contact), NULL);

  return contact->priv->has_chat_widget;
}

gboolean
neuland_contact_get_connected (NeulandContact *contact)
{
  g_return_val_if_fail (NEULAND_IS_CONTACT (contact), FALSE);

  return contact->priv->connected;
}

guint64
neuland_contact_get_last_connected_change (NeulandContact *contact)
{
  g_return_val_if_fail (NEULAND_IS_CONTACT (contact), 0);

  return contact->priv->last_connected_change;
}

static void
neuland_contact_update_last_seen (NeulandContact *contact)
{
  NeulandContactPrivate *priv;
  guint64 last_connected_change;

  g_return_if_fail (NEULAND_IS_CONTACT (contact));

  priv = contact->priv;
  last_connected_change = priv->last_connected_change;

  g_free (priv->last_seen);

  if (priv->connected)
    /* This shouldn't be displayed */
    /* Translators: This is the status message when the contact is currently online. */
    priv->last_seen = g_strdup (_("Now Online"));
  else
    {
      if (last_connected_change == 0)
        /* Translators: This is schown when we haven't seen this contact online yet. */
        priv->last_seen = g_strdup (_("Never"));
      else
        {
          gchar *format;
          gint y_now, m_now, d_now;  /* year, month, day */
          GDateTime *now = g_date_time_new_now_local ();
          GDateTime *last = g_date_time_new_from_unix_local (last_connected_change);
          GDateTime *start_today;
          GDateTime *start_yesterday;
          GDateTime *start_6_days_ago;
          GDateTime *start_year;

          g_date_time_get_ymd (now, &y_now, &m_now, &d_now);

          start_today = g_date_time_new_local (y_now, m_now, d_now, 0, 0, 0);
          start_yesterday = g_date_time_add_days (start_today, -1);
          start_6_days_ago = g_date_time_add_days (start_today, -6);
          start_year = g_date_time_new_local (y_now, 1, 1, 0, 0, 0);

          if (neuland_use_24h_time_format ())
            {
              if (g_date_time_compare (start_today, last) != 1)
                /* start_today <= last */

                /* Translators: Time in 24h format */
                format = _("%H:%M");

              else if (g_date_time_compare (start_yesterday, last) != 1)
                /* start_yesterday <= last */

                /* Translators: This is "Yesterday" followed by a time string in 24h format */
                format = _("Yesterday, %H:%M");

              else if (g_date_time_compare (start_6_days_ago, last) != 1)
                /* 6 or less days ago (not 7 days ago, because we don't want
                   to show Monday 10:12 when today is Monday, too). */

                /* Translators: This is a abbreviated week day name
                   followed by a time string in 24h format */
                format = _("%a, %H:%M");

              else if (g_date_time_compare (start_year, last) != 1)
                /* start_year <= last */

                /* Translators: This is the abbreviated month name and day
                   number followed by a time string in 24h format */
                format = _("%b %d, %H:%M");

              else
                /* last < start_year */

                /* Translators: This is the abbreviated month name, day
                   number, year number followed by a time string in 24h
                   format */
                format = _("%b %d %Y, %H:%M");
            }
          else
            {
              if (g_date_time_compare (start_today, last) != 1)
                /* start_today <= last */
                /* Translators: Time in 12h format */
                format = _("%l:%M %p");
              else if (g_date_time_compare (start_yesterday, last) != 1)
                /* start_yesterday <= last */
                /* Translators: This is "Yesterday" followed by a time string in 12h format */
                format = _("Yesterday, %l:%M %p");

              else if (g_date_time_compare (start_6_days_ago, last) != 1)
                /* 6 or less days ago (not 7 days ago, because we don't want
                   to show Monday 10:12 when today is Monday, too). */

                /* Translators: This is a abbreviated week day name
                   followed by a time string in 12h format */
                format = _("%a, %l:%M %p");

              else if (g_date_time_compare (start_year, last) != 1)
                /* start_year <= last */
                /* Translators: This is the abbreviated month name, day
                   number followed by a time string in 12h format */
                format = _("%b %d, %l:%M %p");

              else
                /* last < start_year */
                /* Translators: This is the abbreviated month name, day
                   number, year number followed by a time string in 12h
                   format. */
                format = _("%b %d %Y, %l:%M %p");
            }

          priv->last_seen = g_date_time_format (last, format);

          g_date_time_unref (now);
          g_date_time_unref (last);
          g_date_time_unref (start_today);
          g_date_time_unref (start_yesterday);
          g_date_time_unref (start_6_days_ago);
          g_date_time_unref (start_year);
        }
    }

  g_object_notify_by_pspec (G_OBJECT (contact),
                            properties[PROP_LAST_SEEN]);
}

static void
neuland_contact_set_last_connected_change (NeulandContact *contact,
                                           guint64 last_connected_change)
{
  g_return_if_fail (NEULAND_IS_CONTACT (contact));

  contact->priv->last_connected_change = last_connected_change;

  g_object_freeze_notify (G_OBJECT (contact));

  neuland_contact_update_last_seen (contact);
  g_object_notify_by_pspec (G_OBJECT (contact),
                            properties[PROP_LAST_CONNECTED_CHANGE]);

  g_object_thaw_notify (G_OBJECT (contact));
}

void
neuland_contact_set_connected (NeulandContact *contact,
                               gboolean connected)
{
  NeulandContactPrivate *priv;

  g_return_if_fail (NEULAND_IS_CONTACT (contact));

  priv = contact->priv;

  if (priv->connected == connected)
    return;

  g_object_freeze_notify (G_OBJECT (contact));

  neuland_contact_set_last_connected_change (contact, (guint64)(g_get_real_time () / 1000000LL));
  priv->connected = connected;
  g_object_notify_by_pspec (G_OBJECT (contact), properties[PROP_CONNECTED]);

  neuland_contact_update_last_seen (contact);

  g_object_thaw_notify (G_OBJECT (contact));
}

gboolean
neuland_contact_get_is_typing (NeulandContact *contact)
{
  g_return_val_if_fail (NEULAND_IS_CONTACT (contact), FALSE);

  return contact->priv->is_typing;
}

gboolean
show_typing_timeout_func (gpointer user_data)
{
  NeulandContact *contact = NEULAND_CONTACT (user_data);
  NeulandContactPrivate *priv = contact->priv;

  priv->show_typing_timeout_id = 0;
  g_object_notify_by_pspec (G_OBJECT (contact), properties[PROP_SHOW_TYPING]);

  return G_SOURCE_REMOVE;
}

void
neuland_contact_set_show_typing (NeulandContact *contact, gboolean show_typing)
{
  NeulandContactPrivate *priv;
  gboolean old_show_typing;

  g_return_if_fail (NEULAND_IS_CONTACT (contact));

  priv = contact->priv;
  old_show_typing = (priv->show_typing_timeout_id != 0);

  if (old_show_typing)
    g_source_remove (priv->show_typing_timeout_id);

  if (show_typing)
    priv->show_typing_timeout_id =
      g_timeout_add_seconds (NEULAND_CONTACT_SHOW_TYPING_TIMEOUT,
                             show_typing_timeout_func, contact);
  else
    priv->show_typing_timeout_id = 0;

  if (show_typing != old_show_typing)
    g_object_notify_by_pspec (G_OBJECT (contact), properties[PROP_SHOW_TYPING]);
}

const gchar *
neuland_contact_get_status_message (NeulandContact *contact)
{
  g_return_val_if_fail (NEULAND_IS_CONTACT (contact), NULL);

  return contact->priv->status_message;
}

gboolean
neuland_contact_get_show_typing (NeulandContact *contact)
{
  g_return_val_if_fail (NEULAND_IS_CONTACT (contact), FALSE);

  return (contact->priv->show_typing_timeout_id != 0);
}

void
neuland_contact_set_is_typing (NeulandContact *contact, gboolean is_typing)
{
  g_return_if_fail (NEULAND_IS_CONTACT (contact));

  contact->priv->is_typing = is_typing;
  g_object_notify_by_pspec (G_OBJECT (contact), properties[PROP_IS_TYPING]);
}

gboolean
neuland_contact_is_request (NeulandContact *contact)
{
  g_return_val_if_fail (NEULAND_IS_CONTACT (contact), FALSE);

  return contact->priv->number < 0;
}

/* Only set on construction; not public. */
static void
neuland_contact_set_tox_id (NeulandContact *contact, gpointer tox_id)
{
  NeulandContactPrivate *priv;

  g_return_if_fail (NEULAND_IS_CONTACT (contact));

  priv = contact->priv;
  priv->tox_id = g_memdup (tox_id, TOX_CLIENT_ID_SIZE);
  priv->tox_id_hex = g_malloc0 (TOX_CLIENT_ID_SIZE * 2 + 1);

  neuland_bin_to_hex_string ((guint8 *)priv->tox_id, priv->tox_id_hex, TOX_CLIENT_ID_SIZE);
}

const gpointer
neuland_contact_get_tox_id (NeulandContact *contact)
{
  g_return_val_if_fail (NEULAND_IS_CONTACT (contact), NULL);

  return contact->priv->tox_id;
}

const gchar *
neuland_contact_get_tox_id_hex (NeulandContact *contact)
{
  g_return_val_if_fail (NEULAND_IS_CONTACT (contact), NULL);

  return contact->priv->tox_id_hex;
}

void
neuland_contact_set_number (NeulandContact *contact, gint64 number)
{
  g_return_if_fail (NEULAND_IS_CONTACT (contact));

  contact->priv->number = number;
  g_object_notify_by_pspec (G_OBJECT (contact), properties[PROP_NUMBER]);
}

gint64
neuland_contact_get_number (NeulandContact *contact)
{
  g_return_val_if_fail (NEULAND_IS_CONTACT (contact), -1);

  return contact->priv->number;
}

/* This function should only be called inside
   neuland_tox_add_file_transfer() right now! */
void
neuland_contact_add_file_transfer (NeulandContact *contact,
                                   NeulandFileTransfer *file_transfer)
{
  NeulandContactPrivate *priv;
  NeulandFileTransferDirection direction;

  g_return_if_fail (NEULAND_IS_CONTACT (contact));
  g_return_if_fail (NEULAND_IS_FILE_TRANSFER (file_transfer));
  g_return_if_fail (neuland_file_transfer_get_file_number (file_transfer) != -1);

  priv = contact->priv;
  direction = neuland_file_transfer_get_direction (file_transfer);

  if (direction == NEULAND_FILE_TRANSFER_DIRECTION_RECEIVE)
    {
      g_hash_table_insert (priv->file_transfers_receive,
                           GINT_TO_POINTER (neuland_file_transfer_get_file_number
                                            (file_transfer)), file_transfer);
    }
  else if (direction == NEULAND_FILE_TRANSFER_DIRECTION_SEND)
    {
      g_hash_table_insert (priv->file_transfers_send,
                           GINT_TO_POINTER (neuland_file_transfer_get_file_number
                                            (file_transfer)), file_transfer);
    }
  else
    g_warn_if_reached ();

  g_hash_table_insert (priv->file_transfers_all, file_transfer, file_transfer);

  /* Ensure we have a chat widget for contact */
  if (!priv->has_chat_widget)
    g_signal_emit (contact, signals[ENSURE_CHAT_WIDGET], 0);
  /* The chat widget will add a file transfer when getting this signal */
  g_signal_emit (contact, signals[NEW_TRANSFER], 0, file_transfer);
}

NeulandFileTransfer *
neuland_contact_get_file_transfer (NeulandContact *contact,
                                   NeulandFileTransferDirection direction,
                                   gint file_number)
{
  NeulandContactPrivate *priv;
  GHashTable *table;

  g_return_val_if_fail (NEULAND_IS_CONTACT (contact), NULL);

  priv = contact->priv;

  table = direction == NEULAND_FILE_TRANSFER_DIRECTION_RECEIVE ?
    priv->file_transfers_receive :
    priv->file_transfers_send;

  return g_hash_table_lookup (table, GINT_TO_POINTER (file_number));
}

/* free returned list with g_list_free() */
GList *
neuland_contact_get_file_transfers (NeulandContact *contact)
{
  g_return_val_if_fail (NEULAND_IS_CONTACT (contact), NULL);

  g_hash_table_get_values (contact->priv->file_transfers_all);
}


/* Returns the preferred name (truncated to 12 chars) for
   contact. String is owned by @contact, don't free it. */
const gchar *
neuland_contact_get_preferred_name (NeulandContact *contact)
{
  g_return_val_if_fail (NEULAND_IS_CONTACT (contact), NULL);

  return contact->priv->preferred_name;
}

const gchar *
neuland_contact_get_last_seen (NeulandContact *contact)
{
  g_return_val_if_fail (NEULAND_IS_CONTACT (contact), NULL);

  return contact->priv->last_seen;
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
      neuland_contact_set_number (contact, g_value_get_int64 (value));
      break;
    case PROP_TOX_ID:
      neuland_contact_set_tox_id (contact, g_value_get_pointer (value));
      break;
    case PROP_NAME:
      neuland_contact_set_name (contact, g_value_get_string (value));
      break;
    case PROP_REQUEST_MESSAGE:
      neuland_contact_set_request_message (contact, g_value_get_string (value));
      break;
    case PROP_CONNECTED:
      neuland_contact_set_connected (contact, g_value_get_boolean (value));
      break;
    case PROP_IS_TYPING:
      neuland_contact_set_is_typing (contact, g_value_get_boolean (value));
      break;
    case PROP_SHOW_TYPING:
      neuland_contact_set_show_typing (contact, g_value_get_boolean (value));
      break;
    case PROP_STATUS:
      contact->priv->status = g_value_get_enum (value);
      break;
    case PROP_STATUS_MESSAGE:
      g_free (contact->priv->status_message);
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
      g_value_set_int64 (value, neuland_contact_get_number (contact));
      break;
    case PROP_TOX_ID:
      g_value_set_pointer (value, neuland_contact_get_tox_id (contact));
      break;
    case PROP_TOX_ID_HEX:
      g_value_set_string (value, neuland_contact_get_tox_id_hex (contact));
      break;
    case PROP_NAME:
      g_value_set_string (value, contact->priv->name);
      break;
    case PROP_PREFERRED_NAME:
      g_value_set_string (value, neuland_contact_get_preferred_name (contact));
      break;
    case PROP_REQUEST_MESSAGE:
      g_value_set_string (value, contact->priv->request_message);
      break;
    case PROP_CONNECTED:
      g_value_set_boolean (value, contact->priv->connected);
      break;
    case PROP_IS_TYPING:
      g_value_set_boolean (value, neuland_contact_get_is_typing (contact));
      break;
    case PROP_SHOW_TYPING:
      g_value_set_boolean (value, neuland_contact_get_show_typing (contact));
      break;
    case PROP_STATUS:
      g_value_set_enum (value, contact->priv->status);
      break;
    case PROP_STATUS_MESSAGE:
      g_value_set_string (value, contact->priv->status_message);
      break;
    case PROP_LAST_SEEN:
      g_value_set_string (value, neuland_contact_get_last_seen (contact));
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
neuland_contact_increase_unread_messages (NeulandContact *contact)
{
  g_return_if_fail (NEULAND_IS_CONTACT (contact));

  contact->priv->unread_messages++;
  g_object_notify_by_pspec (G_OBJECT (contact), properties[PROP_UNREAD_MESSAGES]);
}

void
neuland_contact_reset_unread_messages (NeulandContact *contact)
{
  g_return_if_fail (NEULAND_IS_CONTACT (contact));

  contact->priv->unread_messages = 0;
  g_object_notify_by_pspec (G_OBJECT (contact), properties[PROP_UNREAD_MESSAGES]);
}

void
neuland_contact_send_message (NeulandContact *contact, const gchar *outgoing_message)
{
  g_return_if_fail (NEULAND_IS_CONTACT (contact));

  g_signal_emit (contact,
                 signals[OUTGOING_MESSAGE],
                 0,
                 outgoing_message);
}

void
neuland_contact_send_action (NeulandContact *contact, gchar *outgoing_action)
{
  g_return_if_fail (NEULAND_IS_CONTACT (contact));

  g_signal_emit (contact,
                 signals[OUTGOING_ACTION],
                 0,
                 outgoing_action);
}

void
neuland_contact_signal_incoming_message (NeulandContact *contact,
                                         const gchar *incoming_message)
{
  NeulandContactPrivate *priv;

  g_return_if_fail (NEULAND_IS_CONTACT (contact));

  priv = contact->priv;

  /* See note [*0] */
  if (!priv->has_chat_widget)
    g_signal_emit (contact, signals[ENSURE_CHAT_WIDGET], 0);
  if (!priv->has_chat_widget)
    g_warning ("Apparently there is no chat widget for contact %s, even though "
               "its creation has been requested. The following message might go lost:\n%s",
               priv->preferred_name, incoming_message);

  g_signal_emit (contact,
                 signals[INCOMING_MESSAGE],
                 0,
                 incoming_message);
}

void
neuland_contact_signal_incoming_action (NeulandContact *contact,
                                        const gchar *incoming_action)
{
  NeulandContactPrivate *priv;

  g_return_if_fail (NEULAND_IS_CONTACT (contact));

  priv = contact->priv;

  /* See note [*0] */
  if (!priv->has_chat_widget)
    g_signal_emit (contact, signals[ENSURE_CHAT_WIDGET], 0);
  if (!priv->has_chat_widget)
    g_warning ("Apparently there is no chat widget for contact %s, even though "
               "its creation has been requested. The following action might go lost:\n%s",
               priv->preferred_name, incoming_action);

  g_signal_emit (contact,
                 signals[INCOMING_ACTION],
                 0,
                 incoming_action);
}

/* This isn't useful for anything right now, maybe later? */
static void
incoming_action_or_message (NeulandContact *contact,
                            const gchar *message,
                            gpointer user_data)
{
  /* empty */
}

static void
neuland_contact_finalize (GObject *object)
{
  NeulandContact *contact = NEULAND_CONTACT (object);
  NeulandContactPrivate *priv = contact->priv;

  g_debug ("neuland_contact_finalize (%p)", object);

  g_free (priv->name);
  g_free (priv->preferred_name);
  g_free (priv->status_message);
  g_free (priv->request_message);
  g_free (priv->tox_id);
  g_free (priv->tox_id_hex);
  g_free (priv->last_seen);

  g_hash_table_destroy (priv->file_transfers_all);
  g_hash_table_destroy (priv->file_transfers_send);
  g_hash_table_destroy (priv->file_transfers_receive);

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
                        G_MAXINT32,
                        -1,
                        G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT);

  properties[PROP_TOX_ID] =
    g_param_spec_pointer ("tox-id",
                          "Tox ID",
                          "The contact's Tox ID in binary format",
                          G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY);

  properties[PROP_TOX_ID_HEX] =
    g_param_spec_string ("tox-id-hex",
                         "Tox ID hex",
                         "The contact's Tox ID as a hex string",
                         "",
                         G_PARAM_READABLE);

  properties[PROP_PREFERRED_NAME] =
    g_param_spec_string ("preferred-name",
                         "Preffered name",
                         "Returns the first none empty property of: name, tox-id-hex."
                         "Truncates to 12 chars",
                         "",
                         G_PARAM_READABLE);

  properties[PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "The contact's name",
                         "",
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT);

  properties[PROP_REQUEST_MESSAGE] =
    g_param_spec_string ("request-message",
                         "Request message",
                         "Message from contact to request us to add them",
                         "",
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
                          "TRUE if the contact is currently typing, FALSE if not",
                          FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT);

  properties[PROP_SHOW_TYPING] =
    g_param_spec_boolean ("show-typing",
                          "Show typing",
                          "TRUE if contact should see us as currently typing, FALSE if not",
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

  properties[PROP_LAST_SEEN] =
    g_param_spec_string ("last-seen",
                         "Last seen",
                         "A string informing in a human friendly way when we "
                         "last saw the contact online",
                         "Last Seen: Never",
                         G_PARAM_READABLE);

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

  signals[NEW_TRANSFER] =
    g_signal_new ("new-transfer",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  NEULAND_TYPE_CONTACT);

};

static void
neuland_contact_init (NeulandContact *contact)
{
  NeulandContactPrivate *priv;

  g_debug ("neuland_contact_init");

  contact->priv = neuland_contact_get_instance_private (contact);
  priv = contact->priv;

  priv->file_transfers_receive = g_hash_table_new (NULL, NULL);
  priv->file_transfers_send = g_hash_table_new (NULL, NULL);
  priv->file_transfers_all = g_hash_table_new (NULL, NULL);
}

NeulandContact *
neuland_contact_new (const guint8 *tox_id, gint64 contact_number,
                     guint64 last_connected_change)
{
  NeulandContact *neuland_contact;

  return g_object_new (NEULAND_TYPE_CONTACT,
                       "tox-id", tox_id,
                       "number", contact_number,
                       "last-connected-change", last_connected_change,
                       NULL);
}
