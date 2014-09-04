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

#include <string.h>

#include "neuland-enums.h"

#include "neuland-tox.h"
#include "neuland-file-transfer.h"
#include "neuland-file-transfer-row.h"

#define NEULAND_DEFAULT_STATUS_MESSAGE "I'm testing Neuland!"
#define NEULAND_DEFAULT_NAME "Neuland User"
#define MAX_SEND_DATA_ATTEMPTS 100

struct _NeulandToxPrivate
{
  Tox *tox_struct;
  gchar *data_path;
  gchar *tox_id_hex;
  gchar *name;
  gchar *status_message;
  GHashTable *contacts_ht;  /* key: tox friend number -> value: contact*/
  GHashTable *requests_ht;  /* key: contact -> value: contact  */
  NeulandContactStatus status;
  gboolean running;
  gint64 pending_requests;
  gboolean is_sending_file_transfers;
  GHashTable *file_transfers_sending_ht;
  GHashTable *file_transfers_receiving_ht;
  GHashTable *file_transfers_all_ht;
  GMutex mutex;
};

G_DEFINE_TYPE_WITH_PRIVATE (NeulandTox, neuland_tox, G_TYPE_OBJECT)

typedef struct
{
  gchar *address;
  guint16 port;
  gchar *pub_key;
  gboolean is_ipv6;
} NeulandToxDhtNode;

typedef enum {
  SEND_TYPE_MESSAGE,
  SEND_TYPE_ACTION,
} NeulandToxSendType;

/* Possible errors returned by tox_add_friend  */
const gchar *
tox_faerr_to_string (gint32 error)
{
  switch (error)
    {
    case TOX_FAERR_TOOLONG: return "TOX_FAERR_TOOLONG";
    case TOX_FAERR_NOMESSAGE: return "TOX_FAERR_NOMESSAGE";
    case TOX_FAERR_OWNKEY: return "TOX_FAERR_OWNKEY";
    case TOX_FAERR_ALREADYSENT: return "TOX_FAERR_ALREADYSENT";
    case TOX_FAERR_UNKNOWN: return "TOX_FAERR_UNKNOWN";
    case TOX_FAERR_BADCHECKSUM: return "TOX_FAERR_BADCHECKSUM";
    case TOX_FAERR_SETNEWNOSPAM: return "TOX_FAERR_SETNEWNOSPAM";
    case TOX_FAERR_NOMEM: return "TOX_FAERR_NOMEM";
    default: return "[Unknown error number]";
    }
}

const gchar *
tox_filecontrol_type_to_string (gint32 control_type)
{
  switch (control_type)
    {
    case TOX_FILECONTROL_ACCEPT: return "TOX_FILECONTROL_ACCEPT";
    case TOX_FILECONTROL_PAUSE: return "TOX_FILECONTROL_PAUSE";
    case TOX_FILECONTROL_KILL: return "TOX_FILECONTROL_KILL";
    case TOX_FILECONTROL_FINISHED: return "TOX_FILECONTROL_FINISHED";
    case TOX_FILECONTROL_RESUME_BROKEN: return "TOX_FILECONTROL_RESUME_BROKEN";
    default: return "[Unknown error number]";
    }
}

static NeulandToxDhtNode bootstrap_nodes[] = {
  { "192.254.75.98"  , 33445, "951C88B7E75C867418ACDB5D273821372BB5BD652740BCDF623A4FA293E75D2F", FALSE },
  { "192.210.149.121", 33445, "F404ABAA1C99A9D37D61AB54898F56793E1DEF8BD46B1038B9D822E8460FAB67", FALSE },
};

enum {
  PROP_0,
  PROP_DATA_PATH,
  PROP_TOX_ID_HEX,
  PROP_NAME,
  PROP_STATUS,
  PROP_STATUS_MESSAGE,
  PROP_PENDING_REQUESTS,
  PROP_N
};

enum {
  CONTACT_ADD,
  REMOVE_CONTACTS,
  ACCEPT_REQUESTS,
  LAST_SIGNAL
};

static GParamSpec *properties[PROP_N] = {NULL, };
static guint signals[LAST_SIGNAL] = { 0 };

GList *
neuland_tox_get_contacts (NeulandTox *tox)
{
  g_return_if_fail (NEULAND_IS_TOX (tox));
  g_debug ("neuland_tox_get_contacts ...");
  return g_hash_table_get_values (tox->priv->contacts_ht);
}

NeulandContact *
neuland_tox_get_contact_by_number (NeulandTox *tox, gint64 number)
{
  GHashTable *ht = tox->priv->contacts_ht;
  return  (g_hash_table_lookup (ht, GINT_TO_POINTER (number)));
}

typedef struct {
  gint32 contact_number;
  guint8 *str;
  NeulandTox *tox;
} DataStr;

typedef struct {
  gint32 contact_number;
  guint integer;
  NeulandTox *tox;
} DataInt;

static void
add_idle_with_data_string (GSourceFunc idle_func,
                           gint32 contact_number,
                           const guint8 *str,
                           guint16 length,
                           NeulandTox *tox)
{
  DataStr *data = g_new0 (DataStr, 1);

  data->contact_number = contact_number;
  data->str = g_strndup (str, length);
  data->tox = tox;

  g_idle_add (idle_func, data);
}

static free_data_str (DataStr *data)
{
  g_free (data->str);
  g_free (data);
}

static void
add_idle_with_data_integer (GSourceFunc idle_func,
                            gint32 contact_number,
                            guint8 integer,
                            NeulandTox *tox)
{
  DataInt *data = g_new0 (DataInt, 1);

  data->contact_number = contact_number;
  data->integer = integer;
  data->tox = tox;

  g_idle_add (idle_func, data);
}

static free_data_integer (DataInt *data)
{
  g_free (data);
}

static gboolean
on_connection_status_idle (gpointer user_data)
{
  DataInt *data = user_data;
  NeulandTox *tox = data->tox;
  NeulandContact *contact = neuland_tox_get_contact_by_number (tox, data->contact_number);

  if (contact != NULL)
    {
      neuland_contact_set_connected (contact, data->integer);
    }

  free_data_integer (data);

  return G_SOURCE_REMOVE;
}

static void
on_connection_status (Tox *tox_struct,
                      gint32 contact_number,
                      guint8 status,
                      gpointer user_data)
{
  add_idle_with_data_integer (on_connection_status_idle, contact_number,
                              status, NEULAND_TOX (user_data));
}

static gboolean
on_user_status_idle (gpointer user_data)
{
  DataInt *data = user_data;
  NeulandContact *contact = neuland_tox_get_contact_by_number (data->tox, data->contact_number);

  if (contact != NULL)
    g_object_set (contact, "status", data->integer, NULL);

  g_free (data);

  return G_SOURCE_REMOVE;
}

static void
on_user_status (Tox *tox_struct,
                gint32 contact_number,
                guint8 status,
                void *user_data)
{
  add_idle_with_data_integer (on_user_status_idle, contact_number,
                              status, NEULAND_TOX (user_data));
}

static gboolean
on_name_change_idle (gpointer user_data)
{
  DataStr *data = user_data;
  NeulandContact *contact = neuland_tox_get_contact_by_number (data->tox, data->contact_number);
  g_debug ("contact %p changed name from %s to %s",
           contact, neuland_contact_get_name (contact), data->str);

  if (contact != NULL)
    g_object_set (contact, "name", data->str, NULL);

  free_data_str (data);

  return G_SOURCE_REMOVE;
}

static void
on_name_change (Tox *tox_struct,
                gint32 contact_number,
                const guint8 *new_name,
                guint16 length,
                gpointer user_data)
{

  add_idle_with_data_string (on_name_change_idle, contact_number,
                             new_name, length, NEULAND_TOX (user_data));
}

static gboolean
on_status_message_idle (gpointer user_data)
{
  DataStr *data = user_data;
  NeulandTox *tox = data->tox;
  NeulandContact *contact = neuland_tox_get_contact_by_number (tox, data->contact_number);

  if (contact != NULL)
    g_object_set (contact, "status-message", data->str, NULL);

  free_data_str (data);

  return G_SOURCE_REMOVE;
}

static void
on_status_message (Tox *tox_struct,
                   gint32 contact_number,
                   const guint8 *new_message,
                   guint16 length,
                   gpointer user_data)
{
  add_idle_with_data_string (on_status_message_idle, contact_number,
                             new_message, length, NEULAND_TOX (user_data));
}

static gboolean
on_contact_message_idle (gpointer user_data)
{
  DataStr *data = user_data;
  NeulandTox *tox = data->tox;
  NeulandContact *contact = neuland_tox_get_contact_by_number (tox, data->contact_number);

  neuland_contact_signal_incoming_message (contact, data->str);
  free_data_str (data);

  return G_SOURCE_REMOVE;
}

static void
on_contact_message (Tox *tox_struct,
                    gint32 contact_number,
                    const guint8 *message,
                    guint16 length,
                    gpointer user_data)
{
  add_idle_with_data_string (on_contact_message_idle, contact_number,
                             message, length, NEULAND_TOX (user_data));
}

static gboolean
on_contact_action_idle (gpointer user_data)
{
  DataStr *data = user_data;
  NeulandTox *tox = data->tox;
  NeulandContact *contact = neuland_tox_get_contact_by_number (tox, data->contact_number);

  neuland_contact_signal_incoming_action (contact, data->str);
  free_data_str (data);

  return G_SOURCE_REMOVE;
}

static void
on_contact_action (Tox *tox_struct,
                   gint32 contact_number,
                   const guint8 *action,
                   guint16 length,
                   gpointer user_data)
{
  add_idle_with_data_string (on_contact_action_idle, contact_number,
                             action, length, NEULAND_TOX (user_data));
}

static gboolean
on_typing_change_idle (gpointer user_data)
{
  DataInt *data = user_data;
  NeulandTox *tox = data->tox;
  NeulandContact *contact = neuland_tox_get_contact_by_number (tox, data->contact_number);

  if (contact != NULL)
    g_object_set (contact, "is-typing", data->integer, NULL);

  return G_SOURCE_REMOVE;
}

static void
on_typing_change (Tox *tox_struct,
                  gint32 contact_number,
                  guint8 is_typing,
                  gpointer user_data)
{
  add_idle_with_data_integer (on_typing_change_idle, contact_number,
                              is_typing, NEULAND_TOX (user_data));
}

typedef struct {
  guint8 *public_key;
  guint8 *message;
  NeulandTox *tox;
} DataFriendRequest;

static void
neuland_tox_send (NeulandTox *tox,
                  NeulandContact *contact,
                  gchar *text,
                  NeulandToxSendType type)
{
  g_return_if_fail (NEULAND_IS_TOX (tox));
  g_return_if_fail (NEULAND_IS_CONTACT (contact));

  NeulandToxPrivate *priv = tox->priv;
  Tox *tox_struct = priv->tox_struct;
  gint contact_number = neuland_contact_get_number (contact);
  gint64 total_bytes = strlen (text);
  gchar *preview = g_utf8_substring (text, 0, MIN (g_utf8_strlen (text, 40), 10));
  gint preview_bytes = strlen (preview);

  gchar *format_string;

  if (type == SEND_TYPE_MESSAGE)
    format_string = "neuland_tox_send message to contact %p: \"%s%s\"";
  else if (type == SEND_TYPE_ACTION)
    format_string = "neuland_tox_send action to contact %p: \"%s%s\"";
  else
    {
      g_warning ("Unknown NeulandToxSendType enum value: %i", type);
      g_return_if_reached ();
    }

  g_debug (format_string, contact, preview, preview_bytes < total_bytes ? "..." : "");

  gint64 sent_bytes = 0;
  while (sent_bytes < total_bytes)
    {
      gint64 remaining_bytes = total_bytes - sent_bytes;
      gint64 bytes;

      /* Start address of the first char we will be sending in *this* loop run */
      gchar *first_char = text + sent_bytes;

      if (remaining_bytes <= TOX_MAX_MESSAGE_LENGTH)
        bytes = remaining_bytes;
      else
        {
          /* Don't split UTF-8 chars!
           *
           * Start address of the first character we will be sending
           * in the *next* loop run. Notice: g_utf8_prev_char (x) < x,
           * hence the + 1, or we could waste one byte.
           */
          gchar *first_char_next = g_utf8_prev_char (first_char + TOX_MAX_MESSAGE_LENGTH + 1);

          bytes =  first_char_next - first_char;
        }

      g_debug ("neuland_tox_send: Sending %i of %i bytes (bytes %i to %i)",
               bytes, total_bytes, sent_bytes + 1, sent_bytes + bytes);

      g_mutex_lock (&priv->mutex);

      if (type == SEND_TYPE_MESSAGE)
        tox_send_message (tox->priv->tox_struct, contact_number,
                          first_char, bytes);
      else if (type == SEND_TYPE_ACTION)
        tox_send_action (tox->priv->tox_struct, contact_number,
                         first_char, bytes);

      g_mutex_unlock (&priv->mutex);

      sent_bytes = sent_bytes + bytes;
    }

  g_free (preview);
}

static void
on_outgoing_message_cb (NeulandContact *contact,
                        gchar *message,
                        gpointer user_data)
{
  NeulandTox *tox = NEULAND_TOX (user_data);
  neuland_tox_send (tox, contact, message, SEND_TYPE_MESSAGE);
}


static void
on_outgoing_action_cb (NeulandContact *contact,
                       gchar *action,
                       gpointer user_data)
{
  NeulandTox *tox = NEULAND_TOX (user_data);
  neuland_tox_send (tox, contact, action, SEND_TYPE_ACTION);
}

static void
on_show_typing_cb (GObject *obj,
                   GParamSpec *pspec,
                   gpointer user_data)
{
  NeulandContact *contact = NEULAND_CONTACT (obj);
  NeulandTox *tox = NEULAND_TOX (user_data);
  NeulandToxPrivate *priv = tox->priv;

  g_mutex_lock (&priv->mutex);

  tox_set_user_is_typing (priv->tox_struct,
                          neuland_contact_get_number (contact),
                          neuland_contact_get_show_typing (contact));

  g_mutex_unlock (&priv->mutex);
}

static gboolean
on_friend_request_idle (gpointer user_data)
{
  DataFriendRequest *data = user_data;
  NeulandTox *tox = data->tox;
  NeulandToxPrivate *priv = tox->priv;
  guint8 *public_key = data->public_key;
  gchar *message = data->message;

  gchar public_key_string[TOX_CLIENT_ID_SIZE * 2 + 1] = {0};
  neuland_bin_to_hex_string (public_key, public_key_string, TOX_CLIENT_ID_SIZE);

  g_message ("Received contact request from: %s", public_key_string);

  NeulandContact *contact = neuland_contact_new (public_key, -1, 0);
  neuland_contact_set_request_message (contact, data->message);


  g_object_connect (contact,
                    "signal::outgoing-message", on_outgoing_message_cb, tox,
                    "signal::outgoing-action", on_outgoing_action_cb, tox,
                    "signal::notify::show-typing", on_show_typing_cb, tox,
                    NULL);

  g_hash_table_insert (priv->requests_ht, contact, contact);
  g_signal_emit (tox, signals[CONTACT_ADD], 0, contact);
  g_object_notify_by_pspec (G_OBJECT (tox), properties[PROP_PENDING_REQUESTS]);

  g_free (data->public_key);
  g_free (data->message);
  g_free (data);

  return G_SOURCE_REMOVE;
}

static void
on_friend_request (Tox *tox_struct,
                   const guint8 *public_key,
                   const guint8 *message,
                   guint16 length,
                   gpointer user_data)
{
  DataFriendRequest *data = g_new0 (DataFriendRequest, 1);

  data->public_key = g_memdup (public_key, TOX_FRIEND_ADDRESS_SIZE);
  data->message = g_strndup (message, length);
  data->tox = NEULAND_TOX (user_data);

  g_idle_add (on_friend_request_idle, data);
}

typedef struct
{
  NeulandFileTransfer *file_transfer;
  NeulandFileTransferState state;
  guint64 transferred_size;
} DataUpdateFileTransferIdle;

/* This GSourceFunc is used to make the desired property changes for
   file_transfer inside the main loop. */
static gboolean
neuland_tox_update_file_transfer_idle (gpointer user_data)
{
  DataUpdateFileTransferIdle *data = (DataUpdateFileTransferIdle *)user_data;
  NeulandFileTransfer *file_transfer = NEULAND_FILE_TRANSFER (data->file_transfer);
  NeulandFileTransferState state = data->state;
  guint64 transferred_size = data->transferred_size;

  if (transferred_size)
    neuland_file_transfer_add_transferred_size (file_transfer, transferred_size);

  if (state) /*  The enum has: NEULAND_FILE_TRANSFER_STATE_NONE = 0 */
    {
      GEnumClass *eclass = g_type_class_peek (NEULAND_TYPE_FILE_TRANSFER_STATE);
      GEnumValue *eval = g_enum_get_value (eclass, state);

      g_debug ("neuland_tox_update_file_transfer_idle: changing state "
               "for transfer %p to %s", file_transfer, eval->value_nick);
      neuland_file_transfer_set_requested_state (file_transfer, state);
    }

  g_object_unref (file_transfer);
  g_free (user_data);
  return G_SOURCE_REMOVE;
}

typedef struct
{
  NeulandTox *tox;
  NeulandFileTransfer *file_transfer;
} DataSendFileTransfer;

gpointer
neuland_tox_send_file_transfer (gpointer user_data)
{
  /* TODO: Thread safety! */

  DataSendFileTransfer *data = (DataSendFileTransfer*)user_data;
  NeulandTox *tox = NEULAND_TOX (data->tox);
  NeulandToxPrivate *priv = tox->priv;
  NeulandFileTransfer *file_transfer = NEULAND_FILE_TRANSFER (data->file_transfer);

  gint64 contact_number = neuland_file_transfer_get_contact_number (file_transfer);
  guint8 file_number = neuland_file_transfer_get_file_number (file_transfer);
  const gchar *name = neuland_file_transfer_get_file_name (file_transfer);
  guint64 total_file_size = neuland_file_transfer_get_file_size (file_transfer);
  gboolean resuming = neuland_file_transfer_get_transferred_size (file_transfer) > 0;
  DataUpdateFileTransferIdle *idle_out_data = g_new0 (DataUpdateFileTransferIdle, 1);
  gsize count;

  g_debug ("Starting thread for file transfer %p \"%s\"", file_transfer, name);

  idle_out_data->file_transfer = g_object_ref (file_transfer);

  if (resuming)
    neuland_file_transfer_prepare_resume_sending (file_transfer);

  while (TRUE)
    {
      gsize data_size;
      gpointer data_buffer;

      g_mutex_lock (&priv->mutex);
      data_size = (gsize)tox_file_data_size (priv->tox_struct, contact_number);
      g_mutex_unlock (&priv->mutex);

      g_assert (data_size > 0);

      data_buffer = g_malloc0 (data_size);
      count = neuland_file_transfer_get_next_data (file_transfer, data_buffer, data_size);

      if (count == -1)
        {
          g_debug ("Failed to get next data for file transfer %p \"%s\", "
                   "going to kill transfer.", file_transfer, name);
          idle_out_data->state = NEULAND_FILE_TRANSFER_STATE_KILLED_BY_US;
          goto out;
        }

      if (count != 0)
        {
          gint fails = 0;
          while (TRUE)
            {
              gint ret;
              switch (neuland_file_transfer_get_state (file_transfer))
                {
                case NEULAND_FILE_TRANSFER_STATE_KILLED_BY_US:
                  g_debug ("File transfer %p \"%s\" has been killed by us, "
                           "going to leave sending thread", file_transfer, name);
                  goto out;
                  break;

                case NEULAND_FILE_TRANSFER_STATE_KILLED_BY_CONTACT:
                  g_debug ("File transfer %p \"%s\" has been killed by contact, "
                           "going to leave sending thread", file_transfer, name);
                  goto out;
                  break;

                case NEULAND_FILE_TRANSFER_STATE_PAUSED_BY_US:
                  g_debug ("File transfer %p \"%s\" has been paused by us, "
                           "going to leave sending thread", file_transfer, name);
                  goto out;
                  break;

                case NEULAND_FILE_TRANSFER_STATE_PAUSED_BY_CONTACT:
                  g_debug ("File transfer %p \"%s\" has been paused by contact, "
                           "going to leave sending thread", file_transfer, name);
                  goto out;
                  break;
                }

              g_mutex_lock (&priv->mutex);
              ret = tox_file_send_data (priv->tox_struct, contact_number,
                                        file_number, data_buffer, (gint)count);
              g_mutex_unlock (&priv->mutex);

              if (ret == 0)
                {
                  /* Set "transferred-size" property in the main loop. */
                  DataUpdateFileTransferIdle *data = g_new0 (DataUpdateFileTransferIdle, 1);
                  data->file_transfer = g_object_ref (file_transfer);
                  data->transferred_size = count;
                  g_idle_add (neuland_tox_update_file_transfer_idle, data);
                  break; /* for loop */
                }
              else
                {
                  if (++fails >= 500)
                    {
                      g_warning ("Going to kill transfer %p \"%s\" after "
                                 "%i failed tox_file_send_data() calls.",
                                 file_transfer, name, fails);
                      idle_out_data->state = NEULAND_FILE_TRANSFER_STATE_KILLED_BY_US;
                      goto out;
                    }

                  /* g_debug ("tox_file_send_data() failed, trying again later"); */
                  g_usleep (10000);
                }
            }
        }
      else /* count == 0 */
        {
          g_debug ("Thread for file transfer %p \"%s\" finished transferring data",
                   file_transfer, name);
          idle_out_data->state = NEULAND_FILE_TRANSFER_STATE_FINISHED;
          goto out;
        }
    }

 out:
  /* Apply the changes to @file_transfer in the main loop */
  g_idle_add (neuland_tox_update_file_transfer_idle, idle_out_data);

  g_object_unref (file_transfer);
  g_free (data);

  g_debug ("Leaving thread for file transfer %p \"%s\"", file_transfer, name);

  return;
}

/* This callback is responsible for sanity checking the state
   requested change and sending control packages with
   tox_file_send_control() according to the new state of the file
   transfer. If tox_file_send_control() fails, the state of the
   transfer not changed. */
static void
on_file_transfer_requested_state_changed_cb (GObject *gobject,
                                             GParamSpec *pspec,
                                             gpointer user_data)
{
  NeulandFileTransfer *file_transfer = NEULAND_FILE_TRANSFER (gobject);
  NeulandTox *tox = NEULAND_TOX (user_data);
  NeulandToxPrivate *priv = tox->priv;
  NeulandFileTransferState requested_state =
    neuland_file_transfer_get_requested_state (file_transfer);
  NeulandFileTransferDirection direction = neuland_file_transfer_get_direction (file_transfer);
  gint64 contact_number = neuland_file_transfer_get_contact_number (file_transfer);
  guint8 file_number = neuland_file_transfer_get_file_number (file_transfer);
  guint64 file_size = neuland_file_transfer_get_file_size (file_transfer);
  gboolean resuming = neuland_file_transfer_get_transferred_size (file_transfer) > 0;
  gchar *info = neuland_file_transfer_get_info_string (file_transfer);
  GEnumClass *eclass = g_type_class_peek (NEULAND_TYPE_FILE_TRANSFER_STATE);
  GEnumValue *ev;

  gint control_type = -100;     /* -100 for unset */
  gint send_receive = -1;       /*   -1 for unset */
  gboolean start_sending_thread = FALSE;

  g_debug ("on_file_transfer_requested_state_changed_cb: %s", info);
  g_free (info);

  /* Figure out if we need to send a control package */

  switch (direction)
    {
    case NEULAND_FILE_TRANSFER_DIRECTION_SEND:
      send_receive = 0;
      switch (requested_state)
        {
        case NEULAND_FILE_TRANSFER_STATE_IN_PROGRESS:
          /* When we resume we first have to send an accept package,
             before starting to send again. */
          if (resuming)
            control_type = TOX_FILECONTROL_ACCEPT;
          start_sending_thread = TRUE;
          break;

        case NEULAND_FILE_TRANSFER_STATE_PAUSED_BY_US:
          control_type = TOX_FILECONTROL_PAUSE;
          break;

        case NEULAND_FILE_TRANSFER_STATE_KILLED_BY_US:
          control_type = TOX_FILECONTROL_KILL;
          break;

        case NEULAND_FILE_TRANSFER_STATE_FINISHED:
          control_type = TOX_FILECONTROL_FINISHED;
          break;

        case NEULAND_FILE_TRANSFER_STATE_PAUSED_BY_CONTACT: /* fall through */
        case NEULAND_FILE_TRANSFER_STATE_KILLED_BY_CONTACT: /* fall through */
        case NEULAND_FILE_TRANSFER_STATE_FINISHED_CONFIRMED:
          /* empty */
          break;

        default:
          ev = g_enum_get_value (eclass, requested_state);
          g_warning ("Handling requested_state '%s' for outgoing transfers not yet "
                     "implemented in on_file_transfer_requested_state_changed_cb!",
                     ev->value_nick);
          break;
        }
      break;

    case NEULAND_FILE_TRANSFER_DIRECTION_RECEIVE:
      send_receive = 1;
      switch (requested_state)
        {
        case NEULAND_FILE_TRANSFER_STATE_IN_PROGRESS:
          /* Starting and resuming transfers paused by us are the same
             for receiving transfers, unlike for sending transfers
             above. */
          control_type = TOX_FILECONTROL_ACCEPT;
          break;

        case NEULAND_FILE_TRANSFER_STATE_PAUSED_BY_US:
          control_type = TOX_FILECONTROL_PAUSE;
          break;

        case NEULAND_FILE_TRANSFER_STATE_KILLED_BY_US:
          control_type = TOX_FILECONTROL_KILL;
          break;

        case NEULAND_FILE_TRANSFER_STATE_FINISHED_CONFIRMED:
          control_type = TOX_FILECONTROL_FINISHED;
          break;

        case NEULAND_FILE_TRANSFER_STATE_PAUSED_BY_CONTACT: /* fall through */
        case NEULAND_FILE_TRANSFER_STATE_KILLED_BY_CONTACT: /* fall through */
        case NEULAND_FILE_TRANSFER_STATE_FINISHED:
          /* empty */
          break;

        default:
          ev = g_enum_get_value (eclass, requested_state);
          g_warning ("Handling requested_state '%s' for incoming transfers not yet "
                     "implemented in on_file_transfer_requested_state_changed_cb!",
                     ev->value_nick);
        }
      break;
    }

  /* Send a control package if necessary */
  if (control_type != -100)
    {
      gint ret;
      g_debug ("\n"
               "< Outgoing file control package for transfer %p >\n"
               "  contact_number: %i\n"
               "  file_number   : %i\n"
               "  send_receive  : %i\n"
               "  control_type  : %s\n",
               file_transfer,
               contact_number,
               send_receive,
               file_number,
               tox_filecontrol_type_to_string(control_type));

      g_mutex_lock (&priv->mutex);
      ret = tox_file_send_control (priv->tox_struct,
                                   contact_number,
                                   send_receive,
                                   file_number,
                                   control_type,
                                   NULL, 0);
      g_mutex_unlock (&priv->mutex);

      if (ret != 0)
        {
          g_warning ("Error on calling tox_file_send_control() with %s "
                     "for transfer %p, not changing state",
                     tox_filecontrol_type_to_string (control_type), file_transfer);
          //neuland_file_transfer_set_state (file_transfer, NEULAND_FILE_TRANSFER_STATE_BROKEN);
        }
      else
        /* Change the real state property if sending control package succeeded */
        neuland_file_transfer_set_state (file_transfer, requested_state);
    }
  else
    /* Change the real state */
    neuland_file_transfer_set_state (file_transfer, requested_state);

  /* Start sending thread if necessary */
  if (start_sending_thread)
    {
      DataSendFileTransfer *data = g_new (DataSendFileTransfer, 1);
      data->tox = tox;
      data->file_transfer = g_object_ref (file_transfer);
      g_thread_new ("file-transfer", neuland_tox_send_file_transfer, data);
    }
}

/* Connects the @file_transfer signals to this tox session and adds
   the transfer to the contact it belongs to. */
void
neuland_tox_add_file_transfer (NeulandTox *tox,
                               NeulandFileTransfer *file_transfer)
{
  g_return_if_fail (NEULAND_IS_TOX (tox));
  g_return_if_fail (NEULAND_IS_FILE_TRANSFER (file_transfer));
  NeulandToxPrivate *priv = tox->priv;

  NeulandFileTransferDirection direction = neuland_file_transfer_get_direction (file_transfer);
  gint64 contact_number = neuland_file_transfer_get_contact_number (file_transfer);
  guint64 file_size = neuland_file_transfer_get_file_size (file_transfer);
  gchar *file_name = g_strdup (neuland_file_transfer_get_file_name (file_transfer));

  NeulandContact *contact = neuland_tox_get_contact_by_number (tox, contact_number);

  g_signal_connect (file_transfer, "notify::requested-state",
                    G_CALLBACK (on_file_transfer_requested_state_changed_cb), tox);

  if (direction == NEULAND_FILE_TRANSFER_DIRECTION_SEND)
    {
      g_return_if_fail (neuland_file_transfer_get_file_number (file_transfer) == -1);
      g_debug ("Adding new file transfer - sending: (%p)", file_transfer);

      /* Before we can add the transfer to the contacts file_transfers_sending_ht hash
         table we need to know the file number. */
      g_mutex_lock (&priv->mutex);

      gint file_number = tox_new_file_sender (priv->tox_struct,
                                              contact_number,
                                              file_size,
                                              file_name,
                                              strlen (file_name));

      g_mutex_unlock (&priv->mutex);

      if (file_number > -1)
        {
          neuland_file_transfer_set_file_number (file_transfer, file_number);

          g_hash_table_insert (priv->file_transfers_sending_ht, file_transfer, file_transfer);
          g_hash_table_insert (priv->file_transfers_all_ht, file_transfer, file_transfer);

          neuland_contact_add_file_transfer (contact, file_transfer);
          /* Now we have to wait for TOX_FILECONTROL_ACCEPT ... */
        }
      else
        g_warning ("Error when calling_new_file_sender() for transfer %s", file_name);
    }
  else if (direction == NEULAND_FILE_TRANSFER_DIRECTION_RECEIVE)
    {
      g_debug ("Adding new file transfer - receiving: (%p)", file_transfer);
      /* We got the file number with the send request. */
      g_return_if_fail (neuland_file_transfer_get_file_number (file_transfer) != -1);

      g_hash_table_insert (priv->file_transfers_receiving_ht, file_transfer, file_transfer);
      g_hash_table_insert (priv->file_transfers_all_ht, file_transfer, file_transfer);

      neuland_contact_add_file_transfer (contact, file_transfer);
    }

  g_free (file_name);
}

typedef struct {
  gint32 contact_number;
  guint8 file_number;
  guint64 file_size;
  gchar *file_name;
  NeulandTox *tox;
} DataFileSendRequest;

static gboolean
on_file_send_request_idle (gpointer user_data)
{
  DataFileSendRequest *data = user_data;

  g_message ("Got file send request:\n"
             "      contact number: %i\n"
             "      file_number: %i\n"
             "      file_name: %s\n"
             "      file size: %i",
             data->contact_number,
             data->file_number,
             data->file_name,
             data->file_size);

  NeulandContact *contact =
    neuland_tox_get_contact_by_number (data->tox, data->contact_number);

  if (contact == NULL)
    return;

  NeulandFileTransfer *file_transfer =
    neuland_file_transfer_new_receiving (data->contact_number,
                                         data->file_number,
                                         data->file_name,
                                         data->file_size);

  g_warn_if_fail (NEULAND_IS_TOX (data->tox));
  g_warn_if_fail (NEULAND_IS_FILE_TRANSFER (file_transfer));

  neuland_tox_add_file_transfer (data->tox, file_transfer);

  g_free (data->file_name);
  g_free (data);

  return G_SOURCE_REMOVE;
}

static void
on_file_send_request (Tox *tox_struct,
                      gint32 contact_number,
                      guint8 file_number,
                      guint64 file_size,
                      const guint8 *file_name,
                      guint16 file_name_length,
                      gpointer user_data)
{
  DataFileSendRequest *data = g_new0 (DataFileSendRequest, 1);

  data->contact_number = contact_number;
  data->file_number = file_number;
  data->file_size = file_size;
  data->file_name = g_strndup (file_name, file_name_length);
  data->tox = NEULAND_TOX (user_data);

  g_idle_add (on_file_send_request_idle, data);
}

static NeulandFileTransfer *
neuland_tox_get_file_transfer (NeulandTox *tox,
                               gint32 contact_number,
                               NeulandFileTransferDirection direction,
                               guint8 file_number)
{
  g_return_val_if_fail (NEULAND_IS_TOX (tox), NULL);
  NeulandContact *nc = neuland_tox_get_contact_by_number (tox, contact_number);

  return neuland_contact_get_file_transfer (nc, direction, file_number);
}

typedef struct
{
  NeulandTox *tox;
  gint32 contact_number;
  guint8 file_number;
  GByteArray *data_array;
} DataFileData;

static gboolean
on_file_data_idle (gpointer user_data)
{
  /* g_debug ("on_file_data_idle"); */
  DataFileData *data = user_data;
  NeulandTox *tox = data->tox;
  GByteArray *data_array = data->data_array;
  NeulandFileTransfer *file_transfer =
    neuland_tox_get_file_transfer (tox, data->contact_number,
                                   NEULAND_FILE_TRANSFER_DIRECTION_RECEIVE,
                                   data->file_number);
  NeulandFileTransferState state = neuland_file_transfer_get_state (file_transfer);

  if (file_transfer != NULL)
    {
      if (state == NEULAND_FILE_TRANSFER_STATE_KILLED_BY_CONTACT ||
          state == NEULAND_FILE_TRANSFER_STATE_KILLED_BY_US)
        /* We do NOT ignore packages for PAUSED transfers here,
           because this is an idle function and some data can still
           arrive here after the pause control package has
           arrived/been sent. */
        {
          GEnumClass *eclass = g_type_class_peek (NEULAND_TYPE_FILE_TRANSFER_STATE);
          GEnumValue *ev = g_enum_get_value (eclass, state);

          g_debug ("Ignoring incoming data package for file transfer %p with state \"%s\"",
                   file_transfer, ev->value_nick);
        }
      else
        {
          gssize count =  neuland_file_transfer_append_data (file_transfer, data_array);
          if (count == -1)
            {
              g_warning ("Failed to append incoming data to file "
                         "transfer %p, going to kill transfer");
              neuland_file_transfer_set_requested_state (file_transfer,
                                                         NEULAND_FILE_TRANSFER_STATE_KILLED_BY_US);
            }
          else
            neuland_file_transfer_add_transferred_size (file_transfer, count);
        }
    }
  else
    g_debug ("Got data package for non-existent file transfer");

  g_byte_array_free (data->data_array, TRUE);
  g_free (data);

  return G_SOURCE_REMOVE;
}

static void
on_file_data (Tox *tox_struct,
              gint32 contact_number,
              guint8 file_number,
              const guint8 *file_data,
              guint16 file_data_length,
              gpointer user_data)
{
  /* g_debug ("on_file_data"); */

  NeulandTox *tox = NEULAND_TOX (user_data);

  DataFileData *data = g_new0 (DataFileData, 1);
  data->tox = tox;
  data->contact_number = contact_number;
  data->file_number = file_number;
  data->data_array = g_byte_array_new_take (g_memdup (file_data, file_data_length),
                                            file_data_length);

  g_idle_add (on_file_data_idle, data);
}

typedef struct
{
  NeulandTox *tox;
  gint32 contact_number;
  guint8 receive_send;
  guint8 file_number;
  guint8 control_type;
  GByteArray *data_array;
} DataFileControl;

static gboolean
on_file_control_idle (gpointer user_data)
{
  g_debug ("on_file_control_idle");
  DataFileControl *data_struct = (DataFileControl *)user_data;
  NeulandTox *tox = data_struct->tox;
  NeulandFileTransferDirection direction = data_struct->receive_send == 0 ?
    NEULAND_FILE_TRANSFER_DIRECTION_RECEIVE :
    NEULAND_FILE_TRANSFER_DIRECTION_SEND;

  NeulandFileTransfer *file_transfer =
    neuland_tox_get_file_transfer (tox, data_struct->contact_number,
                                   direction,
                                   data_struct->file_number);
  if (file_transfer != NULL)
    {
      switch (direction)
        {
        case NEULAND_FILE_TRANSFER_DIRECTION_SEND:
          switch (data_struct->control_type)
            {
            case TOX_FILECONTROL_ACCEPT:
              /* PENDING -> IN_PROGRESS */
              neuland_file_transfer_set_requested_state
                (file_transfer, NEULAND_FILE_TRANSFER_STATE_IN_PROGRESS);
              break;

            case TOX_FILECONTROL_PAUSE:
              neuland_file_transfer_set_requested_state
                (file_transfer, NEULAND_FILE_TRANSFER_STATE_PAUSED_BY_CONTACT);
              break;

            case TOX_FILECONTROL_KILL:
              neuland_file_transfer_set_requested_state
                (file_transfer, NEULAND_FILE_TRANSFER_STATE_KILLED_BY_CONTACT);
              break;

            case TOX_FILECONTROL_FINISHED:
              /* FINISHED -> FINISHED_CONFIRMED */
              neuland_file_transfer_set_requested_state
                (file_transfer, NEULAND_FILE_TRANSFER_STATE_FINISHED_CONFIRMED);
              break;

            default:
              g_warning ("in on_file_control_idle(): Handling control_type %s for "
                         "outgoing transfers not implemented yet",
                         tox_filecontrol_type_to_string (data_struct->control_type));
              break;
            }
          break; /* NEULAND_FILE_TRANSFER_DIRECTION_SEND */

        case NEULAND_FILE_TRANSFER_DIRECTION_RECEIVE:
          switch (data_struct->control_type)
            {
            case TOX_FILECONTROL_FINISHED:
              /* IN_PROGRESS -> FINISHED */
              neuland_file_transfer_set_requested_state
                (file_transfer, NEULAND_FILE_TRANSFER_STATE_FINISHED);
              /* FINISHED -> FINISHED_CONFIRMED */
              neuland_file_transfer_set_requested_state
                (file_transfer, NEULAND_FILE_TRANSFER_STATE_FINISHED_CONFIRMED);
              break;

            case TOX_FILECONTROL_KILL:
              neuland_file_transfer_set_requested_state
                (file_transfer, NEULAND_FILE_TRANSFER_STATE_KILLED_BY_CONTACT);
              break;

            case TOX_FILECONTROL_PAUSE:
              neuland_file_transfer_set_requested_state
                (file_transfer, NEULAND_FILE_TRANSFER_STATE_PAUSED_BY_CONTACT);
              break;

            case TOX_FILECONTROL_ACCEPT:
              neuland_file_transfer_set_requested_state
                (file_transfer, NEULAND_FILE_TRANSFER_STATE_IN_PROGRESS);
              break;

            default:
              g_warning ("in on_file_control_idle(): Handling control_type %s for "
                         "incoming transfers not implemented yet",
                         tox_filecontrol_type_to_string (data_struct->control_type));
              break;

            }
          break; /* NEULAND_FILE_TRANSFER_DIRECTION_RECEIVE */

        default:
          g_warn_if_reached ();
        }

    }
  else
    g_warning ("Failed to retrieve file transfer for control package");

  g_byte_array_free (data_struct->data_array, TRUE);
  g_free (data_struct);

  return G_SOURCE_REMOVE;
}

static void
on_file_control (Tox *tox_struct,
                 gint32 contact_number,
                 guint8 receive_send,
                 guint8 file_number,
                 guint8 control_type,
                 const guint8 *data,
                 guint16 length,
                 gpointer user_data)
{
  g_debug ("\n"
           "< Incoming file control package >\n"
           "  contact_number: %i\n"
           "  file_number   : %i\n"
           "  receive_send  : %i\n"
           "  control_type  : %s\n",
           contact_number,
           file_number,
           receive_send,
           tox_filecontrol_type_to_string(control_type));

  DataFileControl *data_struct = g_new0 (DataFileControl, 1);
  NeulandTox *tox = NEULAND_TOX (user_data);
  data_struct->tox = tox;
  data_struct->receive_send = receive_send;
  data_struct->contact_number = contact_number;
  data_struct->file_number = file_number;
  data_struct->control_type = control_type;
  data_struct->data_array = g_byte_array_new_take (g_memdup (data, length), length);

  g_idle_add (on_file_control_idle, data_struct);
}

static void
neuland_tox_connect_callbacks (NeulandTox *tox)
{
  NeulandToxPrivate *priv = tox->priv;
  Tox *tox_struct = priv->tox_struct;

  g_mutex_lock (&priv->mutex);

  tox_callback_connection_status (tox_struct, on_connection_status, tox);
  tox_callback_user_status (tox_struct, on_user_status, tox);
  tox_callback_name_change (tox_struct, on_name_change, tox);
  tox_callback_status_message (tox_struct, on_status_message, tox);
  tox_callback_friend_message (tox_struct, on_contact_message, tox);
  tox_callback_friend_action (tox_struct, on_contact_action, tox);
  tox_callback_typing_change (tox_struct, on_typing_change, tox);
  tox_callback_friend_request (tox_struct, on_friend_request, tox);

  tox_callback_file_send_request (tox_struct, on_file_send_request, tox);
  tox_callback_file_data (tox_struct, on_file_data, tox);
  tox_callback_file_control (tox_struct, on_file_control, tox);

  g_mutex_unlock (&priv->mutex);

  /* TODO: */
  /* tox_callback_group_invite (tox_struct, NULL, tox); */
  /* tox_callback_group_message (tox_struct, NULL, tox); */
  /* tox_callback_group_action (tox_struct, NULL, tox); */
  /* tox_callback_group_namelist_change (tox_struct, NULL, tox); */
}

static void
neuland_tox_set_data_path (NeulandTox *tox, gchar *data_path)
{
  NeulandToxPrivate *priv = tox->priv;
  Tox *tox_struct = priv->tox_struct;
  gchar *data;
  gsize length;
  GError *error = NULL;
  gboolean data_path_exists = TRUE;

  g_message ("Setting data path for tox instance %p to \"%s\".", tox, data_path);

  if (data_path != NULL)
    {
      if (g_file_get_contents (data_path, &data, &length, &error))
        {
          g_mutex_lock (&priv->mutex);
          gint ret = tox_load (priv->tox_struct, data, length);
          g_mutex_unlock (&priv->mutex);

          if (ret == 0)
            priv->data_path = data_path;
          else
            {
              g_warning ("tox_load () for data path \"%s\" failed with return value %i. Setting data path to NULL.",
                         data_path, ret);
              priv->data_path = NULL;
            }
        }
      else
        {
          if (error->code == G_FILE_ERROR_NOENT) // No such file or directory
            {
              /* Tox will start up without a file, creating a new identity.
                 when closing we will try to save the tox data to this
                 location. */
              g_message ("File \"%s\" not found. Will try to save new data to it on exit.", data_path);
              priv->data_path = data_path;
              data_path_exists = FALSE;
            }
          else
            {
              g_warning ("Failed to get file content from file \"%s\", "
                         "reason: %s, setting data path to NULL", data_path, error->message);
              priv->data_path = NULL;
            }
        }
    }


  guint8 address[TOX_FRIEND_ADDRESS_SIZE] = {0,};
  guint8 name[TOX_MAX_NAME_LENGTH] = {0, };
  guint8 status_message[TOX_MAX_STATUSMESSAGE_LENGTH] = {0,};
  guchar hex_string[TOX_FRIEND_ADDRESS_SIZE * 2 + 1] = {0,};

  g_mutex_lock (&priv->mutex);

  tox_get_address (priv->tox_struct, address);

  g_mutex_unlock (&priv->mutex);

  neuland_bin_to_hex_string (address, hex_string, TOX_FRIEND_ADDRESS_SIZE);
  priv->tox_id_hex = g_strndup (hex_string, TOX_FRIEND_ADDRESS_SIZE * 2);

  if (priv->data_path == NULL || !data_path_exists)
    {
      g_message ("Got NULL data path or path to a not existing data file, going to use "
                 "the default name and status message",
                 NEULAND_DEFAULT_STATUS_MESSAGE);
      neuland_tox_set_name (tox, NEULAND_DEFAULT_NAME);
      neuland_tox_set_status_message (tox, NEULAND_DEFAULT_STATUS_MESSAGE);
    }
  else
    {
      guint16 l;

      g_mutex_lock (&priv->mutex);

      l = tox_get_self_name (tox_struct, name);
      priv->name = g_strndup (name, l);

      l = tox_get_self_status_message (tox_struct, status_message, TOX_MAX_STATUSMESSAGE_LENGTH);
      priv->status_message = g_strndup (status_message, l);

      g_mutex_unlock (&priv->mutex);

      g_debug ("Setting our name from tox data file: \"%s\"", priv->name);
      g_debug ("Setting our status message from tox data file: \"%s\"", priv->status_message);
    }

}

static void
neuland_tox_load_contacts (NeulandTox *tox)
{
  NeulandToxPrivate *priv = tox->priv;
  Tox *tox_struct = priv->tox_struct;
  GHashTable *contacts_ht = tox->priv->contacts_ht;

  g_mutex_lock (&priv->mutex);

  guint32 n_contacts = tox_count_friendlist (tox_struct);
  guint32 contact_list[n_contacts];
  tox_get_friendlist (tox_struct, contact_list, n_contacts);

  g_debug ("Loading contacts ...");

  int i;
  for (i = 0; i < n_contacts; i++)
    {
      gint contact_number = contact_list[i];

      /* Skip contacts that we already have NeulandContact objects for */
      if (g_hash_table_contains (contacts_ht, GINT_TO_POINTER (contact_number)))
        {
          g_debug ("  skipping contact with number %i; already added", contact_number);
          continue;
        }

      /* Create NeulandContacts and add them to this NeulandTox instance. */
      g_debug ("  adding contact with number %i", contact_number);

      guint8 client_id[TOX_CLIENT_ID_SIZE] = {0};
      tox_get_client_id (tox_struct, contact_number, client_id);

      NeulandContact *contact =  neuland_contact_new (client_id, contact_number,
                                                      tox_get_last_online (tox_struct, contact_number));
      gchar tox_name[TOX_MAX_NAME_LENGTH];
      gchar status_message[TOX_MAX_STATUSMESSAGE_LENGTH];

      gint l;
      l = tox_get_name (tox_struct, contact_number, tox_name);
      gchar *new_tox_name = g_strndup (tox_name, l);
      l = tox_get_status_message (tox_struct, contact_number,
                                  status_message, TOX_MAX_STATUSMESSAGE_LENGTH);
      gchar *new_status_message = g_strndup (status_message, l);

      g_object_set (contact,
                    "name", new_tox_name,
                    "status-message", new_status_message,
                    NULL);

      g_object_connect (contact,
                        "signal::outgoing-message", on_outgoing_message_cb, tox,
                        "signal::outgoing-action", on_outgoing_action_cb, tox,
                        "signal::notify::show-typing", on_show_typing_cb, tox,
                        NULL);
      g_hash_table_insert (contacts_ht, GINT_TO_POINTER (contact_number), contact);

      g_free (new_tox_name);
      g_free (new_status_message);
    }

  g_mutex_unlock (&priv->mutex);
}

void
neuland_tox_add_contact_from_hex_address (NeulandTox *tox,
                                          const gchar *hex_address,
                                          const gchar *message)
{
  g_debug ("neuland_tox_add_contact_from_hex_address %s", hex_address);
  /* Tox wants at least one byte for the message, so in case the
     message entry is empty we send one space. */
  gchar *tmp_message = g_strdup (g_strcmp0 (message, "") == 0 ? " " : message);
  NeulandToxPrivate *priv = tox->priv;
  guint8 bin_address[TOX_FRIEND_ADDRESS_SIZE] = {0,};
  gboolean valid_hex = neuland_hex_string_to_bin (hex_address,
                                                  bin_address,
                                                  TOX_FRIEND_ADDRESS_SIZE);
  g_mutex_lock (&priv->mutex);

  gint32 friend_number = tox_add_friend (priv->tox_struct, bin_address,
                                         tmp_message, strlen (tmp_message));

  g_mutex_unlock (&priv->mutex);

  g_free (tmp_message);

  if (friend_number < 0)
    {
      g_warning ("Failed to add friend from hex address \"%s\". Tox error number: %i (%s)",
                 hex_address, friend_number, tox_faerr_to_string (friend_number));
      return;
    }

  neuland_tox_load_contacts (tox);

  NeulandContact *contact =
    g_hash_table_lookup (priv->contacts_ht, GINT_TO_POINTER (friend_number));
  g_signal_emit (tox, signals[CONTACT_ADD], 0, contact);
}

void
remove_contacts (NeulandTox *tox,
                 GList *removed_contacts,
                 gpointer user_data)
{
  NeulandToxPrivate *priv = tox->priv;

  GList *l;
  for (l = removed_contacts; l; l = l->next)
    {
      NeulandContact *contact = l->data;
      gint64 number = neuland_contact_get_number (contact);

      if (number < 0)
        {
          g_debug ("Removing contact %p from requests hash table");
          g_hash_table_remove (priv->requests_ht, contact);
        }
      else
        {
          g_debug ("Removing contact %p from contacts hash table");
          g_hash_table_remove (priv->contacts_ht,
                               GINT_TO_POINTER (neuland_contact_get_number (contact)));
        }
    }
}

void
neuland_tox_remove_contacts (NeulandTox *tox, GList *contacts)
{
  g_return_if_fail (NEULAND_IS_TOX (tox));
  g_return_if_fail (contacts != NULL);
  g_return_if_fail (NEULAND_IS_CONTACT (contacts->data));

  g_debug ("neuland_tox_remove_contact");

  NeulandToxPrivate *priv = tox->priv;

  GList *removed_contacts = NULL;
  gboolean pending_requests_changed = FALSE;
  GList *l;
  for (l = contacts; l; l = l->next)
    {
      NeulandContact *contact  = l->data;

      if (neuland_contact_is_request (contact))
        {
          removed_contacts = g_list_prepend (removed_contacts, contact);
          pending_requests_changed = TRUE;
        }
      else
        {
          g_mutex_lock (&priv->mutex);
          gint ret = tox_del_friend (priv->tox_struct, neuland_contact_get_number (contact));
          g_mutex_unlock (&priv->mutex);

          if (ret == -1)
            g_warning ("Calling tox_del_friend failed for contact %p", contact);
          else
            removed_contacts = g_list_prepend (removed_contacts, contact);
        }
    }

  if (removed_contacts)
    g_signal_emit (tox, signals[REMOVE_CONTACTS], 0, removed_contacts);

  if (pending_requests_changed)
    g_object_notify_by_pspec (G_OBJECT (tox), properties[PROP_PENDING_REQUESTS]);

  g_list_free (removed_contacts);
}


/* Add each contact in the @contacts list. Notice that we update each
   contact in place from a 'request' contact with a friend number of
   -1 to a normal contact with the new friend number given by
   tox_add_friend_norequest. */
void
neuland_tox_accept_contact_requests (NeulandTox *tox,
                                     GList *contacts)
{
  g_return_if_fail (NEULAND_IS_TOX (tox));
  NeulandToxPrivate *priv = tox->priv;

  GList *accepted_contacts = NULL;
  GList *l;
  for (l = contacts; l; l = l->next)
    {
      NeulandContact *contact = l->data;
      g_return_if_fail (neuland_contact_is_request (contact));

      g_mutex_lock (&priv->mutex);
      gint32 number = tox_add_friend_norequest (priv->tox_struct,
                                                neuland_contact_get_tox_id (contact));
      g_mutex_unlock (&priv->mutex);

      if (number < 0)
        g_warning ("Failed to add contact request from Tox ID %s",
                   neuland_contact_get_tox_id (contact));
      else
        {
          /* contact has a tox friend number now, so override the -1 with that new number. */
          neuland_contact_set_number (contact, number);

          /* Removing @contact from @requests_ht results in a
             g_object_unref() call on contact, so we have to call
             g_object_ref() beforehand, or @contact would be destroyed. */
          g_object_ref (contact);
          g_hash_table_remove (priv->requests_ht, contact);
          g_hash_table_insert (priv->contacts_ht, GINT_TO_POINTER (number), contact);

          g_message ("Added contact \"%s\" (%p) from request as number %i",
                     neuland_contact_get_preferred_name (contact), contact, number);
          accepted_contacts = g_list_prepend (accepted_contacts, contact);
        }
    }

  if (accepted_contacts)
    {
      g_signal_emit (tox, signals[ACCEPT_REQUESTS], 0, accepted_contacts);
      g_object_notify_by_pspec (G_OBJECT (tox), properties[PROP_PENDING_REQUESTS]);
    }

  g_list_free (accepted_contacts);
}

void
neuland_tox_save_and_kill (NeulandTox *tox)
{
  NeulandToxPrivate *priv = tox->priv;
  if (priv->data_path)
    {
      g_mutex_lock (&priv->mutex);

      gsize size = tox_size (priv->tox_struct);
      gchar *contents = g_malloc0 (size);

      tox_save (priv->tox_struct, contents);

      g_mutex_unlock (&priv->mutex);

      g_message ("Saving tox data (size: %i bytes) to '%s' ...", size, priv->data_path);
      GError *e = NULL;
      if (!g_file_set_contents (priv->data_path, contents, size, &e))
        g_warning ("Could not save tox data file, error was: %s", e->message);
      g_free (contents);
    }
  else
    g_message ("No data path given on startup; closing without saving any data");

  g_debug ("Killing tox ...");

  priv->running = FALSE;
  g_mutex_lock (&priv->mutex);
  tox_kill (priv->tox_struct);
  g_mutex_unlock (&priv->mutex);
}

void
neuland_tox_set_name (NeulandTox *tox,
                      const gchar *name)
{
  NeulandToxPrivate *priv = tox->priv;

  g_return_if_fail (name != NULL);

  if (g_strcmp0 (name, priv->name) == 0)
    return;

  g_mutex_lock (&priv->mutex);

  gint ret = tox_set_name (priv->tox_struct, name,
                           MIN (strlen (name), TOX_MAX_NAME_LENGTH));

  g_mutex_unlock (&priv->mutex);

  if (ret == 0)
    {
      g_debug ("Set name for NeulandTox %p to \"%s\"", tox, name);
      g_free (priv->name);
      priv->name = g_strdup (name);
    }
  else
    g_warning ("Failed to set name for NeulandTox %p to name: \"%s\"",
               tox, name);

  /* We notify even if setting failed, so that widgets used to set the
     status message don't keep the message that failed to be set. */
  g_object_notify_by_pspec (G_OBJECT (tox), properties[PROP_NAME]);
}

const gchar *
neuland_tox_get_name (NeulandTox *tox)
{
  return tox->priv->name;
}

void
neuland_tox_set_status (NeulandTox *tox, NeulandContactStatus status)
{
  NeulandToxPrivate *priv = tox->priv;
  Tox *tox_struct = priv->tox_struct;

  GEnumClass *eclass = g_type_class_peek (NEULAND_TYPE_CONTACT_STATUS);
  GEnumValue *eval = g_enum_get_value (eclass, status);
  g_debug ("Setting status for tox instance %p to: %s", tox, eval->value_name);

  g_mutex_lock (&priv->mutex);

  tox_set_user_status (tox_struct, (guint8)status);

  g_mutex_unlock (&priv->mutex);

  priv->status = status;
  g_object_notify_by_pspec (G_OBJECT (tox), properties[PROP_STATUS]);
}

NeulandContactStatus
neuland_tox_get_status (NeulandTox *tox)
{
  return tox->priv->status;
}

void
neuland_tox_set_status_message (NeulandTox *tox,
                                const gchar *status_message)
{
  NeulandToxPrivate *priv = tox->priv;
  Tox *tox_struct = priv->tox_struct;

  g_return_if_fail (status_message != NULL);

  if (g_strcmp0 (status_message, priv->status_message) == 0)
    return;

  g_mutex_lock (&priv->mutex);

  gint ret = tox_set_status_message (tox_struct, status_message,
                                     MIN (strlen (status_message),
                                          TOX_MAX_STATUSMESSAGE_LENGTH));

  g_mutex_unlock (&priv->mutex);

  if (ret == 0)
    {
      g_debug ("Set status message for NeulandTox %p to \"%s\"",
               tox, status_message);
      g_free (priv->status_message);
      priv->status_message = g_strdup (status_message);
    }
  else
    {
      // keep old message on failure
      g_warning ("Failed to set status message for NeulandTox %p to \"%s\"",
                 tox, status_message);
    }

  /* We notify even if setting failed, so that widgets used to set the
     status message don't keep the message that failed to be set. */
  g_object_notify_by_pspec (G_OBJECT (tox), properties[PROP_STATUS_MESSAGE]);
}

const gchar *
neuland_tox_get_status_message (NeulandTox *tox)
{
  return tox->priv->status_message;
}

gint64
neuland_tox_get_pending_requests (NeulandTox *tox)
{
  return g_hash_table_size (tox->priv->requests_ht);
}

const char *
neuland_tox_get_tox_id_hex (NeulandTox *tox)
{
  return tox->priv->tox_id_hex;
}

static void
neuland_tox_set_property (GObject *object,
                          guint property_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
  NeulandTox *nt = NEULAND_TOX (object);
  NeulandToxPrivate *priv = nt->priv;

  switch (property_id)
    {
    case PROP_DATA_PATH:
      neuland_tox_set_data_path (nt, g_value_dup_string (value));
      break;
    case PROP_NAME:
      neuland_tox_set_name (nt, g_value_get_string (value));
      break;
    case PROP_STATUS:
      neuland_tox_set_status (nt, g_value_get_enum (value));
      break;
    case PROP_STATUS_MESSAGE:
      neuland_tox_set_status_message (nt, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
neuland_tox_get_property (GObject *object,
                          guint property_id,
                          GValue *value,
                          GParamSpec *pspec)
{
  NeulandTox *nt = NEULAND_TOX (object);
  NeulandToxPrivate *priv = nt->priv;

  switch (property_id)
    {
    case PROP_DATA_PATH:
      g_value_set_string (value, priv->data_path);
      break;
    case PROP_TOX_ID_HEX:
      g_value_set_string (value, neuland_tox_get_tox_id_hex (nt));
      break;
    case PROP_NAME:
      g_value_set_string (value, neuland_tox_get_name (nt));
      break;
    case PROP_STATUS_MESSAGE:
      g_value_set_string (value, priv->status_message);
      break;
    case PROP_PENDING_REQUESTS:
      g_value_set_int64 (value, neuland_tox_get_pending_requests (nt));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
neuland_tox_dispose (GObject *object)
{
  g_debug ("neuland_tox_dispose %p", object);
  NeulandTox *nt = NEULAND_TOX (object);
  NeulandToxPrivate *priv = nt->priv;

  g_hash_table_destroy (priv->contacts_ht);
  g_hash_table_destroy (priv->requests_ht);
  g_hash_table_destroy (priv->file_transfers_all_ht);

  G_OBJECT_CLASS (neuland_tox_parent_class)->dispose (object);
}

static void
neuland_tox_finalize (GObject *object)
{
  g_debug ("neuland_tox_finalize %p", object);
  NeulandTox *nt = NEULAND_TOX (object);
  NeulandToxPrivate *priv = nt->priv;

  neuland_tox_save_and_kill (nt);

  g_free (priv->tox_id_hex);
  g_free (priv->data_path);
  g_free (priv->name);
  g_free (priv->status_message);

  G_OBJECT_CLASS (neuland_tox_parent_class)->finalize (object);
}

static void
neuland_tox_class_init (NeulandToxClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  // STILL UNUSED
  //klass->connection_status_changed = neuland_tox_connection_status_changed;

  gobject_class->set_property = neuland_tox_set_property;
  gobject_class->get_property = neuland_tox_get_property;
  gobject_class->dispose = neuland_tox_dispose;
  gobject_class->finalize = neuland_tox_finalize;

  klass->remove_contacts = remove_contacts;

  properties[PROP_DATA_PATH] =
    g_param_spec_string ("data-path",
                         "Data path",
                         "Path to a tox data file",
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_READWRITE);
  properties[PROP_TOX_ID_HEX] =
    g_param_spec_string ("tox-id-hex",
                         "Tox ID hex",
                         "Tox ID as a string of hexadecimal digts",
                         NULL,
                         G_PARAM_READABLE);
  properties[PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "Our own name",
                         NEULAND_DEFAULT_NAME,
                         G_PARAM_READWRITE);
  properties[PROP_STATUS] =
    g_param_spec_enum ("status",
                       "Status",
                       "Our own user status",
                       NEULAND_TYPE_CONTACT_STATUS,
                       NEULAND_CONTACT_STATUS_NONE,
                       G_PARAM_READWRITE |
                       G_PARAM_CONSTRUCT);
  properties[PROP_STATUS_MESSAGE] =
    g_param_spec_string ("status-message",
                         "Status message",
                         "Our own status message",
                         NEULAND_DEFAULT_STATUS_MESSAGE,
                         G_PARAM_READWRITE);
  properties[PROP_PENDING_REQUESTS] =
    g_param_spec_int64 ("pending-requests",
                        "Pending requests",
                        "Number of pending contact requests",
                        0,
                        G_MAXINT64,
                        0,
                        G_PARAM_READABLE);

  g_object_class_install_properties (gobject_class,
                                     PROP_N,
                                     properties);

  signals[CONTACT_ADD] =
    g_signal_new ("contact-add",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  NEULAND_TYPE_CONTACT);
  signals[REMOVE_CONTACTS] =
    g_signal_new ("remove-contacts",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (NeulandToxClass, remove_contacts),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_POINTER);
  signals[ACCEPT_REQUESTS] =
    g_signal_new ("accept-requests",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE,
                  1,
                  G_TYPE_POINTER);
}

static void
neuland_tox_init (NeulandTox *tox)
{
  tox->priv = neuland_tox_get_instance_private (tox);
  NeulandToxPrivate *priv = tox->priv;

  priv->tox_struct = tox_new (NULL);

  priv->contacts_ht = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                             NULL, g_object_unref);
  priv->requests_ht = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                             NULL, g_object_unref);

  priv->file_transfers_all_ht = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);
  priv->file_transfers_sending_ht = g_hash_table_new (NULL, NULL);
  priv->file_transfers_receiving_ht = g_hash_table_new (NULL, NULL);

  g_mutex_init (&priv->mutex);
}

static void
neuland_tox_bootstrap (NeulandTox *tox)
{
  NeulandToxPrivate *priv = tox->priv;
  Tox *tox_struct = priv->tox_struct;
  int i;
  guint8 *pub_key_bin = g_malloc0 (TOX_CLIENT_ID_SIZE);

  for (i = 0; i < G_N_ELEMENTS (bootstrap_nodes); i++)
    {
      NeulandToxDhtNode node = bootstrap_nodes[i];
      g_debug ("Bootsrapping from %s:%u %s", node.address, node.port, node.pub_key);

      gboolean valid_string = neuland_hex_string_to_bin (node.pub_key, pub_key_bin, TOX_CLIENT_ID_SIZE);
      if (!valid_string)
        g_warning ("Ignoring invalid key: %s", node.pub_key);
      else
        {
          g_mutex_lock (&priv->mutex);

          int ret = tox_bootstrap_from_address (tox_struct,
                                                node.address,
                                                node.port,
                                                pub_key_bin);

          g_mutex_unlock (&priv->mutex);
        }
    }
  g_free (pub_key_bin);
}

static void
neuland_tox_start (NeulandTox *tox)
{
  NeulandToxPrivate *priv = tox->priv;
  Tox *tox_struct = priv->tox_struct;

  g_mutex_lock (&priv->mutex);
  gint connected = tox_isconnected (tox_struct);
  g_mutex_unlock (&priv->mutex);

  if (connected == 1)
    return;

  neuland_tox_bootstrap (tox);

  while (priv->running)
    {
      guint32 interval;

      /* /\* Debugging *\/ */
      /* if (!g_mutex_trylock (&priv->mutex)) */
      /*   g_message ("tox_do thread has to wait!"); */
      /* else */
      /*   g_mutex_unlock (&priv->mutex); */

      g_mutex_lock (&priv->mutex);

      tox_do (tox_struct);
      interval = tox_do_interval (tox_struct);

      g_mutex_unlock (&priv->mutex);

      //g_debug ("tox_do, new interval: %i", interval * 1000);
      g_usleep ((gulong) 1000 * interval);
    }

  g_debug ("Leaving tox_do thread");
}

NeulandTox *
neuland_tox_new (gchar *data_path)
{
  g_debug ("neuland_tox_new for data: %s", data_path);

  NeulandTox *nt = NEULAND_TOX (g_object_new (NEULAND_TYPE_TOX,
                                              "data-path", data_path,
                                              NULL));
  if (data_path != NULL)
    neuland_tox_load_contacts (nt);

  neuland_tox_connect_callbacks (nt);
  nt->priv->running = TRUE;
  GThread *thread = g_thread_new(NULL, (GThreadFunc) neuland_tox_start, nt);

  return nt;
}
