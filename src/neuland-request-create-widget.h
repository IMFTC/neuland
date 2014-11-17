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

#ifndef __NEULAND_REQUEST_CREATE_H__
#define __NEULAND_REQUEST_CREATE_H__

#include <gtk/gtk.h>

#define NEULAND_TYPE_REQUEST_CREATE_WIDGET            (neuland_request_create_widget_get_type ())
#define NEULAND_REQUEST_CREATE_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEULAND_TYPE_REQUEST_CREATE_WIDGET, NeulandRequestCreateWidget))
#define NEULAND_REQUEST_CREATE_WIDGET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NEULAND_TYPE_REQUEST_CREATE_WIDGET, NeulandRequestCreateWidgetClass))
#define NEULAND_IS_REQUEST_CREATE_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEULAND_TYPE_REQUEST_CREATE_WIDGET))
#define NEULAND_IS_REQUEST_CREATE_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NEULAND_TYPE_REQUEST_CREATE_WIDGET))
#define NEULAND_REQUEST_CREATE_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), NEULAND_TYPE_REQUEST_CREATE_WIDGET, NeulandRequestCreateWidgetClass))

typedef struct _NeulandRequestCreateWidget        NeulandRequestCreateWidget;
typedef struct _NeulandRequestCreateWidgetPrivate NeulandRequestCreateWidgetPrivate;
typedef struct _NeulandRequestCreateWidgetClass   NeulandRequestCreateWidgetClass;

struct _NeulandRequestCreateWidget
{
  GtkBox parent;

  NeulandRequestCreateWidgetPrivate *priv;
};

struct _NeulandRequestCreateWidgetClass
{
  GtkBoxClass parent;
};

GType neuland_request_create_widget_get_type (void);

GtkWidget *
neuland_request_create_widget_new (void);

const gchar *
neuland_request_create_widget_get_tox_id (NeulandRequestCreateWidget *widget);

gchar *
neuland_request_create_widget_get_message (NeulandRequestCreateWidget *widget);

void
neuland_request_create_widget_clear (NeulandRequestCreateWidget *widget);

#endif /* __NEULAND_REQUEST_CREATE_H__ */
