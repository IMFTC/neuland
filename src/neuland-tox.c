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

// TODO: Thread-safety
#include <string.h>

#include "neuland-tox.h"
#include "neuland-enums.h"

#define NEULAND_DEFAULT_STATUS_MESSAGE "I'm testing Neuland!"
#define NEULAND_DEFAULT_NAME "Neuland User"

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
} DataStructStr;

typedef struct {
  gint32 contact_number;
  guint integer;
  NeulandTox *tox;
} DataStructInt;

static void
add_idle_with_data_string (GSourceFunc idle_func,
                           gint32 contact_number,
                           const guint8 *str,
                           guint16 length,
                           NeulandTox *tox)
{
  DataStructStr *data = g_new0 (DataStructStr, 1);

  data->contact_number = contact_number;
  data->str = g_strndup (str, length);
  data->tox = tox;

  g_idle_add (idle_func, data);
}

static free_data_str (DataStructStr *data)
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
  DataStructInt *data = g_new0 (DataStructInt, 1);

  data->contact_number = contact_number;
  data->integer = integer;
  data->tox = tox;

  g_idle_add (idle_func, data);
}

static free_data_integer (DataStructInt *data)
{
  g_free (data);
}

static gboolean
on_connection_status_idle (gpointer user_data)
{
  DataStructInt *data = user_data;
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
  DataStructInt *data = user_data;
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
  DataStructStr *data = user_data;
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
  DataStructStr *data = user_data;
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
  DataStructStr *data = user_data;
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
  DataStructStr *data = user_data;
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
  DataStructInt *data = user_data;
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

void
neuland_tox_send_message (NeulandTox *tox,
                          NeulandContact *contact,
                          gchar *message)
{
  NeulandToxPrivate *priv = tox->priv;

  gint contact_number;
  g_object_get (contact, "contact-number", &contact_number, NULL);

  g_mutex_lock (&priv->mutex);

  tox_send_message (tox->priv->tox_struct, contact_number,
                    message, strlen (message));

  g_mutex_unlock (&priv->mutex);
}

typedef struct {
  guint8 *public_key;
  guint8 *message;
  NeulandTox *tox;
} DataStructFriendRequest;

static void
on_outgoing_message_cb (NeulandContact *contact,
                        gchar *message,
                        gpointer user_data)
{
  g_debug ("on_outgoing_message_cb contact: %p message: %s", contact, message);
  NeulandTox *tox = NEULAND_TOX (user_data);
  NeulandToxPrivate *priv = tox->priv;
  Tox *tox_struct = priv->tox_struct;

  gint contact_number;
  g_object_get (contact, "number", &contact_number, NULL);
  guint ret;

  g_mutex_lock (&priv->mutex);

  ret = tox_send_message (tox_struct, contact_number, message, strlen (message));

  g_mutex_unlock (&priv->mutex);
}

static void
on_outgoing_action_cb (NeulandContact *contact,
                       gchar *action,
                       gpointer user_data)
{
  g_debug ("on_outgoing_action_cb contact: %p action: %s", contact, action);
  NeulandTox *tox = NEULAND_TOX (user_data);
  NeulandToxPrivate *priv = tox->priv;
  Tox *tox_struct = priv->tox_struct;

  gint contact_number;
  g_object_get (contact, "number", &contact_number, NULL);
  guint ret;

  g_mutex_lock (&priv->mutex);

  ret = tox_send_action (tox_struct, contact_number, action, strlen (action));

  g_mutex_unlock (&priv->mutex);
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
  DataStructFriendRequest *data = user_data;
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
  DataStructFriendRequest *data = g_new0 (DataStructFriendRequest, 1);

  data->public_key = g_memdup (public_key, TOX_FRIEND_ADDRESS_SIZE);
  data->message = g_strndup (message, length);
  data->tox = NEULAND_TOX (user_data);

  g_idle_add (on_friend_request_idle, data);
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

  g_mutex_unlock (&priv->mutex);

  /* TODO: */
  /* tox_callback_group_invite (tox_struct, NULL, tox); */
  /* tox_callback_group_message (tox_struct, NULL, tox); */
  /* tox_callback_group_action (tox_struct, NULL, tox); */
  /* tox_callback_group_namelist_change (tox_struct, NULL, tox); */
  /* tox_callback_file_send_request (tox_struct, NULL, tox); */
  /* tox_callback_file_control (tox_struct, NULL, tox); */
  /* tox_callback_file_data (tox_struct, NULL, tox); */
}

static void
neuland_tox_set_data_path (NeulandTox *tox, gchar *data_path)
{
  NeulandToxPrivate *priv = tox->priv;
  Tox *tox_struct = priv->tox_struct;
  gchar *data;
  gsize length;
  GError *error = NULL;

  g_message ("data_path: %s", data_path);
  if (data_path != NULL)
    {

      if (g_file_get_contents (data_path, &data, &length, &error))
        {
          g_debug ("Setting tox data_path to: %s", priv->data_path);

          g_mutex_lock (&priv->mutex);
          tox_load (priv->tox_struct, data, length);
          g_mutex_unlock (&priv->mutex);

          priv->data_path = data_path;
        }
      else
        {
          if (error->code == G_FILE_ERROR_NOENT) // No such file or directory
            {
              /* Tox will start up without a file, creating a new identity.
                 when closing we will try to save the tox data to this
                 location. */
              g_message ("File %s not found. Will try to save new data to it on exit.", data_path);
              priv->data_path = data_path;
            }
          else
            {
              g_debug ("%s", error->message);
              priv->data_path = NULL;
            }
        }
    }

  g_mutex_lock (&priv->mutex);

  guint8 address[TOX_FRIEND_ADDRESS_SIZE] = {0,};
  guint8 name[TOX_MAX_NAME_LENGTH] = {0, };
  guint8 status_message[TOX_MAX_STATUSMESSAGE_LENGTH] = {0,};
  guchar hex_string[TOX_FRIEND_ADDRESS_SIZE * 2 + 1] = {0,};
  guint16 l;

  tox_get_address (priv->tox_struct, address);
  neuland_bin_to_hex_string (address, hex_string, TOX_FRIEND_ADDRESS_SIZE);
  tox->priv->tox_id_hex = g_strndup (hex_string, TOX_FRIEND_ADDRESS_SIZE * 2);

  l = tox_get_self_status_message (tox_struct, status_message, TOX_MAX_STATUSMESSAGE_LENGTH);
  tox->priv->status_message = g_strndup (status_message, l);

  l = tox_get_self_name (tox_struct, name);
  tox->priv->name = g_strndup (name, l);

  g_mutex_unlock (&priv->mutex);

  /* If we have no name yet, set a default name and status message */
  if (l <= 0)
    {
      tox->priv->name = g_strdup (NEULAND_DEFAULT_NAME);
      tox->priv->status_message = g_strdup (NEULAND_DEFAULT_STATUS_MESSAGE);
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
  g_message ("neuland_tox_add_contact_from_hex_address %s", hex_address);
  gchar *tmp_message = g_strcmp0 (message, "") != 0 ?
    g_strdup (message) : g_strdup ("Let's tox?");
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
    return;

  neuland_tox_load_contacts (tox);

  NeulandContact *contact =
    g_hash_table_lookup (priv->contacts_ht, GINT_TO_POINTER (friend_number));
  g_signal_emit (tox, signals[CONTACT_ADD], 0, contact);
}

void
remove_contacts (NeulandTox *tox,
                 GSList *removed_contacts,
                 gpointer user_data)
{
  NeulandToxPrivate *priv = tox->priv;

  GSList *l;
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
          g_hash_table_remove (priv->contacts_ht, contact);
        }
    }
}

void
neuland_tox_remove_contacts (NeulandTox *tox, GSList *contacts)
{
  g_return_if_fail (NEULAND_IS_TOX (tox));
  g_return_if_fail (NEULAND_IS_CONTACT (contacts->data));

  g_debug ("neuland_tox_remove_contact");

  NeulandToxPrivate *priv = tox->priv;

  GSList *removed_contacts = NULL;
  gboolean pending_requests_changed = FALSE;
  GSList *l;
  for (l = contacts; l; l = l->next)
    {
      NeulandContact *contact  = l->data;

      if (neuland_contact_is_request (contact))
        {
          removed_contacts = g_slist_prepend (removed_contacts, contact);
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
            removed_contacts = g_slist_prepend (removed_contacts, contact);
        }
    }

  if (removed_contacts)
    g_signal_emit (tox, signals[REMOVE_CONTACTS], 0, removed_contacts);

  if (pending_requests_changed)
    g_object_notify_by_pspec (G_OBJECT (tox), properties[PROP_PENDING_REQUESTS]);

  g_slist_free (removed_contacts);
}


/* Add each contact in the @contacts list. Notice that we update each
   contact in place from a 'request' contact with a friend number of
   -1 to a normal contact with the new friend number given by
   tox_add_friend_norequest. */
void
neuland_tox_accept_contact_requests (NeulandTox *tox,
                                     GSList *contacts)
{
  g_return_if_fail (NEULAND_IS_TOX (tox));
  NeulandToxPrivate *priv = tox->priv;

  GSList *accepted_contacts = NULL;
  GSList *l;
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

          g_message ("Added contact %s (%p) from request as number %i",
                     neuland_contact_get_preferred_name (contact), contact, number);
          accepted_contacts = g_slist_prepend (accepted_contacts, contact);
        }
    }
  if (accepted_contacts)
    g_object_notify_by_pspec (G_OBJECT (tox), properties[PROP_PENDING_REQUESTS]);
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
  g_debug ("neuland_tox_set_name: %s ...", name);
  NeulandToxPrivate *priv = tox->priv;

  g_free (priv->name);

  gchar *name_dup = g_strdup (name ? name : "");

  g_mutex_lock (&priv->mutex);

  gint ret = tox_set_name (priv->tox_struct, name_dup, MIN (strlen (name), TOX_MAX_NAME_LENGTH));

  g_mutex_unlock (&priv->mutex);

  if (ret != 0)
    g_warning ("Failed to set our own name. Tried to set name: '%s'", name);
  else
    {
      priv->name = name_dup;
      g_object_notify_by_pspec (G_OBJECT (tox), properties[PROP_NAME]);
    }
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
  g_message ("Setting NeulandTox (%p) status to: %s", tox, eval->value_name);

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

  gchar *status_message_tmp = status_message ? g_strdup (status_message) : "";

  g_mutex_lock (&priv->mutex);

  gint ret = tox_set_status_message (tox_struct, status_message_tmp,
                                     MIN (strlen (status_message_tmp),
                                          TOX_MAX_STATUSMESSAGE_LENGTH));

  g_mutex_unlock (&priv->mutex);

  if (ret == 0)
    {
      g_debug ("Successfully called tox_set_status_message"
               "for tox instance %p with message: '%s'",
               tox_struct, status_message_tmp);
      g_free (priv->status_message);
      priv->status_message = status_message_tmp;
    }
  else
    {
      // keep old message on failure
      g_free (status_message_tmp);
      g_warning ("Failure on calling tox_set_status_message"
                 "for tox instance %p with message '%s'",
                 tox_struct, status_message);
    }
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

static char *
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
      g_value_set_string (value, priv->name);
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
    g_param_spec_string ("self-name",
                         "Self name",
                         "Our own name",
                         NEULAND_DEFAULT_NAME,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT);
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
}

static void
neuland_tox_init (NeulandTox *tox)
{
  tox->priv = neuland_tox_get_instance_private (tox);
  NeulandToxPrivate *priv = tox->priv;

  priv->tox_struct = tox_new (TOX_ENABLE_IPV6_DEFAULT);

  priv->contacts_ht = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                             NULL, g_object_unref);
  priv->requests_ht = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                             NULL, g_object_unref);
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
                                                node.is_ipv6,
                                                g_htons (node.port),
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

  g_message ("Leaving tox_do thread");
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
