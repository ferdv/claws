
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

size_t put_subst(Substitution *subst, gchar *replace, gchar *with) {
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

  return strlen(with);
}

/* pack the substitution strings into a contiguous buffer for easy caching */
gchar *pack_with_data(struct Substs *substs, gsize bufstep) {
  gsize bufsize = substs->count * bufstep;
  debug_print("bufsize = %lu, bufstep = %lu, count = %d\n", bufsize, bufstep, substs->count);
  gchar *buffer = g_new(gchar, bufsize);
  gchar *current = buffer;

  for (int i = 0; i < substs->count; ++i, current += bufstep) {
    g_assert(current < buffer + bufsize);
    g_assert_cmpuint(strlen(substs->data[i].with), <, bufstep);
    strncpy(current, substs->data[i].with, bufstep);
    g_free(substs->data[i].with);
    substs->data[i].with = current;
  }

  debug_print("with_buffer = %p\n", buffer);
  substs->with_buffer_size = bufsize;
  substs->with_buffer = buffer;

  return buffer;
}

gboolean subst_with_data_packed(struct Substs *substs) {
  if ((substs->with_buffer == NULL) || (substs->with_buffer_size == 0))
    return FALSE;

  gchar *first = substs->data[0].with;
  gchar *last = substs->data[substs->count - 1].with;
  gchar *buffer_end = substs->with_buffer + substs->with_buffer_size;
  debug_print("first = %p, last = %p, with_buffer = %p, end = %p\n", first, last,  substs->with_buffer, buffer_end);
  if (first == substs->with_buffer && last < buffer_end)
    return TRUE;

  return FALSE;
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

  debug_print("count = %d", count);
  substs->count = count;
  substs->data = g_new(Substitution, count);
  substs->with_buffer = NULL;
  substs->with_buffer_size = 0;

  gsize longest = 0;

  for (int i = 0; i < count; ++i) {
    gchar *val;
    
    if ((val = g_key_file_get_value(file, SUBST_GROUP, keys[i], NULL /*&error*/)) 
        == NULL) {
      continue;
    }

    longest = MAX(longest, put_subst(&substs->data[i], keys[i], val));
  }

  pack_with_data(substs, longest + 1);

  g_free(path);
  g_key_file_free(file);

  return TRUE;
} 

