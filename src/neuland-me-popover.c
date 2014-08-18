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

#include "neuland-me-popover.h"

struct _NeulandMePopoverPrivate
{
  NeulandTox *tox;
  GtkEntry *name_entry;
  GtkEntry *status_message_entry;
};

G_DEFINE_TYPE_WITH_PRIVATE (NeulandMePopover, neuland_me_popover, GTK_TYPE_POPOVER)

static void
on_entry_activate (NeulandMePopover *me_popover,
                   gpointer user_data)
{
  NeulandMePopoverPrivate *priv = me_popover->priv;
  GtkEntry *entry = GTK_ENTRY (user_data);

  gtk_widget_hide (GTK_WIDGET (me_popover));
}

static void
neuland_me_popover_class_init (NeulandMePopoverClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/tox/neuland/neuland-me-popover.ui");

  gtk_widget_class_bind_template_child_private (widget_class, NeulandMePopover, name_entry);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandMePopover, status_message_entry);

  gtk_widget_class_bind_template_callback (widget_class, on_entry_activate);
}

static void
neuland_me_popover_init (NeulandMePopover *me_popover)
{
  gtk_widget_init_template (GTK_WIDGET (me_popover));

  me_popover->priv = neuland_me_popover_get_instance_private (me_popover);
}

void
neuland_me_popover_on_closed (GObject *gobject, gpointer unused)
{
  NeulandMePopover *me_popover = NEULAND_ME_POPOVER (gobject);
  NeulandMePopoverPrivate *priv = me_popover->priv;
  NeulandTox *tox = priv->tox;

  neuland_tox_set_name (tox, gtk_entry_get_text (priv->name_entry));
  neuland_tox_set_status_message (tox, gtk_entry_get_text (priv->status_message_entry));
}

GtkWidget *
neuland_me_popover_new (NeulandTox *tox)
{
  NeulandMePopover *me_popover;
  me_popover = NEULAND_ME_POPOVER (g_object_new (NEULAND_TYPE_ME_POPOVER, NULL));

  NeulandMePopoverPrivate *priv = me_popover->priv;
  priv->tox = tox;

  g_object_bind_property (tox, "name", priv->name_entry, "text",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (tox, "status-message", priv->status_message_entry, "text",
                          G_BINDING_SYNC_CREATE);

  g_object_connect (me_popover,
                    "signal::closed", neuland_me_popover_on_closed, NULL,
                    NULL);

  return GTK_WIDGET (me_popover);
}
