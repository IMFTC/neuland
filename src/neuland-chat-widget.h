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

#ifndef __NEULAND_CHAT_WIDGET__
#define __NEULAND_CHAT_WIDGET__

#include <gtk/gtk.h>

#include "neuland-tox.h"
#include "neuland-contact.h"


#define NEULAND_TYPE_CHAT_WIDGET            (neuland_chat_widget_get_type ())
#define NEULAND_CHAT_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEULAND_TYPE_CHAT_WIDGET, NeulandChatWidget))
#define NEULAND_CHAT_WIDGET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NEULAND_TYPE_CHAT_WIDGET, NeulandChatWidgetClass))
#define NEULAND_IS_CHAT_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEULAND_TYPE_CHAT_WIDGET))
#define NEULAND_IS_CHAT_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NEULAND_TYPE_CHAT_WIDGET))
#define NEULAND_CHAT_WIDGET_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEULAND_TYPE_CHAT_WIDGET, NeulandChatWidgetClass))

typedef struct _NeulandChatWidget        NeulandChatWidget;
typedef struct _NeulandChatWidgetPrivate NeulandChatWidgetPrivate;
typedef struct _NeulandChatWidgetClass   NeulandChatWidgetClass;

struct _NeulandChatWidget
{
  GtkBox parent_instance;

  NeulandChatWidgetPrivate *priv;
};

struct _NeulandChatWidgetClass
{
  GtkBoxClass parent_class;
};

GtkWidget *
neuland_chat_widget_new (NeulandTox *tox, NeulandContact *contact, gint text_entry_min_height);

NeulandContact *
neuland_chat_widget_get_contact (NeulandChatWidget *widget);

#endif /* __NEULAND_CHAT_WIDGET__ */
