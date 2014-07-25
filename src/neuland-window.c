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

#include "neuland-window.h"
#include "neuland-contact.h"
#include "neuland-contact-widget.h"
#include "neuland-chat-widget.h"
#include "neuland-add-dialog.h"

struct _NeulandWindowPrivate
{
  NeulandTox      *tox;
  GtkWidget       *me_button;
  GtkWidget       *me_widget;
  GtkListBox      *contacts_list_box;
  GtkHeaderBar    *right_header_bar;
  GtkHeaderBar    *left_header_bar;
  GtkStack        *chat_stack;
  GHashTable      *contact_widgets;
  GHashTable      *chat_widgets;
  GtkButton       *requests_button;
  GtkLabel        *pending_requests_label;
  GtkButton       *gear_button;
  GtkButton       *accept_button;
  GtkButton       *discard_button;
  GtkToggleButton *select_button;
  gint             me_button_height;
  GtkWidget       *welcome_widget;
  GtkWidget       *tox_id_label;
  GBinding        *name_binding;
  GBinding        *status_binding;
};

G_DEFINE_TYPE_WITH_PRIVATE (NeulandWindow, neuland_window, GTK_TYPE_APPLICATION_WINDOW)

enum {
  PROP_0,
  PROP_NEULAND_TOX,
  PROP_N
};

static GParamSpec *window_properties[PROP_N] = {NULL, };

void
contacts_list_box_header_func (GtkListBoxRow  *row,
                               GtkListBoxRow  *before,
                               gpointer    user_data)
{
  GtkWidget *current;

  if (before == NULL)
    return;

  current = gtk_list_box_row_get_header (row);
  if (current == NULL)
    {
      current = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_show (current);
      gtk_list_box_row_set_header (row, current);
    }
}


static GtkWidget *
neuland_window_get_chat_widget_for_contact (NeulandWindow *window,
                                            NeulandContact *contact)
{
  NeulandWindowPrivate *priv = window->priv;
  GtkWidget *chat_widget = GTK_WIDGET (g_hash_table_lookup (priv->chat_widgets, contact));
  if (chat_widget != NULL)
    return chat_widget;

  g_debug ("Creating new chat widget for contact %p", contact);
  chat_widget = neuland_chat_widget_new (priv->tox, contact, priv->me_button_height);
  g_hash_table_insert (priv->chat_widgets, contact, chat_widget);
  gtk_container_add (GTK_CONTAINER (priv->chat_stack), chat_widget);

  return chat_widget;
}

/* Show or hide Accept and Discard buttons and set the chat widget's
   sensitivity depending on whether the currently active contact is a
   pending request or not. */
static void
neuland_window_update_request_mode (NeulandWindow *window)
{
  g_debug ("neuland_window_update_request_mode");

  NeulandWindowPrivate *priv = window->priv;
  NeulandContact *contact = neuland_window_get_active_contact (window);
  GtkWidget *chat_widget = neuland_window_get_chat_widget_for_contact (window, contact);
  gboolean is_request = neuland_contact_is_request (contact);

  gtk_widget_set_visible (GTK_WIDGET (priv->accept_button), is_request);
  gtk_widget_set_visible (GTK_WIDGET (priv->discard_button), is_request);
  gtk_widget_set_sensitive (GTK_WIDGET (chat_widget), !is_request);
}


static void
neuland_window_show_chat_for_contact (NeulandWindow *window,
                                      NeulandContact *contact)
{
  g_return_if_fail (NEULAND_IS_WINDOW (window));
  g_return_if_fail (NEULAND_IS_CONTACT (contact));

  NeulandWindowPrivate *priv = window->priv;

  g_debug ("Showing chat for contact: '%s' (%p)", neuland_contact_get_name (contact), contact);

  GtkWidget *chat_widget = GTK_WIDGET (g_hash_table_lookup (priv->chat_widgets, contact));

  if (chat_widget == NULL)
    chat_widget = neuland_window_get_chat_widget_for_contact (window, contact);

  gtk_stack_set_visible_child (GTK_STACK (priv->chat_stack), chat_widget);
  neuland_contact_reset_unread_messages (contact);

  // Destroy old bindings
  g_clear_object (&priv->name_binding);
  g_clear_object (&priv->status_binding);
  // Set up new bindings
  priv->name_binding = g_object_bind_property (contact, "name",
                                               priv->right_header_bar, "title",
                                               G_BINDING_SYNC_CREATE);
  priv->status_binding = g_object_bind_property (contact, "status-message",
                                                 priv->right_header_bar, "subtitle",
                                                 G_BINDING_SYNC_CREATE);
  /* If the contact doesn't have a name yet, show the Tox ID instead. */
  if (strlen(neuland_contact_get_name (contact)) == 0)
    gtk_header_bar_set_title (priv->right_header_bar, neuland_contact_get_tox_id_hex (contact));

  neuland_window_update_request_mode (window);
}

/* Returns the contact whose chat widget is currently visible in window.
   TODO: Maybe active-contact should be a property? */
NeulandContact *
neuland_window_get_active_contact (NeulandWindow *window)
{
  NeulandWindowPrivate *priv = window->priv;
  GtkStack *chat_stack = priv->chat_stack;
  GtkWidget *active_chat_widget = gtk_stack_get_visible_child (chat_stack);

  return neuland_chat_widget_get_contact (NEULAND_CHAT_WIDGET (active_chat_widget));
}

static void
contacts_list_box_row_activated_cb (NeulandWindow *window,
                                    GtkListBoxRow *row,
                                    gpointer user_data)
{
  NeulandContactWidget *widget = NEULAND_CONTACT_WIDGET (gtk_bin_get_child (GTK_BIN (row)));
  NeulandContact *contact = neuland_contact_widget_get_contact (widget);

  neuland_window_show_chat_for_contact (window, contact);
}

static void
neuland_window_on_ensure_chat_widget (NeulandWindow *window,
                                      gpointer user_data)
{
  g_debug ("neuland_window_on_ensure_chat_widget");
  NeulandContact *contact = NEULAND_CONTACT (user_data);
  GtkWidget *chat_widget = neuland_window_get_chat_widget_for_contact (window, contact);
}

static void
neuland_window_on_incoming_message_cb (NeulandWindow *window,
                                       gchar *message,
                                       gpointer user_data)
{
  NeulandWindowPrivate *priv = window->priv;
  GtkStack *chat_stack = priv->chat_stack;
  NeulandContact *contact = NEULAND_CONTACT (user_data);
  GtkWidget *active_chat_widget = gtk_stack_get_visible_child (chat_stack);
  GtkWidget *chat_widget = neuland_window_get_chat_widget_for_contact (window, contact);

  if (chat_widget != active_chat_widget)
    neuland_contact_increase_unread_messages (contact);

}

static void
neuland_window_on_incoming_action_cb (NeulandWindow *window,
                                      gchar         *action,
                                      gpointer       user_data)
{
  NeulandWindowPrivate *priv = window->priv;
  GtkStack *chat_stack = priv->chat_stack;
  NeulandContact *contact = NEULAND_CONTACT (user_data);
  GtkWidget *active_chat_widget = gtk_stack_get_visible_child (chat_stack);
  GtkWidget *chat_widget = neuland_window_get_chat_widget_for_contact (window, contact);

  if (chat_widget != active_chat_widget)
    neuland_contact_increase_unread_messages (contact);
}

static void
neuland_window_activate_first_contact (NeulandWindow *window)
{
  NeulandWindowPrivate *priv = window->priv;
  GtkListBoxRow *first_row = gtk_list_box_get_row_at_index (priv->contacts_list_box, 0);

  if (first_row)
    {
      gtk_list_box_select_row (priv->contacts_list_box, first_row);
      contacts_list_box_row_activated_cb (window, first_row, NULL);
    }
}

static void
neuland_window_add_contact (NeulandWindow *window, NeulandContact *contact)
{
  g_message ("Adding contact %p to NeulandWindow %p", contact, window);
  NeulandWindowPrivate *priv = window->priv;

  g_object_connect (contact,
                    "swapped-signal::ensure-chat-widget",
                    neuland_window_on_ensure_chat_widget, window,
                    "swapped-signal::incoming-message",
                    neuland_window_on_incoming_message_cb, window,
                    "swapped-signal::incoming-action",
                    neuland_window_on_incoming_action_cb, window,
                    NULL);

  GtkWidget *contact_widget = neuland_contact_widget_new (contact);

  g_hash_table_insert (priv->contact_widgets, contact, contact_widget);
  gtk_list_box_insert (priv->contacts_list_box, contact_widget, -1);
}

static void
neuland_window_remove_contact (NeulandWindow *window, NeulandContact *contact)
{
  g_message ("Removing contact %p from NeulandWindow %p", contact, window);
  g_return_if_fail (NEULAND_IS_WINDOW (window));
  g_return_if_fail (NEULAND_IS_CONTACT (contact));

  NeulandWindowPrivate *priv = window->priv;

  /* Destroy the widgets related to contact */
  GtkWidget *contact_widget = GTK_WIDGET (g_hash_table_lookup (priv->contact_widgets, contact));
  GtkWidget *chat_widget = GTK_WIDGET (g_hash_table_lookup (priv->chat_widgets, contact));
  GtkListBoxRow *row = GTK_LIST_BOX_ROW (gtk_widget_get_parent (contact_widget));

  gtk_widget_destroy (GTK_WIDGET (row));
  if (chat_widget)
    {
      g_hash_table_remove (priv->chat_widgets, contact);
      gtk_widget_destroy (chat_widget);
    }

  g_hash_table_remove (priv->contact_widgets, contact);

  neuland_window_activate_first_contact (window);
}

static void
neuland_window_show_welcome_widget (NeulandWindow *window)
{
  NeulandWindowPrivate *priv = window->priv;
  gtk_stack_set_visible_child (priv->chat_stack, priv->welcome_widget);
}

static void
neuland_window_load_contacts (NeulandWindow *window)
{
  g_debug ("neuland_window_load_contacts ...");

  NeulandWindowPrivate *priv = window->priv;
  if (priv->tox == NULL)
    return;

  GList *contacts;
  contacts = neuland_tox_get_contacts (priv->tox);
  if (contacts == NULL) {
    g_debug ("NeulandTox instance %p has no contacts", priv->tox);
    return;
  }
  g_debug ("NeulandTox instance %p has %i contacts in list", priv->tox,
           g_list_length (contacts));

  GList *l;
  for (l = contacts; l != NULL; l = l->next)
    {
      NeulandContact *contact = NEULAND_CONTACT (l->data);
      neuland_window_add_contact (window, contact);
    }

  /*
   * /\* add separator after last row as well *\/
   * GtkWidget *dummy_label = gtk_widget_new (GTK_TYPE_LABEL,
   *                                          "visible", FALSE,
   *                                          NULL);
   * gtk_list_box_insert (priv->contacts_list_box,
   *                      dummy_label, -1);
   * gtk_widget_set_sensitive (gtk_widget_get_parent (dummy_label), FALSE);
   * gtk_widget_set_opacity (gtk_widget_get_parent (dummy_label), 0);
   */

  if (g_list_length (contacts) > 0)
    neuland_window_activate_first_contact (window);
  //DEBUG:
  //g_message ("unreffing contact: %s\n", neuland_contact_get_name (contacts->data));
  //g_object_unref (contacts->data);
}

static void
on_name_change_cb (GObject *object,
                   GParamSpec *pspec,
                   gpointer user_data)
{
  NeulandTox *tox = NEULAND_TOX (object);
  NeulandWindow *window = NEULAND_WINDOW (user_data);

  neuland_contact_widget_set_name (NEULAND_CONTACT_WIDGET
                                   (window->priv->me_widget),
                                   neuland_tox_get_name (tox));
}

static void
on_status_message_change_cb (GObject *object,
                             GParamSpec *pspec,
                             gpointer user_data)
{
  NeulandTox *tox = NEULAND_TOX (object);
  NeulandWindow *window = NEULAND_WINDOW (user_data);

  neuland_contact_widget_set_status_message (NEULAND_CONTACT_WIDGET
                                             (window->priv->me_widget),
                                             neuland_tox_get_status_message (tox));
}

static void
on_contact_add_cb (NeulandWindow *window,
                     GObject *gobject,
                     gpointer user_data)
{
  NeulandContact *contact = NEULAND_CONTACT (gobject);

  neuland_window_add_contact (window, contact);
}

static void
on_contact_remove_cb (NeulandWindow *window,
                       GObject *gobject,
                       gpointer user_data)
{
  NeulandContact *contact = NEULAND_CONTACT (gobject);

  neuland_window_remove_contact (window, contact);
}

static void
on_pending_requests_cb (NeulandWindow *window,
                        GObject *gobject,
                        gpointer user_data)
{
  NeulandWindowPrivate *priv = window->priv;
  NeulandTox *tox = priv->tox;

  gtk_widget_set_visible (GTK_WIDGET (priv->requests_button),
                          neuland_tox_get_pending_requests (tox) > 0);
}


static void
neuland_window_set_tox (NeulandWindow *window, NeulandTox *tox)
{
  g_debug ("neuland_window_set_tox");
  g_return_if_fail (NEULAND_IS_WINDOW (window));
  g_return_if_fail (NEULAND_IS_TOX (tox));

  NeulandWindowPrivate *priv = window->priv;
  priv->tox = g_object_ref (tox);
  neuland_window_load_contacts (window);
  gchar *id;

  g_object_get (priv->tox, "tox-id", &id, NULL);
  g_message ("Tox ID for window %p: %s", window, id);
  gtk_label_set_text (GTK_LABEL (priv->tox_id_label), id);

  g_object_bind_property (tox, "pending-requests", priv->pending_requests_label, "label", 0);

  g_object_connect (tox,
                    "signal::notify::self-name", on_name_change_cb, window,
                    "signal::notify::status-message", on_status_message_change_cb, window,
                    "swapped-signal::contact-add", on_contact_add_cb, window,
                    "swapped-signal::contact-remove", on_contact_remove_cb, window,
                    "swapped-signal::notify::pending-requests", on_pending_requests_cb, window,
                    NULL);
  neuland_contact_widget_set_name (NEULAND_CONTACT_WIDGET (priv->me_widget),
                                   neuland_tox_get_name (tox));
  neuland_contact_widget_set_message (NEULAND_CONTACT_WIDGET (priv->me_widget),
                                      neuland_tox_get_status_message (tox));

}

static void
neuland_window_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  g_debug ("neuland_window_set_property");
  NeulandWindow *window = NEULAND_WINDOW (object);

  switch (property_id)
    {
    case PROP_NEULAND_TOX:
      neuland_window_set_tox (window, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
neuland_window_get_property  (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  g_debug ("neuland_window_get_property");

  NeulandWindow *window = NEULAND_WINDOW (object);

  switch (property_id)
    {
    case PROP_NEULAND_TOX:
      neuland_window_set_tox (window, NEULAND_TOX (g_value_get_object (value)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

void
set_selection_mode (GtkWidget *widget, gboolean *data)
{
  GtkWidget *child = gtk_bin_get_child (GTK_BIN (widget));
  if (NEULAND_IS_CONTACT_WIDGET (child))
    neuland_contact_widget_show_check_box (NEULAND_CONTACT_WIDGET (child), *data);
}

static void
neuland_window_selection_state_changed (GSimpleAction *action,
                                        GVariant      *parameter,
                                        gpointer       user_data)
{
  g_debug ("neuland_window_selection_state_changed");
  NeulandWindow *window = NEULAND_WINDOW (user_data);
  GtkListBox *contacts_list_box = window->priv->contacts_list_box;
  GtkStyleContext *context_left =
    gtk_widget_get_style_context (GTK_WIDGET (window->priv->left_header_bar));
  GtkStyleContext *context_right =
    gtk_widget_get_style_context (GTK_WIDGET (window->priv->right_header_bar));

  gboolean selection_enabled = g_variant_get_boolean (parameter);
  g_debug (selection_enabled ? "selection enabled" : "selection disabled");

  gtk_container_foreach (GTK_CONTAINER (contacts_list_box),
                         (GtkCallback) set_selection_mode, &selection_enabled);

  if (selection_enabled)
    {
      gtk_style_context_add_class (context_left, "selection-mode");
      gtk_style_context_add_class (context_right, "selection-mode");
    }
  else
    {
      gtk_style_context_remove_class (context_left, "selection-mode");
      gtk_style_context_remove_class (context_right, "selection-mode");
    }
  g_simple_action_set_state (action, parameter);
}

void
neuland_window_change_status (GSimpleAction *action,
                              GVariant      *parameter,
                              gpointer       user_data)
{
  NeulandWindow *window = NEULAND_WINDOW (user_data);
  NeulandContactStatus status = (NeulandContactStatus)g_variant_get_int32 (parameter);
  g_debug ("Changing own status to %i", status);

  neuland_tox_set_status (window->priv->tox, status);
  neuland_contact_widget_set_status (NEULAND_CONTACT_WIDGET (window->priv->me_widget), status);
  g_simple_action_set_state (action, parameter);
}

static void
on_add_dialog_response (GtkDialog *dialog,
                        gint response_id,
                        gpointer user_data)
{
  NeulandWindow *window = NEULAND_WINDOW (user_data);
  NeulandWindowPrivate *priv = window->priv;
  NeulandTox *tox = priv->tox;
  char *tox_id = g_strdup (neuland_add_dialog_get_tox_id (NEULAND_ADD_DIALOG (dialog)));
  char *message = g_strdup (neuland_add_dialog_get_message (NEULAND_ADD_DIALOG (dialog)));
  gtk_widget_destroy (GTK_WIDGET (dialog));

  if (response_id == GTK_RESPONSE_OK)
    neuland_tox_add_contact_from_hex_address (priv->tox, tox_id, message);

  g_free (tox_id);
  g_free (message);
}

static void
activate_add_contact (GSimpleAction *action,
                      GVariant *parameter,
                      gpointer user_data)
{
  NeulandWindow *window = NEULAND_WINDOW (user_data);
  GtkWidget *add_dialog = neuland_add_dialog_new ();
  gtk_window_set_transient_for (GTK_WINDOW (add_dialog), GTK_WINDOW (window));

  g_signal_connect (add_dialog, "response", G_CALLBACK (on_add_dialog_response), window);

  gtk_widget_show_all (add_dialog);
}

static void
activate_accept_request (GSimpleAction *action,
                         GVariant *parameter,
                         gpointer user_data)
{
  NeulandWindow *window = NEULAND_WINDOW (user_data);
  NeulandContact *contact = neuland_window_get_active_contact (window);
  NeulandTox *tox = window->priv->tox;

  neuland_tox_accept_contact_request (tox, contact);
  neuland_window_update_request_mode (window);
}

static void
activate_discard_request (GSimpleAction *action,
                          GVariant *parameter,
                          gpointer user_data)
{
  NeulandWindow *window = NEULAND_WINDOW (user_data);
  NeulandWindowPrivate *priv = window->priv;
  NeulandContact *contact = neuland_window_get_active_contact (window);

  neuland_tox_remove_contact (priv->tox, contact);
}

/* Filter function for showing only existing contacts */
static gboolean
contacts_list_box_filter_func_contacts (GtkListBoxRow *row,
                                        gpointer user_data)
{
  NeulandWindow *window = NEULAND_WINDOW (user_data);
  NeulandContactWidget *contact_widget =
    NEULAND_CONTACT_WIDGET (gtk_bin_get_child (GTK_BIN (row)));
  NeulandContact *contact = neuland_contact_widget_get_contact (contact_widget);

  return !neuland_contact_is_request (contact);
}

/* Filter function for showing only open friend requests */
static gboolean
contacts_list_box_filter_func_requests (GtkListBoxRow *row,
                                        gpointer user_data)
{
  NeulandWindow *window = NEULAND_WINDOW (user_data);
  NeulandContactWidget *contact_widget =
    NEULAND_CONTACT_WIDGET (gtk_bin_get_child (GTK_BIN (row)));
  NeulandContact *contact = neuland_contact_widget_get_contact (contact_widget);

  return neuland_contact_is_request (contact);
}

static void
neuland_window_show_requests_state_changed (GSimpleAction *action,
                                            GVariant *parameter,
                                            gpointer user_data)
{
  NeulandWindow *window = NEULAND_WINDOW (user_data);
  gboolean show_requests = g_variant_get_boolean (parameter);
  GtkListBox *list_box = window->priv->contacts_list_box;

  if (show_requests)
    gtk_list_box_set_filter_func (list_box, contacts_list_box_filter_func_requests,
                                  window, NULL);
  else
    gtk_list_box_set_filter_func (list_box, contacts_list_box_filter_func_contacts,
                                  window, NULL);

  gtk_list_box_invalidate_filter (list_box);
  g_simple_action_set_state (action, parameter);
}

static GActionEntry win_entries[] = {
  { "change-status", NULL, "i", "0", neuland_window_change_status },
  { "selection-state", NULL, NULL, "false", neuland_window_selection_state_changed },
  { "add-contact", activate_add_contact },
  { "accept-request", activate_accept_request },
  { "discard-request", activate_discard_request },
  { "show-requests", NULL, NULL, "false", neuland_window_show_requests_state_changed }
};

static void
neuland_window_dispose (GObject *object)
{
  g_debug ("neuland_window_dispose ...");
  NeulandWindow *window = NEULAND_WINDOW (object);
  g_clear_object (&window->priv->tox);

  G_OBJECT_CLASS (neuland_window_parent_class)->dispose (object);
}

static void
neuland_window_finalize (GObject *object)
{
  g_debug ("neuland_window_finalize");
  NeulandWindow *window = NEULAND_WINDOW (object);
  NeulandWindowPrivate *priv = window->priv;

  g_hash_table_destroy (priv->contact_widgets);
  g_hash_table_destroy (priv->chat_widgets);

  G_OBJECT_CLASS (neuland_window_parent_class)->finalize (object);
}

static void
neuland_window_class_init (NeulandWindowClass *klass)
{
  g_debug ("neuland_window_class_init");
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/tox/neuland/neuland-window.ui");

  gtk_widget_class_bind_template_child_private (widget_class, NeulandWindow, me_button);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandWindow, contacts_list_box);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandWindow, chat_stack);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandWindow, right_header_bar);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandWindow, left_header_bar);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandWindow, gear_button);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandWindow, select_button);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandWindow, accept_button);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandWindow, discard_button);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandWindow, requests_button);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandWindow, pending_requests_label);

  gtk_widget_class_bind_template_callback(widget_class, contacts_list_box_row_activated_cb);

  gobject_class->set_property = neuland_window_set_property;
  gobject_class->get_property = neuland_window_get_property;
  gobject_class->dispose = neuland_window_dispose;
  gobject_class->finalize = neuland_window_finalize;

  window_properties[PROP_NEULAND_TOX] =
    g_param_spec_object ("neuland-tox",
                         "NeulandTox",
                         "The tox instance represented by the window",
                         NEULAND_TYPE_TOX,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (gobject_class,
                                     PROP_N,
                                     window_properties);
}

static void
neuland_window_init (NeulandWindow *window)
{
  g_debug ("neuland_window_init");
  GtkBuilder *builder;

  gtk_widget_init_template (GTK_WIDGET (window));
  window->priv = neuland_window_get_instance_private (window);

  NeulandWindowPrivate *priv = window->priv;
  priv->contact_widgets = g_hash_table_new (NULL, NULL);
  priv->chat_widgets = g_hash_table_new (NULL, NULL);

  builder = gtk_builder_new_from_resource ("/org/tox/neuland/neuland-window-menu.ui");
  gtk_builder_add_from_resource (builder, "/org/tox/neuland/neuland-me-status-menu.ui", NULL);
  gtk_builder_add_from_resource (builder, "/org/tox/neuland/neuland-welcome-widget.ui", NULL);

  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (priv->gear_button),
                                  (GMenuModel*) gtk_builder_get_object (builder, "win-menu"));

  g_action_map_add_action_entries (G_ACTION_MAP (window), win_entries, G_N_ELEMENTS (win_entries), window);

  priv->welcome_widget = GTK_WIDGET (gtk_builder_get_object  (builder, "welcome-widget"));
  GtkWidget *tox_id_label = GTK_WIDGET (gtk_builder_get_object (builder, "tox_id_label"));
  priv->tox_id_label = tox_id_label;

  gtk_container_add (GTK_CONTAINER (priv->chat_stack), priv->welcome_widget);

  priv->me_widget = neuland_contact_widget_new (NULL);
  neuland_contact_widget_set_name (NEULAND_CONTACT_WIDGET (priv->me_widget), "...");
  neuland_contact_widget_set_status (NEULAND_CONTACT_WIDGET (priv->me_widget), NEULAND_CONTACT_STATUS_NONE);

  gtk_container_add (GTK_CONTAINER (priv->me_button), priv->me_widget);
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (priv->me_button),
                                  (GMenuModel*) gtk_builder_get_object (builder, "me-status-menu"));

  gtk_widget_get_preferred_height (priv->me_button, NULL, &priv->me_button_height);

  /* Set up list box for contacts */
  gtk_list_box_set_header_func (priv->contacts_list_box, contacts_list_box_header_func, NULL, NULL);
  gtk_list_box_set_filter_func (priv->contacts_list_box, contacts_list_box_filter_func_contacts, window, NULL);

  g_object_unref (G_OBJECT (builder));
}

GtkWidget *
neuland_window_new (NeulandTox *tox)
{
  NeulandWindow *window;

  g_debug ("window new ...");
  g_return_if_fail (NEULAND_IS_TOX (tox));
  window = g_object_new (NEULAND_TYPE_WINDOW,
                         "neuland-tox", tox,
                         NULL);
  return GTK_WIDGET (window);
}
