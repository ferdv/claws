#ifndef __SC_UTILS_H__
#define __SC_UTILS_H__

#include "utils.h"

#define rc_filepath(fname)  (g_strconcat(\
      get_rc_dir(),\
      G_DIR_SEPARATOR_S,\
      fname,\
      NULL))

#define message_dialog(type, ...)  (gtk_message_dialog_new(\
      (GtkWindow *) compose->window,\
      GTK_DIALOG_DESTROY_WITH_PARENT,\
      type,\
      GTK_BUTTONS_OK,\
      __VA_ARGS__))

/*#define message_dialog(...)  (gtk_message_dialog_new(\
      (GtkWindow *) compose->window,\
      GTK_DIALOG_DESTROY_WITH_PARENT,\
      GTK_MESSAGE_INFO,\
      GTK_BUTTONS_OK,\
      __VA_ARGS__))
*/

#define error_message(dlg, ...) (\
    dlg = message_dialog(GTK_MESSAGE_ERROR, __VA_ARGS__),\
    gtk_dialog_run(GTK_DIALOG(dlg)),\
    gtk_widget_destroy(dlg));

#define info_message(dlg, ...) (\
    dlg = message_dialog(GTK_MESSAGE_INFO, __VA_ARGS__),\
    gtk_dialog_run(GTK_DIALOG(dlg)),\
    gtk_widget_destroy(dlg));

#endif /* __SC_UTILS_H__ */
