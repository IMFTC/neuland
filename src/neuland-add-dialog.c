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

#include "neuland-add-dialog.h"

struct _NeulandAddDialogPrivate
{
  GtkEntry *tox_id_entry;
  GtkEntry *message_entry;
  GtkButton *add_button;
};

G_DEFINE_TYPE_WITH_PRIVATE (NeulandAddDialog, neuland_add_dialog, GTK_TYPE_DIALOG)

static void
on_entry_activate (NeulandAddDialog *add_dialog,
                   gpointer user_data)
{
  GtkEntry *entry = GTK_ENTRY (user_data);

  gtk_entry_get_text (entry);

  gtk_dialog_response (GTK_DIALOG (add_dialog), GTK_RESPONSE_OK);
}


static gboolean
validate_address (NeulandAddDialog *add_dialog)
{
  NeulandAddDialogPrivate *priv = add_dialog->priv;

  const gchar *hex_address = neuland_add_dialog_get_tox_id (add_dialog);
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
on_entry_changed (GObject *gobject,
                  gpointer user_data)
{
  NeulandAddDialog *add_dialog = NEULAND_ADD_DIALOG (gobject);
  NeulandAddDialogPrivate *priv = add_dialog->priv;

  GtkEntry *entry = GTK_ENTRY (user_data);
  gboolean address_is_valid = validate_address (add_dialog);

  gtk_widget_set_sensitive (GTK_WIDGET (priv->add_button), address_is_valid);

  GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (entry));
  if (address_is_valid || g_strcmp0 (gtk_entry_get_text (entry), "") == 0)
    gtk_style_context_remove_class (context, "error");
  else
    gtk_style_context_add_class (context, "error");
}


static void
neuland_add_dialog_class_init (NeulandAddDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/tox/neuland/neuland-add-dialog.ui");

  gtk_widget_class_bind_template_child_private (widget_class, NeulandAddDialog, tox_id_entry);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandAddDialog, message_entry);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandAddDialog, add_button);

  gtk_widget_class_bind_template_callback (widget_class, on_entry_activate);
  gtk_widget_class_bind_template_callback (widget_class, on_entry_changed);
}

/* String owned by the widget! */
const gchar *
neuland_add_dialog_get_tox_id (NeulandAddDialog *add_dialog)
{
  NeulandAddDialogPrivate *priv = add_dialog->priv;

  return gtk_entry_get_text (priv->tox_id_entry);
}

/* String owned by the widget! */
const gchar *
neuland_add_dialog_get_message (NeulandAddDialog *add_dialog)
{
  NeulandAddDialogPrivate *priv = add_dialog->priv;

  return gtk_entry_get_text (priv->message_entry);
}

static void
neuland_add_dialog_init (NeulandAddDialog *add_dialog)
{
  GtkBuilder *builder;
  gtk_widget_init_template (GTK_WIDGET (add_dialog));

  add_dialog->priv = neuland_add_dialog_get_instance_private (add_dialog);
}

GtkWidget *
neuland_add_dialog_new ()
{
  NeulandAddDialog *add_dialog;

  add_dialog = NEULAND_ADD_DIALOG (g_object_new (NEULAND_TYPE_ADD_DIALOG,
                                             /* Setting this in
                                                neuland-add-dialog.ui
                                                is not working - why? */
                                             "use-header-bar", TRUE,
                                             NULL));

  return GTK_WIDGET (add_dialog);
}
