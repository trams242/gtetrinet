
/*
 *  GTetrinet
 *  Copyright (C) 1999, 2000, 2001, 2002, 2003  Ka-shu Wong (kswong@zip.com.au)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
# include "../config.h"
#endif

#include <gtk/gtk.h>
#include <gnome.h>
#include <stdlib.h>
#include <time.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <signal.h>
#include <gconf/gconf-client.h>

#include "gtetrinet.h"
#include "config.h"
#include "client.h"
#include "tetrinet.h"
#include "tetris.h"
#include "fields.h"
#include "partyline.h"
#include "winlist.h"
#include "misc.h"
#include "commands.h"
#include "sound.h"
#include "string.h"

#include "images/fields.xpm"
#include "images/partyline.xpm"
#include "images/winlist.xpm"
#include "images/play.xpm"
#include "images/pause.xpm"
#include "images/stop.xpm"
#include "images/team24.xpm"
#include "images/connect.xpm"
#include "images/disconnect.xpm"

gint keypress (GtkWidget *widget, GdkEventKey *key);
gint keyrelease (GtkWidget *widget, GdkEventKey *key);
void switch_focus (GtkNotebook *notebook,
                   GtkNotebookPage *page,
                   guint page_num);
void commands_checkstate (void);

static GtkWidget *pixmapdata_label (char **d, char *str);
static int gtetrinet_key (int keyval, int mod);
static void make_menus (GnomeApp *);

static void connect_command (void);
static void disconnect_command (void);
static void team_command (void);
#ifdef ENABLE_DETACH
static void detach_command (void);
#endif
static void start_command (void);
static void end_command (void);
static void pause_command (void);
static void preferences_command (void);
static void about_command (void);
static void show_start_button (void);
static void show_stop_button (void);
static void show_connect_button (void);
static void show_disconnect_button (void);

static GIOChannel *io_channel;
static guint source;
static gboolean io_channel_cb (GIOChannel *source, GIOCondition condition);
static int resolved;
static int cancel_connect;

static GnomeUIInfo menubar[];
static GnomeUIInfo toolbar[];
static GtkWidget *pfields, *pparty, *pwinlist;
static GtkWidget *winlistwidget, *partywidget, *fieldswidget;
static GtkWidget *notebook;

GtkWidget *app;

char *option_connect = 0, *option_nick = 0, *option_team = 0, *option_pass = 0;
int option_spec = 0;

int gamemode = ORIGINAL;

int fields_width, fields_height;

gulong keypress_signal;

GConfClient *gconf_client;

static const struct poptOption options[] = {
    {"connect", 'c', POPT_ARG_STRING, &option_connect, 0, N_("Connect to server"), N_("SERVER")},
    {"nickname", 'n', POPT_ARG_STRING, &option_nick, 0, N_("Set nickname to use"), N_("NICKNAME")},
    {"team", 't', POPT_ARG_STRING, &option_team, 0, N_("Set team name"), N_("TEAM")},
    {"spectate", 's', POPT_ARG_NONE, &option_spec, 0, N_("Connect as a spectator"), NULL},
    {"password", 'p', POPT_ARG_STRING, &option_pass, 0, N_("Spectator password"), N_("PASSWORD")},
    {NULL, 0, 0, NULL, 0, NULL, NULL}
};

GnomeUIInfo gamemenu[] = {
    GNOMEUIINFO_ITEM(N_("_Connect to server..."), NULL, connect_command, NULL),
    GNOMEUIINFO_ITEM(N_("_Disconnect from server"), NULL, disconnect_command, NULL),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM(N_("Change _team..."), NULL, team_command, NULL),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM(N_("_Start game"), NULL, start_command, NULL),
    GNOMEUIINFO_ITEM(N_("_Pause game"), NULL, pause_command, NULL),
    GNOMEUIINFO_ITEM(N_("_End game"), NULL, end_command, NULL),
    /* Detach stuff is not ready, says Ka-shu, so make it configurable at
     * compile time for now. */
#ifdef ENABLE_DETACH
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM(N_("Detac_h page..."), NULL, detach_command, NULL),
#endif /* ENABLE_DETACH */
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_MENU_EXIT_ITEM(destroymain, NULL),
    GNOMEUIINFO_END
};

GnomeUIInfo settingsmenu[] = {
    GNOMEUIINFO_MENU_PREFERENCES_ITEM(preferences_command, NULL),
    GNOMEUIINFO_END
};

GnomeUIInfo helpmenu[] = {
    GNOMEUIINFO_MENU_ABOUT_ITEM(about_command, NULL),
    GNOMEUIINFO_END
};

GnomeUIInfo menubar[] = {
    GNOMEUIINFO_MENU_GAME_TREE(gamemenu),
    GNOMEUIINFO_MENU_SETTINGS_TREE(settingsmenu),
    GNOMEUIINFO_MENU_HELP_TREE(helpmenu),
    GNOMEUIINFO_END
};

GnomeUIInfo toolbar[] = {
    GNOMEUIINFO_ITEM_DATA(N_("Connect"), N_("Connect to a server"), connect_command, NULL, connect_xpm),
    GNOMEUIINFO_ITEM_DATA(N_("Disconnect"), N_("Disconnect from the current server"), disconnect_command, NULL, disconnect_xpm),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM_DATA(N_("Start game"), N_("Start a new game"), start_command, NULL, play_xpm),
    GNOMEUIINFO_ITEM_DATA(N_("End game"), N_("End the current game"), end_command, NULL, stop_xpm),
    GNOMEUIINFO_ITEM_DATA(N_("Pause game"), N_("Pause the game"), pause_command, NULL, pause_xpm),
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM_DATA(N_("Change team"), N_("Change your current team name"), team_command, NULL, team24_xpm),
#ifdef ENABLE_DETACH
    GNOMEUIINFO_SEPARATOR,
    GNOMEUIINFO_ITEM_STOCK(N_("Detach page"), N_("Detach the current notebook page"), detach_command, "gtk-cut"),
#endif
    GNOMEUIINFO_END
};

static int gtetrinet_poll_func(GPollFD *passed_fds,
                               guint nfds,
                               int timeout)
{ /* passing a timeout wastes time, even if data is ready... don't do that */
  int ret = 0;
  struct pollfd *fds = (struct pollfd *)passed_fds;

  ret = poll(fds, nfds, 0);
  if (!ret && timeout)
    ret = poll(fds, nfds, timeout);

  return (ret);
}

int main (int argc, char *argv[])
{
    GtkWidget *label;
    GdkPixbuf *icon_pixbuf;
    GError *err = NULL;
    
    bindtextdomain(PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(PACKAGE, "UTF-8");
    textdomain(PACKAGE);

    srand (time(NULL));

    gnome_program_init (APPID, APPVERSION, LIBGNOMEUI_MODULE,
                        argc, argv, GNOME_PARAM_POPT_TABLE, options,
                        GNOME_PARAM_NONE);

    textbox_setup (); /* needs to be done before text boxes are created */
    
    /* Initialize the GConf library */
    if (!gconf_init (argc, argv, &err))
    {
      fprintf (stderr, _("Failed to init GConf: %s\n"), err->message);
      g_error_free (err); 
      err = NULL;
    }
  
    /* Start a GConf client */
    gconf_client = gconf_client_get_default ();
  
    /* Add the GTetrinet directories to the list of directories that GConf client must watch */
    gconf_client_add_dir (gconf_client, "/apps/gtetrinet/sound",
                          GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
    
    gconf_client_add_dir (gconf_client, "/apps/gtetrinet/themes",
                          GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  
    gconf_client_add_dir (gconf_client, "/apps/gtetrinet/keys",
                          GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

    gconf_client_add_dir (gconf_client, "/apps/gtetrinet/partyline",
                          GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

    /* Request notification of change for these gconf keys */
    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/sound/midi_player",
                             (GConfClientNotifyFunc) sound_midi_player_changed,
			     NULL, NULL, NULL);
                             
    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/sound/enable_sound",
                             (GConfClientNotifyFunc) sound_enable_sound_changed,
			     NULL, NULL, NULL);
                             
    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/sound/enable_midi",
                             (GConfClientNotifyFunc) sound_enable_midi_changed,
			     NULL, NULL, NULL);
                             
    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/themes/theme_dir",
                             (GConfClientNotifyFunc) themes_theme_dir_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/down",
                             (GConfClientNotifyFunc) keys_down_changed,
                             NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/left",
                             (GConfClientNotifyFunc) keys_left_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/right",
                             (GConfClientNotifyFunc) keys_right_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/rotate_left",
                             (GConfClientNotifyFunc) keys_rotate_left_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/rotate_right",
                             (GConfClientNotifyFunc) keys_rotate_right_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/drop",
                             (GConfClientNotifyFunc) keys_drop_changed, NULL,
			     NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/message",
			     (GConfClientNotifyFunc) keys_message_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/discard",
			     (GConfClientNotifyFunc) keys_discard_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/special1",
                             (GConfClientNotifyFunc) keys_special1_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/special2",
                             (GConfClientNotifyFunc) keys_special2_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/special3",
                             (GConfClientNotifyFunc) keys_special3_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/special4",
                             (GConfClientNotifyFunc) keys_special4_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/special5",
                             (GConfClientNotifyFunc) keys_special5_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/keys/special6",
                             (GConfClientNotifyFunc) keys_special6_changed,
			     NULL, NULL, NULL);

    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/partyline/enable_timestamps",
                             (GConfClientNotifyFunc) partyline_enable_timestamps_changed,
			     NULL, NULL, NULL);
    
    gconf_client_notify_add (gconf_client, "/apps/gtetrinet/partyline/enable_channel_list",
                             (GConfClientNotifyFunc) partyline_enable_channel_list_changed,
			     NULL, NULL, NULL);

    /* load settings */
    config_loadconfig ();

    /* initialise some stuff */
    fields_init ();
    if (!g_thread_supported()) g_thread_init (NULL);

    /* first set up the display */

    /* create the main window */
    app = gnome_app_new (APPID, APPNAME);

    g_signal_connect (G_OBJECT(app), "destroy",
                        GTK_SIGNAL_FUNC(destroymain), NULL);
    keypress_signal = g_signal_connect (G_OBJECT(app), "key-press-event",
                                        GTK_SIGNAL_FUNC(keypress), NULL);
    g_signal_connect (G_OBJECT(app), "key-release-event",
                        GTK_SIGNAL_FUNC(keyrelease), NULL);
    gtk_widget_set_events (app, GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

    /* create and set the window icon */
    icon_pixbuf = gdk_pixbuf_new_from_file (PIXMAPSDIR "/gtetrinet.png", NULL);
    if (icon_pixbuf)
    {
      gtk_window_set_icon (GTK_WINDOW (app), icon_pixbuf);
      gdk_pixbuf_unref (icon_pixbuf);
    }

    /* create the notebook */
    notebook = gtk_notebook_new ();
    gtk_notebook_set_tab_pos (GTK_NOTEBOOK(notebook), GTK_POS_TOP);

    /* put it in the main window */
    gnome_app_set_contents (GNOME_APP(app), notebook);

    /* make menus + toolbar */
    make_menus (GNOME_APP(app));

    /* create the pages in the notebook */
    fieldswidget = fields_page_new ();
    gtk_widget_set_sensitive (fieldswidget, TRUE);
    gtk_widget_show (fieldswidget);
    pfields = gtk_hbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER(pfields), 0);
    gtk_container_add (GTK_CONTAINER(pfields), fieldswidget);
    gtk_widget_show (pfields);
    label = pixmapdata_label (fields_xpm, _("Playing Fields"));
    gtk_widget_show (label);
    gtk_notebook_append_page (GTK_NOTEBOOK(notebook), pfields, label);

    partywidget = partyline_page_new ();
    gtk_widget_show (partywidget);
    pparty = gtk_hbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER(pparty), 0);
    gtk_container_add (GTK_CONTAINER(pparty), partywidget);
    gtk_widget_show (pparty);
    label = pixmapdata_label (partyline_xpm, _("Partyline"));
    gtk_widget_show (label);
    gtk_notebook_append_page (GTK_NOTEBOOK(notebook), pparty, label);

    winlistwidget = winlist_page_new ();
    gtk_widget_show (winlistwidget);
    pwinlist = gtk_hbox_new (FALSE, 0);
    gtk_container_set_border_width (GTK_CONTAINER(pwinlist), 0);
    gtk_container_add (GTK_CONTAINER(pwinlist), winlistwidget);
    gtk_widget_show (pwinlist);
    label = pixmapdata_label (winlist_xpm, _("Winlist"));
    gtk_widget_show (label);
    gtk_notebook_append_page (GTK_NOTEBOOK(notebook), pwinlist, label);

    /* add signal to focus the text entry when switching to the partyline page*/
    g_signal_connect_after(G_OBJECT (notebook), "switch-page",
		           GTK_SIGNAL_FUNC (switch_focus),
		           NULL);

    gtk_widget_show (notebook);
    g_object_set (G_OBJECT (notebook), "can-focus", FALSE, NULL);

    partyline_show_channel_list (list_enabled);
    gtk_widget_show (app);

    /* initialise some stuff */
    commands_checkstate ();

    /* check command line params */
#ifdef DEBUG
    printf ("option_connect: %s\n"
            "option_nick: %s\n"
            "option_team: %s\n"
            "option_pass: %s\n"
            "option_spec: %i\n",
            option_connect, option_nick, option_team,
            option_pass, option_spec);
#endif
    if (option_nick) GTET_O_STRCPY(nick, option_nick);
    if (option_team) GTET_O_STRCPY(team, option_team);
    if (option_pass) GTET_O_STRCPY(specpassword, option_pass);
    if (option_spec) spectating = TRUE;
    if (option_connect) {
      connectingdialog_new ();
      client_init (option_connect, nick);
    }

    /* Don't schedule if data is ready, glib should do this itself,
     * but welcome to anything that works... */
    g_main_context_set_poll_func(NULL, gtetrinet_poll_func);

    /* gtk_main() */
    gtk_main ();

    client_disconnect ();
    /* cleanup */
    fields_cleanup ();
    sound_stopmidi ();

    return 0;
}

GtkWidget *pixmapdata_label (char **d, char *str)
{
    GdkPixbuf *pb;
    GtkWidget *box, *widget;

    box = gtk_hbox_new (FALSE, 0);

    pb = gdk_pixbuf_new_from_xpm_data ((const char **)d);
    widget = gtk_image_new_from_pixbuf (pb);
    gtk_widget_show (widget);
    gtk_box_pack_start (GTK_BOX(box), widget, TRUE, TRUE, 0);
  
    widget = gtk_label_new (str);
    gtk_widget_show (widget);
    gtk_box_pack_start (GTK_BOX(box), widget, TRUE, TRUE, 0);

    return box;
}

/* called when the main window is destroyed */
void destroymain (void)
{
    gtk_main_quit ();
}

/*
 The key press/release handlers requires a little hack:
 There is no indication whether each keypress/release is a real press
 or a real release, or whether it is just typematic action.
 However, if it is a result of typematic action, the keyrelease and the
 following keypress event have the same value in the time field of the
 GdkEventKey struct.
 The solution is: when a keyrelease event is received, the event is stored
 and a timeout handler is installed.  if a subsequent keypress event is
 received with the same value in the time field, the keyrelease event is
 discarded.  The keyrelease event is sent if the timeout is reached without
 being cancelled.
 This results in slightly slower responses for key releases, but it should not
 be a big problem.
 */

GdkEventKey k;
gint keytimeoutid = 0;

gint keytimeout (gpointer data)
{
    data = data; /* to get rid of the warning */

    if (playing)
    {
      if (gdk_keyval_to_lower (k.keyval) == keys[K_DOWN]) {
        /* if it is a quick press, nudge it down one more step */
        if (downpressed == 1) tetrinet_timeout ();
        downpressed = 0;
        tetrinet_settimeout (tetrinet_timeoutduration());
      }
      gtetrinet_draw_current_block ();
    }

    keytimeoutid = 0;
    return FALSE;
}

/* Return TRUE if the key is processed, FALSE otherwise */
gint keypress (GtkWidget *widget, GdkEventKey *key)
{
    int cur_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
    int pfields_page = gtk_notebook_page_num (GTK_NOTEBOOK (notebook), pfields);

    /* Check if it's a GTetrinet key (Alt+1, Alt+2, Alt+3) */
    if (gtetrinet_key (key->keyval, key->state & (GDK_MOD1_MASK)))
    {
      g_signal_stop_emission_by_name (G_OBJECT(widget), "key-press-event");
      return TRUE;
    }

    /* Check if we're in the playing fields */
    if (cur_page == pfields_page)
    {
      /* keys for the playing field - key releases needed - install timeout */
      if (keytimeoutid && key->time == k.time)
        gtk_timeout_remove (keytimeoutid);

      if (spectating) {
        /* spectator keys */
        switch (keyval) {
        case GDK_1: bigfieldnum = 1; break;
        case GDK_2: bigfieldnum = 2; break;
        case GDK_3: bigfieldnum = 3; break;
        case GDK_4: bigfieldnum = 4; break;
        case GDK_5: bigfieldnum = 5; break;
        case GDK_6: bigfieldnum = 6; break;
        default:    goto notfieldkey;
        }
        tetrinet_updatelevels ();
        fields_redraw ();
        g_signal_stop_emission_by_name (G_OBJECT(widget), "key-press-event");
        return TRUE;
      }
    notfieldkey:
      if (!ingame) return FALSE;
      
      if (gdk_keyval_to_lower (keyval) == keys[K_GAMEMSG])
      {
        /*
         * We block the keypress signaling, so we can freely write things
         * in the text widget.
         */
        g_signal_handler_block (app, keypress_signal);
        fields_gmsginputactivate (TRUE);
        fields_gmsginput (TRUE);
        gmsgstate = 1;
        gtetrinet_draw_current_block ();
        g_signal_stop_emission_by_name (G_OBJECT(widget), "key-press-event");
        return TRUE;
      }
      
      if (paused || !playing) return FALSE;

      switch (gdk_keyval_to_lower (keyval))
      {
      case keys[K_ROTRIGHT]:
        if (!nextblocktimeout)
          sound_playsound (S_ROTATE);
        tetris_blockrotate (1);
        gtetrinet_draw_current_block ();
        g_signal_stop_emission_by_name (G_OBJECT(widget), "key-press-event");
        return TRUE;
        
      case keys[K_ROTLEFT]:
        if (!nextblocktimeout)
          sound_playsound (S_ROTATE);
        tetris_blockrotate (-1);
        gtetrinet_draw_current_block ();
        g_signal_stop_emission_by_name (G_OBJECT(widget), "key-press-event");
        return TRUE;
        
      case keys[K_RIGHT]:
        tetris_blockmove (1);
        gtetrinet_draw_current_block ();
        g_signal_stop_emission_by_name (G_OBJECT(widget), "key-press-event");
        return TRUE;
        
      case keys[K_LEFT]:
        tetris_blockmove (-1);
        gtetrinet_draw_current_block ();
        g_signal_stop_emission_by_name (G_OBJECT(widget), "key-press-event");
        return TRUE;
        
      case keys[K_DOWN]:
        if (!downpressed) {
            tetrinet_timeout ();
            downpressed = 1;
            tetrinet_settimeout (DOWNDELAY);
        }
        gtetrinet_draw_current_block ();
        g_signal_stop_emission_by_name (G_OBJECT(widget), "key-press-event");
        return TRUE;
        
      case keys[K_DROP]:
      {
        int sound;
        if (!nextblocktimeout) {
            tetris_blockdrop ();
            tetris_solidify ();
            tetrinet_nextblock ();
            sound = tetrinet_removelines();
            if (sound >= 0) sound_playsound (sound);
            else sound_playsound (S_DROP);
            tetrinet_sendfield (0);
        }
        gtetrinet_draw_current_block ();
        g_signal_stop_emission_by_name (G_OBJECT(widget), "key-press-event");
        return TRUE;
      }
      
      case keys[K_DISCARD]:
        tetrinet_specialkey(-1);
        gtetrinet_draw_current_block ();
        g_signal_stop_emission_by_name (G_OBJECT(widget), "key-press-event");
        return TRUE;

      case keys[K_SPECIAL1]:
	tetrinet_specialkey(1);
        gtetrinet_draw_current_block ();
        g_signal_stop_emission_by_name (G_OBJECT(widget), "key-press-event");
        return TRUE;

      case keys[K_SPECIAL2]:
	tetrinet_specialkey(2);
        gtetrinet_draw_current_block ();
        g_signal_stop_emission_by_name (G_OBJECT(widget), "key-press-event");
        return TRUE;
    
      case keys[K_SPECIAL3]:
	tetrinet_specialkey(3);
        gtetrinet_draw_current_block ();
        g_signal_stop_emission_by_name (G_OBJECT(widget), "key-press-event");
        return TRUE;

      case keys[K_SPECIAL4]:
	tetrinet_specialkey(4);
        gtetrinet_draw_current_block ();
        g_signal_stop_emission_by_name (G_OBJECT(widget), "key-press-event");
        return TRUE;
    
      case keys[K_SPECIAL5]:
	tetrinet_specialkey(5);
        gtetrinet_draw_current_block ();
        g_signal_stop_emission_by_name (G_OBJECT(widget), "key-press-event");
        return TRUE;
          
      case keys[K_SPECIAL6]:
	tetrinet_specialkey(6);
        gtetrinet_draw_current_block ();
        g_signal_stop_emission_by_name (G_OBJECT(widget), "key-press-event");
        return TRUE;
      }
    }

    return FALSE;
}

gint keyrelease (GtkWidget *widget, GdkEventKey *key)
{
    int cur_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
    int pfields_page = gtk_notebook_page_num(GTK_NOTEBOOK(notebook), pfields);
    
    /* Main window - check the notebook */
    if (cur_page == pfields_page)
    {
        k = *key;
        keytimeoutid = gtk_timeout_add (10, keytimeout, 0);
        g_signal_stop_emission_by_name (G_OBJECT(widget), "key-release-event");
        return TRUE;
    }
    return FALSE;
}

/*
 * Returns TRUE if the key combination is reserved for GTetrinet, right now:
 * Alt+1, Alt+2 and Alt+3
 * Returns FALSE in any other case.
 */
static int gtetrinet_key (int keyval, int mod)
{
  if (mod != GDK_MOD1_MASK)
    return FALSE;
    
  switch (keyval)
  {
  case GDK_1: gtk_notebook_set_current_page (GTK_NOTEBOOK(notebook), 0); break;
  case GDK_2: gtk_notebook_set_current_page (GTK_NOTEBOOK(notebook), 1); break;
  case GDK_3: gtk_notebook_set_current_page (GTK_NOTEBOOK(notebook), 2); break;
  default:
    return FALSE;
  }
  
  return TRUE;
}

/* funky page detach stuff */

/* Type to hold primary widget and its label in the notebook page */
typedef struct {
    GtkWidget *parent;
    GtkWidget *widget;
    int pageNo;
} WidgetPageData;

void destroy_page_window (GtkWidget *window, gpointer data)
{
    WidgetPageData *pageData = (WidgetPageData *)data;
    window = window;

    /* Put widget back into a page */
    gtk_widget_reparent (pageData->widget, pageData->parent);

    /* Select it */
    gtk_notebook_set_current_page (GTK_NOTEBOOK(notebook), pageData->pageNo);

    /* Free return data */
    g_free (data);
}

void move_current_page_to_window (void)
{
    WidgetPageData *pageData;
    GtkWidget *page, *child, *newWindow;
    GList *dlist;
    gint pageNo;
    char *title;

    /* Extract current page's widget & it's parent from the notebook */
    pageNo = gtk_notebook_get_current_page (GTK_NOTEBOOK(notebook));
    page   = gtk_notebook_get_nth_page (GTK_NOTEBOOK(notebook), pageNo );
    dlist  = gtk_container_get_children (GTK_CONTAINER(page));
    if (!dlist ||  !(dlist->data))
    {
        /* Must already be a window */
        if (dlist)
           g_list_free (dlist);
        return;
    }
    child = (GtkWidget *)dlist->data;
    g_list_free (dlist);

    /* Create new window for widget, plus container, etc. */
    newWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    title = g_object_get_data (G_OBJECT(child), "title");
    if (!title)
        title = "GTetrinet";
    gtk_window_set_title (GTK_WINDOW (newWindow), title);
    gtk_container_set_border_width (GTK_CONTAINER (newWindow), 0);

    /* Attach key events to window */
    g_signal_connect (G_OBJECT(newWindow), "key-press-event",
                        GTK_SIGNAL_FUNC(keypress), NULL);
    g_signal_connect (G_OBJECT(newWindow), "key-release-event",
                        GTK_SIGNAL_FUNC(keyrelease), NULL);
    gtk_widget_set_events (newWindow, GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);
    gtk_window_set_resizable (GTK_WINDOW(newWindow), TRUE);

    /* Create store to point us back to page for later */
    pageData = g_new( WidgetPageData, 1 );
    pageData->parent = page;
    pageData->widget = child;
    pageData->pageNo = pageNo;

    /* Move main widget to window */
    gtk_widget_reparent (child, newWindow);

    /* Pass ID of parent (to put widget back) to window's destroy */
    g_signal_connect (G_OBJECT(newWindow), "destroy",
                        GTK_SIGNAL_FUNC(destroy_page_window),
                        (gpointer)(pageData));

    gtk_widget_show_all( newWindow );

    /* cure annoying side effect */
    if (gmsgstate)
        fields_gmsginput(TRUE);
    else
        fields_gmsginput(FALSE);

}

/* show the fields notebook tab */
void show_fields_page (void)
{
    gtk_notebook_set_current_page (GTK_NOTEBOOK(notebook), 0);
}

/* show the partyline notebook tab */
void show_partyline_page (void)
{
    gtk_notebook_set_current_page (GTK_NOTEBOOK(notebook), 1);
}

void unblock_keyboard_signal (void)
{
    g_signal_handler_unblock (app, keypress_signal);
}

void switch_focus (GtkNotebook *notebook,
                   GtkNotebookPage *page,
                   guint page_num)
{

    notebook = notebook;	/* Suppress compile warnings */
    page = page;		/* Suppress compile warnings */

    if (connected)
      switch (page_num)
      {
        case 0:
          if (gmsgstate) fields_gmsginputactivate (1);
          else partyline_entryfocus ();
          break;
        case 1: partyline_entryfocus (); break;
        case 2: winlist_focus (); break;
      }
}

void make_menus (GnomeApp *app)
{
  gnome_app_create_menus (app, menubar);

  gtk_accel_map_add_entry ("<GTetrinet-Main>/Game/Start", gdk_keyval_from_name ("n"), GDK_CONTROL_MASK);
  gtk_accel_map_add_entry ("<GTetrinet-Main>/Game/Pause", gdk_keyval_from_name ("p"), GDK_CONTROL_MASK);
  gtk_accel_map_add_entry ("<GTetrinet-Main>/Game/Stop", gdk_keyval_from_name ("s"), GDK_CONTROL_MASK);
  gtk_accel_map_add_entry ("<GTetrinet-Main>/Game/Connect", gdk_keyval_from_name ("c"), GDK_CONTROL_MASK);
  gtk_accel_map_add_entry ("<GTetrinet-Main>/Game/Disconnect", gdk_keyval_from_name ("d"), GDK_CONTROL_MASK);
  gtk_accel_map_add_entry ("<GTetrinet-Main>/Game/Change_Team", gdk_keyval_from_name ("t"), GDK_CONTROL_MASK);
  
  gtk_menu_item_set_accel_path (GTK_MENU_ITEM (gamemenu[0].widget), "<GTetrinet-Main>/Game/Connect");
  gtk_menu_item_set_accel_path (GTK_MENU_ITEM (gamemenu[1].widget), "<GTetrinet-Main>/Game/Disconnect");
  gtk_menu_item_set_accel_path (GTK_MENU_ITEM (gamemenu[3].widget), "<GTetrinet-Main>/Game/Change_Team");
  gtk_menu_item_set_accel_path (GTK_MENU_ITEM (gamemenu[5].widget), "<GTetrinet-Main>/Game/Start");
  gtk_menu_item_set_accel_path (GTK_MENU_ITEM (gamemenu[6].widget), "<GTetrinet-Main>/Game/Pause");
  gtk_menu_item_set_accel_path (GTK_MENU_ITEM (gamemenu[7].widget), "<GTetrinet-Main>/Game/Stop");

  gnome_app_create_toolbar (app, toolbar);
  gtk_widget_hide (toolbar[4].widget);
  gtk_widget_hide (toolbar[1].widget);
}

/* callbacks */

void connect_command (void)
{
    connectdialog_new ();
}

void disconnect_command (void)
{
    client_disconnect ();
}

void team_command (void)
{
    teamdialog_new ();
}

#ifdef ENABLE_DETACH
void detach_command (void)
{
    move_current_page_to_window ();
}
#endif

void start_command (void)
{
  char buf[22];
  
  g_snprintf (buf, sizeof(buf), "%i %i", 1, playernum);
  client_outmessage (OUT_STARTGAME, buf);
}

void show_connect_button (void)
{
  gtk_widget_hide (toolbar[1].widget);
  gtk_widget_show (toolbar[0].widget);
}

void show_disconnect_button (void)
{
  gtk_widget_hide (toolbar[0].widget);
  gtk_widget_show (toolbar[1].widget);
}

void show_stop_button (void)
{
  gtk_widget_hide (toolbar[3].widget);
  gtk_widget_show (toolbar[4].widget);
}

void show_start_button (void)
{
  gtk_widget_hide (toolbar[4].widget);
  gtk_widget_show (toolbar[3].widget);
}

void end_command (void)
{
  char buf[22];
  
  g_snprintf (buf, sizeof(buf), "%i %i", 0, playernum);
  client_outmessage (OUT_STARTGAME, buf);
}

void pause_command (void)
{
  char buf[22];
  
  g_snprintf (buf, sizeof(buf), "%i %i", paused?0:1, playernum);
  client_outmessage (OUT_PAUSE, buf);
}

void preferences_command (void)
{
    prefdialog_new ();
}


/* the following function enable/disable things */

void commands_checkstate ()
{
    if (connected) {
        gtk_widget_set_sensitive (gamemenu[0].widget, FALSE);
        gtk_widget_set_sensitive (gamemenu[1].widget, TRUE);

        gtk_widget_set_sensitive (toolbar[0].widget, FALSE);
        gtk_widget_set_sensitive (toolbar[1].widget, TRUE);
    }
    else {
        gtk_widget_set_sensitive (gamemenu[0].widget, TRUE);
        gtk_widget_set_sensitive (gamemenu[1].widget, FALSE);

        gtk_widget_set_sensitive (toolbar[0].widget, TRUE);
        gtk_widget_set_sensitive (toolbar[1].widget, FALSE);
    }
    if (moderator) {
        if (ingame) {
            gtk_widget_set_sensitive (gamemenu[5].widget, FALSE);
            gtk_widget_set_sensitive (gamemenu[6].widget, TRUE);
            gtk_widget_set_sensitive (gamemenu[7].widget, TRUE);

            gtk_widget_set_sensitive (toolbar[3].widget, FALSE);
            gtk_widget_set_sensitive (toolbar[4].widget, TRUE);
            gtk_widget_set_sensitive (toolbar[5].widget, TRUE);
        }
        else {
            gtk_widget_set_sensitive (gamemenu[5].widget, TRUE);
            gtk_widget_set_sensitive (gamemenu[6].widget, FALSE);
            gtk_widget_set_sensitive (gamemenu[7].widget, FALSE);

            gtk_widget_set_sensitive (toolbar[3].widget, TRUE);
            gtk_widget_set_sensitive (toolbar[4].widget, FALSE);
            gtk_widget_set_sensitive (toolbar[5].widget, FALSE);
        }
    }
    else {
        gtk_widget_set_sensitive (gamemenu[5].widget, FALSE);
        gtk_widget_set_sensitive (gamemenu[6].widget, FALSE);
        gtk_widget_set_sensitive (gamemenu[7].widget, FALSE);

        gtk_widget_set_sensitive (toolbar[3].widget, FALSE);
        gtk_widget_set_sensitive (toolbar[4].widget, FALSE);
        gtk_widget_set_sensitive (toolbar[5].widget, FALSE);
    }
    if (ingame || spectating) {
        gtk_widget_set_sensitive (gamemenu[3].widget, FALSE);

        gtk_widget_set_sensitive (toolbar[7].widget, FALSE);
    }
    else {
        gtk_widget_set_sensitive (gamemenu[3].widget, TRUE);

        gtk_widget_set_sensitive (toolbar[7].widget, TRUE);
    }

    partyline_connectstatus (connected);

    if (ingame) partyline_status (_("Game in progress"));
    else if (connected) {
        char buf[256];
        GTET_O_STRCPY(buf, _("Connected to\n"));
        GTET_O_STRCAT(buf, server);
        partyline_status (buf);
    }
    else partyline_status (_("Not connected"));
}


/* about... */

void about_command (void)
{
    GtkWidget *hbox;
    GdkPixbuf *logo;
    static GtkWidget *about = NULL;

    if (!GTK_IS_WINDOW (about))
    {
      const char *authors[] = {"Ka-shu Wong <kswong@zip.com.au>",
                               "James Antill <james@and.org>",
			       "Jordi Mallach <jordi@sindominio.net>",
			       "Dani Carbonell <bocata@panete.net>",
                               NULL};
      const char *documenters[] = {"Jordi Mallach <jordi@sindominio.net>",
                                   NULL};
      /* Translators: translate as your names & emails */
      const char *translators = _("translator_credits");

      logo = gdk_pixbuf_new_from_file (PIXMAPSDIR "/gtetrinet.png", NULL);
    
      about = gnome_about_new (APPNAME, APPVERSION,
                               "\xc2\xa9 1999, 2000, 2001, 2002, 2003 Ka-shu Wong",
                               _("A Tetrinet client for GNOME.\n"),
                               authors,
                               documenters,
			      strcmp (translators, "translator_credits") != 0 ?
				       translators : NULL,
			      logo);

      if (logo != NULL)
		  g_object_unref (logo);

      hbox = gtk_hbox_new (TRUE, 0);
      gtk_box_pack_start (GTK_BOX (hbox),
		  gnome_href_new ("http://gtetrinet.sourceforge.net/", _("GTetrinet Home Page")),
		  FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (GTK_DIALOG (about)->vbox),
		          hbox, TRUE, FALSE, 0);
      gtk_widget_show_all (hbox);

      g_signal_connect(G_OBJECT(about), "destroy",
		       G_CALLBACK(gtk_widget_destroyed), &about);

      gtk_widget_show (about);
    }
    else
    {
      gtk_window_present (GTK_WINDOW (about));
    }
}


/*
 this function processes incoming messages
 */
void gtetrinet_inmessage (int msgtype, char *data) // Interface function
{
    int tmp_pnum = 0;
    gchar buf[1024];
    
    if (msgtype != IN_PLAYERJOIN && msgtype != IN_PLAYERLEAVE && msgtype != IN_TEAM &&
        msgtype != IN_PLAYERNUM && msgtype != IN_CONNECT && msgtype != IN_F && msgtype != IN_WINLIST)
    {
        partylineupdate (1);
        moderatorupdate (1);
    }
#ifdef DEBUG
    printf ("%d %s\n", msgtype, data);
#endif
    /* process the message */
    switch (msgtype)
    {
    case IN_CONNECT:
        list_issued = 0;
        up_chan_list_source = g_timeout_add (30000, (GSourceFunc) partyline_update_channel_list, NULL);
        partyline_joining_channel ("");
        show_start_button ();
        show_disconnect_button ();
        break;
        
    case IN_DISCONNECT:
        if (!connected) {
            data = _("Server disconnected");
            goto connecterror;
        }
        tetrinet_disconnect ();
        commands_checkstate ();
        partyline_playerlist (NULL, NULL, NULL, 0, NULL, 0);
        partyline_namelabel (NULL, NULL);
        fieldslabelupdate ();
        g_source_remove (up_chan_list_source);
        winlist_clear ();
        fields_attdefclear ();
        fields_gmsgclear ();
        partyline_fmt (_("%c%c*** Disconnected from server"),
                       TETRI_TB_C_DARK_GREEN, TETRI_TB_BOLD);
        partyline_clear_list_channel ();
        partyline_joining_channel (NULL);
        show_connect_button ();
        break;
        
    case IN_CONNECTERROR:
    connecterror:
        {
            GtkWidget *dialog;
            gchar *data_utf8;
            connectingdialog_destroy ();
            GTET_O_STRCPY (buf, _("Error connecting: "));
            data_utf8 = g_locale_to_utf8 (data, -1, NULL, NULL, NULL);
            GTET_O_STRCAT (buf, data_utf8);
            dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
                                             GTK_MESSAGE_ERROR,
                                             GTK_BUTTONS_OK,
                                             buf);
            gtk_dialog_run (GTK_DIALOG(dialog));
            gtk_widget_destroy (dialog);
            g_free (data_utf8);
            show_connect_button ();
        }
        break;
        
    case IN_PLAYERNUM:
      if (tetrinet_do_playernum (data))
      {
        partyline_fmt (_("%c%c*** Connected to server"),
                       TETRI_TB_C_DARK_GREEN, TETRI_TB_BOLD);
        
        partyline_namelabel (nick, NULL);
        commands_checkstate ();
        connectingdialog_destroy ();
        connectdialog_connected ();
      
        /* show partyline on successful connect */
        partyline_update_channel_list ();
        show_partyline_page ();
      }

      if (!spectating)
      {
        /* update display */
        playerlistupdate ();
        partyline_namelabel (nick, team);
        fieldslabelupdate ();
        if (connected) {
          checkmoderatorstatus ();
          partylineupdate_join (nick);
          partylineupdate_team (nick, team);
        }
      }
      break;
      
    case IN_PLAYERJOIN:
      if (tetrinet_do_playerjoin (data))
      {
        playerlistupdate ();
        /* update fields display */
        fieldslabelupdate ();
        /* display */
        partylineupdate_join (playernames[pnum]);
        /* check moderator status */
        checkmoderatorstatus ();
        /* update channel list */
        partyline_update_channel_list ();
        /* send out our field */
        /* if (!spectating) tetrinet_resendfield (); */
      }
      break;
      
    case IN_PLAYERLEAVE:
      tmp_pnum = tetrinet_do_playerleave (data);
      if (tmp_pnum != -1)
      {
        partylineupdate_leave (tmp_pnum);
        tetrinet_remove_player (tmp_pnum);
        playerlistupdate ();
        /* update fields display */
        fieldslabelupdate ();
        
        fields_drawfield (playerfield(tmp_pnum), fields[tmp_pnum]);
        /* check moderator status */
        checkmoderatorstatus ();
      }
      break;
      
    case IN_KICK:
      tmp_pnum = tetrinet_do_playerkick (data);

      if (tmp_pnum == -1)
        break;
      
      if ((tmp_pnum == playernum) && !spectating)
        g_snprintf (buf, sizeof(buf),
                    _("%c%c*** You have been kicked from the game"),
                    TETRI_TB_C_DARK_GREEN, TETRI_TB_BOLD);
      else
        g_snprintf (buf, sizeof(buf),
                    _("%c*** %c%s%c%c has been kicked from the game"),
                    TETRI_TB_C_DARK_GREEN, TETRI_TB_BOLD,
                    playernames[tmp_pnum],
                    TETRI_TB_RESET, TETRI_TB_C_DARK_GREEN);
      partyline_text (buf);
      
      tetrinet_remove_player (tmp_pnum);

      break;
      
    case IN_TEAM:
      tmp_pnum = tetrinet_do_teamname (data);
      if (tmp_pnum != -1)
      {
        playerlistupdate ();
        /* update fields display */
        fieldslabelupdate ();
        /* display */
        partylineupdate_team (playernames[tmp_pnum], teamnames[tmp_pnum]);
      }
      break;
      
    case IN_PLINE:
    {
      gchar *aux, *line;
      
      tmp_pnum = tetrinet_do_playerline (data, &aux);

      if (tmp_num == -1)
        break;
      
      line = nocolor (aux);
      g_free (aux);
      
      if (tmp_pnum != 0)
      {
        if (list_enabled && (list_issued > 0))
        {
          if (*line == '(')
          {
            partyline_add_channel (line);
            break;
          }
                      
          if (!strncmp ("List", line, 4))
          {
            partyline_more_channel_lines ();
            break;
          }
                      
          if (!strncmp ("TetriNET", line, 8))
            break;
                      
          if (!strncmp ("You do NOT", line, 10))
          {
            /* we will use the error message as list stopper */
            list_issued--;
            if (list_issued <= 0)
              stop_list();
            break;
          }
                    
          if (!strncmp ("Use", line, 3))
            break;
                      
          //if (line != NULL) g_free (line);
        }
        /* detect whenever we have joined a channel */
        if (!strncmp ("has joined", &line[strlen (playernames[playernum])+1], 10))
        {
          partyline_joining_channel (&line[strlen (playernames[playernum])+20]);
        }
        else if (!strncmp ("Joined existing Channel", line, 23))
        {
          partyline_joining_channel (&line[26]);
        }
        else if (!strncmp ("Created new Channel", line, 19))
        {
          partyline_joining_channel (&line[22]);
        }
        else if (!strncmp ("You have joined", line, 15))
        {
          partyline_joining_channel (&line[16]);
        }
                  
        aux = g_strconcat ("You tell ", playernames[playernum], ": --- MARK ---", NULL);
        if (!strcmp (aux, line))
        {
          list_issued--;
          if (list_issued <= 0)
            stop_list ();
          break;
        }
                    
        if (tetrix) {
          g_snprintf (buf, sizeof(buf), "*** %s", token);
          partyline_text (buf);
          break;
        }
        else
          plinemsg ("Server", token);
      }
      else
      {
        if (tmp_pnum == playernum)
        {
          gchar *line = nocolor (token);
          if (!strncmp (line, "(msg) --- MARK ---", 18))
          {
            list_issued--;
            if (list_issued <= 0)
              stop_list();
          }
          else
            plinemsg (playernames[tmp_pnum], token);
          //g_free (line);
        }
        else
          plinemsg (playernames[tmp_pnum], token);
      }
    }
    break;
    
    case IN_PLINEACT:
    {
      gchar *aux;
      tmp_pnum = tetrinet_do_playerline (data, &aux);
      
      if (tmp_pnum != -1)
      {
        plineact (playernames[pnum], aux);
        g_free (aux);
      }
    }
    break;
    
    case IN_PLAYERLOST:
      tetrinet_do_playerlost (data);
      break;
      
    case IN_PLAYERWON:
      tmp_pnum = tetrinet_do_playerwon (data);
      
      if (tmp_pnum == -1)
        break;
      
      if (teamnames[tmp_pnum][0])
        g_snprintf (buf, sizeof(buf),
                    _("%c*** Team %c%s%c%c has won the game"),
                    TETRI_TB_C_DARK_RED, TETRI_TB_BOLD,
                    teamnames[tmp_pnum],
                    TETRI_TB_RESET, TETRI_TB_C_DARK_RED);
      else
        g_snprintf (buf, sizeof(buf),
                    _("%c*** %c%s%c%c has won the game"),
                    TETRI_TB_C_DARK_RED, TETRI_TB_BOLD,
                    playernames[tmp_pnum],
                    TETRI_TB_RESET, TETRI_TB_C_DARK_RED);
      
      partyline_text (buf);
      break;
      
    case IN_NEWGAME:
      tetrinet_do_newgame (data);
      
      fields_setlines (0);
      fields_setspeciallabel (-1);
      fields_gmsginput (FALSE);
      fields_gmsginputclear ();
      fields_attdefclear ();
      fields_gmsgclear ();
      if (!spectating)
        fields_drawnextblock (next_block);        

      commands_checkstate ();
      partyline_fmt (_("%c*** The game has %cstarted"),
                     TETRI_TB_C_BRIGHT_RED, TETRI_TB_BOLD);
      show_stop_button ();
      /* switch to playerfields when game starts */
      show_fields_page ();
      sound_playsound (S_GAMESTART);
      sound_playmidi (midifile);
      break;
      
    case IN_INGAME:
      tetrinet_do_ingame (data);
      tetrinet_setspeciallabel (-1);
      fields_gmsginput (FALSE);
      fields_gmsginputclear ();
      commands_checkstate ();
      partyline_fmt(_("%c*** The game is %cin progress"),
                    TETRI_TB_C_BRIGHT_RED, TETRI_TB_BOLD);;
      show_stop_button ();
      break;
      
    case IN_PAUSE:
    {
      int newstate = tetrinet_do_pause (data);
      /* bail out if no state change */
      if (! (newstate ^ paused)) break;
      if (newstate) {
        tetrinet_pausegame ();
        partyline_fmt (_("%c*** The game has %cpaused"),
                       TETRI_TB_C_BRIGHT_RED, TETRI_TB_BOLD);
        fields_attdeffmt (_("The game has %c%cpaused"),
                          TETRI_TB_BOLD, TETRI_TB_C_DARK_GREEN);
      }
      else {
        tetrinet_resumegame ();
        partyline_fmt (_("%c*** The game has %cresumed"),
                       TETRI_TB_C_BRIGHT_RED, TETRI_TB_BOLD);
        fields_attdeffmt (_("The game has %c%cresumed"),
                          TETRI_TB_BOLD, TETRI_TB_C_DARK_GREEN);
      }
      commands_checkstate ();
    }
    break;
    
    case IN_ENDGAME:
      if (tetrinet_do_endgame ())
        sound_playsound (S_YOUWIN);
      sound_stopmidi ();
      fields_drawnextblock (blankblock);
      clearallfields ();
      fields_drawspecials ();
      fields_setlines (-1);
      fields_setlevel (-1);
      fields_setactivelevel (-1);
      fields_setspeciallabel (-1);
      if (gmsgstate)
      {
        gmsgstate = 0;
        fields_gmsginput (FALSE);
        fields_gmsginputclear ();
        unblock_keyboard_signal ();
      }

      commands_checkstate ();
      partyline_fmt (_("%c*** The game has %cended"),
                     TETRI_TB_C_BRIGHT_RED, TETRI_TB_BOLD);
      show_start_button ();
      /* go back to partyline when game ends */
      show_partyline_page ();
      break;
      
    case IN_F:
      tmp_pnum = tetrinet_do_f (data);
      if (tmp_pnum == -1)
        break;
      
      if (ingame)
        fields_drawfield (playerfield(tmp_pnum), fields[tmp_pnum]);
      break;
      
    case IN_SB:
    {
      TetrinetSpecial *sp = tetrinet_do_special (data);
      gtetrinet_notify_special (sp);
      tetrinet_special_destroy (sp);
      gtetrinet_draw_current_block ();
    } 
    break;
      
    case IN_LVL:
      tetrinet_do_lvl (data);
      if (ingame) tetrinet_updatelevels ();
      break;
        
    case IN_GMSG:
    {
      gchar *aux = tetrinet_do_gmsg (data);
      
      fields_gmsgadd (aux);
      g_free (aux);
      sound_playsound (S_MESSAGE);
      
      break;
    }
        
    case IN_WINLIST:
    {
      TetrinetWinlist *list = tetrinet_do_winlist (data);
      int i = 0;

      winlist_clear ();
      while (i < list->item_count)
        winlist_additem (list->item[i++]);

      tetrinet_winlist_destroy (list);
    }
    break;

    /* FIXME: todo el tema de los espectadores */
    case IN_SPECLIST:
    {
      gchar *channel = tetrinet_do_speclist (data);
      gchar *aux = g_strdup_printf (_("%c*** You have joined %c%s%c%c"),
                                    TETRI_TB_C_DARK_BLUE, TETRI_TB_BOLD, channel,
                                    TETRI_TB_RESET, TETRI_TB_C_DARK_BLUE);
      partyline_text (buf);
      playerlistupdate ();
      
      g_free (aux);
      g_free (channel);
    }
    break;
    
    case IN_SPECJOIN:
    {
      TetrinetSpectator *spec = tetrinet_do_specjoin (data);
      gchar *aux = g_strdup_printf (_("%c*** %c%s%c%c has joined the spectators"
                                      " %c%c(%c%s%c%c%c)"),
                                    TETRI_TB_C_DARK_BLUE, TETRI_TB_BOLD,
                                    spec->name,
                                    TETRI_TB_RESET, TETRI_TB_C_DARK_BLUE,
                                    TETRI_TB_C_GREY, TETRI_TB_BOLD,  TETRI_TB_BOLD,
                                    spec->info,
                                    TETRI_TB_RESET, TETRI_TB_C_GREY, TETRI_TB_BOLD);
      partyline_text (buf);
      playerlistupdate ();
      tetrinet_spectator_destroy (spec);
      g_free (aux);
    }
    break;
    
    case IN_SPECLEAVE:
    {
      TetrinetSpectator *spec = tetrinet_do_specleave (data);
      gchar *aux = g_strdup_printf (_("%c*** %c%s%c%c has left the spectators"
                                      " %c%c(%c%s%c%c%c)"),
                                    TETRI_TB_C_DARK_BLUE, TETRI_TB_BOLD,
                                    spec->name,
                                    TETRI_TB_RESET, TETRI_TB_C_DARK_BLUE,
                                    TETRI_TB_C_GREY, TETRI_TB_BOLD,  TETRI_TB_BOLD,
                                    spec->info,
                                    TETRI_TB_RESET, TETRI_TB_C_GREY, TETRI_TB_BOLD);
      partyline_text (buf);
      playerlistupdate ();
      tetrinet_spectator_destroy (spec);
      g_free (aux);
    }
    break;
    
    case IN_SMSG:
    {
      TetrinetMessage *message = tetrinet_do_smsg (data);
      plinesmsg (message);
      tetrinet_destroy_message (message);
    }
    break;
    
    case IN_SACT:
    {
      TetrinetMessage *message = tetrinet_do_smsg (data);
      plinesact (message);
      tetrinet_destroy_message (message);
    }
    break;
    
    default:
      break;
    }
}

static void
gtetrinet_notify_special (TetrinetSpecial *sp)
{
  gchar *buf, *buf2, *buf3, *final;
  
  switch (sp->type)
  {
  case S_ADDALL1:
  case S_ADDALL2:
  case S_ADDALL4: /* bad for everyone ... */
    g_assert (!sp->to);
    if (sp->from == playernum)
      buf = g_strdup_printf ("%c%c%s%c",
                             TETRI_TB_BOLD,
                             TETRI_TB_C_BLACK,
                             sbinfo[sp->type].info,
                             TETRI_TB_RESET);
    else
      buf = g_strdup_printf ("%c%c%s%c%c",
                             TETRI_TB_BOLD,
                             TETRI_TB_C_BRIGHT_RED,
                             sbinfo[sp->type].info,
                             TETRI_TB_C_BRIGHT_RED,
                             TETRI_TB_BOLD);
    break;
    
  case S_ADDLINE:
  case S_CLEARBLOCKS:
  case S_CLEARSPECIAL:
  case S_BLOCKQUAKE:
  case S_BLOCKBOMB: /* badish stuff for someone */
    if (sp->to == playernum)
      buf = g_strdup_printf ("%c%c%s%c",
                             TETRI_TB_BOLD,
                             TETRI_TB_C_BRIGHT_RED,
                             sbinfo[sp->type].info,
                             TETRI_TB_RESET);
    else if (sp->from == playernum)
      buf = g_strdup_printf ("%c%c%s%c",
                             TETRI_TB_BOLD,
                             TETRI_TB_C_BLACK,
                             sbinfo[sp->type].info,
                             TETRI_TB_RESET);
    else
      buf = g_strdup_printf ("%c%c%s%c%c",
                             TETRI_TB_BOLD,
                             TETRI_TB_C_DARK_RED,
                             sbinfo[sp->type].info,
                             TETRI_TB_C_DARK_RED,
                             TETRI_TB_BOLD);
    break;
    
  case S_CLEARLINE:
  case S_NUKEFIELD:
  case S_SWITCH:
  case S_GRAVITY: /* goodish stuff for someone */
    if (sp->to == playernum)
      buf = g_strdup_printf ("%c%c%s%c",
                             TETRI_TB_BOLD,
                             TETRI_TB_C_BRIGHT_GREEN,
                             sbinfo[sp->type].info,
                             TETRI_TB_RESET);
    else
      buf = g_strdup_printf ("%c%c%s%c%c",
                             TETRI_TB_BOLD,
                             TETRI_TB_C_DARK_GREEN,
                             sbinfo[sp->type].info,
                             TETRI_TB_C_DARK_GREEN,
                             TETRI_TB_BOLD);
    break;
  }
  
  if (sp->to)
  {
    if (sp->to == playernum)
      buf2 = g_strdup_printf (_(" on %c%c%s%c%c"),
                              TETRI_TB_BOLD,
                              TETRI_TB_C_BRIGHT_BLUE,
                              playernames[sp->to],
                              TETRI_TB_C_BRIGHT_BLUE,
                              TETRI_TB_BOLD);
    else
      buf2 = g_strdup_printf (_(" on %c%c%s%c%c"),
                              TETRI_TB_BOLD,
                              TETRI_TB_C_DARK_BLUE,
                              playernames[sp->to],
                              TETRI_TB_C_DARK_BLUE,
                              TETRI_TB_BOLD);    
  }
  else
  {
    buf2 = g_strdup (_(" to All"));
  }
  
  if (sp->from)
  {
    if (sp->from == playernum)
      buf3 = g_strdup_printf (_(" by %c%c%s%c%c"),
                              TETRI_TB_BOLD,
                              TETRI_TB_C_BRIGHT_BLUE,
                              playernames[sp->from],
                              TETRI_TB_C_BRIGHT_BLUE,
                              TETRI_TB_BOLD);
    else
      buf3 = g_strdup_printf (_(" by %c%c%s%c%c"),
                              TETRI_TB_BOLD,
                              TETRI_TB_C_DARK_BLUE,
                              playernames[sp->from],
                              TETRI_TB_C_DARK_BLUE,
                              TETRI_TB_BOLD);
    
    final = g_strconcat (buf, buf2, buf3, NULL);
    g_free (buf);
    g_free (buf2);
    g_free (buf3);
  }
  else
  {
    final = g_strconcat (buf, buf2, NULL);
    g_free (buf);
    g_free (buf2);
  }
  
  fields_attdefmsg (final);
  g_free (final);
}
 

gint
gtetrinet_next_block_timeout (void)
{
  if (paused) return TRUE; /* come back later */
  if (!playing) return FALSE;
  if (tetrinet_do_nextblock ())
  {
    gtetrinet_do_playerlost ();
    return FALSE;
  }

  nextblocktimeout = 0;
  fields_drawnextblock (tetris_getblock (nextblock, nextorient));
  gtetrinet_settimeout (tetrinet_timeoutduration());

  return FALSE;
}

void
gtetrinet_settimeout (int duration) // Library function
{
  if (movedowntimeout)
    gtk_timeout_remove (movedowntimeout);
  movedowntimeout = gtk_timeout_add (duration, (GtkFunction)tetrinet_timeout,
                                     NULL);
}

void
gtetrinet_removetimeout (void) // Library function
{
  if (movedowntimeout)
    gtk_timeout_remove (movedowntimeout);
  movedowntimeout = 0;
}

int
gtetrinet_timeout (void) // Library function
{
  int sound;
  
  if (paused) return TRUE;
  if (!playing) return FALSE;
  if (downpressed) downpressed ++;
  if (tetris_blockdown ()) {
    if (!playing) /* player died within tetris_blockdown() */
      return FALSE;
    if (downcount) {
      tetris_solidify ();
      tetrinet_nextblock ();
      tetrinet_sendfield (0);
      sound = tetrinet_removelines ();
      if (sound>=0) sound_playsound (sound);
      else sound_playsound (S_SOLIDIFY);
      downcount = 0;
    }
    else downcount ++;
  }
  else downcount = 0;
  gtetrinet_draw_current_block ();
  return TRUE;
}

void
gtetrinet_nextblock (void) // Library function
{
  if (nextblocktimeout) return;
  gtetrinet_removetimeout ();
  nextblocktimeout = gtk_timeout_add (NEXTBLOCKDELAY, (GtkFunction)gtetrinet_next_block_timeout,
                                      NULL);
}

void
gtetrinet_draw_current_block (void)
{
  FIELD *new = tetris_drawcurrentblock ();

  fields_drawfield (playerfield (playernum), new);
  g_free (new);
}

static gboolean
io_channel_cb (GIOChannel *source, GIOCondition condition)
{
  gchar *buf;
  
  source = source; /* get rid of the warnings */
  
  switch (condition)
  {
  case G_IO_IN :
    if (client_readmsg (&buf) < 0)
    {
      g_warning ("client_readmsg failed, aborting connection\n");
      client_disconnect ();
    }
    else
    {
      if (strlen (buf)) client_inmessage (buf);
      
      if (strncmp ("noconnecting", buf, 12) == 0)
      {
        connected = 1; /* so we can disconnect :) */
        client_disconnect ();
      }
      g_free (buf);
    }
    break;
    
  default:
    break;
  }
  
  return TRUE;
}

static void
gtetrinet_create_connection (TetrinetObject *obj)
{
  GThread *thread;
  gchar *cad;
        
  errno = 0;
  resolved = 0;
  cancel_connect = 0;
  
  thread = g_thread_create ((GThreadFunc) gtetrinet_resolv_hostname, NULL, FALSE, NULL);
  
  /* wait until the hostname is resolved */
  while ((resolved == 0) && !cancel_connect)
  {
    if (gtk_events_pending ())
      gtk_main_iteration ();
  }
  
  /* If the user has canceled the connection */
  if (cancel_connect)
  {
    if (obj->sock != 0) /* close the socket */
    {
      shutdown (obj->sock, 2);
      close (obj->sock);
    }
    return;
  }

  /* Check the resolved value */
  switch (resolved)
  {
  case ERR_RESOLV:
    if (errno)
      cad = g_strconcat ("noconnecting ", strerror (errno), NULL);
    else
      cad = g_strconcat ("noconnecting ", _("Couldn't resolve hostname."), NULL);

    client_inmessage (cad); // FIXME
    g_free (cad);
    
    return;
    
  case ERR_SOCKET:
    if (errno)
      cad = g_strconcat ("noconnecting ", strerror (errno), NULL);
    else
      cad = g_strconcat ("noconnecting ", _("Couldn't create socket."), NULL);

    client_inmessage (cad); // FIXME
    g_free (cad);

    return;
    
  case ERR_CONNECT:
    if (errno)
      cad = g_strconcat ("noconnecting ", strerror (errno), NULL);
    else
      cad = g_strconcat ("noconnecting ", _("Couldn't connect to "), server, NULL);

    client_inmessage (cad); // FIXME
    g_free (cad);

    return;
  }

  /**
   * Set up the g_io_channel
   * We should set it with no encoding and no buffering, just to simplify things */
  io_channel = g_io_channel_unix_new (obj->sock);
  g_io_channel_set_encoding (io_channel, NULL, NULL);
  g_io_channel_set_buffered (io_channel, FALSE);
  source = g_io_add_watch (io_channel, G_IO_IN, (GIOFunc)io_channel_cb, NULL);
}

gpointer
gtetrinet_resolv_hostname (void)
{
#ifdef USE_IPV6
    char hbuf[NI_MAXHOST];
    struct addrinfo hints, *res, *res0;
    struct sockaddr_in6 sa;
    char service[10];
#else
    struct hostent *h;
    struct sockaddr_in sa;
#endif

    /* set up the connection */

#ifdef USE_IPV6
    snprintf(service, 9, "%d", spectating?SPECPORT:PORT);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(server, service, &hints, &res0)) {
        /* set errno = 0 so that we know it's a getaddrinfo error */
        errno = 0;
        resolved = ERR_RESOLV;
        g_thread_exit (GINT_TO_POINTER (-1));
    }
    for (res = res0; res; res = res->ai_next) {
        sock = socket (res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sock < 0) {
            if (res->ai_next)
                continue;
            else {
                freeaddrinfo(res0);
                resolved = ERR_SOCKET;
                g_thread_exit (GINT_TO_POINTER (-1));
            }
        }
        getnameinfo(res->ai_addr, res->ai_addrlen, hbuf, sizeof(hbuf), NULL, 0, 0);
        if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
            if (res->ai_next) {
                close(sock);
                continue;
            } else {
                close(sock);
                freeaddrinfo(res0);
                resolved = ERR_CONNECT;
                g_thread_exit (GINT_TO_POINTER (-1));
            }
        }
        break;
    }
    freeaddrinfo(res0);
#else
    h = gethostbyname (server);
    if (h == 0) {
        /* set errno = 0 so that we know it's a gethostbyname error */
        errno = 0;
        resolved = ERR_RESOLV;
        g_thread_exit (GINT_TO_POINTER (-1));
    }
    memset (&sa, 0, sizeof (sa));
    memcpy (&sa.sin_addr, h->h_addr, h->h_length);
    sa.sin_family = h->h_addrtype;
    sa.sin_port = htons (spectating?SPECPORT:PORT);

    sock = socket (sa.sin_family, SOCK_STREAM, 0);
    if (sock < 0)
    {
      resolved = ERR_SOCKET;
      g_thread_exit (GINT_TO_POINTER (-1));
    }

    if (connect (sock, (struct sockaddr *)&sa, sizeof(sa)) < 0)
    {
        resolved = ERR_CONNECT;
        g_thread_exit (GINT_TO_POINTER (-1));
    }
#endif

    resolved = 1;
    return (GINT_TO_POINTER (1));
}

int
gtetrinet_readmsg (gchar **str)
{
    gint bytes = 0;
    gchar buf[1024];
    GError *error = NULL;
    gint i = 0;
  
    do
    {
      switch (g_io_channel_read_chars (io_channel, &buf[i], 1, &bytes, &error))
      {
        case G_IO_STATUS_EOF :
          g_warning ("End of file (server closed connection).");
          return -1;
          break;
        
        case G_IO_STATUS_AGAIN :
          g_warning ("Resource temporarily unavailable.");
          return -1;
          break;
        
        case G_IO_STATUS_ERROR :
          g_warning ("Error");
          return -1;
          break;
        
        case G_IO_STATUS_NORMAL :
          if (error != NULL)
          {
            g_warning ("ERROR READING: %s\n", error->message);
            g_error_free (error);
            return -1;
          }; break;
      }
      i++;
    } while ((bytes == 1) && (buf[i-1] != (gchar)0xFF) && (i<1024));
    buf[i-1] = 0;

#ifdef DEBUG
    printf ("< %s\n", buf);
#endif
    
    *str = g_strdup (buf);

    return 0;
}

void
gtetrinet_disconnect (TetrinetObject *obj)
{
  if (obj->connected)
  {
    if (gtk_main_level())
      client_inmessage (obj, "disconnect");
    g_source_destroy (g_main_context_find_source_by_id (NULL, source));
    g_io_channel_shutdown (io_channel, TRUE, NULL);
    g_io_channel_unref (io_channel);
    shutdown (obj->sock, 2);
    close (obj->sock);
    client_disconnect (obj);
  }
  else /* Still resolving name or connecting to the host */
  {
      /* FIXME: We should kill the thread here */
    cancel_connect++;
  }
}
