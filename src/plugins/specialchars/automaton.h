#ifndef __AUTOMATON_H__
#define __AUTOMATON_H__

#include <glib.h>
#include <gtk/gtk.h>


typedef struct Automaton_ Automaton;

typedef struct StateTransition_ StateTransition;

typedef struct Substitution_ Substitution;

typedef gboolean (*AcceptFunc)(guint, gpointer, gpointer);

struct StateTransition_ {
  guint from_state;
  gunichar input;
  gint to_state;
};

struct Substitution_ {
  char *find;
  char *subst;
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

gboolean match_automaton_iter(Automaton a, GtkTextIter *iter, gpointer result);

gboolean match_iter(GtkTextIter *iter, gpointer result);



struct Automaton_ {
  StateTransition *table;
  guint size;
  AcceptFunc accept;
  void *accept_data;
};

#endif /* __AUTOMATON_H__ */
