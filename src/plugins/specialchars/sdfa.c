
#include <glib.h>

#include "sdfa.h"

#include "utils.h"
#include "sc_utils.h"

#define MAX_ASCII 127

static SDFA sdfa_;

SDFA *sdfa_new(StateTransition *table, gint size) {
  SDFA *a = g_new(SDFA, 1); //{table, size, accept, accept_data};
  a->table = table;
  a->table_size = size;

  return a;
}

SDFA *sdfa_new_copy(StateTransition *table, gint size) {
  SDFA *a = g_new(SDFA, 1);
  a->table = g_new(StateTransition, size);
  memcpy(a->table, table, size);
  a->table = table;
  a->table_size = size;

  return a;
}

void sdfa_free(SDFA *a) {
  g_free(a->table);
  g_free(a);

  return;
}

void sdfa_load(SDFA a) {
  sdfa_ = a;
}

void sdfa_print(SDFA a) {
  StateTransition *tr;
  for (int i = 0; i < a.table_size; ++i) {
    tr = &a.table[i];
    debug_print("%d -- %c -> %d\n", tr->from, tr->input, tr->to);
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

SDFA *sdfa_generate(struct Substs *substs) {
  gchar *ch;
  guint state, prevstate, freshstate;
  guint key;
  StateTransition *st, *st_table;
  /*GHashTable *table =
    g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);*/

  if (!subst_with_data_packed(substs)) {
    debug_print("With data not packed!\n");
    return NULL;
  }

  GTree *tree = g_tree_new_full(key_compare, NULL, NULL, g_free);
  //GHashTableIter iter;
/*  GHashTable *subst_table =
    g_hash_table_new(g_direct_hash, g_direct_equal);*/

  freshstate = 1;

  for (int i = 0; i < substs->count; ++i) {
    ch = substs->data[i].replace;
    state = 0;
    prevstate = 0;
    do {
      key = pair_key(state, *ch);

      //if ((st = g_hash_table_lookup(table, GINT_TO_POINTER(key))) == NULL) {
      if ((st = g_tree_lookup(tree, GINT_TO_POINTER(key))) == NULL) {
        st = g_new(StateTransition, 1);
        st->from = state;
        st->input = *ch;
        st->to = freshstate;
        st->result = -1;
        prevstate = state;
        state = freshstate;
        ++freshstate;
        debug_print("Char %c, key %d not found. Inserting %d -- %c -> %d.\n",
            *ch, key, st->from, st->input, st->to);
        //g_hash_table_insert(table, GINT_TO_POINTER(key), st);
        g_tree_insert(tree, GINT_TO_POINTER(key), st);
      }
      else if (*ch != st->input) {
        debug_print("WARNING: Transition tree is corrupted!");
      }
      else {
        prevstate = state;
        state = st->to;
      }
    } while (*(++ch) != '\0');

    // Calculate the offset in the substitutions buffer (with_buffer).
    // Here we assume that the substitutions are all packed in a continuous
    // chunk of memory.
    if (subst_with_data_packed(substs)) {
      st->result = substs->data[i].with - substs->with_buffer;
      debug_print("i = %d, st->to = %d, result = %d\n", i, st->to, st->result);
    }
    else {
      debug_print("Subst data not packed.\n");
      g_tree_destroy(tree);
      return NULL;
    }
/*    g_hash_table_insert(
        subst_table,
        GINT_TO_POINTER(state),
        substs[i].with);*/
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

  SDFA *a = g_new(SDFA, 1);
  a->table = st_table;
  a->table_size = size;
  a->subst_buffer = substs->with_buffer;
  a->subst_buffer_size = substs->with_buffer_size;
//  a->accept = (AcceptFunc) accept_replace;
//  a->accept_data = subst_table;
//  a->free_accept_data = (FreeDataFunc) g_hash_table_destroy;
//  a->dump_accept_data = (DumpDataFunc) dump_hash_table;

  return a;
}

gboolean sdfa_dump(SDFA a, const gchar *fname) {
  FILE *f;
  gchar *fullpath = rc_filepath(fname);

  debug_print("Dumping automaton data to %s (size = %d)...\n",
      fullpath, a.table_size);
  if ((f = g_fopen(fullpath, "wb")) == NULL) {
    debug_print("Failed to open file %s for writing.\n", fullpath);
    g_free(fullpath);
    perror(NULL);
    return FALSE;
  }

  // write the size
  if (fwrite(&a.table_size, sizeof(a.table_size), 1, f) < 1) {
    debug_print("Error while trying to write the table size. ");
    perror(NULL);
    g_free(fullpath);
    fclose(f);
    return FALSE;
  }

  if (fwrite(a.table, sizeof(StateTransition), a.table_size, f) < a.table_size) {
    debug_print("Error while trying to dump %d state transitions.\n", a.table_size);
    perror(NULL);
    g_free(fullpath);
    fclose(f);
    return FALSE;
  }

  if (a.subst_buffer != NULL && a.subst_buffer_size > 0) {
    if (fwrite(&a.subst_buffer_size, sizeof(a.subst_buffer_size), 1, f) < 1) {
      debug_print("Error while trying to write the subst buffer size. ");
      perror(NULL);
      g_free(fullpath);
      fclose(f);
      return FALSE;
   }

    if (fwrite(a.subst_buffer, sizeof(*a.subst_buffer), a.subst_buffer_size, f)
        < a.table_size) {

      debug_print("Error while trying to dump %lu bytes of subst buffer.\n",
            a.subst_buffer_size);
      perror(NULL);
      g_free(fullpath);
      fclose(f);
      return FALSE;
    }

    // get the dump of the accept data & add it to the file
  }

  fclose(f);

  SDFA new;

  debug_print("Reading automaton data from %s...\n", fullpath);
  if ((f = g_fopen(fullpath, "rb")) == NULL) {
    debug_print("Failed to open file %s for reading.\n", fullpath);
    perror(NULL);
    g_free(fullpath);
    return FALSE;
  }

  // read the size
  if (fread(&new.table_size, sizeof(new.table_size), 1, f) < 1) {
    debug_print("Error while reading the table size. ");
    perror(NULL);
    fclose(f);
    g_free(fullpath);
    return FALSE;
  }

  debug_print("Read size: %d.\n", new.table_size);
  new.table = g_new(StateTransition, new.table_size);

  if (fread(new.table, sizeof(StateTransition), new.table_size, f) < new.table_size) {
    debug_print("Error while trying to read %d state transitions.\n", new.table_size);
    perror(NULL);
    g_free(new.table);
    fclose(f);
    g_free(fullpath);
    return FALSE;
  }

  new.subst_buffer = g_new(gchar, new.subst_buffer_size);

  if (fread(&new.subst_buffer_size, sizeof(new.subst_buffer_size), 1, f) < 1) {
    debug_print("Error while reading subst buffer size. ");
    perror(NULL);
    fclose(f);
    g_free(new.table);
    g_free(fullpath);
    return FALSE;
  }

  if (fread(new.subst_buffer,
        sizeof(*new.subst_buffer), new.subst_buffer_size, f) < new.table_size) {
    debug_print("Error while trying to read %lu bytes of subst buffer.\n",
        new.subst_buffer_size);
    perror(NULL);
    fclose(f);
    g_free(new.table);
    g_free(new.subst_buffer);
    g_free(fullpath);
    return FALSE;
  }

  debug_print("Automaton read.\n");

  if (a.table_size == new.table_size
      && memcmp(a.table, new.table, new.table_size) == 0
      && a.subst_buffer_size == new.subst_buffer_size
      && memcmp(a.subst_buffer, new.subst_buffer, new.subst_buffer_size) == 0) {
    debug_print("Same automaton.\n");
  }
  else {
    debug_print("VERIFICATION FAILED.\n");
  }

  g_free(fullpath);
  g_free(new.table);
  g_free(new.subst_buffer);
  return TRUE;
}

static gint next_state(
    SDFA a,
    sdfa_state_t state,
    sdfa_char_t ch,
    gshort *result) {

  static guint last = 0;

  /* State transitions are assumed to be sorted by source state.
   * We optimise (a little) for lookups of subsequent source states. This
   * would be wrong if we had cycles / iteration. */
  if (a.table[last].from >= state)
    last = 0;

  for (int i = last; i < a.table_size; ++i) {
    if (a.table[i].from == state && a.table[i].input == ch) {
      last = i;
      *result = a.table[i].result;
      return a.table[i].to;
    }
  }

  return -1;
}

gboolean sdfa_match_iter(GtkTextIter *iter, gchar **result) {
  return sdfa_match_iter_automaton(sdfa_, iter, result);
}

gboolean sdfa_match_iter_automaton(SDFA a, GtkTextIter *iter, gchar **result) {
  gunichar uch;
  guchar ch;
  sdfa_state_t state = 0;
  sdfa_state_t nstate;
  gshort result_offset = -1;

  // we'll assume that the starting state is not accepting, i.e., we don't match
  // an empty string
  do {
    uch = gtk_text_iter_get_char(iter);

    if (uch < MAX_ASCII)
      ch = (guchar) uch;
    else
      return FALSE;

    nstate = next_state(a, state, ch, &result_offset);

    debug_print("State transition on %c from %d to %d\n", ch, state, nstate);

    if (nstate == -1) {
      gtk_text_iter_backward_char(iter);
      break;
    }

    state = nstate;

  } while (gtk_text_iter_forward_char(iter));

  // if we have a result, the last transition was an "accepting" one
  if (result_offset >= 0) {
    debug_print("to->state %d: result = %d\n", state, result_offset);
    *result = a.subst_buffer + result_offset;
    return TRUE;
  }

  return FALSE;
}


