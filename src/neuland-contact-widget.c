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

#include "neuland-contact-widget.h"

struct _NeulandContactWidgetPrivate {
  NeulandContact *contact;
  GtkLabel *name_label;
  GtkLabel *status_label;
  GtkNotebook *indicator_notebook;
  GtkLabel *unread_messages;
  GtkImage *status_image;
};

G_DEFINE_TYPE_WITH_PRIVATE(NeulandContactWidget, neuland_contact_widget, GTK_TYPE_BOX)

enum {
  NOTEBOOK_PAGE_COUNTER,
  NOTEBOOK_PAGE_CHECKBOX
};

static void
neuland_contact_widget_dispose (GObject *object)
{
  g_debug ("neuland_contact_widget_dispose (%p)", object);
  NeulandContactWidget *widget = NEULAND_CONTACT_WIDGET (object);
  G_OBJECT_CLASS (neuland_contact_widget_parent_class)->dispose (object);
}

static void
neuland_contact_widget_finalize (GObject *object)
{
  g_debug ("neuland_contact_widget_finalize (%p)", object);
  G_OBJECT_CLASS (neuland_contact_widget_parent_class)->finalize (object);
}

void
neuland_contact_widget_set_status (NeulandContactWidget *self,
                                   NeulandContactStatus new_status)
{
  gchar *icon_name;
  g_debug ("neuland_contact_widget_set_status: %u", new_status);
  switch (new_status)
    {
    case NEULAND_CONTACT_STATUS_NONE:
      g_debug ("   icon: user-availabe");
      icon_name = "user-available";
      break;
    case NEULAND_CONTACT_STATUS_BUSY:
      icon_name = "user-busy";
      g_debug ("   icon: user-busy");
      break;
    case NEULAND_CONTACT_STATUS_AWAY:
      g_debug ("   icon: user-away");
      icon_name = "user-away";
      break;
    default:
      g_debug ("Invalid NeulandContactStatus: %u", new_status);
      return;
    }
  gtk_image_set_from_icon_name (self->priv->status_image, icon_name, GTK_ICON_SIZE_MENU);
}

void
neuland_contact_widget_set_message (NeulandContactWidget *self,
                                    const gchar *new_message)
{
  gtk_label_set_text (GTK_LABEL (self->priv->status_label), new_message);
}

void
neuland_contact_widget_set_status_message (NeulandContactWidget *self,
                                           gchar *status_message)
{
  gtk_label_set_label (self->priv->status_label, status_message);
}


static void
neuland_contact_widget_class_init (NeulandContactWidgetClass *klass)
{
  g_debug ("neuland_contact_widget_class_init");
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/tox/neuland/neuland-contact-widget.ui");
  gtk_widget_class_bind_template_child_private (widget_class, NeulandContactWidget, name_label);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandContactWidget, status_label);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandContactWidget, unread_messages);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandContactWidget, status_image);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandContactWidget, indicator_notebook);

  gobject_class->dispose = neuland_contact_widget_dispose;
  gobject_class->finalize = neuland_contact_widget_finalize;
}

static void
neuland_contact_widget_init (NeulandContactWidget *self)
{
  g_debug ("neuland_contact_widget_init");
  gtk_widget_init_template (GTK_WIDGET (self));

  self->priv = neuland_contact_widget_get_instance_private (self);
  gtk_notebook_set_current_page (self->priv->indicator_notebook, 0);
}

void
neuland_contact_widget_set_name (NeulandContactWidget *self,
                                 gchar *name)
{
  gtk_label_set_text (self->priv->name_label, name);

}

NeulandContact*
neuland_contact_widget_get_contact (NeulandContactWidget *self)
{
  return self->priv->contact;
}

static void
neuland_contact_widget_status_changed_cb (GObject *obj,
                                          GParamSpec *pspec,
                                          gpointer user_data)
{
  g_debug ("neuland_contact_widget_status_changed_cb");
  NeulandContact *nf = NEULAND_CONTACT (obj);
  NeulandContactWidget *nfw = NEULAND_CONTACT_WIDGET (user_data);
  NeulandContactStatus status;
  g_object_get (nf, "status", &status, NULL);
  neuland_contact_widget_set_status (nfw, status);
}

neuland_contact_widget_set_connected (NeulandContactWidget *self,
                                      gboolean connected)
{
  if (!connected)
    gtk_image_set_from_icon_name (self->priv->status_image,
                                  "user-offline",
                                  GTK_ICON_SIZE_MENU);
}

neuland_contact_widget_connected_changed_cb (GObject *obj,
                                             GParamSpec *pspec,
                                             gpointer user_data)
{
  g_debug ("neuland_contact_widget_connected_changed_cb");
  NeulandContact *nf = NEULAND_CONTACT (obj);
  NeulandContactWidget *nfw = NEULAND_CONTACT_WIDGET (user_data);
  gboolean connected;
  g_object_get (nf, "connected", &connected, NULL);
  neuland_contact_widget_set_connected (nfw, connected);
}

static void
neuland_contact_widget_unread_messages_cb (NeulandContact *contact,
                                           GParamSpec *pspec,
                                           gpointer user_data)
{
  NeulandContactWidget *widget = NEULAND_CONTACT_WIDGET (user_data);

  guint count;
  g_object_get (contact, "unread-messages", &count, NULL);
  gchar *string = g_strdup_printf ("%u", count);
  gtk_label_set_text (widget->priv->unread_messages, string);
  g_free (string);

  if (count == 0)
    gtk_widget_set_opacity (GTK_WIDGET (widget->priv->unread_messages), 0);
  else
    gtk_widget_set_opacity (GTK_WIDGET (widget->priv->unread_messages), 1);
}

static void
neuland_contact_widget_status_message_changed_cb (GObject *obj,
                                                  GParamSpec *pspec,
                                                  gpointer user_data)
{
  g_debug ("neuland_contact_widget_status_message_changed_cb");
  NeulandContact *nf = NEULAND_CONTACT (obj);
  NeulandContactWidget *nfw = NEULAND_CONTACT_WIDGET (user_data);
  gchar *status_message;

  g_object_get (nf, "status-message", &status_message, NULL);
  neuland_contact_widget_set_status_message (nfw, status_message);
}

void
neuland_contact_widget_show_check_box (NeulandContactWidget *self,
                                       gboolean show)
{
  g_return_if_fail (NEULAND_IS_CONTACT_WIDGET (self));
  GtkNotebook *notebook = self->priv->indicator_notebook;

  if (show)
    gtk_notebook_set_current_page (notebook, NOTEBOOK_PAGE_CHECKBOX);
  else
    gtk_notebook_set_current_page (notebook, NOTEBOOK_PAGE_COUNTER);
}


GtkWidget *
neuland_contact_widget_new (NeulandContact *contact)
{
  g_debug ("neuland_contact_widget_new (%p)", contact);
  NeulandContactWidget *nfw;
  nfw = NEULAND_CONTACT_WIDGET (g_object_new (NEULAND_TYPE_CONTACT_WIDGET, NULL));
  nfw->priv->contact = contact;
  if (contact != NULL) {
    // Destroy ourself when the contact is finalized
    //g_object_weak_ref (G_OBJECT (contact), (GWeakNotify)gtk_widget_destroy, GTK_WIDGET (nfw));
    g_object_bind_property (contact, "name", nfw->priv->name_label, "label", G_BINDING_SYNC_CREATE);
    g_object_connect (contact,
                      "signal::notify::connected", neuland_contact_widget_connected_changed_cb, nfw,
                      "signal::notify::status", neuland_contact_widget_status_changed_cb, nfw,
                      "signal::notify::status-message", neuland_contact_widget_status_message_changed_cb, nfw,
                      "signal::notify::unread-messages", neuland_contact_widget_unread_messages_cb, nfw,
                      NULL);
    neuland_contact_widget_status_message_changed_cb (G_OBJECT (contact), NULL, nfw);
  }
  return GTK_WIDGET (nfw);
}
