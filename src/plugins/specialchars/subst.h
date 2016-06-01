#ifndef __SUBST_H__
#define __SUBST_H__

#include <glib.h>

typedef struct Substitution_ Substitution;

struct Substitution_ {
  gchar *replace;
  gchar *with;
};

struct Substs { 
  guint count;
  Substitution *data;
  gsize with_buffer_size;
  gchar *with_buffer;
};

void subst_print_substs(struct Substs substs);

/**
 * Load substitutions from a key file (.ini).
 */
gboolean subst_load_from_file(
    const gchar *fname, 
    struct Substs *substs,
    GError **error);

gboolean subst_with_data_packed(struct Substs *substs);

#endif /* __SUBSTITUTIONS_H__ */
