#ifndef __AUTOMATON_H__
#define __AUTOMATON_H__

#include <glib.h>
#include <gtk/gtk.h>

#include "subst.h"

typedef struct Automaton_ Automaton;

typedef struct StateTransition_ StateTransition;

typedef gboolean (*AcceptFunc)(guint state, gpointer data, gpointer result);

typedef void (*FreeDataFunc)(gpointer);

typedef gboolean (*DumpDataFunc)(
    gpointer accept_data, 
    void **outbuffer, 
    guint *size);


struct StateTransition_ {
  guint from_state;
  gunichar input;
  gint to_state;
};

Automaton example_automaton;

Automaton new_automaton(
    StateTransition *table, 
    gint size, 
    AcceptFunc accept,
    void *accept_data);

void load_automaton(Automaton automaton);

Automaton *generate_automaton(Substitution *substs, guint numsubsts);

void print_automaton(Automaton a);

void free_automaton_data(Automaton a);

void free_automaton(Automaton *a);

gboolean match_automaton_iter(Automaton a, GtkTextIter *iter, gpointer result);

gboolean match_iter(GtkTextIter *iter, gpointer result);

gboolean dump_automaton(Automaton a, const gchar *fname);

struct Automaton_ {
  StateTransition *table;
  guint size;
  AcceptFunc accept;
  gpointer accept_data;
  FreeDataFunc free_accept_data;
  DumpDataFunc dump_accept_data;
};

#endif /* __AUTOMATON_H__ */
