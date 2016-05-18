
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

gboolean dump_hash_table(GHashTable *table, gchar **buffer, guint *size) {
  guint elements = g_hash_table_size(table);
  GHashTableIter iter;
  gchar *str, *bufp;
  gpointer pkey;
  
  *size = 0;

  g_hash_table_iter_init(&iter, table);
  while (g_hash_table_iter_next(&iter, &pkey, (gpointer) &str)) {
    *size += sizeof(guint) + strlen(str) + 1;
  }

  debug_print("Dumping hash table %d %u\n", elements, *size);
  *buffer = g_new(gchar, *size);
  bufp = *buffer;
  g_hash_table_iter_init(&iter, table);
  while (g_hash_table_iter_next(&iter, &pkey, (gpointer) &str)) {
    debug_print("%d -> %s.\n", GPOINTER_TO_INT(pkey), str);
    memcpy(bufp, &pkey, sizeof(GPOINTER_TO_INT(pkey)));
    bufp += sizeof(GPOINTER_TO_INT(pkey));
    bufp = g_stpcpy(bufp, str);
    ++bufp;
  }
  debug_print("");

  return TRUE;
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
  a->free_accept_data = (FreeDataFunc) g_hash_table_destroy;
  a->dump_accept_data = (DumpDataFunc) dump_hash_table;

  return a;
}

void free_automaton_data(Automaton a) {
  g_free(a.table);

  if (a.free_accept_data != NULL) {
    (*a.free_accept_data)(a.accept_data);
  }

  return;
}

void free_automaton(Automaton *a) {
  free_automaton_data(*a);
  g_free(a);

  return;
}

gboolean dump_automaton(Automaton a, const gchar *fname) {
  FILE *f;
  debug_print("Construct fullpath...\n");
  gchar *fullpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, fname, NULL);

  debug_print("Dumping automaton data to %s (size = %d)...\n", fullpath, a.size);
  if ((f = g_fopen(fullpath, "wb")) == NULL) {
    debug_print("Failed to open file %s for writing.\n", fullpath);
    perror(NULL);
    return FALSE;
  }

  // write the size
  if (fwrite(&a.size, sizeof(a.size), 1, f) < 1) {
    debug_print("Error while trying to write the table size. ");
    perror(NULL);
    fclose(f);
    return FALSE;
  }

  if (fwrite(a.table, sizeof(StateTransition), a.size, f) < a.size) {
    debug_print("Error while trying to dump %d state transitions.\n", a.size);
    perror(NULL);
    fclose(f);
    return FALSE;
  }

  if (a.accept_data != NULL && a.dump_accept_data != NULL) {
    gchar *buf;
    guint size;

    // get the dump of the accept data & add it to the file
    if ((*a.dump_accept_data)(a.accept_data, (gpointer) &buf, &size)) {
      if (fwrite(buf, sizeof(gchar), size, f) < size) {
        debug_print("Error while trying to dump %d bytes of accept data.\n", 
            size);
        perror(NULL);
        fclose(f);
      }
      g_free(buf);
    }
  }

  fclose(f);

  Automaton new;

  debug_print("Reading automaton data from %s...\n", fullpath);
  if ((f = g_fopen(fullpath, "rb")) == NULL) {
    debug_print("Failed to open file %s for reading.\n", fullpath);
    perror(NULL);
    return FALSE;
  }

  // read the size
  if (fread(&new.size, sizeof(new.size), 1, f) < 1) {
    debug_print("Error while reading the table size. ");
    perror(NULL);
    fclose(f);
    return FALSE;
  }

  debug_print("Read size: %d.\n", new.size);
  new.table = g_new(StateTransition, new.size);

  if (fread(new.table, sizeof(StateTransition), new.size, f) < new.size) {
    debug_print("Error while trying to read %d state transitions.\n", new.size);
    perror(NULL);
    g_free(new.table);
    fclose(f);
    return FALSE;
  }

  debug_print("Automaton read.\n");

  if (a.size == new.size && memcmp(a.table, new.table, new.size) == 0) {
    debug_print("Same automaton.\n");
  }
  else {
    debug_print("VERIFICATION FAILED.\n");
  }

  g_free(new.table);
  return TRUE;
}

static gint next_state(Automaton a, guint state, gunichar ch) {
  
  static guint last = 0;

  /* State transitions are assumed to be sorted by source state.
   * We optimise (a little) for lookups of subsequent source states. */
  if (a.table[last].from_state > state)
    last = 0;

  for (int i = last; i < a.size; ++i) {
    if (a.table[i].from_state == state && a.table[i].input == ch) {
      last = i;
      return a.table[i].to_state;
    }
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


