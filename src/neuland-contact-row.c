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

#include <string.h>

#include "neuland-contact-row.h"

struct _NeulandContactRowPrivate {
  NeulandContact *contact;
  GtkLabel *name_label;
  GtkLabel *status_label;
  GtkNotebook *indicator_notebook;
  GtkLabel *unread_messages;
  GtkImage *status_image;
  GtkCheckButton *selected_check_button;
  gboolean selected;
};

G_DEFINE_TYPE_WITH_PRIVATE (NeulandContactRow, neuland_contact_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  NOTEBOOK_PAGE_COUNTER,
  NOTEBOOK_PAGE_CHECKBOX
};

enum {
  PROP_0,
  PROP_SELECTED,
  PROP_N
};

static GParamSpec *properties[PROP_N] = {NULL, };

static void
neuland_contact_row_dispose (GObject *object)
{
  g_debug ("neuland_contact_row_dispose (%p)", object);
  NeulandContactRow *widget = NEULAND_CONTACT_ROW (object);

  G_OBJECT_CLASS (neuland_contact_row_parent_class)->dispose (object);
}

static void
neuland_contact_row_finalize (GObject *object)
{
  g_debug ("neuland_contact_row_finalize (%p)", object);
  G_OBJECT_CLASS (neuland_contact_row_parent_class)->finalize (object);
}

void
neuland_contact_row_set_status (NeulandContactRow *contact_row,
                                NeulandContactStatus new_status)
{
  gchar *icon_name;
  g_debug ("neuland_contact_row_set_status: %u", new_status);
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
  gtk_image_set_from_icon_name (contact_row->priv->status_image, icon_name, GTK_ICON_SIZE_MENU);
}

void
neuland_contact_row_set_message (NeulandContactRow *contact_row,
                                 const gchar *new_message)
{
  gtk_label_set_text (GTK_LABEL (contact_row->priv->status_label), new_message);
}

void
neuland_contact_row_set_status_message (NeulandContactRow *contact_row,
                                        const gchar *status_message)
{
  gtk_label_set_label (contact_row->priv->status_label, status_message);
}

void
neuland_contact_row_toggle_selected (NeulandContactRow *contact_row)
{
  NeulandContactRowPrivate *priv = contact_row->priv;

  priv->selected = !priv->selected;
  g_object_notify_by_pspec (G_OBJECT (contact_row), properties[PROP_SELECTED]);
}

void
neuland_contact_row_set_selected (NeulandContactRow *contact_row,
                                  gboolean selected)
{
  NeulandContactRowPrivate *priv = contact_row->priv;
  if (priv->selected == selected)
    return;

  priv->selected = selected;
  g_object_notify_by_pspec (G_OBJECT (contact_row), properties[PROP_SELECTED]);
}

gboolean
neuland_contact_row_get_selected (NeulandContactRow *contact_row)
{
  NeulandContactRowPrivate *priv = contact_row->priv;
  return priv->selected;
}

static void
neuland_contact_row_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  NeulandContactRow *contact_row = NEULAND_CONTACT_ROW (object);

  switch (property_id)
    {
    case PROP_SELECTED:
      neuland_contact_row_set_selected (contact_row, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
neuland_contact_row_get_property (GObject      *object,
                                  guint         property_id,
                                  GValue       *value,
                                  GParamSpec   *pspec)
{
  NeulandContactRow *contact_row = NEULAND_CONTACT_ROW (object);

  switch (property_id)
    {
    case PROP_SELECTED:
      g_value_set_boolean (value, neuland_contact_row_get_selected (contact_row));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
neuland_contact_row_class_init (NeulandContactRowClass *klass)
{
  g_debug ("neuland_contact_row_class_init");
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/tox/neuland/neuland-contact-row.ui");
  gtk_widget_class_bind_template_child_private (widget_class, NeulandContactRow, name_label);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandContactRow, status_label);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandContactRow, unread_messages);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandContactRow, status_image);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandContactRow, indicator_notebook);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandContactRow, selected_check_button);

  gobject_class->dispose = neuland_contact_row_dispose;
  gobject_class->finalize = neuland_contact_row_finalize;

  gobject_class->set_property = neuland_contact_row_set_property;
  gobject_class->get_property = neuland_contact_row_get_property;

  properties[PROP_SELECTED] =
    g_param_spec_boolean ("selected",
                          "Selected",
                          "TRUE if the row is selected (check box is activated), FALSE if not",
                          FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT);

  g_object_class_install_properties (gobject_class,
                                     PROP_N,
                                     properties);
}

static void
neuland_contact_row_init (NeulandContactRow *contact_row)
{
  g_debug ("neuland_contact_row_init");
  gtk_widget_init_template (GTK_WIDGET (contact_row));

  contact_row->priv = neuland_contact_row_get_instance_private (contact_row);
  NeulandContactRowPrivate *priv = contact_row->priv;

  gtk_notebook_set_current_page (priv->indicator_notebook, 0);

  g_object_bind_property (priv->selected_check_button, "active", contact_row, "selected",
                          G_BINDING_BIDIRECTIONAL);
}

void
neuland_contact_row_set_name (NeulandContactRow *contact_row, const char *name)
{
  gtk_label_set_text (contact_row->priv->name_label, name);
}

/* Sets name to the name of contact, or to the tox id, if name is not
   set */
void
neuland_contact_row_update_name (NeulandContactRow *contact_row)
{
  NeulandContactRowPrivate *priv = contact_row->priv;
  NeulandContact *contact = priv->contact;
  GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (priv->name_label));

  const gchar *name = neuland_contact_get_name (contact);

  if (name && strlen (name) > 0)
    {
      gtk_style_context_remove_class (context, "dim-label");
      neuland_contact_row_set_name (contact_row, name);
    }
  else
    {
      /* Looks like there is currently some gtk+ bug here; adding
         dim-label doesn't cause the label to redraw at once. */
      gtk_style_context_add_class (context, "dim-label");
      neuland_contact_row_set_name (contact_row, neuland_contact_get_tox_id_hex (contact));
    }
}

NeulandContact*
neuland_contact_row_get_contact (NeulandContactRow *contact_row)
{
  return contact_row->priv->contact;
}


static void
neuland_contact_row_name_changed_cb (GObject *obj,
                                     GParamSpec *pspec,
                                     gpointer user_data)
{
  g_debug ("neuland_contact_row_name_changed_cb");
  NeulandContactRow *ncw = NEULAND_CONTACT_ROW (user_data);
  neuland_contact_row_update_name (ncw);
}

static void
neuland_contact_row_status_changed_cb (GObject *obj,
                                       GParamSpec *pspec,
                                       gpointer user_data)
{
  g_debug ("neuland_contact_row_status_changed_cb");
  NeulandContact *contact = NEULAND_CONTACT (obj);
  NeulandContactRow *contact_row = NEULAND_CONTACT_ROW (user_data);
  NeulandContactStatus status;
  g_object_get (contact, "status", &status, NULL);
  neuland_contact_row_set_status (contact_row, status);
}

neuland_contact_row_set_connected (NeulandContactRow *contact_row,
                                   gboolean connected)
{
  g_message ("neuland_contact_row_set_connected");

  NeulandContactRowPrivate *priv = contact_row->priv;
  NeulandContact *contact = priv->contact;

  /* If the widget has a contact (the 'me' one has none), check if we
   * have seen this contact online before. If we have, use the
   * 'user-offline' icon, if not, use the 'user-invisible' icon (The
   * user might not have accepted our contact request yet). */
  if (contact)
    {
      if (connected)
        neuland_contact_row_set_status_message
          (contact_row, neuland_contact_get_status_message (contact));
      else
        {
          guint64 last_connected_change = neuland_contact_get_last_connected_change (contact);
          if (last_connected_change != 0)
            /* Seen online before */
            gtk_image_set_from_icon_name (contact_row->priv->status_image,
                                          "user-offline",
                                          GTK_ICON_SIZE_MENU);
          else
            /* Not seen online yet */
            gtk_image_set_from_icon_name (contact_row->priv->status_image,
                                          "user-invisible",
                                          GTK_ICON_SIZE_MENU);

          gchar *string = g_strdup_printf ("Last seen: %s", neuland_contact_get_last_seen (contact));
          gtk_label_set_text (priv->status_label, string);
          g_free (string);
        }

    }
  else
    {
      /* TODO: This isn't used, we still lack the ability to be
         offline while the window is being shown. */
      gtk_image_set_from_icon_name (contact_row->priv->status_image,
                                    "user-offline",
                                    GTK_ICON_SIZE_MENU);
    }
}


static void
neuland_contact_row_connected_changed_cb (GObject *obj,
                                          GParamSpec *pspec,
                                          gpointer user_data)
{
  g_debug ("neuland_contact_row_connected_changed_cb");
  NeulandContact *contact = NEULAND_CONTACT (obj);
  NeulandContactRow *contact_row = NEULAND_CONTACT_ROW (user_data);
  gboolean connected;
  g_object_get (contact, "connected", &connected, NULL);
  neuland_contact_row_set_connected (contact_row, connected);
}

static void
neuland_contact_row_unread_messages_cb (NeulandContact *contact,
                                        GParamSpec *pspec,
                                        gpointer user_data)
{
  NeulandContactRow *widget = NEULAND_CONTACT_ROW (user_data);

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
neuland_contact_row_status_message_changed_cb (GObject *obj,
                                               GParamSpec *pspec,
                                               gpointer user_data)
{
  g_debug ("neuland_contact_row_status_message_changed_cb");
  NeulandContact *contact = NEULAND_CONTACT (obj);
  NeulandContactRow *contact_row = NEULAND_CONTACT_ROW (user_data);
  const gchar *status_message = neuland_contact_get_status_message (contact);

  g_object_get (contact, "status-message", &status_message, NULL);
  neuland_contact_row_set_status_message (contact_row, status_message);
}

void
neuland_contact_row_show_selection (NeulandContactRow *contact_row,
                                    gboolean *show_selection)
{
  if NEULAND_IS_CONTACT_ROW (contact_row)
    {
      GtkNotebook *notebook = contact_row->priv->indicator_notebook;

      if (*show_selection)
        gtk_notebook_set_current_page (notebook, NOTEBOOK_PAGE_CHECKBOX);
      else
        {
          gtk_notebook_set_current_page (notebook, NOTEBOOK_PAGE_COUNTER);
          neuland_contact_row_set_selected (contact_row, FALSE);
        }
    }
}

GtkWidget *
neuland_contact_row_new (NeulandContact *contact)
{
  g_debug ("neuland_contact_row_new (%p)", contact);
  NeulandContactRow *contact_row;
  contact_row = NEULAND_CONTACT_ROW (g_object_new (NEULAND_TYPE_CONTACT_ROW, NULL));
  contact_row->priv->contact = contact;
  if (contact != NULL) {
    // Destroy ourself when the contact is finalized
    //g_object_weak_ref (G_OBJECT (contact), (GWeakNotify)gtk_widget_destroy, GTK_WIDGET (contact_row));
    g_object_connect (contact,
                      "signal::notify::name", neuland_contact_row_name_changed_cb, contact_row,
                      "signal::notify::connected", neuland_contact_row_connected_changed_cb, contact_row,
                      "signal::notify::status", neuland_contact_row_status_changed_cb, contact_row,
                      "signal::notify::status-message", neuland_contact_row_status_message_changed_cb, contact_row,
                      "signal::notify::unread-messages", neuland_contact_row_unread_messages_cb, contact_row,
                      NULL);

    neuland_contact_row_update_name (contact_row);
    neuland_contact_row_status_message_changed_cb (G_OBJECT (contact), NULL, contact_row);
    neuland_contact_row_status_changed_cb (G_OBJECT (contact), NULL, contact_row);
    neuland_contact_row_connected_changed_cb (G_OBJECT (contact), NULL, contact_row);
  }
  return GTK_WIDGET (contact_row);
}
