/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2001 Hiroyuki Yamamoto
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "intl.h"
#include "main.h"
#include "prefs.h"
#include "prefs_matcher.h"
#include "prefs_common.h"
#include "mainwindow.h"
#include "foldersel.h"
#include "manage_window.h"
#include "inc.h"
#include "matcher.h"
#include "utils.h"
#include "gtkutils.h"
#include "alertpanel.h"
#include "folder.h"

static struct Matcher {
	GtkWidget *window;

	GtkWidget *close_btn;

	GtkWidget *predicate_combo;
	GtkWidget *header_combo;

	GtkWidget *criteria_list;
	GtkWidget *predicate_list;
	GtkWidget *bool_op_list;

	GtkWidget *header_entry;
	GtkWidget *header_label;
	GtkWidget *value_entry;
	GtkWidget *value_label;
	GtkWidget *predicate_label;
	GtkWidget *case_chkbtn;
	GtkWidget *regexp_chkbtn;

	GtkWidget *cond_clist;
} matcher;

/* choice in the list */

enum {
	CRITERIA_ALL = 0,
	CRITERIA_SUBJECT = 1,
	CRITERIA_FROM = 2,
	CRITERIA_TO = 3,
	CRITERIA_CC = 4,
	CRITERIA_TO_OR_CC = 5,
	CRITERIA_NEWSGROUPS = 6,
	CRITERIA_AGE_GREATER = 7,
	CRITERIA_AGE_LOWER = 8,
	CRITERIA_HEADER = 9,
	CRITERIA_HEADERS_PART = 10,
	CRITERIA_BODY_PART = 11,
	CRITERIA_MESSAGE = 12
};

gchar * bool_op_text [] = {
	"or", "and"
};

gchar * predicate_text [] = {
	"contains", "does not contain"
};

gchar * criteria_text [] = {
	"All messages", "Subject",
	"From", "To", "Cc", "To or Cc",
	"Newsgroups",
	"Age greater than", "Age lower than",
	"Header", "Headers part",
	"Body part", "Whole message"
};

gint get_sel_from_list(GtkList * list)
{
	gint row = 0;
	void * sel;
	GList * child;

	sel = list->selection->data;
	for(child = list->children ; child != NULL ;
	    child = g_list_next(child)) {
		if (child->data == sel)
			return row;
		row ++;
	}
	
	return row;
}

enum {
	PREDICATE_CONTAINS = 0,
	PREDICATE_DOES_NOT_CONTAIN = 1
};

static MatcherList * tmp_matchers;

#define VSPACING		12
#define VSPACING_NARROW		4
#define DEFAULT_ENTRY_WIDTH	80
#define PREFSBUFSIZE		1024

/* widget creating functions */
static void prefs_matcher_create	(void);

static void prefs_matcher_set_dialog	(void);

/*
static void prefs_matcher_set_list	(void);
static gint prefs_matcher_clist_set_row	(gint	 row);
*/

/* callback functions */

/*
static void prefs_matcher_select_dest_cb	(void);
*/
static void prefs_matcher_register_cb	(void);
static void prefs_matcher_substitute_cb	(void);
static void prefs_matcher_delete_cb	(void);
static void prefs_matcher_up		(void);
static void prefs_matcher_down		(void);
static void prefs_matcher_select	(GtkCList	*clist,
					 gint		 row,
					 gint		 column,
					 GdkEvent	*event);

static void prefs_matcher_key_pressed	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	 data);
static void prefs_matcher_close		(void);
static gint prefs_matcher_deleted(GtkWidget *widget, GdkEventAny *event,
				  gpointer data);
static void prefs_matcher_criteria_select(GtkList *list,
					  GtkWidget *widget,
					  gpointer user_data);
static void prefs_matcher_set_list(void);

void prefs_matcher_open(MatcherList * matchers)
{
	inc_autocheck_timer_remove();

	if (!matcher.window) {
		prefs_matcher_create();
	}

	manage_window_set_transient(GTK_WINDOW(matcher.window));
	gtk_widget_grab_focus(matcher.close_btn);

	tmp_matchers = matchers;
	prefs_matcher_set_dialog();

	gtk_widget_show(matcher.window);
}

static void prefs_matcher_create(void)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *close_btn;
	GtkWidget *confirm_area;

	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *vbox3;
	GtkWidget *table1;

	GtkWidget *hbox1;

	GtkWidget *header_combo;
	GtkWidget *header_entry;
	GtkWidget *header_label;
	GtkWidget *criteria_combo;
	GtkWidget *criteria_list;
	GtkWidget *criteria_label;
	GtkWidget *value_label;
	GtkWidget *value_entry;
	GtkWidget *predicate_combo;
	GtkWidget *predicate_list;
	GtkWidget *predicate_label;
	GtkWidget *bool_op_combo;
	GtkWidget *bool_op_list;
	GtkWidget *bool_op_label;

	GtkWidget *regexp_chkbtn;
	GtkWidget *case_chkbtn;

	GtkWidget *reg_hbox;
	GtkWidget *btn_hbox;
	GtkWidget *arrow;
	GtkWidget *reg_btn;
	GtkWidget *subst_btn;
	GtkWidget *del_btn;

	GtkWidget *cond_hbox;
	GtkWidget *cond_scrolledwin;
	GtkWidget *cond_clist;

	GtkWidget *btn_vbox;
	GtkWidget *up_btn;
	GtkWidget *down_btn;

	GList *combo_items;
	gint i;

	gchar *title[] = {_("Registered rules")};

	debug_print(_("Creating matcher setting window...\n"));

	window = gtk_window_new (GTK_WINDOW_DIALOG);
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	gtk_window_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal (GTK_WINDOW (window), TRUE);
	gtk_window_set_policy (GTK_WINDOW (window), FALSE, TRUE, FALSE);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	gtkut_button_set_create (&confirm_area, &close_btn, _("Close"),
				 NULL, NULL, NULL, NULL);
	gtk_widget_show (confirm_area);
	gtk_box_pack_end (GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default (close_btn);

	gtk_window_set_title (GTK_WINDOW(window),
			      _("Condition setting"));
	gtk_signal_connect (GTK_OBJECT(window), "delete_event",
			    GTK_SIGNAL_FUNC(prefs_matcher_deleted), NULL);
	gtk_signal_connect (GTK_OBJECT(window), "key_press_event",
			    GTK_SIGNAL_FUNC(prefs_matcher_key_pressed), NULL);
	gtk_signal_connect (GTK_OBJECT(window), "focus_in_event",
			    GTK_SIGNAL_FUNC(manage_window_focus_in), NULL);
	gtk_signal_connect (GTK_OBJECT(window), "focus_out_event",
			    GTK_SIGNAL_FUNC(manage_window_focus_out), NULL);
	gtk_signal_connect (GTK_OBJECT(close_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_matcher_close), NULL);

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_box_pack_start (GTK_BOX (vbox), vbox1, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 2);

	table1 = gtk_table_new (2, 3, FALSE);
	gtk_widget_show (table1);

	gtk_box_pack_start (GTK_BOX (vbox1), table1, FALSE, TRUE, 0);
	gtk_table_set_row_spacings (GTK_TABLE (table1), 8);
	gtk_table_set_col_spacings (GTK_TABLE (table1), 8);

	/* criteria combo box */

	criteria_label = gtk_label_new (_("Match type"));
	gtk_widget_show (criteria_label);
	gtk_misc_set_alignment (GTK_MISC (criteria_label), 0, 0.5);
	gtk_table_attach (GTK_TABLE (table1), criteria_label, 0, 1, 0, 1,
			  GTK_FILL, 0, 0, 0);

	criteria_combo = gtk_combo_new ();
	gtk_widget_show (criteria_combo);

	combo_items = NULL;

	for(i = 0 ; i < (gint) (sizeof(criteria_text) / sizeof(gchar *)) ;
	    i++) {
		combo_items = g_list_append(combo_items,
					    (gpointer) _(criteria_text[i]));
	}
	gtk_combo_set_popdown_strings(GTK_COMBO(criteria_combo), combo_items);

	g_list_free(combo_items);

	gtk_widget_set_usize (criteria_combo, 120, -1);
	gtk_table_attach (GTK_TABLE (table1), criteria_combo, 0, 1, 1, 2,
			  0, 0, 0, 0);
	criteria_list = GTK_COMBO(criteria_combo)->list;
	gtk_signal_connect (GTK_OBJECT (criteria_list), "select-child",
			    GTK_SIGNAL_FUNC (prefs_matcher_criteria_select),
			    NULL);

	gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(criteria_combo)->entry),
			       FALSE);

	/* header name */

	header_label = gtk_label_new (_("Header name"));
	gtk_widget_show (header_label);
	gtk_misc_set_alignment (GTK_MISC (header_label), 0, 0.5);
	gtk_table_attach (GTK_TABLE (table1), header_label, 1, 2, 0, 1,
			  GTK_FILL, 0, 0, 0);

	header_combo = gtk_combo_new ();
	gtk_widget_show (header_combo);
	gtk_widget_set_usize (header_combo, 96, -1);
	gtkut_combo_set_items (GTK_COMBO (header_combo),
			       "Subject", "From", "To", "Cc", "Reply-To",
			       "Sender", "X-ML-Name", "X-List", "X-Sequence",
			       "X-Mailer",
			       NULL);
	gtk_table_attach (GTK_TABLE (table1), header_combo, 1, 2, 1, 2,
			  0, 0, 0, 0);
	header_entry = GTK_COMBO (header_combo)->entry;
	gtk_entry_set_editable (GTK_ENTRY (header_entry), TRUE);

	/* value */

	value_label = gtk_label_new (_("Value"));
	gtk_widget_show (value_label);
	gtk_misc_set_alignment (GTK_MISC (value_label), 0, 0.5);
	gtk_table_attach (GTK_TABLE (table1), value_label, 2, 3, 0, 1,
			  GTK_FILL, 0, 0, 0);

	value_entry = gtk_entry_new ();
	gtk_widget_show (value_entry);
	gtk_widget_set_usize (value_entry, 200, -1);
	gtk_table_attach (GTK_TABLE (table1), value_entry, 2, 3, 1, 2,
			  0, 0, 0, 0);


	/* predicate */

	vbox2 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, TRUE, TRUE, 0);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, TRUE, 0);

	predicate_label = gtk_label_new (_("Predicate"));
	gtk_widget_show (predicate_label);
	gtk_box_pack_start (GTK_BOX (hbox1), predicate_label,
			    FALSE, FALSE, 0);

	predicate_combo = gtk_combo_new ();
	gtk_widget_show (predicate_combo);
	gtk_widget_set_usize (predicate_combo, 120, -1);
	predicate_list = GTK_COMBO(predicate_combo)->list;
	gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(predicate_combo)->entry),
			       FALSE);

	combo_items = NULL;

	for(i = 0 ; i < (gint) (sizeof(predicate_text) / sizeof(gchar *)) ;
	    i++) {
		combo_items = g_list_append(combo_items,
					    (gpointer) _(predicate_text[i]));
	}
	gtk_combo_set_popdown_strings(GTK_COMBO(predicate_combo), combo_items);

	g_list_free(combo_items);

	gtk_box_pack_start (GTK_BOX (hbox1), predicate_combo,
			    FALSE, FALSE, 0);

	vbox3 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox3);
	gtk_box_pack_start (GTK_BOX (hbox1), vbox3, TRUE, TRUE, 0);

	PACK_CHECK_BUTTON (vbox3, case_chkbtn, _("Case sensitive"));
	PACK_CHECK_BUTTON (vbox3, regexp_chkbtn, _("Use regexp"));

	/* register / substitute / delete */

	reg_hbox = gtk_hbox_new (FALSE, 4);
	gtk_widget_show (reg_hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), reg_hbox, FALSE, FALSE, 0);

	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_widget_show (arrow);
	gtk_box_pack_start (GTK_BOX (reg_hbox), arrow, FALSE, FALSE, 0);
	gtk_widget_set_usize (arrow, -1, 16);

	btn_hbox = gtk_hbox_new (TRUE, 4);
	gtk_widget_show (btn_hbox);
	gtk_box_pack_start (GTK_BOX (reg_hbox), btn_hbox, FALSE, FALSE, 0);

	reg_btn = gtk_button_new_with_label (_("Register"));
	gtk_widget_show (reg_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), reg_btn, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (reg_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_matcher_register_cb), NULL);

	subst_btn = gtk_button_new_with_label (_(" Substitute "));
	gtk_widget_show (subst_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), subst_btn, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (subst_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_matcher_substitute_cb),
			    NULL);

	del_btn = gtk_button_new_with_label (_("Delete"));
	gtk_widget_show (del_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), del_btn, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (del_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_matcher_delete_cb), NULL);

	/* boolean operation */

	bool_op_label = gtk_label_new (_("Boolean Op"));
	gtk_misc_set_alignment (GTK_MISC (value_label), 0, 0.5);
	gtk_widget_show (bool_op_label);
	gtk_box_pack_start (GTK_BOX (btn_hbox), bool_op_label,
			    FALSE, FALSE, 0);

	bool_op_combo = gtk_combo_new ();
	gtk_widget_show (bool_op_combo);
	gtk_widget_set_usize (bool_op_combo, 50, -1);
	bool_op_list = GTK_COMBO(bool_op_combo)->list;
	gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(bool_op_combo)->entry),
			       FALSE);

	combo_items = NULL;

	for(i = 0 ; i < (gint) (sizeof(bool_op_text) / sizeof(gchar *)) ;
	    i++) {
		combo_items = g_list_append(combo_items,
					    (gpointer) _(bool_op_text[i]));
	}
	gtk_combo_set_popdown_strings(GTK_COMBO(bool_op_combo), combo_items);

	g_list_free(combo_items);

	gtk_box_pack_start (GTK_BOX (btn_hbox), bool_op_combo,
			    FALSE, TRUE, 0);

	cond_hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (cond_hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), cond_hbox, TRUE, TRUE, 0);

	cond_scrolledwin = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (cond_scrolledwin);
	gtk_widget_set_usize (cond_scrolledwin, -1, 150);
	gtk_box_pack_start (GTK_BOX (cond_hbox), cond_scrolledwin,
			    TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (cond_scrolledwin),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	cond_clist = gtk_clist_new_with_titles(1, title);
	gtk_widget_show (cond_clist);
	gtk_container_add (GTK_CONTAINER (cond_scrolledwin), cond_clist);
	gtk_clist_set_column_width (GTK_CLIST (cond_clist), 0, 80);
	gtk_clist_set_selection_mode (GTK_CLIST (cond_clist),
				      GTK_SELECTION_BROWSE);
	GTK_WIDGET_UNSET_FLAGS (GTK_CLIST (cond_clist)->column[0].button,
				GTK_CAN_FOCUS);
	gtk_signal_connect (GTK_OBJECT (cond_clist), "select_row",
			    GTK_SIGNAL_FUNC (prefs_matcher_select), NULL);

	btn_vbox = gtk_vbox_new (FALSE, 8);
	gtk_widget_show (btn_vbox);
	gtk_box_pack_start (GTK_BOX (cond_hbox), btn_vbox, FALSE, FALSE, 0);

	up_btn = gtk_button_new_with_label (_("Up"));
	gtk_widget_show (up_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), up_btn, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (up_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_matcher_up), NULL);

	down_btn = gtk_button_new_with_label (_("Down"));
	gtk_widget_show (down_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), down_btn, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (down_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_matcher_down), NULL);

	gtk_widget_show_all(window);

	matcher.window    = window;
	matcher.close_btn = close_btn;

	matcher.criteria_list = criteria_list;
	matcher.header_combo = header_combo;
	matcher.header_entry = header_entry;
	matcher.header_label = header_label;
	matcher.value_entry = value_entry;
	matcher.value_label = value_label;
	matcher.predicate_label = predicate_label;
	matcher.predicate_list = predicate_list;
	matcher.predicate_combo = predicate_combo;
	matcher.case_chkbtn = case_chkbtn;
	matcher.regexp_chkbtn = regexp_chkbtn;
	matcher.bool_op_list = bool_op_list;

	matcher.cond_clist   = cond_clist;
}

static gint prefs_matcher_clist_set_row(gint row, MatcherProp * prop)
{
	GtkCList *clist = GTK_CLIST(matcher.cond_clist);
	gchar * cond_str[1];
	gchar * matcher_str;

	if (prop == NULL) {
		cond_str[0] = _("(New)");
		return gtk_clist_append(clist, cond_str);
	}

	matcher_str = matcherprop_to_string(prop);
	cond_str[0] = matcher_str;
	if (row < 0)
		row = gtk_clist_append(clist, cond_str);
	else
		gtk_clist_set_text(clist, row, 0, cond_str[0]);
	g_free(matcher_str);

	return row;
}

static void prefs_matcher_reset_condition(void)
{
	gtk_list_select_item(GTK_LIST(matcher.criteria_list), 0);
	gtk_list_select_item(GTK_LIST(matcher.predicate_list), 0);
	gtk_entry_set_text(GTK_ENTRY(matcher.header_entry), "");
	gtk_entry_set_text(GTK_ENTRY(matcher.value_entry), "");
}

 static void prefs_matcher_set_dialog(void)
{
	GtkCList *clist = GTK_CLIST(matcher.cond_clist);
	GSList * cur;
	gboolean bool_op = 0;

	gtk_clist_freeze(clist);
	gtk_clist_clear(clist);

	prefs_matcher_clist_set_row(-1, NULL);
	if (tmp_matchers != NULL) {
		for (cur = tmp_matchers->matchers ; cur != NULL ;
		     cur = g_slist_next(cur)) {
			MatcherProp * prop;
			prop = (MatcherProp *) cur->data;
			prefs_matcher_clist_set_row(-1, prop);
			matcherprop_free(prop);
		}

		bool_op = tmp_matchers->bool_and;
	}
	gtk_clist_thaw(clist);

	gtk_list_select_item(GTK_LIST(matcher.bool_op_list), bool_op);

	prefs_matcher_reset_condition();
}

static void prefs_matcher_set_list(void)
{
	gchar * matcher_str;
	MatcherProp * prop;
	gint row = 1;
	GSList * l;
	gchar * tmp;

	if (tmp_matchers == NULL)
		return;

	/* free old */

	for(l = tmp_matchers->matchers ; l != NULL ; l = g_slist_next(l))
		matcherprop_free((MatcherProp *) l->data);
	g_slist_free(tmp_matchers->matchers);
	tmp_matchers->matchers = NULL;

	/* set new */

	while (gtk_clist_get_text(GTK_CLIST(matcher.cond_clist),
				  row, 0, &matcher_str)) {

		if (strcmp(matcher_str, _("(New)")) != 0) {
			tmp = matcher_str;
			prop = matcherprop_parse(&tmp);
			
			if (tmp == NULL)
				break;
			
			tmp_matchers->matchers =
				g_slist_append(tmp_matchers->matchers, prop);
		}
		row ++;
	}

	tmp_matchers->bool_and = get_sel_from_list(GTK_LIST(matcher.bool_op_list));

}

static gint prefs_matcher_get_matching_from_criteria(gint criteria_id)
{
	switch (criteria_id) {
	case CRITERIA_ALL:
		return MATCHING_ALL;
	case CRITERIA_SUBJECT:
		return MATCHING_SUBJECT;
	case CRITERIA_FROM:
		return MATCHING_FROM;
	case CRITERIA_TO:
		return MATCHING_TO;
	case CRITERIA_CC:
		return MATCHING_CC;
	case CRITERIA_TO_OR_CC:
		return MATCHING_TO_OR_CC;
	case CRITERIA_NEWSGROUPS:
		return MATCHING_NEWSGROUPS;
	case CRITERIA_AGE_GREATER:
		return MATCHING_AGE_GREATER;
	case CRITERIA_AGE_LOWER:
		return MATCHING_AGE_LOWER;
	case CRITERIA_HEADER:
		return MATCHING_HEADER;
	case CRITERIA_HEADERS_PART:
		return MATCHING_HEADERS_PART;
	case CRITERIA_BODY_PART:
		return MATCHING_BODY_PART;
	case CRITERIA_MESSAGE:
		return MATCHING_MESSAGE;
	default:
		return -1;
	}
}

static gint prefs_matcher_not_criteria(gint matcher_criteria)
{
	switch(matcher_criteria) {
	case MATCHING_SUBJECT:
		return MATCHING_NOT_SUBJECT;
	case MATCHING_FROM:
		return MATCHING_NOT_FROM;
	case MATCHING_TO:
		return MATCHING_NOT_TO;
	case MATCHING_CC:
		return MATCHING_NOT_CC;
	case MATCHING_TO_OR_CC:
		return MATCHING_NOT_TO_AND_NOT_CC;
	case MATCHING_NEWSGROUPS:
		return MATCHING_NOT_NEWSGROUPS;
	case MATCHING_HEADER:
		return MATCHING_NOT_HEADER;
	case MATCHING_HEADERS_PART:
		return MATCHING_NOT_HEADERS_PART;
	case MATCHING_MESSAGE:
		return MATCHING_NOT_MESSAGE;
	case MATCHING_BODY_PART:
		return MATCHING_NOT_BODY_PART;
	default:
		return matcher_criteria;
	}
}

static MatcherProp * prefs_matcher_dialog_to_matcher()
{
	MatcherProp * matcherprop;
	gint criteria;
	gint matchtype;
	gint value_pred;
	gint value_criteria;
	gboolean use_regexp;
	gboolean case_sensitive;
	gchar * header;
	gchar * expr;
	gint age;
	gchar * age_str;

	value_criteria = get_sel_from_list(GTK_LIST(matcher.criteria_list));

	criteria = prefs_matcher_get_matching_from_criteria(value_criteria);

	value_pred = get_sel_from_list(GTK_LIST(matcher.predicate_list));

	use_regexp = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(matcher.regexp_chkbtn));
	case_sensitive = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(matcher.case_chkbtn));

	if (value_pred != PREDICATE_CONTAINS)
		criteria = prefs_matcher_not_criteria(criteria);

	if (use_regexp) {
		if (case_sensitive)
			matchtype = MATCHING_REGEXP;
		else
			matchtype = MATCHING_REGEXPCASE;
	}
	else {
		if (case_sensitive)
			matchtype = MATCHING_MATCH;
		else
			matchtype = MATCHING_MATCHCASE;
	}

	header = NULL;
	expr = NULL;
	age = 0;

	switch (value_criteria) {
	case CRITERIA_ALL:
		break;

	case CRITERIA_SUBJECT:
	case CRITERIA_FROM:
	case CRITERIA_TO:
	case CRITERIA_CC:
	case CRITERIA_TO_OR_CC:
	case CRITERIA_NEWSGROUPS:
	case CRITERIA_HEADERS_PART:
	case CRITERIA_BODY_PART:
	case CRITERIA_MESSAGE:
		expr = gtk_entry_get_text(GTK_ENTRY(matcher.value_entry));

		if (*expr == '\0') {
		    alertpanel_error(_("Match string is not set."));
		    return NULL;
		}

		break;

	case CRITERIA_AGE_GREATER:
	case CRITERIA_AGE_LOWER:
		age_str = gtk_entry_get_text(GTK_ENTRY(matcher.value_entry));

		if (*age_str == '\0') {
		    alertpanel_error(_("Age is not set."));
		    return NULL;
		}

		age = atoi(age_str);

		break;

	case CRITERIA_HEADER:

		header = gtk_entry_get_text(GTK_ENTRY(matcher.header_entry));
		expr = gtk_entry_get_text(GTK_ENTRY(matcher.value_entry));

		if (*header == '\0') {
		    alertpanel_error(_("Header name is not set."));
		    return NULL;
		}
		if (*expr == '\0') {
		    alertpanel_error(_("Match string is not set."));
		    return NULL;
		}

		break;
	}

	matcherprop =  matcherprop_new(criteria, header, matchtype, expr, age);
	
	return matcherprop;
}

static void prefs_matcher_register_cb(void)
{
	MatcherProp * matcherprop;
	
	matcherprop = prefs_matcher_dialog_to_matcher();
	if (matcherprop == NULL)
		return;

	prefs_matcher_clist_set_row(-1, matcherprop);

	matcherprop_free(matcherprop);

	prefs_matcher_reset_condition();
}

static void prefs_matcher_substitute_cb(void)
{
	GtkCList *clist = GTK_CLIST(matcher.cond_clist);
	gint row;
	MatcherProp * matcherprop;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row == 0)
		return;
	
	matcherprop = prefs_matcher_dialog_to_matcher();
	if (matcherprop == NULL)
		return;

	prefs_matcher_clist_set_row(row, matcherprop);

	matcherprop_free(matcherprop);

	prefs_matcher_reset_condition();
}

static void prefs_matcher_delete_cb(void)
{
	GtkCList *clist = GTK_CLIST(matcher.cond_clist);
	gint row;

	if (!clist->selection) return;
	row = GPOINTER_TO_INT(clist->selection->data);
	if (row == 0)
		return;

	gtk_clist_remove(clist, row);
}

static void prefs_matcher_up(void)
{
	GtkCList *clist = GTK_CLIST(matcher.cond_clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row > 1)
		gtk_clist_row_move(clist, row, row - 1);
}

static void prefs_matcher_down(void)
{
	GtkCList *clist = GTK_CLIST(matcher.cond_clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row >= 1 && row < clist->rows - 1)
		gtk_clist_row_move(clist, row, row + 1);
}

static void prefs_matcher_select(GtkCList *clist, gint row, gint column,
				 GdkEvent *event)
{
	gchar * matcher_str;
	gchar * tmp;
	MatcherProp * prop;
	gboolean negative_cond;

	if (!gtk_clist_get_text(GTK_CLIST(matcher.cond_clist),
				row, 0, &matcher_str))
		return;

	negative_cond = FALSE;

	if (row == 0) {
		prefs_matcher_reset_condition();
		return;
	}

	tmp = matcher_str;
	prop = matcherprop_parse(&tmp);
	if (tmp == NULL)
		return;

	switch(prop->criteria) {
	case MATCHING_ALL:
		gtk_list_select_item(GTK_LIST(matcher.criteria_list),
				     CRITERIA_ALL);
		break;

	case MATCHING_NOT_SUBJECT:
		negative_cond = TRUE;
	case MATCHING_SUBJECT:
		gtk_list_select_item(GTK_LIST(matcher.criteria_list),
				     CRITERIA_SUBJECT);
		break;

	case MATCHING_NOT_FROM:
		negative_cond = TRUE;
	case MATCHING_FROM:
		gtk_list_select_item(GTK_LIST(matcher.criteria_list),
				     CRITERIA_FROM);
		break;

	case MATCHING_NOT_TO:
		negative_cond = TRUE;
	case MATCHING_TO:
		gtk_list_select_item(GTK_LIST(matcher.criteria_list),
				     CRITERIA_TO);
		break;

	case MATCHING_NOT_CC:
		negative_cond = TRUE;
	case MATCHING_CC:
		gtk_list_select_item(GTK_LIST(matcher.criteria_list),
				     CRITERIA_CC);
		break;

	case MATCHING_NOT_NEWSGROUPS:
		negative_cond = TRUE;
	case MATCHING_NEWSGROUPS:
		gtk_list_select_item(GTK_LIST(matcher.criteria_list),
				     CRITERIA_NEWSGROUPS);
		break;

	case MATCHING_NOT_TO_AND_NOT_CC:
		negative_cond = TRUE;
	case MATCHING_TO_OR_CC:
		gtk_list_select_item(GTK_LIST(matcher.criteria_list),
				     CRITERIA_TO_OR_CC);
		break;

	case MATCHING_NOT_BODY_PART:
		negative_cond = TRUE;
	case MATCHING_BODY_PART:
		gtk_list_select_item(GTK_LIST(matcher.criteria_list),
				     CRITERIA_BODY_PART);
		break;

	case MATCHING_NOT_MESSAGE:
		negative_cond = TRUE;
	case MATCHING_MESSAGE:
		gtk_list_select_item(GTK_LIST(matcher.criteria_list),
				     CRITERIA_MESSAGE);
		break;

	case MATCHING_NOT_HEADERS_PART:
		negative_cond = TRUE;
	case MATCHING_HEADERS_PART:
		gtk_list_select_item(GTK_LIST(matcher.criteria_list),
				     CRITERIA_HEADERS_PART);
		break;

	case MATCHING_NOT_HEADER:
		negative_cond = TRUE;
	case MATCHING_HEADER:
		gtk_list_select_item(GTK_LIST(matcher.criteria_list),
				     CRITERIA_HEADER);
		break;

	case MATCHING_AGE_GREATER:
		gtk_list_select_item(GTK_LIST(matcher.criteria_list),
				     CRITERIA_AGE_GREATER);
		break;

	case MATCHING_AGE_LOWER:
		gtk_list_select_item(GTK_LIST(matcher.criteria_list),
				     CRITERIA_AGE_LOWER);
		break;
	}
	
	switch(prop->criteria) {
	case MATCHING_ALL:
		break;

	case MATCHING_NOT_SUBJECT:
	case MATCHING_NOT_FROM:
	case MATCHING_NOT_TO:
	case MATCHING_NOT_CC:
	case MATCHING_NOT_TO_AND_NOT_CC:
	case MATCHING_NOT_NEWSGROUPS:
	case MATCHING_NOT_HEADERS_PART:
	case MATCHING_NOT_BODY_PART:
	case MATCHING_NOT_MESSAGE:
	case MATCHING_SUBJECT:
	case MATCHING_FROM:
	case MATCHING_TO:
	case MATCHING_CC:
	case MATCHING_TO_OR_CC:
	case MATCHING_NEWSGROUPS:
	case MATCHING_HEADERS_PART:
	case MATCHING_BODY_PART:
	case MATCHING_MESSAGE:
		gtk_entry_set_text(GTK_ENTRY(matcher.value_entry), prop->expr);
		break;

	case MATCHING_AGE_GREATER:
	case MATCHING_AGE_LOWER:
		gtk_entry_set_text(GTK_ENTRY(matcher.value_entry), itos(prop->age));
		break;

	case MATCHING_NOT_HEADER:
	case MATCHING_HEADER:
		gtk_entry_set_text(GTK_ENTRY(matcher.header_entry), prop->header);
		gtk_entry_set_text(GTK_ENTRY(matcher.value_entry), prop->expr);
		break;
	}

	if (negative_cond)
		gtk_list_select_item(GTK_LIST(matcher.predicate_list), 1);
	else
		gtk_list_select_item(GTK_LIST(matcher.predicate_list), 0);

	switch(prop->matchtype) {
	case MATCHING_MATCH:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(matcher.regexp_chkbtn), FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(matcher.case_chkbtn), TRUE);
		break;

	case MATCHING_MATCHCASE:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(matcher.regexp_chkbtn), FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(matcher.case_chkbtn), FALSE);
		break;

	case MATCHING_REGEXP:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(matcher.regexp_chkbtn), TRUE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(matcher.case_chkbtn), TRUE);
		break;

	case MATCHING_REGEXPCASE:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(matcher.regexp_chkbtn), TRUE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(matcher.case_chkbtn), FALSE);
		break;
	}
}

static void prefs_matcher_criteria_select(GtkList *list,
					  GtkWidget *widget,
					  gpointer user_data)
{
	gint value;

	value = get_sel_from_list(GTK_LIST(matcher.criteria_list));

	switch (value) {
	case CRITERIA_ALL:
		gtk_widget_set_sensitive(matcher.header_combo, FALSE);
		gtk_widget_set_sensitive(matcher.header_label, FALSE);
		gtk_widget_set_sensitive(matcher.value_label, FALSE);
		gtk_widget_set_sensitive(matcher.value_entry, FALSE);
		gtk_widget_set_sensitive(matcher.predicate_label, FALSE);
		gtk_widget_set_sensitive(matcher.predicate_combo, FALSE);
		gtk_widget_set_sensitive(matcher.case_chkbtn, FALSE);
		gtk_widget_set_sensitive(matcher.regexp_chkbtn, FALSE);
		break;

	case CRITERIA_SUBJECT:
	case CRITERIA_FROM:
	case CRITERIA_TO:
	case CRITERIA_CC:
	case CRITERIA_TO_OR_CC:
	case CRITERIA_NEWSGROUPS:
	case CRITERIA_HEADERS_PART:
	case CRITERIA_BODY_PART:
	case CRITERIA_MESSAGE:
		gtk_widget_set_sensitive(matcher.header_combo, FALSE);
		gtk_widget_set_sensitive(matcher.header_label, FALSE);
		gtk_widget_set_sensitive(matcher.value_label, TRUE);
		gtk_widget_set_sensitive(matcher.value_entry, TRUE);
		gtk_widget_set_sensitive(matcher.predicate_label, TRUE);
		gtk_widget_set_sensitive(matcher.predicate_combo, TRUE);
		gtk_widget_set_sensitive(matcher.case_chkbtn, TRUE);
		gtk_widget_set_sensitive(matcher.regexp_chkbtn, TRUE);
		break;

	case CRITERIA_AGE_GREATER:
	case CRITERIA_AGE_LOWER:
		gtk_widget_set_sensitive(matcher.header_combo, FALSE);
		gtk_widget_set_sensitive(matcher.header_label, FALSE);
		gtk_widget_set_sensitive(matcher.value_label, TRUE);
		gtk_widget_set_sensitive(matcher.value_entry, TRUE);
		gtk_widget_set_sensitive(matcher.predicate_label, FALSE);
		gtk_widget_set_sensitive(matcher.predicate_combo, FALSE);
		gtk_widget_set_sensitive(matcher.case_chkbtn, FALSE);
		gtk_widget_set_sensitive(matcher.regexp_chkbtn, FALSE);
		break;

	case CRITERIA_HEADER:
		gtk_widget_set_sensitive(matcher.header_combo, TRUE);
		gtk_widget_set_sensitive(matcher.header_label, TRUE);
		gtk_widget_set_sensitive(matcher.value_label, TRUE);
		gtk_widget_set_sensitive(matcher.value_entry, TRUE);
		gtk_widget_set_sensitive(matcher.predicate_label, TRUE);
		gtk_widget_set_sensitive(matcher.predicate_combo, TRUE);
		gtk_widget_set_sensitive(matcher.case_chkbtn, TRUE);
		gtk_widget_set_sensitive(matcher.regexp_chkbtn, TRUE);
		break;
	}
}

static void prefs_matcher_key_pressed(GtkWidget *widget, GdkEventKey *event,
				     gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		prefs_matcher_close();
}

static void prefs_matcher_close(void)
{
	prefs_matcher_set_list();
	gtk_widget_hide(matcher.window);
}

static gint prefs_matcher_deleted(GtkWidget *widget, GdkEventAny *event,
				  gpointer data)
{
	prefs_matcher_close();
	return TRUE;
}
