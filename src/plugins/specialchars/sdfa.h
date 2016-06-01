#ifndef __SDFA_H__
#define __SDFA_H__

#include <glib.h>
#include <gtk/gtk.h>

#include "subst.h"

#if __STDC_VERSION__ >= 199901L
typedef _Bool sdfa_bool_t;
#else
typedef gboolean sdfa_bool_t;
#endif

typedef guchar sdfa_char_t;
typedef guint sdfa_state_t;

typedef struct SDFA_ SDFA;

typedef struct StateTransition_ StateTransition;

struct StateTransition_ {
  sdfa_state_t from;
  sdfa_char_t input;
  sdfa_state_t to;
  gshort result;
};

struct SDFA_ {
  guint table_size;
  StateTransition *table;
  gsize subst_buffer_size;
  gchar *subst_buffer;
};

SDFA *sdfa_new(StateTransition *table, gint size);
SDFA *sdfa_new_copy(StateTransition *table, gint size);

void sdfa_free(SDFA *a);

void sdfa_load(SDFA a);

//SDFA *sdfa_generate(Substitution *substs, guint numsubsts);
SDFA *sdfa_generate(struct Substs *substs);

void sdfa_print(SDFA a);

gboolean sdfa_dump(SDFA a, const gchar *fname);
//void free_automaton_data(Automaton a);

gboolean sdfa_match_iter_automaton(SDFA a, GtkTextIter *iter, gchar **result);

gboolean sdfa_match_iter(GtkTextIter *iter, gchar **result);


#endif /* __SDFA_H__ */
