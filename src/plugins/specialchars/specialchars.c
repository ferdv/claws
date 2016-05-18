/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2012 the Claws Mail Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#include <glib.h>
#include <glib/gi18n.h>

#include "automaton.h"

#include "version.h"
#include "claws.h"
#include "menu.h"
#include "mainwindow.h"
#include "compose.h"
#include "plugin.h"
#include "utils.h"
#include "hooks.h"
#include "log.h"
#include "menu.h"

#define PLUGIN_NAME (_("Special Chars"))

static Substitution substs[] = {
  {"\\rightarrow", "U+2192"},
  {"\\rightalarm", "U+0040"},
  {"\\Rightarrow", "U+21D2"},
};

#define message_dialog(...)  (gtk_message_dialog_new(\
      (GtkWindow *) compose->window,\
      GTK_DIALOG_DESTROY_WITH_PARENT,\
      GTK_MESSAGE_INFO,\
      GTK_BUTTONS_OK,\
      __VA_ARGS__))

#define message(dlg, ...) (\
    dlg = message_dialog(__VA_ARGS__),\
    gtk_dialog_run(GTK_DIALOG(dlg)),\
    gtk_widget_destroy(dlg));

/*static gboolean is_specialchar(gunichar ch, gpointer data) {
  switch (ch) {
    case '\\':
      return TRUE;
    default:
      return FALSE;
  }
}*/

static gboolean find_specialchar(GtkTextIter *iter) {
  do {
    switch (gtk_text_iter_get_char(iter)) {
      case '\\':
        return TRUE;
    }
  } while (gtk_text_iter_forward_char(iter));

  return FALSE;
}

static guint hook_id;

static void specialchars_cb(GtkAction *action, gpointer data) {
  Compose *compose = (Compose *) data;
  GtkTextView *text_view = GTK_TEXT_VIEW(compose->text);
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);
  GtkTextIter start_iter, end_iter, iter;
  GtkWidget *dialog;
  gchar *subst_string, u8ch[7];
  gint u8ch_len;
  gunichar uch;

  debug_print("specialchars_cb\n");

  //message(dialog, "Special Chars is alive!");

  debug_print("Constructing automaton.");
  Automaton *a = generate_automaton(substs, sizeof(substs) / sizeof(Substitution));
  print_automaton(*a);
  load_automaton(*a);
  dump_automaton(*a, "test.dfa");

  gtk_text_buffer_get_start_iter(buffer, &iter);

  while (find_specialchar(&iter)) {
    //message(dialog, "Found the char.");
    start_iter = iter;
    if (match_iter(&iter, &subst_string)) {
      gtk_text_iter_forward_char(&iter);
      end_iter = iter;
      sscanf(subst_string, "U+%06X", &uch);
      u8ch_len = g_unichar_to_utf8(uch, u8ch);
      u8ch[u8ch_len] = '\0';
      message(dialog, 
          "Found string at %d-%d. To be substituted with \"%s\" (U+%04X)", 
          gtk_text_iter_get_offset(&start_iter),
          gtk_text_iter_get_offset(&end_iter),
          u8ch,
          uch);
      gtk_text_buffer_delete(buffer, &start_iter, &end_iter);
      gtk_text_buffer_insert(
          buffer, &start_iter, u8ch, u8ch_len);
      iter = start_iter;
    }
  }

  free_automaton(a);
  return;
}


static GtkActionEntry specialchars_entries[] = {{
  "Edit/SpecialChars",
  NULL,
  N_("Special Chars"), NULL, NULL, G_CALLBACK(specialchars_cb)
}};

gboolean my_log_hook(gpointer source, gpointer data)
{
	LogText *logtext = (LogText *)source;

	g_print("*** Demo Plugin log: %s\n", logtext->text);

	return FALSE;
}

gboolean compose_created_hook(gpointer source, gpointer data)
{
  Compose *compose = (Compose *) source;

  //GtkActionGroup *action_group = 
  cm_menu_create_action_group_full(
      compose->ui_manager,
      "SpecialChars", 
      specialchars_entries,
      G_N_ELEMENTS(specialchars_entries), 
      (gpointer) compose);

/*  gtk_action_group_add_actions(
      action_group, 
      specialchars_entry,
      1,
      (gpointer) compose->window);
*/

  MENUITEM_ADDUI_MANAGER(
      compose->ui_manager,
      "/Menu/Edit", 
      "SepcialChars",
      "Edit/SpecialChars",
      GTK_UI_MANAGER_MENUITEM);

  return FALSE;
}

gint plugin_init(gchar **error)
{
	if (!check_plugin_version(MAKE_NUMERIC_VERSION(2,9,2,72),
				VERSION_NUMERIC, PLUGIN_NAME, error))
		return -1;
  
  hook_id = hooks_register_hook(
      COMPOSE_CREATED_HOOKLIST, 
      compose_created_hook, 
      NULL); 
	/*hook_id = hooks_register_hook(LOG_APPEND_TEXT_HOOKLIST, my_log_hook, NULL);*/
	if (hook_id == -1) {
		*error = g_strdup(_("Failed to register Special Chars hook"));
    debug_print("%s\n", *error);
		return -1;
	}

	debug_print("Special Chars plugin loaded\n");

	return 0;
}

gboolean plugin_done(void)
{
	hooks_unregister_hook(COMPOSE_CREATED_HOOKLIST, hook_id);

	debug_print("Special Chars plugin unloaded\n");
	return TRUE;
}

const gchar *plugin_name(void)
{
	return PLUGIN_NAME;
}

const gchar *plugin_desc(void)
{
	return _("This plugin allows various ways of entering some Unicode characters."
	         "In particular, it supports some LaTeX symbol commands and inserts their unicode approximation."
	         "\n\n"
	         "It is very useful. :-)");
}

const gchar *plugin_type(void)
{
	return "Common";
}

const gchar *plugin_licence(void)
{
	return "GPL3+";
}

const gchar *plugin_version(void)
{
	return VERSION;
}

struct PluginFeature *plugin_provides(void)
{
	static struct PluginFeature features[] = 
		{ {PLUGIN_OTHER, N_("Special Chars")},
		  {PLUGIN_NOTHING, NULL}};
	return features;
}
