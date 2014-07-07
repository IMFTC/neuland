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

#include "neuland-chat-widget.h"

#include <string.h>

struct _NeulandChatWidgetPrivate {
  NeulandTox *ntox;
  NeulandContact *contact;
  GtkTextView *text_view;
  GtkTextView *entry_text_view;
  GtkTextBuffer *text_buffer;
  GtkInfoBar *info_bar;
  GtkTextBuffer *entry_text_buffer;
  GtkTextTag *contact_name_tag;
  GtkTextTag *my_name_tag;
  GtkTextTag *is_typing_tag;
  GtkTextTag *action_tag;
  GtkTextMark *scroll_mark; // owned by text_buffer, don't free in finalize!
};

G_DEFINE_TYPE_WITH_PRIVATE (NeulandChatWidget, neuland_chat_widget, GTK_TYPE_BOX)

typedef enum {
  CHAT_DIRECTION_INCOMING,
  CHAT_DIRECTION_OUTGOING
} ChatTextDirection;

static void
neuland_chat_widget_dispose (GObject *object)
{
  g_debug ("neuland_chat_widget_dispose (%p)", object);
  NeulandChatWidget *widget = NEULAND_CHAT_WIDGET (object);
  g_clear_object (&widget->priv->ntox);
  g_clear_object (&widget->priv->contact);

  G_OBJECT_CLASS (neuland_chat_widget_parent_class)->dispose (object);
}

static void
neuland_chat_widget_finalize (GObject *object)
{
  g_debug ("neuland_chat_widget_finalize (%p)", object);
  NeulandChatWidget *widget = NEULAND_CHAT_WIDGET (object);
  G_OBJECT_CLASS (neuland_chat_widget_parent_class)->finalize (object);
}

static gboolean
neuland_chat_widget_scroll_to_bottom (NeulandChatWidget *self)
{
  NeulandChatWidgetPrivate *priv = self->priv;
  GtkTextIter iter;
  GtkTextMark *mark = priv->scroll_mark;

  gtk_text_buffer_get_end_iter (priv->text_buffer, &iter);
  gtk_text_iter_set_line_offset (&iter, 0);
  gtk_text_buffer_move_mark (priv->text_buffer, mark, &iter);
  gtk_text_view_scroll_mark_onscreen (priv->text_view, mark);
}

static void
neuland_chat_widget_set_is_typing (NeulandChatWidget *self, gboolean is_typing)
{
  NeulandChatWidgetPrivate *priv = self->priv;
  GtkTextBuffer *buffer = priv->text_buffer;
  NeulandContact *contact = priv->contact;

  GtkTextMark *is_typing_start_mark = NULL;
  GtkTextIter is_typing_start_iter;
  GtkTextIter end_iter;

  is_typing_start_mark = gtk_text_buffer_get_mark (buffer, "is-typing-start");
  if (is_typing_start_mark != NULL)
    gtk_text_buffer_get_iter_at_mark (buffer, &is_typing_start_iter, is_typing_start_mark);

  gtk_text_buffer_get_end_iter (buffer, &end_iter);

  gchar *name;
  g_object_get (contact, "name", &name, NULL);
  gchar *string;
  if (is_typing && (is_typing_start_mark == NULL))
    {
      gtk_text_buffer_create_mark (buffer, "is-typing-start", &end_iter, TRUE);
      string = g_strdup_printf ("%s is typing ...\n", name);
      gtk_text_buffer_insert_with_tags (buffer, &end_iter, string, -1, priv->is_typing_tag, NULL);
      g_free (string);
    }
  else if (is_typing_start_mark != NULL)
    {
      gtk_text_buffer_delete (buffer, &is_typing_start_iter, &end_iter);
      gtk_text_buffer_delete_mark (buffer, is_typing_start_mark);
    }
  g_free (name);
}

static void
insert_message (NeulandChatWidget *widget,
                gchar* message,
                ChatTextDirection direction)
{
  NeulandChatWidgetPrivate *priv = widget->priv;
  GtkTextBuffer *text_buffer = priv->text_buffer;
  NeulandTox *ntox = priv->ntox;
  NeulandContact *contact = priv->contact;
  GtkTextIter iter;
  const gchar *name;
  GtkTextTag *name_tag;

  switch (direction)
    {
    case CHAT_DIRECTION_INCOMING:
      name = neuland_contact_get_name (contact);
      name_tag = priv->contact_name_tag;
      break;
    case CHAT_DIRECTION_OUTGOING:
      name = neuland_tox_get_name (ntox);
      name_tag = priv->my_name_tag;
      break;
    }

  neuland_chat_widget_set_is_typing (widget, FALSE);
  gchar *prefix = g_strdup_printf ("%s: ", name);
  gtk_text_buffer_get_end_iter (text_buffer, &iter);
  gtk_text_buffer_insert_with_tags (text_buffer, &iter, prefix, -1,
                                    name_tag, NULL);
  gtk_text_buffer_insert (text_buffer, &iter, message, -1);
  gtk_text_buffer_insert (text_buffer, &iter, "\n", -1);
  g_free (prefix);
  neuland_chat_widget_scroll_to_bottom (widget);
}

static void
insert_action (NeulandChatWidget *widget,
               gchar* action,
               ChatTextDirection direction)
{
  NeulandChatWidgetPrivate *priv = widget->priv;
  GtkTextBuffer *text_buffer = priv->text_buffer;
  NeulandTox *ntox = priv->ntox;
  NeulandContact *contact = priv->contact;
  GtkTextIter iter;
  const gchar *name;
  GtkTextTag *name_tag;

  switch (direction)
    {
    case CHAT_DIRECTION_INCOMING:
      name = neuland_contact_get_name (contact);
      name_tag = priv->contact_name_tag;
      break;
    case CHAT_DIRECTION_OUTGOING:
      name = neuland_tox_get_name (ntox);
      name_tag = priv->my_name_tag;
      break;
    }

  neuland_chat_widget_set_is_typing (widget, FALSE);
  gchar *action_string = g_strdup_printf ("* %s %s\n", name, action);
  gtk_text_buffer_get_end_iter (text_buffer, &iter);
  gtk_text_buffer_insert_with_tags (text_buffer, &iter, action_string, -1,
                                    priv->action_tag,
                                    NULL);
  g_free (action_string);
  neuland_chat_widget_scroll_to_bottom (widget);
}

static void
on_outgoing_message_cb (NeulandChatWidget *widget,
                        gchar *message,
                        gpointer user_data)
{
  g_debug ("on_outgoing_message_cb");
  insert_message (widget, message, CHAT_DIRECTION_OUTGOING);
}

static void
on_outgoing_action_cb (NeulandChatWidget *widget,
                       gchar *action,
                       gpointer user_data)
{
  g_debug ("on_outgoing_action_cb");
  insert_action (widget, action, CHAT_DIRECTION_OUTGOING);
}

static void
on_incoming_message_cb (NeulandChatWidget *widget,
                        gchar *message,
                        gpointer user_data)
{
  g_debug ("on_incoming_message_cb");
  insert_message (widget, message, CHAT_DIRECTION_INCOMING);
}

static void
on_incoming_action_cb (NeulandChatWidget *widget,
                       gchar *action,
                       gpointer user_data)
{
  g_debug ("on_incoming_action_cb");
  insert_action (widget, action, CHAT_DIRECTION_INCOMING);
}

static void
neuland_chat_widget_show_not_connected_info (NeulandChatWidget *self,
                                             gboolean show)
{
  gtk_widget_set_visible (GTK_WIDGET (self->priv->info_bar),
                          show);
}

static void
neuland_handle_input (NeulandChatWidget *widget, gchar *string)
{
  NeulandChatWidgetPrivate *priv = widget->priv;

  if (g_ascii_strncasecmp (string, "/", 1) == 0 &&
      g_ascii_strncasecmp (string, "//", 2) != 0)
    {
      if (g_ascii_strncasecmp (string, "/me ", 4) == 0)
        {
          g_debug ("/me command recognized");
          neuland_contact_send_action (priv->contact, string+4);
        }
      else if (g_ascii_strncasecmp (string, "/message ", 9) == 0)
        {
          g_debug ("/message command recognized");
          neuland_tox_set_status_message (priv->ntox, g_strchug (string+9));
        }
      else if (g_ascii_strncasecmp (string, "/nick ", 6) == 0)
        {
          g_debug ("/nick command recognized");
          neuland_tox_set_name (priv->ntox, g_strchug(string+6));
        }
    }
  else
    neuland_contact_send_message (priv->contact, string);
}

static gboolean
entry_text_view_key_press_event_cb (NeulandChatWidget *widget,
                                    GdkEventKey *event,
                                    gpointer user_data)
{
  NeulandChatWidgetPrivate *priv = widget->priv;

  if ((event->keyval == GDK_KEY_Return ||
       event->keyval == GDK_KEY_ISO_Enter ||
       event->keyval == GDK_KEY_KP_Enter) &&
      !(event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)))
    {
      GtkTextIter start_iter;
      GtkTextIter end_iter;
      gchar *string;
      gchar *new_nick = NULL;
      gchar *new_message = NULL;
      gboolean contact_is_connected;
      gtk_text_buffer_get_bounds (priv->entry_text_buffer, &start_iter, &end_iter);
      string = gtk_text_buffer_get_text (priv->entry_text_buffer, &start_iter, &end_iter, FALSE);

      contact_is_connected = neuland_contact_get_connected (priv->contact);
      if (strlen (string) == 0)
        {
          g_message ("Ignoring empty message.");
        }
      else if (contact_is_connected)
        {
          neuland_handle_input (widget, string);
          gtk_text_buffer_delete (priv->entry_text_buffer, &start_iter, &end_iter);
        }
      else
        neuland_chat_widget_show_not_connected_info (widget, !contact_is_connected);

      g_free (string);

      return TRUE;
    }

  return FALSE;
}

static void
neuland_chat_widget_class_init (NeulandChatWidgetClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/tox/neuland/neuland-chat-widget.ui");
  gtk_widget_class_bind_template_child_private (widget_class, NeulandChatWidget, text_view);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandChatWidget, entry_text_view);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandChatWidget, text_buffer);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandChatWidget, entry_text_buffer);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandChatWidget, is_typing_tag);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandChatWidget, action_tag);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandChatWidget, contact_name_tag);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandChatWidget, my_name_tag);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandChatWidget, info_bar);
  gtk_widget_class_bind_template_callback (widget_class, entry_text_view_key_press_event_cb);

  gobject_class->dispose = neuland_chat_widget_dispose;
  gobject_class->finalize = neuland_chat_widget_finalize;
}

static void
neuland_chat_widget_init (NeulandChatWidget *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  self->priv = neuland_chat_widget_get_instance_private (self);
  NeulandChatWidgetPrivate *priv = self->priv;

  GtkTextIter iter;
  gtk_text_buffer_get_end_iter (priv->text_buffer, &iter);
  priv->scroll_mark = gtk_text_buffer_create_mark (priv->text_buffer, "scroll", &iter, TRUE);
}

static void
neuland_chat_widget_is_typing_cb (GObject *obj,
                                  GParamSpec *pspec,
                                  gpointer user_data)
{
  NeulandContact *contact = NEULAND_CONTACT (obj);
  NeulandChatWidget *chat_widget = NEULAND_CHAT_WIDGET (user_data);
  gboolean is_typing;

  g_object_get (contact, "is-typing", &is_typing, NULL);
  neuland_chat_widget_set_is_typing (chat_widget, is_typing);
}

static void
on_connected_changed (GObject *obj,
                      GParamSpec *pspec,
                      gpointer user_data)
{
  NeulandContact    *contact   = NEULAND_CONTACT (obj);
  NeulandChatWidget *widget    = NEULAND_CHAT_WIDGET (user_data);
  gboolean           connected = neuland_contact_get_connected (contact);

  /* Make sure we hide the info bar when a contact comes online but
     don't show it automatically when a contact goes offline since
     that clutters the UI. We only show it once the user actually
     tries to send a message.
  */
  if (connected)
    neuland_chat_widget_show_not_connected_info (widget, FALSE);
}

GtkWidget *
neuland_chat_widget_new (NeulandTox *ntox,
                         NeulandContact *contact,
                         gint text_entry_min_height)
{
  g_debug ("neuland_chat_widget_new");
  g_return_val_if_fail (NEULAND_IS_CONTACT (contact), NULL);
  g_return_val_if_fail (NEULAND_IS_TOX (ntox), NULL);
  NeulandChatWidget *ncw = NEULAND_CHAT_WIDGET (g_object_new (NEULAND_TYPE_CHAT_WIDGET, NULL));
  NeulandChatWidgetPrivate *priv = ncw->priv;
  priv->ntox = g_object_ref (ntox);
  priv->contact = contact;
  g_object_connect (contact,
                    "signal::notify::is-typing", neuland_chat_widget_is_typing_cb, ncw,
                    "signal::notify::connected", on_connected_changed, ncw,
                    "swapped-signal::incoming-message", on_incoming_message_cb, ncw,
                    "swapped-signal::incoming-action", on_incoming_action_cb, ncw,
                    "swapped-signal::outgoing-message", on_outgoing_message_cb, ncw,
                    "swapped-signal::outgoing-action", on_outgoing_action_cb, ncw,
                    NULL);

  gtk_widget_set_size_request (GTK_WIDGET (priv->entry_text_view), -1, text_entry_min_height);
  neuland_contact_set_has_chat_widget (contact, TRUE);

  return GTK_WIDGET (ncw);
}
