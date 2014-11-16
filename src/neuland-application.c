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

#include "config.h"

#include <gtk/gtk.h>
#include <tox/tox.h>
#include <string.h>
#include <glib/gi18n.h>

#include "neuland-window.h"
#include "neuland-application.h"
#include "neuland-contact.h"
#include "neuland-tox.h"

/* Relative to XDG_CONFIG_HOME */
#define DEFAULT_TOX_DATA_PATH "tox/data.neuland"

gchar *neuland_authors[] = { "Volker Sobek <reklov@live.com>", NULL };

struct _NeulandApplicationPrivate
{
  gpointer unused;
};

G_DEFINE_TYPE_WITH_PRIVATE (NeulandApplication, neuland_application, GTK_TYPE_APPLICATION)

static void
neuland_application_finalize (GObject *object)
{
  G_OBJECT_CLASS (neuland_application_parent_class)->finalize (object);
}

static void
neuland_application_new_window (NeulandApplication *app, gchar *data_path)
{
  g_debug ("neuland_application_new_window");

  NeulandTox *tox = neuland_tox_new (data_path);

  GtkWidget *window;
  window = neuland_window_new (tox);
  g_object_unref (tox); // window now holds the only reference to tox

  gtk_application_add_window (GTK_APPLICATION (app), GTK_WINDOW (window));
  gtk_widget_show_all (GTK_WIDGET (window));
}

/* Start a throw-away session, no data will be saved. Useful for testing. */
static void
neuland_application_new_transient_identity_activated (GSimpleAction *action,
                                                      GVariant      *parameter,
                                                      gpointer       user_data)
{
  NeulandApplication *app = NEULAND_APPLICATION (user_data);
  neuland_application_new_window (app, NULL);
}

static void
neuland_application_about_activated (GSimpleAction *action,
                                     GVariant      *parameter,
                                     gpointer       user_data)
{
  GtkApplication *application = GTK_APPLICATION (user_data);
  gtk_show_about_dialog (gtk_application_get_active_window (application),
                         "program-name", "Neuland",
                         "version", VERSION,
                         "comments", _("A simple chat client for Tox\n"
                                       "For more information about Tox see https://tox.im"),
                         "copyright", "Copyright © 2014 Volker Sobek",
                         "authors", neuland_authors,
                         /* Translators: This is a special message that shouldn't be translated
                          * literally. It is used in the about box to give credits to
                          * the translators.
                          * Thus, you should translate it to your name and email address.
                          * You should also include other translators who have contributed to
                          * this translation; in that case, please write each of them on a separate
                          * line seperated by newlines (\n).
                          */
                         "translator-credits", _("translator-credits"),
                         "website", "https://github.com/IMFTC/neuland",
                         "website-label", _("Neuland Website"),
                         "license-type", GTK_LICENSE_GPL_3_0,
                         NULL);
}

static void
neuland_application_quit_activated (GSimpleAction *action,
                                    GVariant      *parameter,
                                    gpointer       user_data)
{
  GApplication *app = user_data;
  GList *list, *next;

  g_debug ("neuland_application_quit_activated");

  /* list is owned by @app  */
  list = gtk_application_get_windows (GTK_APPLICATION (app));
  while (list)
    {
      next = list->next;

      gtk_widget_destroy (GTK_WIDGET (list->data));

      list = next;
    }
}


static GActionEntry app_entries[] = {
  { "new-transient-identity", neuland_application_new_transient_identity_activated, NULL, NULL, NULL },
  { "about", neuland_application_about_activated, NULL, NULL, NULL },
  { "quit", neuland_application_quit_activated, NULL, NULL, NULL },
};

static void
neuland_application_open (GApplication  *application,
                          GFile        **files,
                          gint           n_files,
                          const gchar   *hint)
{
  int i;
  for (i = 0; i < n_files; i++)
    {
      gchar *path = g_file_get_path (files[i]);
      g_message ("Creating window for data path \"%s\"", path);
      neuland_application_new_window (NEULAND_APPLICATION (application), path);
      g_free (path);
    }
}

static void
neuland_application_activate (GApplication *application)
{
  g_debug ("neuland_activate");

  gchar *data_path;
  data_path = g_build_filename (g_get_user_config_dir (), DEFAULT_TOX_DATA_PATH, NULL);

  g_debug ("Creating window for default tox data '%s'", data_path);
  neuland_application_new_window (NEULAND_APPLICATION (application), data_path);
  g_free (data_path);
}

static void
neuland_application_startup (GApplication *application)
{
  g_debug ("neuland_application_startup");
  NeulandApplication *neuland = (NeulandApplication*) application;
  GObject *window;
  GtkBuilder *builder;

  G_APPLICATION_CLASS (neuland_application_parent_class)
    ->startup (application);

  GtkCssProvider *css_provider = gtk_css_provider_new ();
  GFile *css_file = g_file_new_for_uri ("resource:///org/tox/neuland/neuland.css");
  gtk_css_provider_load_from_file (css_provider, css_file, NULL);
  g_object_unref (css_file);

  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (css_provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, "/org/tox/neuland/neuland-application-menu.ui", NULL);
  gtk_application_set_app_menu (GTK_APPLICATION (application),
                                G_MENU_MODEL (gtk_builder_get_object (builder, "app-menu")));
  g_action_map_add_action_entries (G_ACTION_MAP (application),
                                   app_entries, G_N_ELEMENTS (app_entries), application);

  struct {
    const gchar *target_dot_action;
    const gchar *accelerators[2];
  } accels[] = {
    { "app.new-transient-identity", { "<Primary>t", NULL } },
    { "app.quit", { "<Primary>q", NULL } },
    { "win.add-contact", { "<Primary>n", NULL} },
    { "win.send-file", { "<Primary>s", NULL} }
  };

  int i;
  for (i = 0; i < G_N_ELEMENTS (accels); i++)
    gtk_application_set_accels_for_action (GTK_APPLICATION (application),
                                           accels[i].target_dot_action,
                                           accels[i].accelerators);

  g_object_unref (builder);
}

static void
neuland_application_shutdown (GApplication *application)
{
  g_debug ("neuland_application_shutdown");
  NeulandApplication *neuland = (NeulandApplication *) application;

  G_APPLICATION_CLASS (neuland_application_parent_class)
    ->shutdown (application);
}

static void
neuland_application_init (NeulandApplication *application)
{
  g_debug ("neuland_application_init");
  application->priv = neuland_application_get_instance_private (application);
}

static void
neuland_application_class_init (NeulandApplicationClass *class)
{
  g_debug ("neuland_application_class_init");
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  application_class->startup = neuland_application_startup;
  application_class->shutdown = neuland_application_shutdown;
  application_class->activate = neuland_application_activate;
  application_class->open = neuland_application_open;

  object_class->finalize = neuland_application_finalize;

}

NeulandApplication *
neuland_application_new (void)
{
  g_debug ("neuland_application_new");
  NeulandApplication *neuland;

  g_set_application_name ("Neuland");

  neuland = g_object_new (NEULAND_TYPE_APPLICATION,
                          "application-id", "org.tox.neuland",
                          "flags", G_APPLICATION_HANDLES_OPEN,
                          "inactivity-timeout", 0,
                          "register-session", TRUE,
                          NULL);

  return neuland;
}

int
main (int argc, char **argv)
{
  NeulandApplication *neuland;
  int status;

  bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  neuland = neuland_application_new ();

  status = g_application_run (G_APPLICATION (neuland), argc, argv);

  g_object_unref (neuland);

  return status;
}
