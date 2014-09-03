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
#include <glib/gi18n.h>

#include "neuland-file-transfer-row.h"

struct _NeulandFileTransferRowPrivate {
  NeulandFileTransfer *file_transfer;
  GtkLabel *file_name_label;
  GtkLabel *progress_label;
  GtkLabel *state_label;
  GtkProgressBar *progress_bar;
  GtkWidget *pause_button;
  GtkWidget *close_button;
  GtkImage *pause_button_image;
  GtkImage *direction_image;
  gchar *total_size_string;
};

G_DEFINE_TYPE_WITH_PRIVATE (NeulandFileTransferRow, neuland_file_transfer_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  PROP_0,
  PROP_STATE,
  PROP_N
};

static void
neuland_file_transfer_row_dispose (GObject *object)
{
  g_debug ("neuland_file_transfer_row_dispose (%p)", object);
  NeulandFileTransferRow *widget = NEULAND_FILE_TRANSFER_ROW (object);

  G_OBJECT_CLASS (neuland_file_transfer_row_parent_class)->dispose (object);
}

static void
neuland_file_transfer_row_finalize (GObject *object)
{
  g_debug ("neuland_file_transfer_row_finalize (%p)", object);
  NeulandFileTransferRow *widget = NEULAND_FILE_TRANSFER_ROW (object);
  NeulandFileTransferRowPrivate *priv = widget->priv;

  g_free (priv->total_size_string);

  G_OBJECT_CLASS (neuland_file_transfer_row_parent_class)->finalize (object);
}

static void
on_file_transfer_transferred_size_changed_cb (GObject *gobject,
                                              GParamSpec *pspec,
                                              gpointer user_data)
{
  NeulandFileTransferRow *file_transfer_row = NEULAND_FILE_TRANSFER_ROW (gobject);
  NeulandFileTransferRowPrivate *priv = file_transfer_row->priv;

  NeulandFileTransfer *file_transfer = file_transfer_row->priv->file_transfer;
  guint64 transferred_size = neuland_file_transfer_get_transferred_size (file_transfer);
  guint64 total_size = neuland_file_transfer_get_file_size (file_transfer);
  gchar *transferred_size_string = g_format_size (transferred_size);
  gchar *text;

  /* TODO: Don't use different units here, this looks odd and is not easy to read. */
  /* Translators: This string represents the transfer progress, like: "1.5 MB of 12 MB" */
  text = g_strdup_printf (_("%s of %s"), transferred_size_string, priv->total_size_string);

  gtk_label_set_text (priv->progress_label, text);
  gtk_progress_bar_set_fraction (priv->progress_bar, (gdouble)transferred_size / total_size);

  g_free (transferred_size_string);
  g_free (text);
}

static void
on_file_transfer_state_changed_cb (GObject *gobject,
                                   GParamSpec *pspec,
                                   gpointer user_data)
{
  NeulandFileTransferRow *file_transfer_row = NEULAND_FILE_TRANSFER_ROW (gobject);
  NeulandFileTransferRowPrivate *priv = file_transfer_row->priv;
  NeulandFileTransfer *file_transfer = file_transfer_row->priv->file_transfer;
  NeulandFileTransferState state = neuland_file_transfer_get_state (file_transfer);

  switch (state)
    {
    case NEULAND_FILE_TRANSFER_STATE_PENDING: /* fall through */
      /* Translators: State name for a file transfer that has not been started/accepted yet */
      gtk_label_set_text (priv->state_label, _("Pending"));
      gtk_widget_set_sensitive (priv->pause_button, FALSE);
      gtk_widget_set_opacity (priv->pause_button, 0);
      break;
    case NEULAND_FILE_TRANSFER_STATE_IN_PROGRESS:
      gtk_image_set_from_icon_name (priv->pause_button_image,
                                    "media-playback-pause-symbolic",
                                    GTK_ICON_SIZE_MENU);
      gtk_widget_set_opacity (priv->pause_button, 1);
      gtk_widget_set_sensitive (priv->pause_button, TRUE);
      gtk_label_set_text (priv->state_label, "");
      break;

    case NEULAND_FILE_TRANSFER_STATE_PAUSED_BY_CONTACT:
      gtk_widget_set_opacity (priv->pause_button, 1);
      gtk_widget_set_sensitive (priv->pause_button, FALSE);
      /* fall through */
    case NEULAND_FILE_TRANSFER_STATE_PAUSED_BY_US:
      gtk_image_set_from_icon_name (priv->pause_button_image,
                                    "media-playback-start-symbolic",
                                    GTK_ICON_SIZE_MENU);
      /* Translators: State name for a file transfer that has been paused. */
      gtk_label_set_text (priv->state_label, _("Paused"));
      break;

    case NEULAND_FILE_TRANSFER_STATE_FINISHED_CONFIRMED:
      /* Translators: State name for a file transfer that has been finished successfully. */
      gtk_label_set_text (priv->state_label, _("Finished"));
      gtk_label_set_text (priv->progress_label, priv->total_size_string);
      gtk_widget_hide (GTK_WIDGET (priv->progress_bar));
      /* fall through */
    case NEULAND_FILE_TRANSFER_STATE_FINISHED:
      gtk_widget_set_opacity (priv->pause_button, 0);
      gtk_widget_set_sensitive (priv->pause_button, FALSE);
      break;

    case NEULAND_FILE_TRANSFER_STATE_KILLED_BY_US:
      gtk_widget_destroy (GTK_WIDGET (file_transfer_row));
      break;

    case NEULAND_FILE_TRANSFER_STATE_KILLED_BY_CONTACT:
      gtk_widget_set_opacity (priv->pause_button, 0);
      gtk_widget_set_sensitive (priv->pause_button, FALSE);
      /* Translators: State name for a file transfer that has been cancelled. */
      gtk_label_set_text (priv->state_label, _("Cancelled"));
      break;
    }
}

static void
on_pause_button_clicked (NeulandFileTransferRow *file_transfer_row,
                         gpointer user_data)
{
  NeulandFileTransferRowPrivate *priv = file_transfer_row->priv;
  NeulandFileTransfer *file_transfer = priv->file_transfer;

  switch (neuland_file_transfer_get_state (file_transfer))
    {
    case NEULAND_FILE_TRANSFER_STATE_IN_PROGRESS:
      neuland_file_transfer_set_requested_state
        (file_transfer,NEULAND_FILE_TRANSFER_STATE_PAUSED_BY_US);
      break;
    case NEULAND_FILE_TRANSFER_STATE_PAUSED_BY_US:
      neuland_file_transfer_set_requested_state
        (file_transfer, NEULAND_FILE_TRANSFER_STATE_IN_PROGRESS);
      break;
    }
}

static void
on_close_button_clicked (NeulandFileTransferRow *file_transfer_row,
                        gpointer user_data)
{
  NeulandFileTransferRowPrivate *priv = file_transfer_row->priv;
  NeulandFileTransfer *file_transfer = priv->file_transfer;

  gtk_widget_set_sensitive (priv->close_button, FALSE);
  NeulandFileTransferState state = neuland_file_transfer_get_state (file_transfer);

  if (state == NEULAND_FILE_TRANSFER_STATE_KILLED_BY_CONTACT ||
      state == NEULAND_FILE_TRANSFER_STATE_FINISHED_CONFIRMED)
    gtk_widget_destroy (GTK_WIDGET (file_transfer_row));
  else
    neuland_file_transfer_set_requested_state
      (file_transfer, NEULAND_FILE_TRANSFER_STATE_KILLED_BY_US);
}

static void
neuland_file_transfer_row_class_init (NeulandFileTransferRowClass *klass)
{
  g_debug ("neuland_file_transfer_row_class_init");
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/tox/neuland/neuland-file-transfer-row.ui");
  gtk_widget_class_bind_template_child_private (widget_class, NeulandFileTransferRow, file_name_label);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandFileTransferRow, pause_button);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandFileTransferRow, pause_button_image);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandFileTransferRow, close_button);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandFileTransferRow, progress_bar);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandFileTransferRow, progress_label);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandFileTransferRow, state_label);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandFileTransferRow, direction_image);

  gtk_widget_class_bind_template_callback (widget_class, on_pause_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_close_button_clicked);

  gobject_class->dispose = neuland_file_transfer_row_dispose;
  gobject_class->finalize = neuland_file_transfer_row_finalize;
}

static void
neuland_file_transfer_row_init (NeulandFileTransferRow *file_transfer_row)
{
  g_debug ("neuland_file_transfer_row_init");
  gtk_widget_init_template (GTK_WIDGET (file_transfer_row));

  file_transfer_row->priv = neuland_file_transfer_row_get_instance_private (file_transfer_row);
  NeulandFileTransferRowPrivate *priv = file_transfer_row->priv;
}

GtkWidget *
neuland_file_transfer_row_new (NeulandFileTransfer *file_transfer)
{
  NeulandFileTransferRow *file_transfer_row;
  NeulandFileTransferRowPrivate *priv;
  NeulandFileTransferDirection direction;

  g_debug ("neuland_file_transfer_row_new");

  g_assert (NEULAND_IS_FILE_TRANSFER (file_transfer));

  file_transfer_row = NEULAND_FILE_TRANSFER_ROW
    (g_object_new (NEULAND_TYPE_FILE_TRANSFER_ROW, NULL));

  /* Set up the row for @file_transfer */
  priv = file_transfer_row->priv;
  priv->file_transfer = file_transfer;

  /* TODO: file-transfer should rather be a property, then we could do
     all this in the poperty's setter function */
  g_object_connect (file_transfer,
                    "swapped-signal::notify::state",
                    on_file_transfer_state_changed_cb, file_transfer_row,
                    "swapped-signal::notify::transferred-size",
                    on_file_transfer_transferred_size_changed_cb, file_transfer_row,
                    NULL);

  g_object_bind_property (file_transfer, "file-name", priv->file_name_label, "label",
                          G_BINDING_SYNC_CREATE);

  direction = neuland_file_transfer_get_direction (file_transfer);
  gtk_image_set_from_icon_name (priv->direction_image,
                                direction == NEULAND_FILE_TRANSFER_DIRECTION_SEND ?
                                "network-transmit-symbolic" : "network-receive-symbolic",
                                GTK_ICON_SIZE_MENU);

  /* Do a little caching ... */
  priv->total_size_string = g_format_size (neuland_file_transfer_get_file_size (file_transfer));

  on_file_transfer_state_changed_cb (G_OBJECT (file_transfer_row),
                                     NULL, file_transfer);
  on_file_transfer_transferred_size_changed_cb (G_OBJECT (file_transfer_row),
                                                NULL, file_transfer);

  return GTK_WIDGET (file_transfer_row);
}
