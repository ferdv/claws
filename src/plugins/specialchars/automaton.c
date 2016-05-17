
#include "automaton.h"

#include "utils.h"

static StateTransition example_states[] = {
  {0, '\\', 1},
  {1, 'r', 2},
  {2, 'i', 3},
  {3, 'g', 4},
  {4, 'h', 5},
  {5, 't', 6},
  {6, 'a', 7},
  {7, 'r', 8},
  {8, 'r', 9},
  {9, 'o', 10},
  {10, 'w', 11},
  {11, '\x03', 13},
  {11, ' ', 13},
  {11, '{', 12},
  {12, '}', 13},
};

static Automaton automaton_;

static gboolean example_accept(guint state, gpointer data, gpointer result) {
  switch (state) {
    case 13:
      return TRUE;
    default: 
      return FALSE;
  }
}

Automaton example_automaton = { 
  example_states, 
  sizeof(example_states) / sizeof(StateTransition),
  example_accept,
  NULL
};

Automaton new_automaton(
    StateTransition *table, 
    gint size, 
    AcceptFunc accept,
    void *accept_data) {
  Automaton result = {table, size, accept, accept_data};

  return result;
}

void load_automaton(Automaton a) {
  automaton_ = a;
}

void print_automaton(Automaton a) {
  StateTransition tr;
  for (int i = 0; i < a.size; ++i) {
    tr = a.table[i];
    debug_print("%d -- %c -> %d\n", tr.from_state, tr.input, tr.to_state);
  }
}

static inline guint cantor_key(guint a, guint b) {
  return ((a + b) * (a + b + 1) + b + 1) / 2;
}

static inline guint pair_key(guint a, guint b) {
  return (a << 16) + b;
}

static gint key_compare(gconstpointer a, gconstpointer b, gpointer data) {
  return a - b;
}

gboolean traverse(guint *a, StateTransition *st, StateTransition **st_table) {
  **st_table = *st;
  ++(*st_table);
  return FALSE;
}

gboolean accept_replace(guint state, GHashTable *table, gchar **out) {
  gchar *str;
  if ((str = g_hash_table_lookup(table, GINT_TO_POINTER(state))) != NULL) {
    debug_print("State %d, substitute with %s.\n", state, str);
    *out = str;
    return TRUE;
  }
  else {
    return FALSE;
  }
}

Automaton *generate_automaton(Substitution *substs, guint numsubsts) {
  gchar *ch;
  guint state, prevstate, freshstate;
  guint key;
  StateTransition *st, *st_table;
  /*GHashTable *table = 
    g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);*/
  GTree *tree = g_tree_new_full(key_compare, NULL, NULL, g_free);
  //GHashTableIter iter;
  GHashTable *subst_table =
    g_hash_table_new(g_direct_hash, g_direct_equal);

  freshstate = 1;

  for (int i = 0; i < numsubsts; ++i) {
    ch = substs[i].find;
    state = 0;
    prevstate = 0;
    do {
      key = pair_key(state, *ch);

      //if ((st = g_hash_table_lookup(table, GINT_TO_POINTER(key))) == NULL) {
      if ((st = g_tree_lookup(tree, GINT_TO_POINTER(key))) == NULL) {
        st = g_new(StateTransition, 1);
        st->from_state = state;
        st->input = *ch;
        st->to_state = freshstate;
        prevstate = state;
        state = freshstate;
        ++freshstate;
        debug_print("Char %c, key %d not found. Inserting %d -- %c -> %d.\n", 
            *ch, key, st->from_state, st->input, st->to_state);
        //g_hash_table_insert(table, GINT_TO_POINTER(key), st);
        g_tree_insert(tree, GINT_TO_POINTER(key), st);
      }
      else if (*ch != st->input) {
        debug_print("WARNING: Transition tree is corrupted!");
      }
      else {
        prevstate = state;
        state = st->to_state;
      }
    } while (*(++ch) != '\0');

    g_hash_table_insert(
        subst_table, 
        GINT_TO_POINTER(state), 
        substs[i].subst);
  }
  
  //int size = g_hash_table_size(table);
  int size = g_tree_nnodes(tree);
  st_table = g_new(StateTransition, size);
  StateTransition *first = st_table;
/*  g_hash_table_iter_init(&iter, table);
  int i = 0;
  gpointer pkey;
  while (g_hash_table_iter_next(&iter, &pkey, &st)) {
    st_table[i] = *st;
    ++i;
  }

  g_hash_table_destroy(table);*/

  g_tree_foreach(tree, (GTraverseFunc) traverse, &first);
  g_tree_destroy(tree);

  Automaton *a = g_new(Automaton, 1);
  a->table = st_table;
  a->size = size;
  a->accept = (AcceptFunc) accept_replace;
  a->accept_data = subst_table;

  return a;
}

static gint next_state(
    Automaton a,
    guint state, 
    gunichar ch) {

  for (int i = 0; i < a.size; ++i) {
    if (a.table[i].from_state == state && a.table[i].input == ch)
      return a.table[i].to_state;
  }

  return -1;
}

gboolean match_iter(GtkTextIter *iter, gpointer result) {
  return match_automaton_iter(automaton_, iter, result);
}

gboolean match_automaton_iter(Automaton a, GtkTextIter *iter, gpointer result) {
  gunichar ch;
  gint state = 0;
  gint nstate;

  // we'll assume that the starting state is not accepting, i.e., we don't match
  // an empty string
  do {
    ch = gtk_text_iter_get_char(iter);

    nstate = next_state(a, state, ch);
    
    debug_print("State transition: %d to %d\n", state, nstate);

    if (nstate == -1)
      return FALSE;
    else if ((*a.accept)(nstate, a.accept_data, result))
      return TRUE;

    state = nstate;

  } while (gtk_text_iter_forward_char(iter));

  return FALSE;
}


