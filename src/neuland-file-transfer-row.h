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

#ifndef __NEULAND_FILE_TRANSFER_ROW__
#define __NEULAND_FILE_TRANSFER_ROW__

#include <gtk/gtk.h>

#include "neuland-file-transfer.h"

#define NEULAND_TYPE_FILE_TRANSFER_ROW            (neuland_file_transfer_row_get_type ())
#define NEULAND_FILE_TRANSFER_ROW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEULAND_TYPE_FILE_TRANSFER_ROW, NeulandFileTransferRow))
#define NEULAND_FILE_TRANSFER_ROW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NEULAND_TYPE_FILE_TRANSFER_ROW, NeulandFileTransferRowClass))
#define NEULAND_IS_FILE_TRANSFER_ROW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEULAND_TYPE_FILE_TRANSFER_ROW))
#define NEULAND_IS_FILE_TRANSFER_ROW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NEULAND_TYPE_FILE_TRANSFER_ROW))
#define NEULAND_FILE_TRANSFER_ROW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEULAND_TYPE_FILE_TRANSFER_ROW, NeulandFileTransferRowClass))

typedef struct _NeulandFileTransferRow        NeulandFileTransferRow;
typedef struct _NeulandFileTransferRowPrivate NeulandFileTransferRowPrivate;
typedef struct _NeulandFileTransferRowClass   NeulandFileTransferRowClass;

struct _NeulandFileTransferRow
{
  GtkListBoxRow parent;

  NeulandFileTransferRowPrivate *priv;
};

struct _NeulandFileTransferRowClass
{
  GtkListBoxRowClass parent;
};

GType
neuland_file_transfer_row_get_type (void);

GtkWidget*
neuland_file_transfer_row_new (NeulandFileTransfer *file_transfer);

#endif /* __NEULAND_FILE_TRANSFER_ROW__ */
