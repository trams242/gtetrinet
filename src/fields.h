#ifndef __GTETRINET_FIELDS_H__
#define __GTETRINET_FIELDS_H__

#include "tetrinet.h"
#include "gtetrinet.h"
#include "config.h"
#include "misc.h"

static int gmsgstate;
static int bigfieldnum;

extern GtkWidget *fields_page_new (void);
extern void fields_page_destroy_contents (void);
extern void fields_init (void);
extern void fields_cleanup (void);

extern void fields_drawfield (int field, TETRINET_FIELD newfield);
extern void fields_redraw (void);
extern void fields_labelupdate (void);
extern void fields_setspeciallabel (int);
extern void fields_drawspecials (void);
extern void fields_drawnextblock (TETRIS_BLOCK block);
extern void fields_attdefmsg (char *text);
extern void fields_attdeffmt (const char *fmt, ...) G_GNUC_PRINTF (1, 2);
extern void fields_attdefclear (void);
extern void fields_setlines (int l);
extern void fields_setlevel (int l);
extern void fields_setactivelevel (int l);
extern void fields_gmsgadd (const char *str);
extern void fields_gmsgclear (void);
extern void fields_gmsginput (int i);
extern void fields_gmsginputclear (void);
extern void fields_gmsginputactivate (int i);
extern const char *fields_gmsginputtext (void);

#endif
