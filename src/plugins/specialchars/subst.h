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
};

void subst_print_substs(struct Substs substs);

gboolean subst_load_from_file(
    const gchar *fname, 
    struct Substs *subst,
    GError **error);

#endif /* __SUBSTITUTIONS_H__ */
