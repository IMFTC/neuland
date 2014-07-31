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
#include "neuland-contact-row.h"
#include "neuland-chat-widget.h"
#include "neuland-add-dialog.h"

struct _NeulandWindowPrivate
{
  NeulandTox      *tox;
  GtkWidget       *me_button;
  GtkWidget       *me_widget;
  GtkWidget       *scrolled_window_contacts;
  GtkWidget       *scrolled_window_requests;
  GtkListBox      *contacts_list_box;
  GtkListBox      *requests_list_box;
  GtkHeaderBar    *right_header_bar;
  GtkHeaderBar    *left_header_bar;
  GtkStack        *chat_stack;
  GtkStack        *side_pane_stack;
  GtkLabel        *pending_requests_label;
  GtkButton       *gear_button;

  GtkRevealer     *requests_button_revealer;
  GtkRevealer     *discard_button_revealer;
  GtkRevealer     *accept_button_revealer;

  GtkToggleButton *select_button;
  gint             me_button_height;
  GtkWidget       *welcome_widget;
  GtkLabel        *welcome_widget_tox_id_label;

  GtkWidget       *request_widget;
  GtkLabel        *request_widget_tox_id_label;
  GtkTextBuffer   *request_widget_text_buffer;

  GHashTable      *contact_row_widgets;
  GHashTable      *chat_widgets;
  GHashTable      *selected_contacts;

  GBinding        *name_binding;
  GBinding        *status_binding;

  /* Remember the selected contact/request when we toggle between
     contacts and requests. */
  NeulandContact  *active_contact;
  NeulandContact  *active_request;

  GtkActionBar    *action_bar;
};

G_DEFINE_TYPE_WITH_PRIVATE (NeulandWindow, neuland_window, GTK_TYPE_APPLICATION_WINDOW)

enum {
  PROP_0,
  PROP_NEULAND_TOX,
  PROP_N
};

static GParamSpec *window_properties[PROP_N] = {NULL, };

void
list_box_header_func (GtkListBoxRow  *row,
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

static NeulandContact *
neuland_window_get_contact_from_row (NeulandWindow *window,
                                     GtkListBoxRow *row)
{
  g_return_if_fail (NEULAND_IS_WINDOW (window));
  g_return_if_fail (GTK_IS_LIST_BOX_ROW (row));

  return neuland_contact_row_get_contact (NEULAND_CONTACT_ROW (row));
}


static GtkWidget *
neuland_window_get_contact_row_for_contact (NeulandWindow *window,
                                            NeulandContact *contact)
{
  GHashTable *contact_row_widgets = window->priv->contact_row_widgets;
  GtkWidget *widget = GTK_WIDGET (g_hash_table_lookup (contact_row_widgets, contact));
  return widget;
}

static void
neuland_window_show_welcome_widget (NeulandWindow *window)
{
  NeulandWindowPrivate *priv = window->priv;
  gtk_stack_set_visible_child (priv->chat_stack, priv->welcome_widget);
  gtk_header_bar_set_title (priv->right_header_bar, "Neuland");
}

/* This shows the 'chat widget' (chat widget or request widget) for
   @contact in the right part (priv->chat_stack) and hooks up the
   window title above it. Notice that the visibility of the
   Accept/Discard buttons is controlled by the state of the
   "show-request" action and not by this function. */
static void
neuland_window_show_chat_for_contact (NeulandWindow *window,
                                      NeulandContact *contact)
{
  g_return_if_fail (NEULAND_IS_WINDOW (window));
  g_return_if_fail (NEULAND_IS_CONTACT (contact));

  NeulandWindowPrivate *priv = window->priv;

  // Destroy old bindings
  g_clear_object (&priv->name_binding);
  g_clear_object (&priv->status_binding);
  // Set up new bindings

  if (contact)
    {
      priv->name_binding = g_object_bind_property (contact, "preferred-name",
                                                   priv->right_header_bar, "title",
                                                   G_BINDING_SYNC_CREATE);
      priv->status_binding = g_object_bind_property (contact, "status-message",
                                                     priv->right_header_bar, "subtitle",
                                                     G_BINDING_SYNC_CREATE);
      if (neuland_contact_is_request (contact))
        {
          /* All requests share this single requests widget. */
          gtk_label_set_label (priv->request_widget_tox_id_label,
                               neuland_contact_get_tox_id_hex (contact));
          gtk_text_buffer_set_text (priv->request_widget_text_buffer,
                                    neuland_contact_get_request_message (contact), -1);
          gtk_stack_set_visible_child (priv->chat_stack, priv->request_widget);
        }
      else
        {
          GtkWidget *chat_widget = neuland_window_get_chat_widget_for_contact (window, contact);
          gtk_stack_set_visible_child (priv->chat_stack, chat_widget);
          neuland_contact_reset_unread_messages (contact);
        }
    }
  else
    neuland_window_show_welcome_widget (window);
}

static NeulandContactRow *
neuland_window_get_row_for_contact (NeulandWindow *window,
                                    NeulandContact *contact)
{
  NeulandWindowPrivate *priv = window->priv;

  return NEULAND_CONTACT_ROW (g_hash_table_lookup (priv->contact_row_widgets, contact));
}

/* Set the active contact. There are actually two active contacts;
   priv->active_request and priv->active_contact. This function sets
   the former if contact is still a request, and the latter if not. */
static void
neuland_window_set_active_contact (NeulandWindow *window,
                                   NeulandContact *contact)
{
  g_return_if_fail (NEULAND_IS_WINDOW (window));

  if (contact)
    g_return_if_fail (NEULAND_IS_CONTACT (contact));

  NeulandWindowPrivate *priv = window->priv;
  gboolean contact_is_request = neuland_contact_is_request (contact);
  gboolean showing_requests = g_variant_get_boolean
    (g_action_group_get_action_state (G_ACTION_GROUP (window), "show-requests"));

  if (contact_is_request)
    g_debug ("Setting active contact (is request): '%s' (%p)",
             contact ? neuland_contact_get_name (contact) : "", contact);
  else
    g_debug ("Setting active contact: '%s' (%p)",
             contact ? neuland_contact_get_name (contact) : "", contact);

  if (contact_is_request)
    {
      priv->active_request = contact;
      if (showing_requests)
        neuland_window_show_chat_for_contact (window, contact);
    }
  else
    {
      priv->active_contact = contact;
      if (!showing_requests)
        neuland_window_show_chat_for_contact (window, contact);
    }
}

/* This signal can come from either of the priv->contacts_list_box or
   priv->requests_list_box. */
static void
contacts_list_box_row_activated_cb (NeulandWindow *window,
                                    GtkListBoxRow *row,
                                    gpointer user_data)
{
  NeulandContactRow *contact_row = NEULAND_CONTACT_ROW (row);
  NeulandContact *contact = neuland_contact_row_get_contact (contact_row);

  /* This allows selection by clicking anywhere on the row, not sure
     if we want that. */
  GVariant *state =  g_action_group_get_action_state (G_ACTION_GROUP (window), "selection");

  if (g_variant_get_boolean (state))
    neuland_contact_row_toggle_selected (contact_row);
  else
    /* TODO: With this in an else clause we don't change the active
       contact when in selection mode, but maybe we should? But it
       feels weird to unselect a contact and activate it at the same
       time. Maybe the proper solution would be to not show any chat
       widget at all, when in selection mode.*/
    neuland_window_set_active_contact (window, contact);

  g_variant_unref (state);
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
neuland_window_activate_contact (NeulandWindow *window,
                                 NeulandContact *contact)
{
  GtkWidget *row = GTK_WIDGET (neuland_window_get_row_for_contact (window, contact));
  if (row != NULL)
    gtk_widget_activate (row);
}


/* Returns TRUE on success, FALSE if there is no row in the list. */
static gboolean
neuland_window_activate_first_contact_or_request (NeulandWindow *window)
{
  NeulandWindowPrivate *priv = window->priv;
  gboolean showing_requests =
    g_variant_get_boolean (g_action_group_get_action_state (G_ACTION_GROUP (window),
                                                            "show-requests"));
  GtkListBox *list_box = showing_requests ? priv->requests_list_box : priv->contacts_list_box;
  GtkWidget *first_row = GTK_WIDGET (gtk_list_box_get_row_at_index (list_box, 0));

  if (first_row != NULL)
    {
      gtk_widget_activate (first_row);
      return TRUE;
    }

  return FALSE;
}

/* This is used to get notified when a contact request has been
   accepted, in which case the contact's number changes from -1 to n,
   where n > -1. */
static void
neuland_window_on_number_changed_cb (NeulandWindow *window,
                                     GParamSpec *pspec,
                                     gpointer user_data)
{
  NeulandWindowPrivate *priv = window->priv;
  NeulandContact *contact = NEULAND_CONTACT (user_data);
  GtkWidget *row = GTK_WIDGET (neuland_window_get_row_for_contact (window, contact));

  if (neuland_contact_get_number (contact) > -1)
    {
      /* Move contact from requests_list_box to contacts_list_box. */
      g_object_ref (row);
      gtk_container_remove (GTK_CONTAINER (priv->requests_list_box), GTK_WIDGET (row));
      gtk_list_box_insert (priv->contacts_list_box, row, -1);
      g_object_unref (row);
    }
}

static void
neuland_window_on_row_selected_changed (NeulandWindow *window,
                                        GParamSpec *pspec,
                                        gpointer user_data)
{
  NeulandWindowPrivate *priv = window->priv;
  NeulandContactRow *contact_row = NEULAND_CONTACT_ROW (user_data);
  NeulandContact *contact = neuland_contact_row_get_contact (contact_row);

  if (neuland_contact_row_get_selected (contact_row))
    g_hash_table_insert (priv->selected_contacts, contact, contact);
  else
    g_hash_table_remove (priv->selected_contacts, contact);

  gint number = g_hash_table_size (priv->selected_contacts);
  GAction *action = g_action_map_lookup_action (G_ACTION_MAP (window), "delete-selected");

  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), number > 0);
  g_debug ("window %p has %i selected contacts", window, number);
}

/* Adds @contact to either the contacts_list_box or the
   requests_list_box of the @window, depening on whether @contact is a
   normal contact or a request and maybe activates it. */
static void
neuland_window_add_contact (NeulandWindow *window, NeulandContact *contact)
{
  g_debug ("Adding contact %p to NeulandWindow %p", contact, window);
  g_return_if_fail (NEULAND_IS_WINDOW (window));
  g_return_if_fail (NEULAND_IS_CONTACT (contact));
  NeulandWindowPrivate *priv = window->priv;

  g_object_connect (contact,
                    "swapped-signal::notify::number",
                    neuland_window_on_number_changed_cb, window,
                    "swapped-signal::ensure-chat-widget",
                    neuland_window_on_ensure_chat_widget, window,
                    "swapped-signal::incoming-message",
                    neuland_window_on_incoming_message_cb, window,
                    "swapped-signal::incoming-action",
                    neuland_window_on_incoming_action_cb, window,
                    NULL);

  GtkWidget *contact_row = neuland_contact_row_new (contact);

  g_object_connect (contact_row,
                    "swapped-signal::notify::selected",
                    neuland_window_on_row_selected_changed, window,
                    NULL);
  gboolean contact_is_request = neuland_contact_is_request (contact);
  GtkListBox *list_box = contact_is_request ? priv->requests_list_box : priv->contacts_list_box;

  g_hash_table_insert (priv->contact_row_widgets, contact, contact_row);
  gtk_list_box_insert (list_box, contact_row, -1);
}

static void
neuland_window_remove_contacts (NeulandWindow *window, GSList *contacts)
{
  g_message ("Removing contacts from NeulandWindow %p", window);
  g_return_if_fail (NEULAND_IS_WINDOW (window));
  g_return_if_fail (contacts);
  g_return_if_fail (NEULAND_IS_CONTACT (contacts->data));

  NeulandWindowPrivate *priv = window->priv;

  /* Remember indices of the first and last removed rows for contacts
     and requests. */
  gint min_requests = -1;
  gint max_requests = -1;
  gint min_contacts = -1;
  gint max_contacts = -1;

  gboolean deleted_active_contact = FALSE;
  gboolean deleted_active_request = FALSE;

  GSList *l;
  for (l = contacts; l; l = l->next)
    {
      NeulandContact *contact = NEULAND_CONTACT (l->data);
      GtkListBoxRow *contact_row =
        GTK_LIST_BOX_ROW (g_hash_table_lookup (priv->contact_row_widgets, contact));
      g_return_if_fail (contact_row);
      gint index = gtk_list_box_row_get_index (contact_row);
      g_return_if_fail (index > -1);

      g_debug ("Removing contact %s (%p) from window %p",
               neuland_contact_get_preferred_name (contact), contact, window);

      if (neuland_contact_is_request (contact))
        {
          if (priv->active_request == contact)
            deleted_active_request = TRUE;
          if (min_requests > -1)
            {
              min_requests = MIN (min_requests, index);
              max_requests = MAX (max_requests, index);
            }
          else
            {
              min_requests = index;
              max_requests = index;
            }
        }
      else
        {
          if (priv->active_request == contact)
            deleted_active_request = TRUE;
          if (min_contacts > -1)
            {
              min_contacts = MIN (min_contacts, index);
              max_contacts = MAX (max_contacts, index);
            }
          else
            {
              min_contacts = index;
              max_contacts = index;
            }
        }
    }

  /* When the active request/contact was among the removed contacts,
     we activate another one (if any are left). We select the first
     existing row of:
     0) row below the bottom most deleted contact
     1) row above the top most deleted contact
     3) none */
  GtkListBoxRow *row_to_activate;
  NeulandContact *contact_to_activate;
  if (deleted_active_request)
    {
      row_to_activate = gtk_list_box_get_row_at_index
        (priv->requests_list_box, max_requests + 1);
      if (row_to_activate == NULL)
        row_to_activate = gtk_list_box_get_row_at_index
          (priv->requests_list_box, min_requests - 1);
      if (row_to_activate)
        contact_to_activate = neuland_window_get_contact_from_row (window, row_to_activate);
      else
        contact_to_activate = NULL;
      neuland_window_activate_contact (window, contact_to_activate);
    }
  if (deleted_active_contact)
    {
      row_to_activate = gtk_list_box_get_row_at_index
        (priv->contacts_list_box, max_contacts + 1);
      if (row_to_activate == NULL)
        row_to_activate = gtk_list_box_get_row_at_index
          (priv->contacts_list_box, min_contacts - 1);
      if (row_to_activate)
        contact_to_activate = neuland_window_get_contact_from_row (window, row_to_activate);
      else
        contact_to_activate = NULL;
      neuland_window_activate_contact (window, contact_to_activate);
    }

  /* Finally, destroy the widgets releated to the contacts and maybe
     remove them from the hash tables as well.  */
  for (l = contacts; l; l = l->next)
    {

      NeulandContact *contact = l->data;
      NeulandContactRow *contact_row =
        NEULAND_CONTACT_ROW (g_hash_table_lookup (priv->contact_row_widgets, contact));
      GtkWidget *chat_widget = GTK_WIDGET (g_hash_table_lookup (priv->chat_widgets, contact));
      /* Remove @contact_row from the hash table of selected rows
         before destroying it. */
      neuland_contact_row_set_selected (contact_row, FALSE);
      gtk_widget_destroy (GTK_WIDGET (contact_row));

      if (chat_widget)
        {
          g_hash_table_remove (priv->chat_widgets, contact);
          gtk_widget_destroy (chat_widget);
        }

      g_hash_table_remove (priv->contact_row_widgets, contact);
    }
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
}

static void
on_name_change_cb (GObject *object,
                   GParamSpec *pspec,
                   gpointer user_data)
{
  NeulandTox *tox = NEULAND_TOX (object);
  NeulandWindow *window = NEULAND_WINDOW (user_data);

  neuland_contact_row_set_name (NEULAND_CONTACT_ROW
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

  neuland_contact_row_set_status_message (NEULAND_CONTACT_ROW
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
on_remove_contacts_cb (NeulandWindow *window,
                       GSList *contacts,
                       gpointer user_data)
{
  neuland_window_remove_contacts (window, contacts);
}

/* This function is not used anywhere yet. */
static void
neuland_window_invalidate_filter (NeulandWindow *window)
{
  gtk_list_box_invalidate_filter (window->priv->contacts_list_box);
}

static void
on_pending_requests_cb (NeulandWindow *window,
                        GObject *gobject,
                        gpointer user_data)
{
  NeulandWindowPrivate *priv = window->priv;
  NeulandTox *tox = priv->tox;
  gint64 pending_requests = neuland_tox_get_pending_requests (tox);

  if (pending_requests > 0)
    {
      gtk_revealer_set_reveal_child (priv->requests_button_revealer, TRUE);
    }
  else
    {
      gtk_revealer_set_reveal_child (priv->requests_button_revealer, FALSE);
      /* Automatically change to show only contacts when no open
         requests are left */
      g_action_group_change_action_state (G_ACTION_GROUP (window), "show-requests",
                                          g_variant_new_boolean (FALSE));
    }
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

  g_object_get (priv->tox, "tox-id-hex", &id, NULL);
  g_message ("Tox ID for window %p: %s", window, id);
  gtk_label_set_text (priv->welcome_widget_tox_id_label, id);

  g_object_bind_property (tox, "pending-requests", priv->pending_requests_label, "label", 0);

  g_object_connect (tox,
                    "signal::notify::self-name", on_name_change_cb, window,
                    "signal::notify::status-message", on_status_message_change_cb, window,
                    "swapped-signal::contact-add", on_contact_add_cb, window,
                    "swapped-signal::remove-contacts", on_remove_contacts_cb, window,
                    "swapped-signal::notify::pending-requests", on_pending_requests_cb, window,
                    NULL);
  neuland_contact_row_set_name (NEULAND_CONTACT_ROW (priv->me_widget),
                                neuland_tox_get_name (tox));
  neuland_contact_row_set_message (NEULAND_CONTACT_ROW (priv->me_widget),
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

static void
neuland_window_selection_state_changed (GSimpleAction *action,
                                        GVariant      *parameter,
                                        gpointer       user_data)
{
  g_debug ("neuland_window_selection_state_changed");

  NeulandWindow *window = NEULAND_WINDOW (user_data);
  NeulandWindowPrivate *priv = window->priv;
  GtkListBox *contacts_list_box = window->priv->contacts_list_box;
  GtkStyleContext *context_left =
    gtk_widget_get_style_context (GTK_WIDGET (window->priv->left_header_bar));
  GtkStyleContext *context_right =
    gtk_widget_get_style_context (GTK_WIDGET (window->priv->right_header_bar));

  gboolean selection_enabled = g_variant_get_boolean (parameter);
  g_debug (selection_enabled ? "selection enabled" : "selection disabled");

  gtk_widget_set_sensitive (GTK_WIDGET (priv->chat_stack), !selection_enabled);

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

  gtk_container_foreach (GTK_CONTAINER (priv->contacts_list_box),
                         (GtkCallback) neuland_contact_row_show_selection, &selection_enabled);

  gtk_widget_set_visible (GTK_WIDGET (priv->action_bar), selection_enabled);
  g_simple_action_set_state (action, parameter);
}

void
neuland_window_status_state_changed (GSimpleAction *action,
                                     GVariant      *parameter,
                                     gpointer       user_data)
{
  NeulandWindow *window = NEULAND_WINDOW (user_data);
  NeulandContactStatus status = (NeulandContactStatus)g_variant_get_int32 (parameter);
  g_debug ("Changing own status to %i", status);

  neuland_tox_set_status (window->priv->tox, status);
  neuland_contact_row_set_status (NEULAND_CONTACT_ROW (window->priv->me_widget), status);
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
add_contact_activated (GSimpleAction *action,
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
accept_request_activated (GSimpleAction *action,
                          GVariant *parameter,
                          gpointer user_data)
{
  NeulandWindow *window = NEULAND_WINDOW (user_data);
  NeulandWindowPrivate *priv = window->priv;
  NeulandContact *contact = priv->active_request;
  NeulandTox *tox = priv->tox;

  GSList *contacts = g_slist_prepend (NULL, contact);
  neuland_tox_accept_contact_requests (tox, contacts);
  g_slist_free (contacts);
}

static void
discard_request_activated (GSimpleAction *action,
                           GVariant *parameter,
                           gpointer user_data)
{
  NeulandWindow *window = NEULAND_WINDOW (user_data);
  NeulandWindowPrivate *priv = window->priv;
  NeulandContact *contact = priv->active_request;
  NeulandTox *tox = priv->tox;

  GSList *contacts = g_slist_prepend (NULL, contact);
  neuland_tox_remove_contacts (priv->tox, contacts);
  g_slist_free (contacts);
}

static void
delete_selected_activated (GSimpleAction *action,
                           GVariant *parameter,
                           gpointer user_data)
{
  NeulandWindow *window = NEULAND_WINDOW (user_data);
  NeulandWindowPrivate *priv = window->priv;
  GList *list = g_hash_table_get_keys (priv->selected_contacts);
  neuland_tox_remove_contacts (priv->tox, list);
  g_list_free (list);
}

static void
neuland_window_show_requests_state_changed (GSimpleAction *action,
                                            GVariant *parameter,
                                            gpointer user_data)
{
  NeulandWindow *window = NEULAND_WINDOW (user_data);
  NeulandWindowPrivate *priv = window->priv;
  gboolean show_requests = g_variant_get_boolean (parameter);

  /* neuland_window_activate_first_contact_or_request() below depends
     on this being already set. */
  g_simple_action_set_state (action, parameter);

  if (show_requests)
    {
      /* When switching to requests we make sure to always select one,
         if there isn't an active selection yet. We can only switch to
         this state if there are requests anyway. */
      NeulandContact *active_request = priv->active_request;
      gtk_stack_set_visible_child (priv->side_pane_stack, priv->scrolled_window_requests);
      if (active_request != NULL)
        neuland_window_activate_contact (window, active_request);
      else
        if (!neuland_window_activate_first_contact_or_request (window))
          {
            g_warning ("Switched to requests while there are no requests,"
                       "going to show the welcome widget.");
            neuland_window_show_welcome_widget (window);
          }
    }
  else
    {
      /* When switching to contacts, we do *not* automatically select
         one, but show the welcome widget in case there is no active
         selection yet. */
      NeulandContact *active_contact = priv->active_contact;
      gtk_stack_set_visible_child (priv->side_pane_stack, priv->scrolled_window_contacts);
      if (active_contact != NULL)
        neuland_window_activate_contact (window, active_contact);
      else
        neuland_window_show_welcome_widget (window);
    }

  /* Show or hide Accept and Discard buttons depending on the action's
     state. */
  gtk_revealer_set_reveal_child (priv->accept_button_revealer, show_requests);
  gtk_revealer_set_reveal_child (priv->discard_button_revealer, show_requests);
}

static GActionEntry win_entries[] = {
  { "change-status", NULL, "i", "0", neuland_window_status_state_changed },
  { "selection", NULL, NULL, "false", neuland_window_selection_state_changed },
  { "add-contact", add_contact_activated },
  { "accept-request", accept_request_activated },
  { "discard-request", discard_request_activated },
  { "delete-selected", delete_selected_activated },
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

  g_hash_table_destroy (priv->contact_row_widgets);
  g_hash_table_destroy (priv->chat_widgets);
  g_hash_table_destroy (priv->selected_contacts);

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
  gtk_widget_class_bind_template_child_private (widget_class, NeulandWindow, requests_list_box);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandWindow, chat_stack);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandWindow, side_pane_stack);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandWindow, scrolled_window_contacts);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandWindow, scrolled_window_requests);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandWindow, right_header_bar);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandWindow, left_header_bar);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandWindow, gear_button);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandWindow, select_button);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandWindow, accept_button_revealer);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandWindow, discard_button_revealer);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandWindow, requests_button_revealer);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandWindow, pending_requests_label);
  gtk_widget_class_bind_template_child_private (widget_class, NeulandWindow, action_bar);

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
  priv->contact_row_widgets = g_hash_table_new (NULL, NULL);
  priv->chat_widgets = g_hash_table_new (NULL, NULL);
  priv->selected_contacts = g_hash_table_new (NULL, NULL);

  builder = gtk_builder_new_from_resource ("/org/tox/neuland/neuland-window-menu.ui");
  gtk_builder_add_from_resource (builder, "/org/tox/neuland/neuland-me-status-menu.ui", NULL);
  gtk_builder_add_from_resource (builder, "/org/tox/neuland/neuland-welcome-widget.ui", NULL);
  gtk_builder_add_from_resource (builder, "/org/tox/neuland/neuland-request-widget.ui", NULL);

  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (priv->gear_button),
                                  (GMenuModel*) gtk_builder_get_object (builder, "win-menu"));

  g_action_map_add_action_entries (G_ACTION_MAP (window), win_entries, G_N_ELEMENTS (win_entries), window);

  /* Prepare welcome widget */
  priv->welcome_widget = GTK_WIDGET (gtk_builder_get_object  (builder, "welcome-widget"));
  priv->welcome_widget_tox_id_label =
    GTK_LABEL (gtk_builder_get_object (builder, "welcome_widget_tox_id_label"));
  g_message ("adding welcome_widget ...");

  gtk_container_add (GTK_CONTAINER (priv->chat_stack), priv->welcome_widget);

  /* Prepare contact request widget */
  priv->request_widget = GTK_WIDGET (gtk_builder_get_object  (builder, "request_widget"));
  priv->request_widget_text_buffer =
    GTK_TEXT_BUFFER (gtk_builder_get_object  (builder, "request_widget_text_buffer"));
  priv->request_widget_tox_id_label =
    GTK_LABEL (gtk_builder_get_object  (builder, "request_widget_tox_id_label"));
  g_message ("adding request_widget ...");
  gtk_container_add (GTK_CONTAINER (priv->chat_stack), priv->request_widget);

  /* Me widget */
  priv->me_widget = neuland_contact_row_new (NULL);
  neuland_contact_row_set_name (NEULAND_CONTACT_ROW (priv->me_widget), "...");
  neuland_contact_row_set_status (NEULAND_CONTACT_ROW (priv->me_widget), NEULAND_CONTACT_STATUS_NONE);

  gtk_container_add (GTK_CONTAINER (priv->me_button), priv->me_widget);
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (priv->me_button),
                                  (GMenuModel*) gtk_builder_get_object (builder, "me-status-menu"));

  gtk_widget_get_preferred_height (priv->me_button, NULL, &priv->me_button_height);

  /* Set up list box for contacts */
  gtk_list_box_set_header_func (priv->contacts_list_box, list_box_header_func, NULL, NULL);
  gtk_list_box_set_header_func (priv->requests_list_box, list_box_header_func, NULL, NULL);
  gtk_stack_set_visible_child (priv->side_pane_stack, priv->scrolled_window_contacts);

  GAction *action;
  action = g_action_map_lookup_action (G_ACTION_MAP (window), "delete-selected");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

  g_object_unref (G_OBJECT (builder));

  neuland_window_show_welcome_widget (window);
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
