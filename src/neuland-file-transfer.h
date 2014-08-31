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

#ifndef __NEULAND_FILE_TRANSFER__
#define __NEULAND_FILE_TRANSFER__

#include <gtk/gtk.h>

#define NEULAND_TYPE_FILE_TRANSFER            (neuland_file_transfer_get_type ())
#define NEULAND_FILE_TRANSFER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEULAND_TYPE_FILE_TRANSFER, NeulandFileTransfer))
#define NEULAND_FILE_TRANSFER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NEULAND_TYPE_FILE_TRANSFER, NeulandFileTransferClass))
#define NEULAND_IS_FILE_TRANSFER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEULAND_TYPE_FILE_TRANSFER))
#define NEULAND_IS_FILE_TRANSFER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NEULAND_TYPE_FILE_TRANSFER))
#define NEULAND_FILE_TRANSFER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEULAND_TYPE_FILE_TRANSFER, NeulandFileTransferClass))

typedef struct _NeulandFileTransfer        NeulandFileTransfer;
typedef struct _NeulandFileTransferPrivate NeulandFileTransferPrivate;
typedef struct _NeulandFileTransferClass   NeulandFileTransferClass;

typedef enum {
  NEULAND_FILE_TRANSFER_DIRECTION_SEND,
  NEULAND_FILE_TRANSFER_DIRECTION_RECEIVE,
  NEULAND_FILE_TRANSFER_DIRECTION_NONE,
} NeulandFileTransferDirection;

typedef enum
{
  NEULAND_FILE_TRANSFER_STATE_NONE = 0,
  NEULAND_FILE_TRANSFER_STATE_PENDING,
  NEULAND_FILE_TRANSFER_STATE_IN_PROGRESS,
  NEULAND_FILE_TRANSFER_STATE_PAUSED,
  NEULAND_FILE_TRANSFER_STATE_KILLED_BY_US,
  NEULAND_FILE_TRANSFER_STATE_KILLED_BY_CONTACT,
  NEULAND_FILE_TRANSFER_STATE_FINISHED,
  NEULAND_FILE_TRANSFER_STATE_FINISHED_CONFIRMED,
  NEULAND_FILE_TRANSFER_STATE_ERROR,
} NeulandFileTransferState;

struct _NeulandFileTransfer
{
  GtkBox parent_instance;

  NeulandFileTransferPrivate *priv;
};

struct _NeulandFileTransferClass
{
  GtkBoxClass parent_class;
};

NeulandFileTransfer *
neuland_file_transfer_new_sending (gint64 contact_number, GFile *file);

NeulandFileTransfer *
neuland_file_transfer_new_receiving (gint64 contact_number, gint8 file_number, const gchar *file_name, guint64 file_size);

NeulandFileTransferDirection
neuland_file_transfer_get_direction (NeulandFileTransfer *file_transfer);

guint64
neuland_file_transfer_get_contact_number (NeulandFileTransfer *file_transfer);

void
neuland_file_transfer_set_file_number (NeulandFileTransfer *file_transfer, gint file_number);

gint8
neuland_file_transfer_get_file_number (NeulandFileTransfer *file_transfer);

guint64
neuland_file_transfer_get_file_size (NeulandFileTransfer *file_transfer);

void
neuland_file_transfer_set_transferred_size (NeulandFileTransfer *file_transfer, guint64 transferred_size);

guint64
neuland_file_transfer_get_transferred_size (NeulandFileTransfer *file_transfer);

gchar *
neuland_file_transfer_get_info_string (NeulandFileTransfer *file_transfer);

const gchar *
neuland_file_transfer_get_file_name (NeulandFileTransfer *file_transfer);

gssize
neuland_file_transfer_append_data (NeulandFileTransfer *file_transfer, GByteArray *data_array);

gssize
neuland_file_transfer_get_next_data (NeulandFileTransfer *file_transfer, gpointer buffer, gint data_size);
#endif /* __NEULAND_FILE_TRANSFER__ */
