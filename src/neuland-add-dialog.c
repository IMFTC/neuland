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

#include "neuland-add-dialog.h"

struct _NeulandAddDialogPrivate
{
  int test;
};

G_DEFINE_TYPE_WITH_PRIVATE (NeulandAddDialog, neuland_add_dialog, GTK_TYPE_DIALOG)

static void
neuland_add_dialog_class_init (NeulandAddDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/tox/neuland/neuland-add-dialog.ui");

}

static void
neuland_add_dialog_init (NeulandAddDialog *self)
{
  GtkBuilder *builder;
  gtk_widget_init_template (GTK_WIDGET (self));
  self->priv = neuland_add_dialog_get_instance_private (self);

  NeulandAddDialogPrivate *priv = self->priv;
}

GtkWidget *
neuland_add_dialog_new ()
{
  NeulandAddDialog *dialog;

  dialog = NEULAND_ADD_DIALOG (g_object_new (NEULAND_TYPE_ADD_DIALOG,
                                             /* Setting this in neuland-add-dialog.ui is not working */
                                             "use-header-bar", TRUE,
                                             NULL));

  return GTK_WIDGET (dialog);
}
