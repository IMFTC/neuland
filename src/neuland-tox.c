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
#include "string.h"

#include "neuland-tox.h"
#include "neuland-enums.h"
#include "neuland-marshal.h"

#define DEFAULT_NEULAND_STATUS_MESSAGE "I'm using unstable software!"
#define DEFAULT_NEULAND_NAME "Neuland User"

struct _NeulandToxPrivate
{
  Tox *tox;
  gchar *data_path;
  gchar *tox_id_hex;
  gchar *name;
  gchar *status_message;
  GHashTable *contacts_ht;
  NeulandContactStatus status;
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
  PROP_N
};

enum {
  DUMMY_SIGNAL,
  LAST_SIGNAL
};

static GParamSpec *properties[PROP_N] = {NULL, };
static guint signals[LAST_SIGNAL] = { 0 };

GList *
neuland_tox_get_contacts (NeulandTox *self)
{
  g_return_if_fail (NEULAND_IS_TOX (self));
  g_debug ("neuland_tox_get_contacts ...");
  return g_hash_table_get_values (self->priv->contacts_ht);
}

NeulandContact *
neuland_tox_get_contact_by_number (NeulandTox *self, gint32 number)
{
  GHashTable *ht = self->priv->contacts_ht;
  return  (g_hash_table_lookup (ht, GINT_TO_POINTER (number)));
}

typedef struct {
  Tox *tox;
  gint32 contact_number;
  guint8 *str;
  NeulandTox *ntox;
} DataStructStr;

typedef struct {
  Tox *tox;
  gint32 contact_number;
  guint integer;
  NeulandTox *ntox;
} DataStructInt;

static void
add_idle_with_data_string (GSourceFunc idle_func,
                           gint32 contact_number,
                           guint8 *str,
                           guint16 length,
                           NeulandTox *ntox)
{
  DataStructStr *data = g_new0 (DataStructStr, 1);

  data->contact_number = contact_number;
  data->str = g_strndup (str, length);
  data->ntox = ntox;

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
                            NeulandTox *ntox)
{
  DataStructInt *data = g_new0 (DataStructInt, 1);

  data->contact_number = contact_number;
  data->integer = integer;
  data->ntox = ntox;

  g_idle_add (idle_func, data);
}

static free_data_integer (DataStructInt *data)
{
  g_free (data);
}

static gboolean
on_connection_status_idle (gpointer user_data)
{
  g_debug ("on_connection_status_idle");
  DataStructInt *data = user_data;
  NeulandContact *contact = neuland_tox_get_contact_by_number (data->ntox, data->contact_number);

  if (contact != NULL)
    neuland_contact_set_connected (contact, data->integer);

  free_data_integer (data);

  return G_SOURCE_REMOVE;
}

static void
on_connection_status (Tox *tox,
                      gint32 contact_number,
                      guint8 status,
                      gpointer user_data)
{
  g_debug ("on_connection_status");
  add_idle_with_data_integer (on_connection_status_idle, contact_number,
                              status, NEULAND_TOX (user_data));
}

static gboolean
on_user_status_idle (gpointer user_data)
{
  g_debug ("on_user_status_idle");
  DataStructInt *data = user_data;
  NeulandContact *contact = neuland_tox_get_contact_by_number (data->ntox, data->contact_number);

  if (contact != NULL)
    g_object_set (contact, "status", data->integer, NULL);

  g_free (data);

  return G_SOURCE_REMOVE;
}

static void
on_user_status (Tox *tox,
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
  NeulandContact *contact = neuland_tox_get_contact_by_number (data->ntox, data->contact_number);

  if (contact != NULL)
    g_object_set (contact, "name", data->str, NULL);

  free_data_str (data);

  return G_SOURCE_REMOVE;
}

static void
on_name_change (Tox *tox,
                gint32 contact_number,
                guint8 *new_name,
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
  NeulandTox *ntox = data->ntox;
  NeulandContact *contact = neuland_tox_get_contact_by_number (ntox, data->contact_number);

  if (contact != NULL)
    g_object_set (contact, "status-message", data->str, NULL);

  free_data_str (data);

  return G_SOURCE_REMOVE;
}

static void
on_status_message (Tox *tox,
                   gint32 contact_number,
                   guint8 *new_message,
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
  NeulandTox *ntox = data->ntox;
  NeulandContact *contact = neuland_tox_get_contact_by_number (ntox, data->contact_number);

  neuland_contact_signal_incoming_message (contact, data->str);
  free_data_str (data);

  return G_SOURCE_REMOVE;
}

static void
on_contact_message (Tox *tox,
                    gint32 contact_number,
                    guint8 *message,
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
  NeulandTox *ntox = data->ntox;
  NeulandContact *contact = neuland_tox_get_contact_by_number (ntox, data->contact_number);

  neuland_contact_signal_incoming_action (contact, data->str);
  free_data_str (data);

  return G_SOURCE_REMOVE;
}

static void
on_contact_action (Tox *tox,
                   gint32 contact_number,
                   guint8 *action,
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
  NeulandTox *ntox = data->ntox;
  NeulandContact *contact = neuland_tox_get_contact_by_number (ntox, data->contact_number);

  if (contact != NULL)
    g_object_set (contact, "is-typing", data->integer, NULL);

  return G_SOURCE_REMOVE;
}

static void
on_typing_change (Tox *tox,
                  gint32 contact_number,
                  guint8 is_typing,
                  gpointer user_data)
{
  add_idle_with_data_integer (on_typing_change_idle, contact_number,
                              is_typing, NEULAND_TOX (user_data));
}

void
neuland_tox_send_message (NeulandTox *ntox,
                          NeulandContact *contact,
                          gchar *message)
{
  NeulandToxPrivate *priv = ntox->priv;

  gint contact_number;
  g_object_get (contact, "contact-number", &contact_number, NULL);
  tox_send_message (ntox->priv->tox, contact_number,
                    message, strlen (message));
}


static void
neuland_tox_connect_callbacks (NeulandTox *self)
{
  Tox *tox = self->priv->tox;
  tox_callback_connection_status (tox, on_connection_status, self);
  tox_callback_user_status (tox, on_user_status, self);
  tox_callback_name_change (tox, on_name_change, self);
  tox_callback_status_message (tox, on_status_message, self);
  tox_callback_friend_message (tox, on_contact_message, self);
  tox_callback_friend_action (tox, on_contact_action, self);
  tox_callback_typing_change (tox, on_typing_change, self);

  /* TODO: */
  /* tox_callback_friend_request (tox, NULL, self); */
  /* tox_callback_group_invite (tox, NULL, self); */
  /* tox_callback_group_message (tox, NULL, self); */
  /* tox_callback_group_action (tox, NULL, self); */
  /* tox_callback_group_namelist_change (tox, NULL, self); */
  /* tox_callback_file_send_request (tox, NULL, self); */
  /* tox_callback_file_control (tox, NULL, self); */
  /* tox_callback_file_data (tox, NULL, self); */
}

static void
on_outgoing_message_cb (NeulandContact *contact,
                        gchar *message,
                        gpointer user_data)
{
  g_debug ("on_outgoing_message_cb contact: %p message: %s", contact, message);
  NeulandTox *ntox = NEULAND_TOX (user_data);
  Tox *tox = ntox->priv->tox;

  gint contact_number;
  g_object_get (contact, "number", &contact_number, NULL);
  guint ret;
  ret = tox_send_message (tox, contact_number, message, strlen (message));
}

static void
on_outgoing_action_cb (NeulandContact *contact,
                       gchar *action,
                       gpointer user_data)
{
  g_debug ("on_outgoing_action_cb contact: %p action: %s", contact, action);
  NeulandTox *ntox = NEULAND_TOX (user_data);
  Tox *tox = ntox->priv->tox;

  gint contact_number;
  g_object_get (contact, "number", &contact_number, NULL);
  guint ret;
  ret = tox_send_action (tox, contact_number, action, strlen (action));
}

static void
neuland_tox_load_contacts (NeulandTox *self)
{
  NeulandToxPrivate *priv = self->priv;
  Tox *tox = priv->tox;

  GHashTable *contacts_ht = self->priv->contacts_ht;
  guint32 n_contacts = tox_count_friendlist (tox);
  guint32 contact_list[n_contacts];
  tox_get_friendlist (tox, contact_list, n_contacts);

  g_debug ("loading contacts ...");
  int i;
  for (i = 0; i < n_contacts; i++)
    {
      gint contact_number = contact_list[i];
      gchar tox_name[TOX_MAX_NAME_LENGTH] = {0};
      gint l;
      GString *contact_name;
      l = tox_get_name (tox, contact_number, tox_name);
      contact_name = g_string_new_len (tox_name, l);
      guint64 last_online = tox_get_last_online (tox, contact_number);
      NeulandContact *contact = neuland_contact_new (contact_number, contact_name->str, last_online);
      g_string_free (contact_name, TRUE);

      g_signal_connect (contact, "outgoing-message", G_CALLBACK (on_outgoing_message_cb), self);
      g_signal_connect (contact, "outgoing-action", G_CALLBACK (on_outgoing_action_cb), self);

      g_hash_table_insert (contacts_ht, GINT_TO_POINTER (contact_number), contact);
    }
  g_debug ("");
}

static void
neuland_tox_set_data_path (NeulandTox *self, gchar *data_path)
{
  NeulandToxPrivate *priv = self->priv;
  Tox *tox = priv->tox;
  gchar *data;
  gsize length;
  GError *error = NULL;

  if (data_path == NULL)
    return;

  g_free (priv->data_path);

  if (g_file_get_contents (data_path, &data, &length, &error))
    {
      g_debug ("Setting tox data_path to: %s", priv->data_path);
      tox_load (priv->tox, data, length);
      priv->data_path = data_path;
    }
  else
    {
      if (error->code == G_FILE_ERROR_NOENT) // No such file or directory
        {
          /* Tox will start up without a file, creating a new identity.
             when closing we will try to save the tox data to this
             location. */
          g_message ("File %s not found. Will try to save new data to it on exit.");
          priv->data_path = data_path;
        }
      else
        {
          g_debug ("%s", error->message);
          priv->data_path = NULL;
        }
      return;
    }

  guint8 address[TOX_FRIEND_ADDRESS_SIZE] = {0,};
  guchar hex_string[TOX_FRIEND_ADDRESS_SIZE * 2 + 1] = {0,};
  tox_get_address (priv->tox, address);
  neuland_bin_to_hex_string (address, hex_string, TOX_FRIEND_ADDRESS_SIZE);
  self->priv->tox_id_hex = g_strndup (hex_string, TOX_FRIEND_ADDRESS_SIZE * 2 + 1);

  guint8 name[TOX_MAX_NAME_LENGTH] = {0, };
  guint16 l = tox_get_self_name (tox, name);
  GString *name_str = g_string_new_len (name, l);
  self->priv->name = name_str->str;
  g_string_free (name_str, FALSE);
}

void
neuland_tox_save_and_kill (NeulandTox *self)
{
  NeulandToxPrivate *priv = self->priv;
  if (priv->data_path)
    {
      gsize size = tox_size (priv->tox);
      gchar *contents = g_malloc0 (size);

      tox_save (priv->tox, contents);

      g_message ("Saving tox data (size: %i bytes) to '%s' ...", size, priv->data_path);
      GError *e = NULL;
      if (!g_file_set_contents (priv->data_path, contents, size, &e))
        g_warning ("Could not save tox data file, error was: %s", e->message);
      g_free (contents);
    }
  else
    g_message ("No data path given on startup; closing without saving any data");

  g_debug ("Killing tox ...");
  tox_kill (priv->tox);
}

void
neuland_tox_set_name (NeulandTox *ntox,
                      const gchar *name)
{
  g_debug ("neuland_tox_set_name: %s ...", name);
  NeulandToxPrivate *priv = ntox->priv;

  g_free (priv->name);

  gchar *name_dup = name ? g_strdup (name) : "";
  priv->name = name_dup;

  gint ret = tox_set_name (priv->tox, name_dup, MIN (strlen (name), TOX_MAX_NAME_LENGTH));
  if (ret != 0)
    g_warning ("Failed to set our own name. Tried to set name: '%s'", name);

  g_object_notify_by_pspec (G_OBJECT (ntox), properties[PROP_NAME]);
}

const gchar *
neuland_tox_get_name (NeulandTox *ntox)
{
  return ntox->priv->name;
}

void
neuland_tox_set_status (NeulandTox *ntox, NeulandContactStatus status)
{
  GEnumClass *eclass = g_type_class_peek (NEULAND_TYPE_CONTACT_STATUS);
  GEnumValue *eval = g_enum_get_value (eclass, status);
  g_message ("Setting NeulandTox (%p) status to: %s", ntox, eval->value_name);

  Tox *tox = ntox->priv->tox;
  tox_set_user_status (tox, (guint8)status);

  ntox->priv->status = status;
  g_object_notify_by_pspec (G_OBJECT (ntox), properties[PROP_STATUS]);
}

NeulandContactStatus
neuland_tox_get_status (NeulandTox *ntox)
{
  return ntox->priv->status;
}

void
neuland_tox_set_status_message (NeulandTox *ntox,
                                const gchar *status_message)
{
  NeulandToxPrivate *priv = ntox->priv;
  Tox *tox = ntox->priv->tox;

  gchar *status_message_tmp = status_message ? g_strdup (status_message) : "";

  gint ret = tox_set_status_message (tox, status_message_tmp,
                                     MIN (strlen (status_message_tmp),
                                          TOX_MAX_STATUSMESSAGE_LENGTH));
  if (ret == 0)
    {
      g_debug ("Successfully called tox_set_status_message"
               "for tox instance %p with message: '%s'",
               tox, status_message_tmp);
      g_free (priv->status_message);
      priv->status_message = status_message_tmp;
    }
  else
    {
      // keep old message on failure
      g_free (status_message_tmp);
      g_warning ("Failure on calling tox_set_status_message"
                 "for tox instance %p with message '%s'",
                 tox, status_message);
    }
  g_object_notify_by_pspec (G_OBJECT (ntox), properties[PROP_STATUS_MESSAGE]);
}

const gchar *
neuland_tox_get_status_message (NeulandTox *ntox)
{
  return ntox->priv->status_message;
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
      g_value_set_string (value, priv->tox_id_hex);
      break;
    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;
    case PROP_STATUS_MESSAGE:
      g_value_set_string (value, priv->status_message);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
neuland_tox_finalize (GObject *object)
{
  g_debug ("neuland_tox_finalize...");
  NeulandTox *nt = NEULAND_TOX (object);
  NeulandToxPrivate *priv = nt->priv;

  neuland_tox_save_and_kill (nt);

  g_free (priv->tox_id_hex);
  g_free (priv->data_path);
  g_free (priv->name);
  g_free (priv->status_message);
  g_hash_table_destroy (priv->contacts_ht);

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
  gobject_class->finalize = neuland_tox_finalize;

  properties[PROP_DATA_PATH] =
    g_param_spec_string ("data-path",
                         "Data path",
                         "Path to a tox data file",
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_READWRITE);
  properties[PROP_TOX_ID_HEX] =
    g_param_spec_string ("tox-id",
                         "Tox ID",
                         "Tox ID as a string of hexadecimal digts",
                         NULL,
                         G_PARAM_READABLE);
  properties[PROP_NAME] =
    g_param_spec_string ("self-name",
                         "Self name",
                         "Our own name",
                         DEFAULT_NEULAND_NAME,
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
                         DEFAULT_NEULAND_STATUS_MESSAGE,
                         G_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class,
                                     PROP_N,
                                     properties);
}

static void
neuland_tox_init (NeulandTox *self)
{
  self->priv = neuland_tox_get_instance_private (self);
  NeulandToxPrivate *priv = self->priv;
  priv->tox = tox_new (TOX_ENABLE_IPV6_DEFAULT);
  priv->contacts_ht = g_hash_table_new (g_direct_hash, g_direct_equal);
}

static void
neuland_tox_bootstrap (NeulandTox *self)
{
  Tox *tox = self->priv->tox;
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
          int ret = tox_bootstrap_from_address (tox,
                                                node.address,
                                                node.is_ipv6,
                                                g_htons (node.port),
                                                pub_key_bin);
        }
    }
  g_free (pub_key_bin);
}

static void
neuland_tox_start (NeulandTox *self)
{
  Tox *tox = self->priv->tox;
  gint connected = tox_isconnected (tox);
  if (connected == 1)
    return;

  neuland_tox_bootstrap (self);
  //TODO: move this to a better place
  while (1)
    {
      guint32 interval;
      tox_do (tox);
      interval = tox_do_interval (tox);
      g_debug ("tox_do, new interval: %i", interval * 1000);
      g_usleep ((gulong) 1000 * interval);
    }
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
  GThread *thread = g_thread_new(NULL, (GThreadFunc) neuland_tox_start, nt);
  return nt;
}
