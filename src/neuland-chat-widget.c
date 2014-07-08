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
  INCOMING,
  OUTGOING
} MessageDirection;

typedef enum {
  TEXT,
  ACTION
} MessageType;



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
insert_text (NeulandChatWidget *widget,
             gchar* message,
             MessageDirection direction,
             MessageType type)
{
  NeulandChatWidgetPrivate *priv = widget->priv;
  GtkTextBuffer *text_buffer = priv->text_buffer;
  NeulandTox *ntox = priv->ntox;
  NeulandContact *contact = priv->contact;
  GtkTextIter iter;
  const gchar *name;
  gchar *prefix;
  GtkTextTag *name_tag;

  switch (direction)
    {
    case INCOMING:
      name = neuland_contact_get_name (contact);
      name_tag = priv->contact_name_tag;
      break;
    case OUTGOING:
      name = neuland_tox_get_name (ntox);
      name_tag = priv->my_name_tag;
      break;
    }

  neuland_chat_widget_set_is_typing (widget, FALSE);

  gtk_text_buffer_get_end_iter (text_buffer, &iter);
  if (type == ACTION)
    {
      prefix = g_strdup_printf ("* %s %s", name, message);
      gtk_text_buffer_insert_with_tags (text_buffer, &iter, prefix, -1,
                                        priv->action_tag,
                                        NULL);
    }
  else
    {
      prefix = g_strdup_printf ("%s: ", name);
      gtk_text_buffer_insert_with_tags (text_buffer, &iter, prefix, -1,
                                        name_tag, NULL);
      gtk_text_buffer_insert (text_buffer, &iter, message, -1);
    }

  g_free (prefix);
  gtk_text_buffer_insert (text_buffer, &iter, "\n", -1);
  neuland_chat_widget_scroll_to_bottom (widget);
}

static void
insert_message (NeulandChatWidget *widget,
                gchar* message,
                MessageDirection direction)
{
  insert_text (widget, message, direction, TEXT);
}

static void
insert_action (NeulandChatWidget *widget,
               gchar* message,
               MessageDirection direction)
{
  insert_text (widget, message, direction, ACTION);
}

static void
on_outgoing_message_cb (NeulandChatWidget *widget,
                        gchar *message,
                        gpointer user_data)
{
  g_debug ("on_outgoing_message_cb");
  insert_message (widget, message, OUTGOING);
}

static void
on_outgoing_action_cb (NeulandChatWidget *widget,
                       gchar *action,
                       gpointer user_data)
{
  g_debug ("on_outgoing_action_cb");
  insert_action (widget, action, OUTGOING);
}

static void
on_incoming_message_cb (NeulandChatWidget *widget,
                        gchar *message,
                        gpointer user_data)
{
  g_debug ("on_incoming_message_cb");
  insert_message (widget, message, INCOMING);
}

static void
on_incoming_action_cb (NeulandChatWidget *widget,
                       gchar *action,
                       gpointer user_data)
{
  g_debug ("on_incoming_action_cb");
  insert_action (widget, action, INCOMING);
}

static gboolean
hide_offline_info (gpointer user_data)
{
  NeulandChatWidget *widget = NEULAND_CHAT_WIDGET (user_data);
  gtk_widget_set_visible (GTK_WIDGET (widget->priv->info_bar),
                          FALSE);

  return G_SOURCE_REMOVE;
}

static void
neuland_chat_widget_show_offline_info (NeulandChatWidget *self,
                                       gboolean show)
{
  static gint id;
  gtk_widget_set_visible (GTK_WIDGET (self->priv->info_bar),
                          show);

  g_source_remove_by_user_data (self);
  if (show)
    g_timeout_add_seconds (6, hide_offline_info, self);
}

static void
neuland_chat_widget_process_input (NeulandChatWidget *widget)
{
  NeulandChatWidgetPrivate *priv = widget->priv;
  gboolean connected = neuland_contact_get_connected (priv->contact);
  gboolean clear_entry = TRUE;
  GtkTextIter start_iter;
  GtkTextIter end_iter;
  gchar *string;

  gtk_text_buffer_get_bounds (priv->entry_text_buffer, &start_iter, &end_iter);
  string = gtk_text_buffer_get_text (priv->entry_text_buffer, &start_iter, &end_iter, FALSE);

  /* String is empty */
  if (strlen (string) == 0) {
    g_debug ("Ignoring empty message.");
  }
  /* String contains Commands */
  else if (g_ascii_strncasecmp (string, "/", 1) == 0 &&
           g_ascii_strncasecmp (string, "//", 2) != 0)
    {
      if (g_ascii_strncasecmp (string, "/me ", 4) == 0)
        {
          g_debug ("/me command recognized");
          if (connected)
            neuland_contact_send_action (priv->contact, string+4);
          else
            {
              neuland_chat_widget_show_offline_info (widget, TRUE);
              clear_entry = FALSE;
            }
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
      else
        g_message ("Unknown command: %s", string);
    }
  /* String contains normal message */
  else if (connected)
    if (g_ascii_strncasecmp (string, "//", 2) == 0)
      neuland_contact_send_message (priv->contact, string+1);
    else
      neuland_contact_send_message (priv->contact, string);
  else
    {
      neuland_chat_widget_show_offline_info (widget, TRUE);
      clear_entry = FALSE;
    }

  if (clear_entry)
    gtk_text_buffer_delete (priv->entry_text_buffer, &start_iter, &end_iter);

  g_free (string);
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
      neuland_chat_widget_process_input (widget);
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
    {
      neuland_chat_widget_show_offline_info (widget, FALSE);
      insert_action (widget, "is now online", INCOMING);
    }
  else
    insert_action (widget, "is now offline", INCOMING);
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
