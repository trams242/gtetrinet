#ifndef __GTETRINET_GTETRINET_H__
#define __GTETRINET_GTETRINET_H__

#include <gtk/gtk.h>
#include <libgnomeui/libgnomeui.h>
#include <gnome.h>
#include <gdk/gdkkeysyms.h>
#include <stdlib.h>
#include <string.h>

#define APPID PACKAGE
#define APPNAME "GTetrinet"
#define APPVERSION VERSION

#include "tetrinet.h"
#include "misc.h"
#include "sound.h"
#include "fields.h"
#include "dialogs.h"
#include "partyline.h"
#include "winlist.h"
#include "config.h"

extern GtkWidget *app;
extern TetrinetObject *obj;

extern void destroymain (void);
extern gint keypress (GtkWidget *widget, GdkEventKey *key);
extern gint keyrelease (GtkWidget *widget, GdkEventKey *key);
extern void move_current_page_to_window (void);
extern void show_fields_page (void);
extern void show_partyline_page (void);
extern void unblock_keyboard_signal (void);
extern gint get_current_notebook_page (void);
extern void commands_checkstate (void);
extern void gtetrinet_inmessage (enum TetrinetInmsg msgtype, char *data);

#endif
