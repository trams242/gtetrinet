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
#include <stdlib.h>
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
//#include "partyline.h"
//#include "dialogs.h"
//#include "misc.h"
//#include "gtetrinet.h"

enum TetrinetInmsg
tetrinet_inmsg_translate (char *str)
{
    int i;
    for (i = 0; inmsgtable[i].str; i++)
        if (strcmp (inmsgtable[i].str, str) == 0)
            return inmsgtable[i].num;

    return 0;
}

char *
tetrinet_outmsg_translate (unsigned int num)
{
    int i;
    for (i = 0; outmsgtable[i].num; i++)
        if (outmsgtable[i].num==num)
          return outmsgtable[i].str;

    return NULL;
}

static struct inmsgt *
get_inmsg_entry(unsigned int num)
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

/* some other useful functions */
static int tetrinet_sendmsg (TetrinetObject *obj, char *str);
static void tetrinet_server_ip (TetrinetObject *obj, unsigned char buf[4]);

enum TetrinetInmsg tetrinet_inmsg_translate (char *str);
char *tetrinet_outmsg_translate (unsigned int);

void
tetrinet_client_init (TetrinetObject *obj)
{
//  GString *s1 = g_string_sized_new(80);
//  GString *s2 = g_string_sized_new(80);
//  GString *iphashbuf = g_string_sized_new(11);
  char s1[80], s2[80], aux[80], iphashbuf[11];
  unsigned char ip[4];
  unsigned int i, len;
  int l;
  
  /* set the game mode */
  inmsg_change (obj);
    
  /* construct message */
  if (obj->gamemode == TETRINET_MODE_TETRIFAST)
    sprintf (s1, "tetrifaster %s 1.13", obj->nick);
  else
    sprintf (s1, "tetrisstart %s 1.13", obj->nick);

  /* do that encoding thingy */
  tetrinet_server_ip (obj, ip);
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
tetrinet_outmessage (TetrinetObject *obj, unsigned int msgtype, char *str)
{
  char *buf = (char*) malloc (sizeof (char) * (strlen (str) + 20));

  strcpy (buf, tetrinet_outmsg_translate (msgtype));
  if (str)
  {
    strcat (buf, " ");
    strcat (buf, str);
  }
  
  switch (msgtype)
  {
  case TETRINET_OUT_DISCONNECT : break;
  case TETRINET_OUT_CONNECTED : break;
  default : tetrinet_sendmsg (obj, buf);
  }

  free (buf);
}

TetrinetServerMsg*
tetrinet_servermsg_new (int msgtype, char *data)
{
  TetrinetServerMsg *msg = (TetrinetServerMsg*) malloc (sizeof (TetrinetServerMsg));

  msg->msgtype = msgtype;
  if (data != NULL)
    msg->data = strdup (data);
  else
    msg->data = NULL;

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
tetrinet_inmessage (char *str)
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

