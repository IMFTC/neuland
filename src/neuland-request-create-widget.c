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
#include <string.h>

#include "neuland-utils.h"
#include "neuland-request-create-widget.h"

struct _NeulandRequestCreateWidgetPrivate
{
  GtkEntry *tox_id_entry;
  GtkTextBuffer *text_buffer;
  GtkButton *add_button;
};

G_DEFINE_TYPE_WITH_PRIVATE (NeulandRequestCreateWidget, neuland_request_create_widget, GTK_TYPE_BOX)

/* static void
 * on_entry_activate (NeulandRequestCreateWidget *widget,
 *                    gpointer user_data)
 * {
 *   GtkEntry *entry = GTK_ENTRY (user_data);
 *
 *   gtk_entry_get_text (entry);
 *
 *   gtk_dialog_response (GTK_DIALOG (widget), GTK_RESPONSE_OK);
 * } */


static gboolean
validate_address (NeulandRequestCreateWidget *widget)
{
  NeulandRequestCreateWidgetPrivate *priv = widget->priv;

  const gchar *hex_address = neuland_request_create_widget_get_tox_id (widget);
  guint8 bin_address[TOX_FRIEND_ADDRESS_SIZE] = {0,};

  if (strlen (hex_address) != TOX_FRIEND_ADDRESS_SIZE * 2)
    return FALSE;

  gboolean is_valid_hex_string =
    neuland_hex_string_to_bin (hex_address, bin_address, TOX_FRIEND_ADDRESS_SIZE);

  if (!is_valid_hex_string)
    return FALSE;

  return TRUE;
}


static void
on_tox_id_entry_changed (GObject *gobject,
                         gpointer user_data)
{
  NeulandRequestCreateWidget *widget = NEULAND_REQUEST_CREATE_WIDGET (gobject);
  NeulandRequestCreateWidgetPrivate *priv = widget->priv;

  GtkEntry *entry = GTK_ENTRY (user_data);
  gboolean address_is_valid = validate_address (widget);

  /* gtk_widget_set_sensitive (GTK_WIDGET (priv->add_button), address_is_valid); */

  GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (entry));
  if (address_is_valid || g_strcmp0 (gtk_entry_get_text (entry), "") == 0)
    gtk_style_context_remove_class (context, "error");
  else
    gtk_style_context_add_class (context, "error");
}


static void
neuland_request_create_widget_class_init (NeulandRequestCreateWidgetClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/tox/neuland/neuland-request-create-widget.ui");

  gtk_widget_class_bind_template_child_private (widget_class, NeulandRequestCreateWidget, tox_id_entry);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandRequestCreateWidget, text_buffer);

  /* gtk_widget_class_bind_template_callback (widget_class, on_entry_activate); */
  gtk_widget_class_bind_template_callback (widget_class, on_tox_id_entry_changed);
}

/* String owned by the widget! */
const gchar *
neuland_request_create_widget_get_tox_id (NeulandRequestCreateWidget *widget)
{
  NeulandRequestCreateWidgetPrivate *priv = widget->priv;

  return gtk_entry_get_text (priv->tox_id_entry);
}

gchar *
neuland_request_create_widget_get_message (NeulandRequestCreateWidget *widget)
{
  NeulandRequestCreateWidgetPrivate *priv = widget->priv;
  GtkTextIter start_iter;
  GtkTextIter end_iter;

  gtk_text_buffer_get_bounds (priv->text_buffer, &start_iter, &end_iter);

  return gtk_text_buffer_get_text (priv->text_buffer, &start_iter, &end_iter, FALSE);
}

void
neuland_request_create_widget_clear (NeulandRequestCreateWidget *widget)
{
  NeulandRequestCreateWidgetPrivate *priv = widget->priv;

  gtk_entry_set_text (priv->tox_id_entry, "");
  gtk_text_buffer_set_text (priv->text_buffer, "", -1);
}

static void
neuland_request_create_widget_init (NeulandRequestCreateWidget *widget)
{
  gtk_widget_init_template (GTK_WIDGET (widget));

  widget->priv = neuland_request_create_widget_get_instance_private (widget);
}

GtkWidget *
neuland_request_create_widget_new ()
{
  NeulandRequestCreateWidget *widget;

  widget = NEULAND_REQUEST_CREATE_WIDGET (g_object_new (NEULAND_TYPE_REQUEST_CREATE_WIDGET,
                                                        NULL));

  return GTK_WIDGET (widget);
}
