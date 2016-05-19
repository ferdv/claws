
#include <string.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "defs.h"
#include "subst.h"
#include "sc_utils.h"

#define SUBST_GROUP "Substitutions"

#define do_file_error(gerror, msg) (\
    debug_print("%s\n", msg),\
    g_set_error_literal(gerror, G_FILE_ERROR, 0, msg))

void subst_print_substs(struct Substs substs) {
  for (int i = 0; i < substs.count; ++i) {
    g_printf("%s / %s]\n", substs.data[i].replace, substs.data[i].with);
  }
}

void put_subst(Substitution *subst, gchar *replace, gchar *with) {
  gchar u8ch[7];
  gint u8ch_len;
  gunichar uch;

  if (sscanf(with, "U+%06X", &uch)) {
    u8ch_len = g_unichar_to_utf8(uch, u8ch);
    u8ch[u8ch_len] = '\0';
    g_assert_cmpuint(u8ch_len, <, strlen(with));
    strcpy(with, u8ch);
  }

  subst->replace = replace;
  subst->with = with;

  return;
}

gboolean subst_load_from_file(
    const gchar *fname, 
    struct Substs *substs, 
    GError **out_error) {

  gchar *path = rc_filepath(fname);
  gsize count;
  GError *error = NULL;
  gchar *errormsg = NULL;

  GKeyFile *file = g_key_file_new();
  gchar **keys;

  debug_print("Loading substitutions from %s.\n", path);
  if (!g_key_file_load_from_file(
        file, path, G_KEY_FILE_KEEP_COMMENTS, &error)) {
    debug_print("Error loading from %s: %s\n", path, error->message);
    g_propagate_error(out_error, error);
    g_free(path);
    return FALSE;
  }

  if (!g_key_file_has_group(file, SUBST_GROUP)) {
    errormsg = 
      g_strdup_printf(_("Substitution group \"%s\" missing."), SUBST_GROUP);
    do_file_error(out_error, errormsg);
    g_free(path);
    g_key_file_free(file);
    return FALSE;
  }

  if ((keys = g_key_file_get_keys(file, SUBST_GROUP, &count, &error)) == NULL) {
    errormsg = g_strdup(_("Error fetching keys."));
    do_file_error(out_error, errormsg);
    g_free(path);
    g_key_file_free(file);
    return FALSE;
  }

  substs->count = count;
  substs->data = g_new(Substitution, count);

  for (int i = 0; i < count; ++i) {
    gchar *val;
    
    if ((val = g_key_file_get_value(file, SUBST_GROUP, keys[i], NULL /*&error*/)) 
        == NULL) {
      continue;
    }

    put_subst(&substs->data[i], keys[i], val);
  }

  g_free(path);
  g_key_file_free(file);

  return TRUE;
} 

