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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>

#include "client.h"
#include "tetrinet.h"
//#include "partyline.h"
//#include "dialogs.h"
//#include "misc.h"
//#include "gtetrinet.h"

/* structures and arrays for message translation */

struct inmsgt {
    enum inmsg_type num;
    char *str;
};

struct outmsgt {
    enum outmsg_type num;
    char *str;
};

/* some of these strings change depending on the game mode selected */
/* these changes are put into effect through the function inmsg_change */
struct inmsgt inmsgtable[] = {
    {TETRINET_IN_CONNECT, "connect"},
    {TETRINET_IN_DISCONNECT, "disconnect"},

    {TETRINET_IN_CONNECTERROR, "noconnecting"},
    {TETRINET_IN_PLAYERNUM, "playernum"},
    {TETRINET_IN_PLAYERJOIN, "playerjoin"},
    {TETRINET_IN_PLAYERLEAVE, "playerleave"},
    {TETRINET_IN_KICK, "kick"},
    {TETRINET_IN_TEAM, "team"},
    {TETRINET_IN_PLINE, "pline"},
    {TETRINET_IN_PLINEACT, "plineact"},
    {TETRINET_IN_PLAYERLOST, "playerlost"},
    {TETRINET_IN_PLAYERWON, "playerwon"},
    {TETRINET_IN_NEWGAME, "newgame"},
    {TETRINET_IN_INGAME, "ingame"},
    {TETRINET_IN_PAUSE, "pause"},
    {TETRINET_IN_ENDGAME, "endgame"},
    {TETRINET_IN_F, "f"},
    {TETRINET_IN_SB, "sb"},
    {TETRINET_IN_LVL, "lvl"},
    {TETRINET_IN_GMSG, "gmsg"},
    {TETRINET_IN_WINLIST, "winlist"},

    {TETRINET_IN_SPECJOIN, "specjoin"},
    {TETRINET_IN_SPECLEAVE, "specleave"},
    {TETRINET_IN_SPECLIST, "speclist"},
    {TETRINET_IN_SMSG, "smsg"},
    {TETRINET_IN_SACT, "sact"},
    {0, 0}
};

static struct inmsgt *
get_inmsg_entry(int num)
{
    int i;
    for (i = 0; inmsgtable[i].num && inmsgtable[i].num != num; i ++);
    return &inmsgtable[i];
}

static void
inmsg_change(TetrinetObject *obj)
{
    switch (obj->gamemode) {
    case TETRINET_MODE_ORIGINAL:
        get_inmsg_entry (TETRINET_IN_PLAYERNUM)->str = "playernum";
        get_inmsg_entry (TETRINET_IN_NEWGAME)->str = "newgame";
        break;
    case TETRINET_MODE_TETRIFAST:
        get_inmsg_entry (TETRINET_IN_PLAYERNUM)->str = ")#)(!@(*3";
        get_inmsg_entry (TETRINET_IN_NEWGAME)->str = "*******";
        break;
    }
}

struct outmsgt outmsgtable[] = {
    {TETRINET_OUT_DISCONNECT, "disconnect"},
    {TETRINET_OUT_CONNECTED, "connected"},

    {TETRINET_OUT_TEAM, "team"},
    {TETRINET_OUT_PLINE, "pline"},
    {TETRINET_OUT_PLINEACT, "plineact"},
    {TETRINET_OUT_PLAYERLOST, "playerlost"},
    {TETRINET_OUT_F, "f"},
    {TETRINET_OUT_SB, "sb"},
    {TETRINET_OUT_LVL, "lvl"},
    {TETRINET_OUT_STARTGAME, "startgame"},
    {TETRINET_OUT_PAUSE, "pause"},
    {TETRINET_OUT_GMSG, "gmsg"},

    {TETRINET_OUT_VERSION, "version"},
    {0, 0}
};

/* some other useful functions */
static int tetrinet_sendmsg (char *str);
static void tetrinet_server_ip (unsigned char buf[4]);

enum inmsg_type tetrinet_inmsg_translate (char *str);
char *tetrinet_outmsg_translate (int);

void
tetrinet_client_init (TetrinetObject *obj);
{
//  GString *s1 = g_string_sized_new(80);
//  GString *s2 = g_string_sized_new(80);
//  GString *iphashbuf = g_string_sized_new(11);
  char s1[80], s2[80], aux[80], iphashbuf[11];
  unsigned char ip[4];
  unsigned int i, len;
  int l;
  
  /* set the game mode */
  inmsg_change();
    
  /* construct message */
  if (obj->gamemode == TETRINET_MODE_TETRIFAST)
    sprintf (s1, "tetrifaster %s 1.13", obj->nick);
  else
    sprintf (s1, "tetrisstart %s 1.13", obj->nick);

  /* do that encoding thingy */
  server_ip (obj, ip);
  sprintf (iphashbuf, "%d", ip[0]*54 + ip[1]*41 + ip[2]*29 + ip[3]*17);
  l = strlen (iphashbuf);

  s2[0] = 0;
//  g_string_append_c(s2, 0);
  for (i = 0; s1[i]; i ++)
//    g_string_append_c(s2, ((((s2->str[i] & 0xFF) + (s1->str[i] & 0xFF)) % 255) ^ iphashbuf->str[i % l]));
    s2[i+1] = (((s2[i] & 0xFF) + (s1[i] & 0xFF)) % 255) ^ iphashbuf[i % l];
  
/*  g_assert(s1->len == i);
    g_assert(s2->len == (i + 1));*/
  len = i + 1;

//  g_string_truncate(s1, 0);
  s1[0] = 0;
  for (i = 0; i < len; i ++)
  {
//    g_string_append_printf(s1, "%02X", s2->str[i] & 0xFF);
    sprintf (aux, "%02X", s2[i] & 0xFF);
    strcat (s1, aux);
  }

  /* now send to server */
  tetrinet_sendmsg (obj, s1);
}

void
tetrinet_outmessage (TetrinetObject *obj, int msgtype, char *str)
{
    char buf[1024];
    GTET_O_STRCPY(buf, outmsg_translate (msgtype));
    if (str) {
        GTET_O_STRCAT(buf, " ");
        GTET_O_STRCAT(buf, str);
    }
    switch (msgtype)
    {
    case TETRINET_OUT_DISCONNECT : break;
    case TETRINET_OUT_CONNECTED : break;
      default : client_sendmsg (obj, buf);
    }
}

TetrinetServerMsg*
tetrinet_servermsg_new (int msgtype, char *data)
{
  TetrinetServerMsg *msg = (TetrinetServerMsg*) malloc (sizeof (TetrinetServerMsg));

  msg->msgtype = msgtype;
  msg->data = data;

  return msg;
}

void
tetrinet_servermsg_destroy (TetrinetServerMsg *msg)
{
  if (msg->data != NULL)
    free (msg->data);
  free (msg);
}

TetrinetServerMsg *
tetrinet_inmessage (TetrinetObject *obj, char *str)
{
    int msgtype, i;
    char *aux = strdup (str);
    TetrinetServerMsg *msg;

    while (aux[i] != 0 && aux[i] != ' ')
      i++;
    aux[i++] = 0;
    msgtype = tetrinet_inmsg_translate (aux);

    if (str[i] != 0)
      msg = tetrinet_servermsg_new (msgtype, &(aux[i]));
    else
      msg = tetrinet_servermsg_new (msgtype, NULL);
    free (aux);
    
    return msg;
}

/* some other useful functions */
int
tetrinet_sendmsg (TetrinetObject *obj, char *str)
{
    char c = 0xFF;

    write (obj->sock, str, strlen (str));
    write (obj->sock, &c, 1);

#ifdef DEBUG
    printf ("> %s\n", str);
#endif

    return 0;
}

void
tetrinet_server_ip (TetrinetObject *obj, unsigned char buf[4])
{
#ifdef USE_IPV6
    struct sockaddr_in6 sin;
    struct sockaddr_in *sin4;
#else
    struct sockaddr_in sin;
#endif
    int len = sizeof(sin);

    getpeername (obj->sock, (struct sockaddr *)&sin, &len);
#ifdef USE_IPV6
    if (sin.sin6_family == AF_INET6) {
	memcpy (buf, ((char *) &sin.sin6_addr) + 12, 4);
    } else {
	sin4 = (struct sockaddr_in *) &sin;
	memcpy (buf, &sin4->sin_addr, 4);
   }
#else
    memcpy (buf, &sin.sin_addr, 4);
#endif
}

int
tetrinet_inmsg_translate (char *str)
{
    int i;
    for (i = 0; inmsgtable[i].str; i++)
        if (strcmp (inmsgtable[i].str, str) == 0)
            return inmsgtable[i].num;

    return 0;
}

char *
tetrinet_outmsg_translate (int num)
{
    int i;
    for (i = 0; outmsgtable[i].num; i++)
        if (outmsgtable[i].num==num)
          return outmsgtable[i].str;

    return NULL;
}
