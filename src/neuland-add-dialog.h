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

#ifndef __NEULAND_ADD_DIALOG_H__
#define __NEULAND_ADD_DIALOG_H__

#include <gtk/gtk.h>

#define NEULAND_TYPE_ADD_DIALOG            (neuland_add_dialog_get_type ())
#define NEULAND_ADD_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEULAND_TYPE_ADD_DIALOG, NeulandAddDialog))
#define NEULAND_ADD_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NEULAND_TYPE_ADD_DIALOG, NeulandAddDialogClass))
#define NEULAND_IS_ADD_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEULAND_TYPE_ADD_DIALOG))
#define NEULAND_IS_ADD_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NEULAND_TYPE_ADD_DIALOG))
#define NEULAND_ADD_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEULAND_TYPE_ADD_DIALOG, NeulandAddDialogClass))

typedef struct _NeulandAddDialog        NeulandAddDialog;
typedef struct _NeulandAddDialogPrivate NeulandAddDialogPrivate;
typedef struct _NeulandAddDialogClass   NeulandAddDialogClass;

struct _NeulandAddDialog
{
  GtkDialog parent;

  NeulandAddDialogPrivate *priv;
};

struct _NeulandAddDialogClass
{
  GtkDialogClass parent;
};

GType neuland_add_dialog_get_type (void);

GtkWidget *
neuland_add_dialog_new (void);

const gchar *
neuland_add_dialog_get_tox_id (NeulandAddDialog *add_dialog);

const gchar *
neuland_add_dialog_get_message (NeulandAddDialog *add_dialog);


#endif /* __NEULAND_ADD_DIALOG_H__ */
