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

#ifndef __NEULAND_WINDOW__
#define __NEULAND_WINDOW__

#include <gtk/gtk.h>

#include "neuland-tox.h"
#include "neuland-contact-widget.h"

#define NEULAND_TYPE_WINDOW            (neuland_window_get_type ())
#define NEULAND_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEULAND_TYPE_WINDOW, NeulandWindow))
#define NEULAND_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NEULAND_TYPE_WINDOW, NeulandWindowClass))
#define NEULAND_IS_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEULAND_TYPE_WINDOW))
#define NEULAND_IS_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NEULAND_TYPE_WINDOW))
#define NEULAND_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEULAND_TYPE_WINDOW, NeulandWindowClass))

typedef struct _NeulandWindow        NeulandWindow;
typedef struct _NeulandWindowPrivate NeulandWindowPrivate;
typedef struct _NeulandWindowClass   NeulandWindowClass;

struct _NeulandWindow
{
  GtkApplicationWindow parent_instance;

  NeulandWindowPrivate *priv;
};

struct _NeulandWindowClass
{
  GtkApplicationWindowClass parent_class;
};

GType
neuland_window_get_type (void);

void
neuland_window_add_contact_widget (NeulandWindow *self,
                                   NeulandContactWidget *nfw);
void
neuland_window_set_status_activated (GSimpleAction *action,
                                     GVariant *parameter,
                                     gpointer user_data);

GtkWidget *
neuland_window_new (NeulandTox *ntox);

#endif /* __NEULAND_WINDOW__ */
