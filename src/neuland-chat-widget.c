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

#define MIN_TIME_INTERVAL (5 * G_TIME_SPAN_MINUTE)

typedef enum {
  DIRECTION_IN,
  DIRECTION_OUT,
} MessageDirection;

typedef enum {
  TYPE_TEXT,
  TYPE_ACTION,
} MessageType;

struct _NeulandChatWidgetPrivate {
  NeulandTox *ntox;
  NeulandContact *contact;

  GtkTextView *text_view;
  GtkTextBuffer *text_buffer;

  // owned by text_buffer, don't free in finalize!
  GtkTextMark *scroll_mark;
  GtkTextMark *tmp_time_string_start_mark;
  GtkTextMark *tmp_time_string_end_mark;

  GtkTextTag *contact_name_tag;
  GtkTextTag *my_name_tag;
  GtkTextTag *is_typing_tag;
  GtkTextTag *action_tag;
  GtkTextTag *timestamp_tag;

  GtkTextView *entry_text_view;
  GtkTextBuffer *entry_text_buffer;

  GtkInfoBar *info_bar;

  GDateTime *last_insert_time;
  MessageDirection last_direction;
  MessageType last_type;
};

G_DEFINE_TYPE_WITH_PRIVATE (NeulandChatWidget, neuland_chat_widget, GTK_TYPE_BOX)

static void
neuland_chat_widget_dispose (GObject *object)
{
  g_debug ("neuland_chat_widget_dispose (%p)", object);
  NeulandChatWidget *widget = NEULAND_CHAT_WIDGET (object);
  NeulandChatWidgetPrivate *priv = widget->priv;

  if (priv->contact != NULL)
    neuland_contact_set_show_typing (priv->contact, FALSE);

  g_clear_object (&priv->ntox);
  g_clear_object (&priv->contact);

  G_OBJECT_CLASS (neuland_chat_widget_parent_class)->dispose (object);
}
  
static void
neuland_chat_widget_finalize (GObject *object)
{
  g_debug ("neuland_chat_widget_finalize (%p)", object);
  NeulandChatWidget *widget = NEULAND_CHAT_WIDGET (object);

  if (widget->priv->last_insert_time != NULL)
    g_date_time_unref (widget->priv->last_insert_time);

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
neuland_chat_widget_set_show_contact_is_typing (NeulandChatWidget *self, gboolean is_typing)
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
      string = g_strdup_printf ("%s is typing...", name);
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
  GtkTextTag *name_tag;
  const gchar *name;
  gchar *time_string;
  gchar *tmp_time_string;
  gchar *prefix;
  gboolean insert_time_stamp;
  gboolean insert_nick;

  GDateTime *time_now = g_date_time_new_now_local ();

  /* Remove tmp detailed time stamp */
  GtkTextIter start_iter;
  GtkTextIter end_iter;
  gtk_text_buffer_get_iter_at_mark (priv->text_buffer,
                                    &start_iter,
                                    priv->tmp_time_string_start_mark);
  gtk_text_buffer_get_iter_at_mark (priv->text_buffer,
                                    &end_iter,
                                    priv->tmp_time_string_end_mark);
  gtk_text_buffer_delete (priv->text_buffer, &start_iter, &end_iter);


  switch (direction)
    {
    case DIRECTION_IN:
      name = neuland_contact_get_name (contact);
      name_tag = priv->contact_name_tag;
      break;
    case DIRECTION_OUT:
      name = neuland_tox_get_name (ntox);
      name_tag = priv->my_name_tag;
      break;
    }

  if (priv->last_insert_time == NULL)
    {
      insert_time_stamp = TRUE;
      insert_nick = TRUE;
    }
  else
    {
      GTimeSpan time_span = g_date_time_difference (time_now,
                                                    priv->last_insert_time);
      insert_time_stamp = (time_span >= MIN_TIME_INTERVAL)
        || direction != priv->last_direction;
      insert_nick = (direction != priv->last_direction)
        || (priv->last_type == TYPE_ACTION);
    }

  neuland_chat_widget_set_show_contact_is_typing (widget, FALSE);

  gtk_text_buffer_get_end_iter (text_buffer, &iter);

  if (insert_time_stamp)
    {
      time_string = g_date_time_format (time_now, "%R\n");
      gtk_text_buffer_insert_with_tags (text_buffer, &iter, time_string, -1,
                                        priv->timestamp_tag, NULL);

      if (priv->last_insert_time)
        g_date_time_unref (priv->last_insert_time);
      priv->last_insert_time = time_now;

      g_free (time_string);
    }

  if (type == TYPE_ACTION)
    {
      prefix = g_strdup_printf ("* %s %s", name, message);
      gtk_text_buffer_insert_with_tags (text_buffer, &iter, prefix, -1,
                                        priv->action_tag,
                                        NULL);
      g_free (prefix);
    }
  else
    {
      if (insert_nick)
        {
          prefix = g_strdup_printf ("%s: ", name);
          gtk_text_buffer_insert_with_tags (text_buffer, &iter, prefix, -1,
                                            name_tag, NULL);
          g_free (prefix);
        }

      gtk_text_buffer_insert (text_buffer, &iter, message, -1);
    }

  gtk_text_buffer_insert (text_buffer, &iter, "\n", -1);

  /* Insert a temporary, more detailed time stamp below the last
     message. It will move down with every new message.
  */
  tmp_time_string = g_date_time_format (time_now, "%X\n");
  gtk_text_buffer_move_mark (priv->text_buffer, priv->tmp_time_string_start_mark, &iter);

  gtk_text_buffer_insert_with_tags (text_buffer, &iter,
                                    tmp_time_string, -1, priv->timestamp_tag, NULL);
  gtk_text_buffer_move_mark (priv->text_buffer, priv->tmp_time_string_end_mark, &iter);
  g_free (tmp_time_string);

  if (direction == DIRECTION_OUT)
    neuland_chat_widget_set_show_contact_is_typing (widget, neuland_contact_get_is_typing (contact));

  priv->last_direction = direction;
  priv->last_type = type;
  neuland_chat_widget_scroll_to_bottom (widget);
}

static void
insert_message (NeulandChatWidget *widget,
                gchar* message,
                MessageDirection direction)
{
  insert_text (widget, message, direction, TYPE_TEXT);
}

static void
insert_action (NeulandChatWidget *widget,
               gchar* message,
               MessageDirection direction)
{
  insert_text (widget, message, direction, TYPE_ACTION);
}

static void
on_outgoing_message_cb (NeulandChatWidget *widget,
                        gchar *message,
                        gpointer user_data)
{
  g_debug ("on_outgoing_message_cb");
  insert_message (widget, message, DIRECTION_OUT);
}

static void
on_outgoing_action_cb (NeulandChatWidget *widget,
                       gchar *action,
                       gpointer user_data)
{
  g_debug ("on_outgoing_action_cb");
  insert_action (widget, action, DIRECTION_OUT);
}

static void
on_incoming_message_cb (NeulandChatWidget *widget,
                        gchar *message,
                        gpointer user_data)
{
  g_debug ("on_incoming_message_cb");
  insert_message (widget, message, DIRECTION_IN);
}

static void
on_incoming_action_cb (NeulandChatWidget *widget,
                       gchar *action,
                       gpointer user_data)
{
  g_debug ("on_incoming_action_cb");
  insert_action (widget, action, DIRECTION_IN);
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
      neuland_contact_set_show_typing (widget->priv->contact, FALSE);
      neuland_chat_widget_process_input (widget);
      return TRUE;
    }

  return FALSE;
}

static void
entry_text_buffer_changed_cb (NeulandChatWidget *widget,
                              gpointer user_data)
{
  NeulandChatWidgetPrivate *priv = widget->priv;
  GtkTextBuffer *buffer = priv->entry_text_buffer;
  NeulandContact *contact = priv->contact;
  GtkTextIter iter_start;
  GtkTextIter iter_end;

  gtk_text_buffer_get_start_iter (buffer, &iter_start);
  gtk_text_buffer_get_iter_at_offset (buffer, &iter_end, 4);

  gchar *text = gtk_text_buffer_get_text (buffer, &iter_start, &iter_end, FALSE);

  if (strlen (text) > 0 && (!g_str_has_prefix (text, "/")
                            || g_str_has_prefix (text, "//") /* quoted commands are messages */
                            || g_str_has_prefix (text, "/me ")))
    /* Typing a message or a command that results in a message or
       action being send */
    neuland_contact_set_show_typing (contact, TRUE);
  else
    /* Typing a command; don't show that we are typing */
    neuland_contact_set_show_typing (contact, FALSE);
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
  gtk_widget_class_bind_template_child_private (widget_class, NeulandChatWidget, timestamp_tag);
  gtk_widget_class_bind_template_callback (widget_class, entry_text_view_key_press_event_cb);
  gtk_widget_class_bind_template_callback (widget_class, entry_text_buffer_changed_cb);

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
  priv->tmp_time_string_start_mark =
    gtk_text_buffer_create_mark (priv->text_buffer, NULL, &iter, TRUE);
  priv->tmp_time_string_end_mark =
    gtk_text_buffer_create_mark (priv->text_buffer, NULL, &iter, TRUE);
  priv->last_insert_time = NULL;
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
  neuland_chat_widget_set_show_contact_is_typing (chat_widget, is_typing);
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
     tries to send a message. */
  if (connected)
    {
      neuland_chat_widget_show_offline_info (widget, FALSE);
      insert_action (widget, "is now online", DIRECTION_IN);
    }
  else
    insert_action (widget, "is now offline", DIRECTION_IN);
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
