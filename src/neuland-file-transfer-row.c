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

  GtkWidget *start_button;
  GtkImage *start_button_image;

  GtkWidget *close_button;
  GtkImage *direction_image;
  gchar *total_size_string;
};

G_DEFINE_TYPE_WITH_PRIVATE (NeulandFileTransferRow, neuland_file_transfer_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  PROP_0,
  PROP_FILE_TRANSFER,
  PROP_N
};

static GParamSpec *properties[PROP_N] = {NULL, };

static void
neuland_file_transfer_row_dispose (GObject *object)
{
  NeulandFileTransferRow *widget = NEULAND_FILE_TRANSFER_ROW (object);

  g_debug ("neuland_file_transfer_row_dispose (%p)", object);

  G_OBJECT_CLASS (neuland_file_transfer_row_parent_class)->dispose (object);
}

static void
neuland_file_transfer_row_finalize (GObject *object)
{
  NeulandFileTransferRow *widget = NEULAND_FILE_TRANSFER_ROW (object);
  NeulandFileTransferRowPrivate *priv = widget->priv;

  g_debug ("neuland_file_transfer_row_finalize (%p)", object);

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
  NeulandFileTransferDirection direction = neuland_file_transfer_get_direction (file_transfer);
  GtkStyleContext *start_button_context = gtk_widget_get_style_context (priv->start_button);

  gtk_style_context_remove_class (start_button_context, "suggested-action");

  switch (state)
    {
    case NEULAND_FILE_TRANSFER_STATE_PENDING:
      switch (direction)
        {
        case NEULAND_FILE_TRANSFER_DIRECTION_RECEIVE:
          gtk_image_set_from_icon_name (priv->start_button_image,
                                        "document-save-as-symbolic",
                                        GTK_ICON_SIZE_MENU);
          gtk_style_context_add_class (start_button_context, "suggested-action");
          gtk_widget_set_sensitive (priv->start_button, TRUE);
          break;

        case NEULAND_FILE_TRANSFER_DIRECTION_SEND:
          gtk_widget_set_sensitive (priv->start_button, FALSE);
          break;

        default:
          break;
        }
      /* Translators: State name for a file transfer that has not been
         started/accepted yet */
      gtk_label_set_text (priv->state_label, _("Pending"));
      break;

    case NEULAND_FILE_TRANSFER_STATE_IN_PROGRESS:
      gtk_image_set_from_icon_name (priv->start_button_image,
                                    "media-playback-pause-symbolic",
                                    GTK_ICON_SIZE_MENU);
      gtk_widget_set_sensitive (priv->start_button, TRUE);
      gtk_label_set_text (priv->state_label, "");
      break;

    case NEULAND_FILE_TRANSFER_STATE_PAUSED_BY_CONTACT:
      gtk_widget_set_sensitive (priv->start_button, FALSE);
      /* fall through */
    case NEULAND_FILE_TRANSFER_STATE_PAUSED_BY_US:
      gtk_image_set_from_icon_name (priv->start_button_image,
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
      gtk_widget_set_opacity (priv->start_button, 0);
      gtk_widget_set_sensitive (priv->start_button, FALSE);
      break;

    case NEULAND_FILE_TRANSFER_STATE_KILLED_BY_US:
      gtk_widget_destroy (GTK_WIDGET (file_transfer_row));
      break;

    case NEULAND_FILE_TRANSFER_STATE_KILLED_BY_CONTACT:
      gtk_widget_set_opacity (priv->start_button, 0);
      gtk_widget_set_sensitive (priv->start_button, FALSE);
      /* Translators: State name for a file transfer that has been cancelled. */
      gtk_label_set_text (priv->state_label, _("Cancelled"));
      gtk_widget_hide (GTK_WIDGET (priv->progress_bar));
      break;

    case NEULAND_FILE_TRANSFER_STATE_ERROR:
      gtk_widget_set_opacity (priv->start_button, 0);
      gtk_widget_set_sensitive (priv->start_button, FALSE);
      /* Translators: State name for a file transfer with an error. */
      gtk_label_set_text (priv->state_label, _("Error"));
      gtk_widget_hide (GTK_WIDGET (priv->progress_bar));
      break;

    default:
      break;
    }
}

gboolean
set_file_from_dialog (NeulandFileTransferRow *file_transfer_row)
{
  NeulandFileTransferRowPrivate *priv;
  NeulandFileTransfer *file_transfer;
  GtkWidget *dialog;
  GFile *file = NULL;
  gint response;

  g_return_val_if_fail (NEULAND_IS_FILE_TRANSFER_ROW (file_transfer_row), FALSE);
  priv = file_transfer_row->priv;
  file_transfer = priv->file_transfer;

  dialog =
    gtk_file_chooser_dialog_new (_("Save as ..."),
                                 GTK_WINDOW (gtk_widget_get_toplevel
                                             (GTK_WIDGET (file_transfer_row))),
                                 GTK_FILE_CHOOSER_ACTION_SAVE,
                                 _("_Save"), GTK_RESPONSE_ACCEPT,
                                 _("_Cancel"), GTK_RESPONSE_REJECT,
                                 NULL);
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), neuland_file_transfer_get_file_name (file_transfer));
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  if (response == GTK_RESPONSE_ACCEPT)
    file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));

  gtk_widget_destroy (dialog);

  if (file)
    neuland_file_transfer_set_file (priv->file_transfer, file);

  g_clear_object (&file);

  return (response == GTK_RESPONSE_ACCEPT);
}

static void
on_start_button_clicked (NeulandFileTransferRow *file_transfer_row,
                         gpointer user_data)
{
  NeulandFileTransferRowPrivate *priv = file_transfer_row->priv;
  NeulandFileTransfer *file_transfer = priv->file_transfer;
  NeulandFileTransferDirection direction = neuland_file_transfer_get_direction (file_transfer);

  switch (neuland_file_transfer_get_state (file_transfer))
    {
    case NEULAND_FILE_TRANSFER_STATE_PENDING:
      if (direction == NEULAND_FILE_TRANSFER_DIRECTION_RECEIVE)
        {
          if (set_file_from_dialog (file_transfer_row))
            neuland_file_transfer_set_requested_state
              (file_transfer, NEULAND_FILE_TRANSFER_STATE_IN_PROGRESS);
        }
      break;

    case NEULAND_FILE_TRANSFER_STATE_IN_PROGRESS:
      neuland_file_transfer_set_requested_state
        (file_transfer, NEULAND_FILE_TRANSFER_STATE_PAUSED_BY_US);
      break;

    case NEULAND_FILE_TRANSFER_STATE_PAUSED_BY_US:
      neuland_file_transfer_set_requested_state
        (file_transfer, NEULAND_FILE_TRANSFER_STATE_IN_PROGRESS);
      break;

    default:
      break;
    }
}

static void
on_close_button_clicked (NeulandFileTransferRow *file_transfer_row,
                         gpointer user_data)
{
  NeulandFileTransferRowPrivate *priv = file_transfer_row->priv;
  NeulandFileTransfer *file_transfer = priv->file_transfer;
  NeulandFileTransferState state = neuland_file_transfer_get_state (file_transfer);

  gtk_widget_set_sensitive (priv->close_button, FALSE);

  if (state == NEULAND_FILE_TRANSFER_STATE_KILLED_BY_CONTACT ||
      state == NEULAND_FILE_TRANSFER_STATE_FINISHED_CONFIRMED ||
      state == NEULAND_FILE_TRANSFER_STATE_ERROR)
    gtk_widget_destroy (GTK_WIDGET (file_transfer_row));
  else
    neuland_file_transfer_set_requested_state
      (file_transfer, NEULAND_FILE_TRANSFER_STATE_KILLED_BY_US);
}

static void
neuland_file_transfer_row_set_file_transfer (NeulandFileTransferRow *row,
                                             NeulandFileTransfer *transfer)
{
  NeulandFileTransferRowPrivate *priv;
  NeulandFileTransferDirection direction;

  g_return_if_fail (NEULAND_IS_FILE_TRANSFER_ROW (row));
  g_return_if_fail (NEULAND_IS_FILE_TRANSFER (transfer));

  priv = row->priv;
  priv->file_transfer = transfer;
  direction = neuland_file_transfer_get_direction (transfer);

  /* Set icon depending on direction */
  gtk_image_set_from_icon_name (priv->direction_image,
                                direction == NEULAND_FILE_TRANSFER_DIRECTION_SEND ?
                                "network-transmit-symbolic" : "network-receive-symbolic",
                                GTK_ICON_SIZE_MENU);

  /* Connect singals and bing properties */
  g_object_connect (transfer,
                    "swapped-signal::notify::state",
                    on_file_transfer_state_changed_cb, row,
                    "swapped-signal::notify::transferred-size",
                    on_file_transfer_transferred_size_changed_cb, row,
                    NULL);
  g_object_bind_property (transfer, "file-name", priv->file_name_label, "label",
                          G_BINDING_SYNC_CREATE);

  /* Do a little caching ... */
  priv->total_size_string = g_format_size (neuland_file_transfer_get_file_size (transfer));

  /* Set up widgets according to the properties of @transfer */
  on_file_transfer_state_changed_cb (G_OBJECT (row), NULL, transfer);
  on_file_transfer_transferred_size_changed_cb (G_OBJECT (row), NULL, transfer);
}

NeulandFileTransfer *
neuland_file_transfer_row_get_file_transfer (NeulandFileTransferRow *row)
{
  g_return_val_if_fail (NEULAND_IS_FILE_TRANSFER_ROW (row), NULL);

  return row->priv->file_transfer;
}

static void
neuland_file_transfer_row_set_property (GObject *object,
                                        guint property_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
  NeulandFileTransferRow *row = NEULAND_FILE_TRANSFER_ROW (object);

  switch (property_id)
    {
    case PROP_FILE_TRANSFER:
      neuland_file_transfer_row_set_file_transfer (row, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
neuland_file_transfer_row_get_property (GObject *object,
                                        guint property_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
  NeulandFileTransferRow *row = NEULAND_FILE_TRANSFER_ROW (object);

  switch (property_id)
    {
    case PROP_FILE_TRANSFER:
      g_value_set_object (value, neuland_file_transfer_row_get_file_transfer (row));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
neuland_file_transfer_row_class_init (NeulandFileTransferRowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_debug ("neuland_file_transfer_row_class_init");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/tox/neuland/neuland-file-transfer-row.ui");
  gtk_widget_class_bind_template_child_private (widget_class, NeulandFileTransferRow, file_name_label);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandFileTransferRow, start_button);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandFileTransferRow, start_button_image);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandFileTransferRow, close_button);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandFileTransferRow, progress_bar);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandFileTransferRow, progress_label);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandFileTransferRow, state_label);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandFileTransferRow, direction_image);

  gtk_widget_class_bind_template_callback (widget_class, on_start_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_close_button_clicked);

  gobject_class->set_property = neuland_file_transfer_row_set_property;
  gobject_class->get_property = neuland_file_transfer_row_get_property;

  gobject_class->dispose = neuland_file_transfer_row_dispose;
  gobject_class->finalize = neuland_file_transfer_row_finalize;

  properties[PROP_FILE_TRANSFER] =
    g_param_spec_object ("file-transfer",
                         "File transfer",
                         "The NeulandFileTransfer represented by this widget",
                         NEULAND_TYPE_FILE_TRANSFER,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (gobject_class,
                                     PROP_N,
                                     properties);
}

static void
neuland_file_transfer_row_init (NeulandFileTransferRow *file_transfer_row)
{
  g_debug ("neuland_file_transfer_row_init");

  file_transfer_row->priv = neuland_file_transfer_row_get_instance_private (file_transfer_row);

  gtk_widget_init_template (GTK_WIDGET (file_transfer_row));
}

GtkWidget *
neuland_file_transfer_row_new (NeulandFileTransfer *file_transfer)
{
  NeulandFileTransferRow *file_transfer_row;

  g_debug ("neuland_file_transfer_row_new");

  g_return_val_if_fail (NEULAND_IS_FILE_TRANSFER (file_transfer), NULL);

  file_transfer_row = g_object_new (NEULAND_TYPE_FILE_TRANSFER_ROW,
                                    "file-transfer", file_transfer,
                                    NULL);

  return GTK_WIDGET (file_transfer_row);
}
