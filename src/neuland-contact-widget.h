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

#ifndef __NEULAND_CONTACT_WIDGET__
#define __NEULAND_CONTACT_WIDGET__

#include <gtk/gtk.h>

#include "neuland-contact.h"

#define NEULAND_TYPE_CONTACT_WIDGET            (neuland_contact_widget_get_type ())
#define NEULAND_CONTACT_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEULAND_TYPE_CONTACT_WIDGET, NeulandContactWidget))
#define NEULAND_CONTACT_WIDGET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NEULAND_TYPE_CONTACT_WIDGET, NeulandContactWidgetClass))
#define NEULAND_IS_CONTACT_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEULAND_TYPE_CONTACT_WIDGET))
#define NEULAND_IS_CONTACT_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NEULAND_TYPE_CONTACT_WIDGET))
#define NEULAND_CONTACT_WIDGET_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEULAND_TYPE_CONTACT_WIDGET, NeulandContactWidgetClass))

typedef struct _NeulandContactWidget        NeulandContactWidget;
typedef struct _NeulandContactWidgetPrivate NeulandContactWidgetPrivate;
typedef struct _NeulandContactWidgetClass   NeulandContactWidgetClass;

struct _NeulandContactWidget
{
  GtkGrid parent;

  NeulandContactWidgetPrivate *priv;
};

struct _NeulandContactWidgetClass
{
  GtkGridClass parent;
};

GType
neuland_contact_widget_get_type (void);

void
neuland_contact_widget_set_unread_messages_count (NeulandContactWidget *self, gint count);

gint
neuland_contact_widget_get_private_number (NeulandContactWidget *self);

NeulandContact*
neuland_contact_widget_get_contact (NeulandContactWidget *self);

void
neuland_contact_widget_set_status (NeulandContactWidget *self, NeulandContactStatus new_status);

void
neuland_contact_widget_set_message (NeulandContactWidget *self, const gchar *new_message);

void
neuland_contact_widget_show_check_box (NeulandContactWidget *self, gboolean show);

GtkWidget*
neuland_contact_widget_new (NeulandContact *contact);

#endif /* __NEULAND_CONTACT_WIDGET__ */
