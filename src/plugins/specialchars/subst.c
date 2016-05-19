
#include <string.h>
//#include <locale.h>

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


/*enum parse_state = {
  SPACE,
  SEARCH,
  SUBST,
  COMMENT
};
*/
/*
inline gchar *eat_until_endline(gchar *contents, gchar *end) {
  // ignore the rest of the line 
  while (contents < end && *contents != '\n')
    ++contents;

  if (*contents == '\n')
    ++contents;

  return contents;
}

inline gchar *eat_whitespace(gchar *contents, gchar *end) {
  while (contents < end && *contents == ' ' || *contents == '\t')
    ++contents;

  return contents;
}

inline gchar *eat_string(gchar *contents, gchar *end, gchar **output) {
  const gchar *start = contents;

  while (contents < end && g_ascii_isprint(*contents))
    ++contents;
  
  if (contents - start > 0) {
    *output = g_new(gchar, contents - start + 1);
    memcpy(
  }

}
*/

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
    g_free(with);
    with = g_new(gchar, strlen(u8ch) + 1);
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
  //GMappedFile *file;
  GError *error = NULL;
  //gchar *contents, *end, *token_start;
  //guint line = 1;
  //gchar *linebuf[256];
  //parse_state state = SPACE;
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
  
/*  if ((file = g_mapped_file_new(path, FALSE, &error)) == NULL) {
    debug_print("Error opening %s: %s\n", path, error->message);
    g_free(path);
    g_propagate_error(out_error, error);
    return FALSE;
  }

  g_free(path);

  if ((contents = g_mapped_file_get_contents(file)) == NULL) {
    debug_print("File is empty.\n");
    g_mapped_file_unref(file);
    return FALSE;
  }

  end = contents + g_mapped_file_get_length(file);

  while (contents < end) {
    eat_whitespace(contents);

    switch (*contents) {
      case '#':
        eat_until_endline(contents);
        break;
      case '\n':
        switch (state) { 
          case SPACE:
          case COMMENT:
            state = SPACE;
            ++line;
            break;
          case SEARCH:
            errormsg = g_strdup_printf(
                _("%s: Error parsing symbols file, line %d"), 
                PLUGIN_NAME, 
                line);
            debug_print("%s\n", errormsg);
            g_set_error_literal(out_error, G_FILE_ERROR, 0, errormsg);
            goto error;
          case SUBST:

            break;
        }
      case ' ':
      case '\t':
        switch (state) {
          case SUBST:

        }
        break;
      default:
        break;
    }
    ++contents;
  }
  return TRUE;

error:
  
  return FALSE;*/
} 


