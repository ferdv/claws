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

#include "sdfa.h"
#include "subst.h"
#include "sc_utils.h"

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

#include "defs.h"

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
  struct Substs substs;
  GError *error;

  debug_print("specialchars_cb\n");

  debug_print("Loading substitutions.");
  
  if (!subst_load_from_file("substs.ini",  &substs, NULL/*&error*/)) {
    debug_print("Loading failed.");
    return;
  }
/*  else {
    subst_print_substs(substs);
  }*/
 
  debug_print("with_buffer = %p\n", substs.with_buffer);
  debug_print("Constructing automaton.\n");
  SDFA *a = sdfa_generate(&substs); // TODO
  if (a == NULL) {
    debug_print("Automaton not generated.\n");
    return;
  }
  sdfa_print(*a);
  sdfa_load(*a);
  sdfa_dump(*a, "test.dfa");

  gtk_text_buffer_get_start_iter(buffer, &iter);

  while (find_specialchar(&iter)) {
    start_iter = iter;
    gtk_text_iter_forward_char(&iter);
    if (sdfa_match_iter(&iter, &subst_string)) {
      gtk_text_iter_forward_char(&iter);
      end_iter = iter;
      info_message(dialog, 
          "Found string at %d-%d. To be substituted with \"%s\"", 
          gtk_text_iter_get_offset(&start_iter),
          gtk_text_iter_get_offset(&end_iter),
          subst_string);
      gtk_text_buffer_delete(buffer, &start_iter, &end_iter);
      gtk_text_buffer_insert(
          buffer, &start_iter, subst_string, strlen(subst_string));
      iter = start_iter;
    }
  }

  sdfa_free(a);
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
