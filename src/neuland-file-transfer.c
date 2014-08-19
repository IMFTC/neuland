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

#include "neuland-enums.h"

#include "neuland-file-transfer.h"

//#include <string.h>
//#include <glib/gi18n.h>

struct _NeulandFileTransferPrivate
{
  NeulandFileTransferDirection direction;
  NeulandFileTransferState state;
  guint64 file_size;
  guint64 transferred_size;
  guint64 contact_number;
  gint file_number;             /* toxcore uses uint8_t, we use int to allow -1 for unset */
  gint buffer_size;
  gchar *file_name;
  GFile *file;
  GInputStream *input_stream;
  GFileOutputStream *output_stream;
  GDateTime *creation_time;
};

G_DEFINE_TYPE_WITH_PRIVATE (NeulandFileTransfer, neuland_file_transfer, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_DIRECTION,
  PROP_FILE_NAME,
  PROP_FILE,
  PROP_FILE_NUMBER,
  PROP_CONTACT_NUMBER,
  PROP_FILE_SIZE,
  PROP_TRANSFERRED_SIZE,
  PROP_STATE,
  PROP_N
};

static GParamSpec *properties[PROP_N] = {NULL, };

static void
neuland_file_transfer_dispose (GObject *object)
{
  g_debug ("neuland_file_transfer_dispose (%p)", object);
  NeulandFileTransfer *file_transfer = NEULAND_FILE_TRANSFER (object);
  NeulandFileTransferPrivate *priv = file_transfer->priv;

  g_clear_object (&priv->output_stream);
  g_clear_object (&priv->input_stream);

  G_OBJECT_CLASS (neuland_file_transfer_parent_class)->dispose (object);
}

static void
neuland_file_transfer_finalize (GObject *object)
{
  g_debug ("neuland_file_transfer_finalize (%p)", object);
  NeulandFileTransfer *file_transfer = NEULAND_FILE_TRANSFER (object);
  NeulandFileTransferPrivate *priv = file_transfer->priv;

  g_clear_object (&priv->file);

  G_OBJECT_CLASS (neuland_file_transfer_parent_class)->finalize (object);
}

void
neuland_file_transfer_set_file_number (NeulandFileTransfer *file_transfer,
                                       gint file_number)
{
  g_return_if_fail (NEULAND_IS_FILE_TRANSFER (file_transfer));
  NeulandFileTransferPrivate *priv = file_transfer->priv;

  priv->file_number = file_number;

  g_object_notify_by_pspec (G_OBJECT (file_transfer), properties[PROP_FILE_NUMBER]);
}

gint8
neuland_file_transfer_get_file_number (NeulandFileTransfer *file_transfer)
{
  g_return_if_fail (NEULAND_IS_FILE_TRANSFER (file_transfer));
  NeulandFileTransferPrivate *priv = file_transfer->priv;

  return priv->file_number;
}

guint64
neuland_file_transfer_get_contact_number (NeulandFileTransfer *file_transfer)
{
  g_return_if_fail (NEULAND_IS_FILE_TRANSFER (file_transfer));
  NeulandFileTransferPrivate *priv = file_transfer->priv;

  return priv->contact_number;
}

static void
neuland_file_transfer_set_file_size (NeulandFileTransfer *file_transfer, guint64 file_size)
{
  g_debug ("neuland_file_transfer_set_file_size");
  g_return_if_fail (NEULAND_IS_FILE_TRANSFER (file_transfer));

  NeulandFileTransferPrivate *priv = file_transfer->priv;

  priv->file_size = file_size;

  g_object_notify_by_pspec (G_OBJECT (file_transfer), properties[PROP_FILE_SIZE]);
}

guint64
neuland_file_transfer_get_file_size (NeulandFileTransfer *file_transfer)
{
  g_return_if_fail (NEULAND_IS_FILE_TRANSFER (file_transfer));
  NeulandFileTransferPrivate *priv = file_transfer->priv;

  return priv->file_size;
}

guint64
neuland_file_transfer_get_transferred_size (NeulandFileTransfer *file_transfer)
{
  g_return_if_fail (NEULAND_IS_FILE_TRANSFER (file_transfer));
  NeulandFileTransferPrivate *priv = file_transfer->priv;

  return priv->transferred_size;
}

static void
neuland_file_transfer_set_file_name (NeulandFileTransfer *file_transfer,
                                     const gchar *name)
{
  g_debug ("neuland_file_transfer_set_file_name");
  g_return_if_fail (NEULAND_IS_FILE_TRANSFER (file_transfer));
  NeulandFileTransferPrivate *priv = file_transfer->priv;

  g_return_if_fail (name != NULL);

  g_free (priv->file_name);
  priv->file_name = g_strdup (name);

  g_object_notify_by_pspec (G_OBJECT (file_transfer), properties[PROP_FILE_NAME]);
}

const gchar *
neuland_file_transfer_get_file_name (NeulandFileTransfer *file_transfer)
{
  g_return_if_fail (NEULAND_IS_FILE_TRANSFER (file_transfer));
  NeulandFileTransferPrivate *priv = file_transfer->priv;

  return priv->file_name;
}

NeulandFileTransferDirection
neuland_file_transfer_get_direction (NeulandFileTransfer *file_transfer)
{
  g_return_if_fail (NEULAND_IS_FILE_TRANSFER (file_transfer));
  NeulandFileTransferPrivate *priv = file_transfer->priv;

  return priv->direction;
}

void
neuland_file_transfer_set_file (NeulandFileTransfer *file_transfer, GFile *file)
{
  g_debug ("neuland_file_transfer_set_file");
  g_return_if_fail (NEULAND_IS_FILE_TRANSFER (file_transfer));
  g_return_if_fail (G_IS_FILE (file));

  NeulandFileTransferPrivate *priv = file_transfer->priv;

  g_clear_object (&priv->file);
  priv->file = g_object_ref (file);

  g_object_freeze_notify (G_OBJECT (file_transfer));
  g_object_notify_by_pspec (G_OBJECT (file_transfer), properties[PROP_FILE]);

  /* For an outgoing transfer set name and size from the GFile. */
  if (priv->direction == NEULAND_FILE_TRANSFER_DIRECTION_SEND)
    {
      GFileInfo *info = g_file_query_info (file,
                                           G_FILE_ATTRIBUTE_STANDARD_SIZE ","
                                           G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                           G_FILE_QUERY_INFO_NONE,
                                           NULL,
                                           NULL);

      goffset size = g_file_info_get_size (info);
      neuland_file_transfer_set_file_size (file_transfer, g_file_info_get_size (info));
      neuland_file_transfer_set_file_name (file_transfer, g_file_info_get_display_name (info));
    }

  g_object_thaw_notify (G_OBJECT (file_transfer));
}

GFile *
neuland_file_transfer_get_file (NeulandFileTransfer *file_transfer)
{
  g_return_if_fail (NEULAND_IS_FILE_TRANSFER (file_transfer));
  NeulandFileTransferPrivate *priv = file_transfer->priv;

  return priv->file;
}

void
neuland_file_transfer_set_state (NeulandFileTransfer *file_transfer,
                                 NeulandFileTransferState state)
{
  g_return_if_fail (NEULAND_IS_FILE_TRANSFER (file_transfer));
  NeulandFileTransferPrivate *priv = file_transfer->priv;

  if (state == priv->state)
    return;

  priv->state = state;

  GEnumClass *eclass = g_type_class_peek (NEULAND_TYPE_FILE_TRANSFER_STATE);
  GEnumValue *ev = g_enum_get_value (eclass, state);
  g_debug ("Setting state of file transfer %p to: %s",
             file_transfer, ev->value_name);

  if (priv->direction == NEULAND_FILE_TRANSFER_DIRECTION_RECEIVE)
    {
      if (state == NEULAND_FILE_TRANSFER_STATE_FINISHED)
        {
          g_debug ("Closing output stream for transfer %p", file_transfer);
          g_output_stream_close (G_OUTPUT_STREAM (priv->output_stream), NULL, NULL);
        }
    }

  g_object_notify_by_pspec (G_OBJECT (file_transfer), properties[PROP_STATE]);
}


NeulandFileTransferState
neuland_file_transfer_get_state (NeulandFileTransfer *file_transfer)
{
  g_return_if_fail (NEULAND_IS_FILE_TRANSFER (file_transfer));
  NeulandFileTransferPrivate *priv = file_transfer->priv;

  return priv->state;
}

gchar *
neuland_file_transfer_get_info_string (NeulandFileTransfer *file_transfer)
{
  g_return_if_fail (NEULAND_IS_FILE_TRANSFER (file_transfer));
  NeulandFileTransferPrivate *priv = file_transfer->priv;

  GEnumClass *eclass = g_type_class_peek (NEULAND_TYPE_FILE_TRANSFER_STATE);
  GEnumValue *ev = g_enum_get_value (eclass, neuland_file_transfer_get_state (file_transfer));

  return g_strdup_printf ("NeulandFileTransfer (%p):\n"
                          "    contact-number: %i\n"
                          "    file-number: %i\n"
                          "    file-name: %s\n"
                          "    file-size: %i\n"
                          "    state: %s\n",
                          file_transfer,
                          priv->contact_number,
                          priv->file_number,
                          priv->file_name,
                          priv->file_size,
                          ev->value_name);
}

static void
neuland_file_transfer_set_property (GObject *object,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
  NeulandFileTransfer *file_transfer = NEULAND_FILE_TRANSFER (object);
  NeulandFileTransferPrivate *priv = file_transfer->priv;

  switch (property_id)
    {
    case PROP_DIRECTION:
      priv->direction = g_value_get_enum (value);
      break;
    case PROP_FILE_NAME:
      neuland_file_transfer_set_file_name (file_transfer, g_value_get_string (value));
      break;
    case PROP_FILE:
      neuland_file_transfer_set_file (file_transfer, G_FILE (g_value_get_object (value)));
      break;
    case PROP_FILE_SIZE:
      neuland_file_transfer_set_file_size (file_transfer, g_value_get_uint64 (value));
      break;
    case PROP_FILE_NUMBER:
      neuland_file_transfer_set_file_number (file_transfer, g_value_get_int (value));
      break;
    case PROP_CONTACT_NUMBER:
      priv->contact_number = g_value_get_int64 (value);
      break;
    case PROP_STATE:
      priv->state = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
neuland_file_transfer_get_property (GObject *object,
                                    guint property_id,
                                    GValue *value,
                                    GParamSpec *pspec)
{
  NeulandFileTransfer *file_transfer = NEULAND_FILE_TRANSFER (object);
  NeulandFileTransferPrivate *priv = file_transfer->priv;

  switch (property_id)
    {
    case PROP_DIRECTION:
      g_value_set_enum (value, neuland_file_transfer_get_direction (file_transfer));
      break;
    case PROP_FILE_NAME:
      g_value_set_string (value, neuland_file_transfer_get_file_name (file_transfer));
      break;
    case PROP_FILE:
      g_value_set_object (value, neuland_file_transfer_get_file (file_transfer));
      break;
    case PROP_FILE_SIZE:
      g_value_set_uint64 (value, neuland_file_transfer_get_file_size (file_transfer));
      break;
    case PROP_TRANSFERRED_SIZE:
      g_value_set_uint64 (value, neuland_file_transfer_get_transferred_size (file_transfer));
      break;
    case PROP_CONTACT_NUMBER:
      g_value_set_int64 (value, neuland_file_transfer_get_contact_number (file_transfer));
      break;
    case PROP_FILE_NUMBER:
      g_value_set_int (value, neuland_file_transfer_get_file_number (file_transfer));
      break;
    case PROP_STATE:
      g_value_set_enum (value, neuland_file_transfer_get_state (file_transfer));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
neuland_file_transfer_class_init (NeulandFileTransferClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = neuland_file_transfer_dispose;
  gobject_class->finalize = neuland_file_transfer_finalize;

  gobject_class->set_property = neuland_file_transfer_set_property;
  gobject_class->get_property = neuland_file_transfer_get_property;

  properties[PROP_DIRECTION] =
    g_param_spec_enum ("direction",
                       "Direction",
                       "Direction of the file transfer as a NeulandFileTransferDirection",
                       NEULAND_TYPE_FILE_TRANSFER_DIRECTION,
                       NEULAND_FILE_TRANSFER_DIRECTION_NONE,
                       G_PARAM_READWRITE |
                       G_PARAM_CONSTRUCT_ONLY);

  properties[PROP_FILE_NAME] =
    g_param_spec_string ("file-name",
                         "File name",
                         "Name of the file",
                         "",
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY);

  properties[PROP_FILE] =
    g_param_spec_object ("file",
                         "File",
                         "GFile representing the transfer's location on our side",
                         G_TYPE_FILE,
                         G_PARAM_READWRITE);

  properties[PROP_FILE_NUMBER] =
    g_param_spec_int ("file-number",
                      "File number",
                      "The file number toxcore is using for this file",
                      -1, 255,
                      -1,
                      G_PARAM_READWRITE |
                      G_PARAM_CONSTRUCT);

  properties[PROP_CONTACT_NUMBER] =
    g_param_spec_int64 ("contact-number",
                        "Contact number",
                        "The contact number toxcore is using for this transfer's contact",
                        -1, G_MAXINT32,
                        -1,
                        G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY);

  properties[PROP_FILE_SIZE] =
    g_param_spec_uint64 ("file-size",
                         "File size",
                         "The file size in bytes",
                         0, G_MAXUINT64,
                         0,
                         G_PARAM_READWRITE);

  properties[PROP_TRANSFERRED_SIZE] =
    g_param_spec_uint64 ("transferred-file-size",
                         "Transferred file size",
                         "The already transferred file size in bytes",
                         0, G_MAXUINT64,
                         0,
                         G_PARAM_READABLE);

  properties[PROP_STATE] =
    g_param_spec_enum ("state",
                       "State",
                       "State of the file transfer as a NeulandFileTransferState",
                       NEULAND_TYPE_FILE_TRANSFER_STATE,
                       NEULAND_FILE_TRANSFER_STATE_PENDING,
                       G_PARAM_READWRITE |
                       G_PARAM_CONSTRUCT);

  g_object_class_install_properties (gobject_class,
                                     PROP_N,
                                     properties);
}

static void
neuland_file_transfer_init (NeulandFileTransfer *file_transfer)
{
  file_transfer->priv = neuland_file_transfer_get_instance_private (file_transfer);
  NeulandFileTransferPrivate *priv = file_transfer->priv;

  priv->creation_time = g_date_time_new_now_local ();
}

void
neuland_file_transfer_append_data (NeulandFileTransfer *file_transfer,
                                   GByteArray *data_array)
{
  g_debug ("neuland_file_transfer_append_data");
  g_return_if_fail (NEULAND_IS_FILE_TRANSFER (file_transfer));
  gchar *info = neuland_file_transfer_get_info_string (file_transfer);

  NeulandFileTransferPrivate *priv = file_transfer->priv;

  /* TODO: Error handling */
  /* TODO: Check if file exists, don't just append! */
  if (priv->output_stream == NULL)
    priv->output_stream = g_file_append_to (priv->file, 0, NULL, NULL);

  g_output_stream_write (G_OUTPUT_STREAM (priv->output_stream),
                         data_array->data,
                         data_array->len,
                         NULL, NULL);
}

gssize
neuland_file_transfer_get_next_data (NeulandFileTransfer *file_transfer,
                                     gpointer buffer,
                                     gint data_size)
{
  NeulandFileTransferPrivate *priv;
  gssize read;

  g_return_if_fail (NEULAND_IS_FILE_TRANSFER (file_transfer));
  g_assert (data_size > 0);

  priv = file_transfer->priv;

  if (!priv->input_stream)
    priv->input_stream = G_INPUT_STREAM (g_file_read (priv->file, NULL, NULL));

  read = g_input_stream_read (priv->input_stream, buffer, data_size, NULL, NULL);

  priv->transferred_size += (guint64)read;

  if (read == 0)
    {
      g_debug ("Closing stream for transfer %p");
      g_input_stream_close (G_INPUT_STREAM (priv->input_stream), NULL, NULL);
    }
  return read;
}


NeulandFileTransfer *
neuland_file_transfer_new_sending (gint64 contact_number,
                                   GFile *file)
{
  g_debug ("neuland_file_transfer_new_sending");
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  NeulandFileTransfer *file_transfer =
    NEULAND_FILE_TRANSFER (g_object_new (NEULAND_TYPE_FILE_TRANSFER,
                                         "contact_number", contact_number,
                                         "direction", NEULAND_FILE_TRANSFER_DIRECTION_SEND,
                                         "file", file,
                                         NULL));

  NeulandFileTransferPrivate *priv = file_transfer->priv;
  g_message ("new sending file transfer: \"%s\"", priv->file_name);

  return file_transfer;
}

NeulandFileTransfer *
neuland_file_transfer_new_receiving (gint64 contact_number,
                                     gint8 file_number,
                                     const gchar *file_name,
                                     guint64 file_size)
{
  g_debug ("neuland_file_transfer_new_receiving");

  /* TODO: This is just hard coded for now ... */
  gchar *path = g_strdup_printf ("%s/neuland-download-%s", g_get_home_dir (), file_name);
  GFile *file = g_file_new_for_path (path);
  g_message ("Saving file to \"%s\"", path);
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  NeulandFileTransfer *file_transfer =
    NEULAND_FILE_TRANSFER (g_object_new (NEULAND_TYPE_FILE_TRANSFER,
                                         "contact_number", contact_number,
                                         "direction", NEULAND_FILE_TRANSFER_DIRECTION_RECEIVE,
                                         "file-number", file_number,
                                         "file-name", file_name,
                                         "file-size", file_size,
                                         "file", file,
                                         NULL));
  g_free (path);
  g_object_unref (file);

  NeulandFileTransferPrivate *priv = file_transfer->priv;
  g_message ("new receiving file transfer: \"%s\" (%i bytes)", priv->file_name, priv->file_size);

  return file_transfer;
}
