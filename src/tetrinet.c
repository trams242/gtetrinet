
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

#include "tetrinet.h"

static guint up_chan_list_source;

static void tetrinet_updatelevels (void);
static void tetrinet_setspeciallabel (int sb);
static void tetrinet_dospecial (int from, int to, int type);
static void tetrinet_specialkey (int pnum);
static void tetrinet_shiftline (FIELD field, int l, int d);
static char translateblock (char c);
static void clearallfields (void);
static void checkmoderatorstatus (void);
static void speclist_clear (void);
static void speclist_add (char *name);
static void speclist_remove (char *name);

TetrinetObject *
tetrinet_object_new (char *server, char *nick, char *team)
{
  TetrinetObject *obj;

  if ((server == NULL) || (nick == NULL))
    return NULL;

  obj = (TetrinetObject*) malloc (sizeof (TetrinetObject));

  obj->magic = 0xdeadbeef;
  obj->server = strdup (server);
  obj->nick = strdup (nick);
  obj->team = strdup (team);
  obj->connected = FALSE;
  obj->spectating = FALSE;
  
  return obj;
}

void
tetrinet_object_set_nickname (TetrinetObject *obj, char *name)
{
  if (obj->nick != NULL)
    free (obj->nick);
  obj->nick = strdup (name);
}

void
tetrinet_object_set_teamname (TetrinetObject *obj, char *name)
{
  if (obj->team != NULL)
    free (obj->team);
  obj->team = strdup (name);
}

void
tetrinet_object_set_server (TetrinetObject *obj, char *name)
{
  if (obj->server != NULL)
    free (obj->server);
  obj->server = strdup (name);
}

void
tetrinet_object_destroy (TetrinetObject *obj)
{
  if ((obj == NULL) || (obj->magic != 0xdeadbeef))
    return;
  
  free (obj->server);
  free (obj->nick);
  free (obj->team);
  free (obj);
}

/* Returns TRUE if it's a confirmation of connection (GUI should be updated) */
int
tetrinet_do_playernum (TetrinetObject *obj, char *data)
{
  int tmp_pnum;
  
  tmp_pnum = atoi (data);
  if (tmp_pnum >= obj->TETRINET_MAX_PLAYERS)
    return -1; // FIXME: dunno what this means
  obj->playernum = tmp_pnum;
  
  if (!obj->connected)
  {
    /* we have successfully connected */
    if (obj->spectating) {
      g_snprintf (buf, sizeof(buf), "%d %s", obj->playernum, obj->specpasswd);
      client_outmessage (obj, OUT_TEAM, buf);
      client_outmessage (obj, OUT_VERSION, APPNAME"-"APPVERSION);
    }
    /* set up stuff */
    obj->connected = TRUE;
    obj->ingame = obj->playing = obj->paused = FALSE;
    obj->playercount = 0;
    if (obj->spectating) obj->tetrix = TRUE;
    else obj->tetrix = FALSE;

    return TRUE;
  }
  
  if (!obj->spectating) {
    /* If we occupy a previously empty slot increase playercount */
    if (obj->playernames[obj->playernum][0] == 0)
      obj->playercount++;
    /* set own player/team info */
    GTET_O_STRCPY (obj->playernames[obj->playernum], obj->nick);
    GTET_O_STRCPY (obj->teamnames[obj->playernum], obj->team);
    /* send team info */
    g_snprintf (buf, sizeof(buf), "%d %s", obj->playernum, obj->team);
    client_outmessage (obj, OUT_TEAM, buf);
  }

  return FALSE;
}

int
tetrinet_do_playerjoin (TetrinetObject *obj, char *data)
{
  int pnum;
  char *token;
  
  /* new player has joined */
  token = strtok (data, " ");
  if (token == NULL)
    return -1;
  obj->pnum = atoi (token);
  if (obj->pnum >= obj->TETRINET_MAX_PLAYERS)
    return -1; // FIXME: dunno what this means
  token = strtok (NULL, "");
  if (token == NULL) break;
  if (obj->playernames[pnum][0] == 0) obj->playercount ++;
  GTET_O_STRCPY (obj->playernames[pnum], token);
  obj->teamnames[pnum][0] = 0;

  return TRUE;
}

int
tetrinet_do_f (TetrinetObject *obj, char *data)
{
  int pnum;
  char *p, *s;
  
  s = strtok (data, " ");
  if (s == NULL) break;
  pnum = atoi (s);
  if (pnum >= TETRINET_MAX_PLAYERS)
    return -1;
  s = strtok (NULL, "");
  if (s == NULL)
    return -1;
  if (*s >= '0') {
    /* setting entire field */
    p = (char *)obj->fields[pnum];
    for (; *s; s ++, p ++)
      *p = translateblock (*s);
  }
  else {
    /* setting specific locations */
    int block = 0, x, y;
    for(; *s; s++) {
      if (*s < '0' && *s >= '!') block = *s - '!';
      else { /* welcome to ASCII hell, x and y tested though */
        x = *s - '3';
        y = *(++s) - '3';
        if (x >= 0 && x < FIELDWIDTH && y >= 0 && y < FIELDHEIGHT)
          obj->fields[pnum][y][x] = block;
      }
    }
  }

  return pnum;
}

void
tetrinet_special_destroy (TetrinetSpecial *sp)
{
  free (sp);
}

TetrinetSpecial *
tetrinet_special_new (int from, int to, int sbnum)
{
  TetrinetSpecial *sp = (TetrinetSpecial*) malloc (sizeof (TetrinetSpecial));

  sp->from = from;
  sp->to = to;
  sp->sbnum = num;

  return sp;
}

TetrinetSpecial *
tetrinet_do_special (TetrinetObject *obj, char *data)
{
  int sbnum, to, from;
  char *token, *sbid;
  TetrinetSpecial *sp;
  
  token = strtok (data, " ");
  if (token == NULL)
    return -1;
  
  to = atoi (token);
  if (to >= obj->TETRINET_MAX_PLAYERS)
    return -1;
  
  sbid = strtok (NULL, " ");
  if (sbid == NULL)
    return -1;
  
  token = strtok (NULL, "");
  if (token == NULL)
    return -1;
  
  from = atoi(token);
  if (from >= obj->TETRINET_MAX_PLAYERS)
    return -1;
  
  for (sbnum = 0; tetrinet_sbinfo[sbnum].id; sbnum ++)
    if (strcmp (sbid, tetrinet_sbinfo[sbnum].id) == 0)
      return -1;
  
  if (!tetrinet_sbinfo[sbnum].id)
    return -1;

  sp = tetrinet_special_new (from, to, sbnum);
  if (obj->ingame) tetrinet_dospecial (obj, sp);

  return sp;
}

int
tetrinet_do_playerleave (TetrinetObject *obj, char *data)
{
  int pnum;
  char *token;
  
  /* player has left */
  token = strtok (data, " ");
  if (token == NULL)
    return -1;
  pnum = atoi (token);
  if (pnum >= obj->TETRINET_MAX_PLAYERS)
    return -1;
  if (!obj->playercount)
    return -1;
  obj->playercount --;

  /* Clear that player's field */
  memset (obj->fields[pnum], 0, FIELDHEIGHT*FIELDWIDTH);

  return pnum;
}

int
tetrinet_do_teamname (TetrinetObject *obj, char *data)
{
  int pnum;
  char *token;
  
  /* parse string */
  token = strtok (data, " ");
  if (token == NULL)
    return -1;
  pnum = atoi (token);
  if (pnum >= obj->TETRINET_MAX_PLAYERS)
    return -1;
  token = strtok (NULL, "");
  if (token == NULL)
    token = "";
  GTET_O_STRCPY (obj->teamnames[pnum], token);
  
  return pnum;
}

int
tetrinet_do_playerline (TetrinetObject *obj, char *data, char **line)
{
  int pnum;
  char *token;
  
  token = strtok (data, " ");
  if (token == NULL)
    return -1;
  pnum = atoi (token);
  if (pnum >= obj->TETRINET_MAX_PLAYERS)
    return -1;
  token = strtok (NULL, "");
  if (token == NULL) token = "";
  
  if (pnum == 0)
    if (strncmp (token, "\04\04\04\04\04\04\04\04", 8) == 0)
      /* tetrix identification string */
      obj->tetrix = TRUE;

  *line = strdup (token);
  
  return pnum;
}

int
tetrinet_do_playerlost (TetrinetObject *obj, char *data)
{
  int pnum;
  
  pnum = atoi (data);
  if (pnum >= TETRINET_MAX_PLAYERS)
    return -1
  /* player is out */
  obj->playerplaying[pnum] = 0;

  return pnum;
}

int
tetrinet_do_playerwon (TetrinetObject *obj, char *data)
{
  int pnum;
  
  pnum = atoi (data);
  if (pnum >= obj->TETRINET_MAX_PLAYERS)
    return -1;

  return pnum;
}


int
tetrinet_do_playerkick (TetrinetObject *obj, char *data)
{
  int pnum;
  char *token;
  
  token = strtok (data, " ");
  if (token == NULL)
    return -1;
  pnum = atoi (token);
  if (pnum >= obj->TETRINET_MAX_PLAYERS)
    return -1;

  return pnum;
}

void
tetrinet_do_newgame (TetrinetObject *obj, char *data)
{
  int i, j;
  char bfreq[128], sfreq[128];
  
  sscanf (data, "%d %d %d %d %d %d %d %128s %128s %d %d",
          &(obj->initialstackheight), &(obj->initiallevel),
          &(obj->linesperlevel), &(obj->levelinc), &(obj->speciallines),
          &(obj->specialcount), &(obj->specialcapacity),
          bfreq, sfreq, &(obj->levelaverage), &(obj->classicmode));

  bfreq[127] = 0;
  sfreq[127] = 0;
            
  /* initialstackheight == seems ok */
  /* initiallevel == seems ok */
  /* linesperlevel == seems ok */
  /* levelinc == seems ok */
            
  if (!obj->speciallines) /* does divide by this number */
    obj->speciallines = 1;
            
  /* specialcount == seems ok */
            
  if ((unsigned int)(obj->specialcapacity) > sizeof(obj->specialblocks))
    obj->specialcapacity = sizeof(obj->specialblocks);
            
  /* levelaverage == seems ok */
  /* classicmode == seems ok */
            
  /*
    decoding the 11233345666677777 block frequecy thingies:
  */
  for (i = 0; i < 7; i ++) obj->blockfreq[i] = 0;
  for (i = 0; i < 9; i ++) obj->specialfreq[i] = 0;
  /* count frequencies */
  for (i = 0; bfreq[i]; i ++)
    obj->blockfreq[bfreq[i]-'1'] ++;
  for (i = 0; sfreq[i]; i ++)
    obj->specialfreq[sfreq[i]-'1'] ++;
  /* make it cumulative */
  for (i = 0, j = 0; i < 7; i ++) {
    j += obj->blockfreq[i];
    obj->blockfreq[i] = j;
  }
  for (i = 0, j = 0; i < 9; i ++) {
    j += obj->specialfreq[i];
    obj->specialfreq[i] = j;
  }
  /* sanity checks */
  if (obj->blockfreq[6] < 100) obj->blockfreq[6] = 100;
  if (obj->specialfreq[8] < 100) obj->specialfreq[8] = 100;

  tetrinet_startgame (obj);
}

void
tetrinet_do_ingame (TetrinetObject *obj, char *data)
{
  FIELD field;
  int x, y, i;
  
  obj->ingame = TRUE;
  obj->playing = FALSE;
  obj->paused = FALSE;
  
  if (!obj->spectating)
  {
    for (y = 0; y < FIELDHEIGHT; y ++)
      for (x = 0; x < FIELDWIDTH; x ++)
        field[y][x] = randomnum(5) + 1;
    tetrinet_updatefield (obj, field);
    tetrinet_sendfield (obj, 1);
    //fields_drawfield (obj, playerfield(obj, obj->playernum), obj->fields[obj->playernum]);
  }
  
  for (i = 0; i <= 6; i ++)
    obj->playerlevels[i] = -1;
}

int
tetrinet_do_pause (TetrinetObject *obj, char *data)
{
  return atoi (data);
}

char*
tetrinet_do_gmsg (TetrinetObject *obj, char *data)
{
  return strdup (data);
}

void
tetrinet_winlist_add (TetrinetWinlist *list, int team, char *name, int score)
{
  list->item_count++;
  list->items = (TetrinetWinlistItem*) realloc (list->items, sizeof (TetrinetWinlistItem) * item_count);

  list->items[item_count - 1].team = team;
  list->items[item_count - 1].name = strdup (name);
  list->items[item_count - 1].team = score;
}

TetrinetWinlist *
tetrinet_winlist_new ()
{
  TetrinetWinlist *list = (TetrinetWinlist*) malloc (sizeof (TetrinetWinlist));

  list->item_count = 0;
  list->items = NULL;

  return list;
}

void
tetrinet_winlist_destroy (TetrinetWinlist *list)
{
  int i = 0;
  
  while (i < item_count)
  {
    free (list->items[i].name);
    i++;
  }

  free (list->items);
  free (list);
}

TetrinetWinlist*
tetrinet_do_winlist (TetrinetObject *obj, char *data)
{
  char *token, *token2;
  int team, score, i = 0;
  TetrinetWinlist *list;

  list = tetrinet_winlist_new ();
  token = strtok (data, " ");
  if (token == NULL) break;
  do
  {
    switch (*token)
    {
    case 'p': team = FALSE; break;
    case 't': team = TRUE; break;
    default: team = FALSE; break;
    }
    token ++;
    token2 = token;
    while (*token2 != ';' && *token2 != 0) token2 ++;
    *token2 = 0;
    token2 ++;
    score = atoi (token2);
    tetrinet_winlist_add (list, team, token, score);
  } while ((token = strtok (NULL, " ")) != NULL);

  return list;
}

void
tetrinet_do_lvl (TetrinetObject *obj, char *data)
{
  char *token;
  int pnum;
  
  token = strtok (data, " ");
  if (token == NULL) break;
  pnum = atoi (token);
  if (pnum >= TETRINET_MAX_PLAYERS)
    break;
  token = strtok (NULL, "");
  if (token == NULL) break;
  obj->playerlevels[pnum] = atoi (token);
}

char *
tetrinet_do_speclist (TetrinetObject *obj, char *data)
{
  char *token, *channel;
  
  speclist_clear (obj);
  token = strtok (data, " ");
  if (token == NULL) break;
  channel = strdup (token);
  while ((token = strtok (NULL, " ")) != NULL)
    speclist_add (obj, token);

  return channel;
}

TetrinetSpectator *
tetrinet_spectator_new (char *name, char *info)
{
  TetrinetSpectator *spec = (TetrinetSpectator*) malloc (sizeof (TetrinetSpectator));

  spec->name = strdup (name);
  spec->info = strdup (info);

  return spec;
}

void
tetrinet_spectator_destroy (TetrinetSpectator *spec)
{
  free (spec->name);
  free (spec->info);
  free (spec);
}

TetrinetSpectator *
tetrinet_do_specleave (TetrinetObject *obj, char *data)
{
  char *name, *info;
  TetrinetSpectator *spec;
  
  name = strtok (data, " ");
  if (name == NULL) break;
  speclist_remove (obj, name);
  info = strtok (NULL, "");
  if (info == NULL) info = "";

  spec = tetrinet_spectator_new (name, info);

  return spec;
}

TetrinetSpectator *
tetrinet_do_specjoin (TetrinetObject *obj, char *data)
{
  char *name, *info;
  TetrinetSpectator *spec;
              
  name = strtok (data, " ");
  if (name == NULL) break;
  speclist_add (obj, name);
  info = strtok (NULL, "");
  if (info == NULL) info = "";

  spec = tetrinet_spectator_new (name, info);

  return spec;
}

TetrinetMessage *
tetrinet_message_new (char *name, char *text, int action)
{
  TetrinetMessage *message = (TetrinetMessage*) malloc (sizeof (TetrinetMessage));

  message->name = strdup (name);
  message->text = strdup (text);
  message->action = action;

  return message;
}

void
tetrinet_message_destroy (TetrinetMessage *message)
{
  free (message->name);
  free (message->text);
  free (message);
}

TetrinetMessage *
tetrinet_do_smsg (TetrinetObject *obj, char *data)
{
  char *name, *text;
      
  name = strtok (data, " ");
  if (name == NULL) break;
  text = strtok (NULL, "");
  if (text == NULL) text = "";

  return tetrinet_message_new (name, text, 0);
}

void
tetrinet_disconnect (TetrinetObject *obj)
{
  if (obj->ingame) tetrinet_endgame (obj);
  obj->connected = obj->ingame = obj->playing = obj->paused = obj->moderator = obj->tetrix = FALSE;
  obj->playernum = obj->moderatornum = obj->playercount = obj->spectatorcount = 0;
  tetrinet_clear_team_names (obj);
  tetrinet_clear_player_names (obj);
}

static void
tetrinet_clear_team_names (TetrinetObject *obj)
{
  /* clear team list */
  int i;
  for (i = 0; i <= 6; i ++)
    obj->teamnames[i][0] = 0;
}

static void
tetrinet_clear_player_names (TetrinetObject *obj)
{
  /* clear player list */
  int i;
  for (i = 0; i <= 6; i ++)
    obj->playernames[i][0] = 0;
}

TetrinetMessage *
tetrinet_playerline (TetrinetObject *obj, const char *text)
{
  char *buf = (char*) malloc (sizeof (char) * (strlen (text) + 10));
  const char *p;
  TetrinetMessage *message;
  
  if (text[0] == '/') {
    p = text+1;
    if (strncasecmp (p, "me ", 3) == 0) {
      p += 3;
      while (*p && isspace(*p)) p++;
      sprintf (buf, "%d %s", obj->playernum, p);
      client_outmessage (obj, OUT_PLINEACT, buf);
      message = tetrinet_message_new (nick, p, 1);
      free (buf);
      return message;
    }
    if (obj->tetrix) {
      sprintf (buf,"%d %s", obj->playernum, text);
      client_outmessage (obj, OUT_PLINE, buf);
      free (buf);
      return NULL;
    }
    /* send the message without showing it in the partyline */
    sprintf (buf, "%d %s", obj->playernum, text);
    client_outmessage (obj, OUT_PLINE, buf);
    free (buf);
    return NULL;
  }
  sprintf (buf, "%d %s", obj->playernum, text);
  client_outmessage (obj, OUT_PLINE, buf);
  message = tetrinet_message_new (nick, text, 0);
  
  free (buf);
  return message;
}

char *
tetrinet_changeteam (TetrinetObject *obj, const char *newteam)
{
    char *buf;

    if (obj->team != NULL)
      free (obj->team);
    obj->team = strdup (newteam);

    if (obj->connected)
    {
      buf = (char*) malloc (sizeof (char) * (strlen (obj->team) + 10));
      sprintf (buf, "%d %s", obj->playernum, obj->team);
      client_outmessage (obj, OUT_TEAM, buf);
      return buf;
    }

    return NULL;
}

void
tetrinet_sendfield (TetrinetObject *obj, int reset)
{
  int x, y, i, d = 0; /* d is the number of differences */
  char buf[1024], *p;

  char diff_buf[15][(FIELDWIDTH + 1)* FIELDHEIGHT * 2] = {0};

  int row_count[15] = {1,1,1,1,1, 1,1,1,1,1, 1,1,1,1,1};
  
  sprintf (buf, "%d ", obj->playernum);

  if(!reset) {
    /* Find out which blocks changed, how, and how many */
    for (y = 0; y < FIELDHEIGHT; y ++) {
      for (x = 0; x < FIELDWIDTH; x ++) {

	const int block = obj->fields[obj->playernum][y][x];

        if ((block < 0) || (block >= 15))
        {
          printf ("sendfield shouldn't reach here, block=%d\n", block);
          continue;
        }
        
	if (block != obj->sentfield[y][x]) {
	  diff_buf[block][row_count[block]++] = x + '3';
	  diff_buf[block][row_count[block]++] = y + '3';
	  d += 2;
	}
      }
    }
    if (d == 0) return; /* no differences */

    for (i = 0; i < 15; ++i)
      if (row_count[i] > 1)
        ++d; /* add an extra value for the '!'+i at the start */
  }
  
  if (reset || d >= (FIELDHEIGHT*FIELDWIDTH)) {
    /* sending entire field is more efficient */
    p = buf + 2;
    for (y = 0; y < FIELDHEIGHT; y ++)
      for (x = 0; x < FIELDWIDTH; x ++)
	*p++ = obj->blocks[(int)(obj->fields[obj->playernum][y][x])];
    *p = 0;
  }
  else { /* so now we need to create the buffer strings */
    for(i = 0; i < 15; ++i) {
      if(row_count[i] > 1) {
	diff_buf[i][0] = '!' + i;
	GTET_O_STRCAT (buf, diff_buf[i]);
      }
    }
  }

  /* send it */
  client_outmessage (obj, OUT_F, buf);
  /* update the one in our memory */
  copyfield (obj->sentfield, obj->fields[playernum]);
}

void
tetrinet_updatefield (TetrinetObject *obj, FIELD field)
{
    copyfield (obj->fields[obj->playernum], field);
}

/* Seems that nobody uses this */
static void
tetrinet_resendfield (TetrinetObject *obj)
{
    char buf[1024], buf2[1024], *p;
    int x, y;
    
    p = buf2;
    for (y = 0; y < FIELDHEIGHT; y ++)
        for (x = 0; x < FIELDWIDTH; x ++)
            *p++ = obj->blocks[(int)(obj->sentfield[y][x])];
    *p = 0;
    g_snprintf (buf, sizeof(buf), "%d %s", obj->playernum, buf2);
    client_outmessage (obj, OUT_F, buf);
}

/* adds a special block to player's collection */
static void
tetrinet_addspecial (TetrinetObject *obj, char sb)
{
    int l, i;
    if (obj->specialblocknum >= obj->specialcapacity) return; /* too many ! */

    /* add to a random location between, 0 and last offset.
     * Ie. If you have X sps already it gets added between 0 and X */
    l = randomnum(++(obj->specialblocknum));
    for (i = obj->specialblocknum; i > l; i --)
        obj->specialblocks[i] = obj->specialblocks[i-1];
    obj->specialblocks[l] = sb;
//    fields_setspeciallabel (obj->specialblocks[0]);
}

/* adds a random special block to the field */
static void
tetrinet_addsbtofield (TetrinetObject *obj, int count)
{
    int s, n, c, x, y, i, j;
    char sb;
    FIELD field;
    copyfield (field, obj->fields[obj->playernum]);
    
    for (i = 0; i < count; i ++) {
        /* get a random special block */
        s = 0;
        n = randomnum(100);
        while (n >= obj->specialfreq[s]) s ++;
        sb = 6 + s;
        /* sb is the special block that we want */
        /* count the number of non-special blocks on the field */
        c = 0;
        for (y = 0; y < FIELDHEIGHT; y ++)
            for (x = 0; x < FIELDWIDTH; x ++)
                if (field[y][x] > 0 && field[y][x] < 6) c ++;
        if (c == 0) { /* drop block */
            /* i *think* this is how it works in the original -
             blocks are not dropped on existing specials,
             and it tries again to find another spot...
             this is because... when a large number of
             blocks are dropped, usually all columns get 1 block
             but sometimes a column or two doesnt get a block */
            for (j = 0; j < 20; j ++) {
                n = randomnum (FIELDWIDTH);
                for (y = 0; y < FIELDHEIGHT; y ++)
                    if (field[y][n]) break;
                if (y == FIELDHEIGHT || field[y][n] < 6) break;
            }
            if (j == 20) goto end;
            y --;
            field[y][n] = sb;
        }
        else { /* choose a random location */
            n = randomnum (c);
            for (y = 0; y < FIELDHEIGHT; y ++)
                for (x = 0; x < FIELDWIDTH; x ++)
                    if (field[y][x] > 0 && field[y][x] < 6) {
                        if (n == 0) {
                            field[y][x] = sb;
                            goto next;
                        }
                        n --;
                    }
        next:
            /* fall through */ ;
        }
    }
end:
    tetrinet_updatefield (obj, field);
}

/* specials */
static void
tetrinet_dospecial (TetrinetObject *obj, TetrinetSpecial *sp)
{
    FIELD field;
    int x, y, i;

    if (!obj->playing) return; /* we're not playing !!! */

    if (sp->from == obj->playernum && sp->type == TETRINET_SPECIAL_SWITCH) {
        /* we have to switch too... */
        sp->from = sp->to;
        sp->to = obj->playernum;
    }

    if (!(sp->to == 0 && sp->from != obj->playernum) && sp->to != obj->playernum)
        return; /* not for this player */

    if (sp->to == 0)
        /* same team */
        if (obj->team[0] && strcmp (obj->teamnames[sp->from], obj->team) == 0) return;

    copyfield (field, obj->fields[obj->playernum]);

    switch (sp->type)
    {
        /* these add alls need to determine team... */
    case TETRINET_SPECIAL_ADDALL1:
        tetris_addlines (1, 2);
        break;
    case TETRINET_SPECIAL_ADDALL2:
        tetris_addlines (2, 2);
        break;
    case TETRINET_SPECIAL_ADDALL4:
        tetris_addlines (4, 2);
        break;
    case TETRINET_SPECIAL_ADDLINE:
        tetris_addlines (1, 1);
        break;
    case TETRINET_SPECIAL_CLEARLINE:
        for (y = FIELDHEIGHT-1; y > 0; y --)
            for (x = 0; x < FIELDWIDTH; x ++)
                field[y][x] = field[y-1][x];
        for (x = 0; x < FIELDWIDTH; x ++) field[0][x] = 0;
        tetrinet_updatefield (obj, field);
        break;
    case TETRINET_SPECIAL_NUKEFIELD:
        memset (field, 0, FIELDWIDTH*FIELDHEIGHT);
        tetrinet_updatefield (obj, field);
        break;
    case TETRINET_SPECIAL_CLEARBLOCKS:
        for (i = 0; i < 10; i ++)
            field[randomnum(FIELDHEIGHT)][randomnum(FIELDWIDTH)] = 0;
        tetrinet_updatefield (obj, field);
        break;
    case TETRINET_SPECIAL_SWITCH:
        copyfield (field, obj->fields[from]);
        for (y = 0; y < 6; y ++)
            for (x = 0; x < FIELDWIDTH; x ++)
                if (field[y][x]) goto bleep;
    bleep:
        i = 6 - y;
        if (i) {
            /* need to move field down i lines */
            for (y = FIELDHEIGHT-1; y >= i; y --)
                for (x = 0; x < FIELDWIDTH; x ++)
                    field[y][x] = field[y-i][x];
            for (y = 0; y < i; y ++)
                for (x = 0; x < FIELDWIDTH; x ++) field[y][x] = 0;
        }
        tetrinet_updatefield (obj, field);
        break;
    case S_CLEARSPECIAL:
        for (y = 0; y < FIELDHEIGHT; y ++)
            for (x = 0; x < FIELDWIDTH; x ++)
                if (field[y][x] > 5) field[y][x] = randomnum (5) + 1;
        tetrinet_updatefield (obj, field);
        break;
    case S_GRAVITY:
        for (y = 0; y < FIELDHEIGHT; y ++)
            for (x = 0; x < FIELDWIDTH; x ++)
                if (field[y][x] == 0) {
                    /* move the above blocks down */
                    for (i = y; i > 0; i --)
                        field[i][x] = field[i-1][x];
                    field[0][x] = 0;
                }
        tetrinet_updatefield (obj, field);
        break;
    case S_BLOCKQUAKE:
        for (y = 0; y < FIELDHEIGHT; y ++) {
            /* [ the original approximation of blockquake frequencies were
                 not quite correct ] */
            /* ### This is a much better approximation and probably how the */
            /* ### original tetrinet does it */
            int s = 0;
            i = randomnum (22);
            if (i < 1) s ++;
            if (i < 4) s ++;
            if (i < 11) s ++;
            if (randomnum(2)) s = -s;
            tetrinet_shiftline (field, y, s);
            /* ### Corrected by Pihvi */
        }
        tetrinet_updatefield (obj, field);
        break;
    case S_BLOCKBOMB:
        {
            int ax[] = {-1, 0, 1, 1, 1, 0, -1, -1};
            int ay[] = {-1, -1, -1, 0, 1, 1, 1, 0};
            int c = 0;
            char block;
            /* find all bomb blocks */
            for (y = 0; y < FIELDHEIGHT; y ++)
                for (x = 0; x < FIELDWIDTH; x ++)
                    if (field[y][x] == 14) {
                        /* remove the bomb */
                        field[y][x] = 0;
                        /* grab the squares around it */
                        for (i = 0; i < 8; i ++) {
                            if (y+ay[i] >= FIELDHEIGHT || y+ay[i] < 0 ||
                                x+ax[i] >= FIELDWIDTH || x+ax[i] < 0) continue;
                            block = field[y+ay[i]][x+ax[i]];
                            if (block == 14) block = 0;
                            else field[y+ay[i]][x+ax[i]] = 0;
                            buf[c] = block;
                            c ++;
                        }
                    }
            /* scatter blocks */
            for (i = 0; i < c; i ++)
                field[randomnum(FIELDHEIGHT-6)+6][randomnum(FIELDWIDTH)] = buf[i];
        }
        tetrinet_updatefield (obj, field);
        break;
    }
    tetris_removelines (obj, NULL);
    tetrinet_sendfield (obj, 0);
}

static void
tetrinet_shiftline (FIELD field, int l, int d)
{
    int i;
    if (d > 0) { /* to the right */
        for (i = FIELDWIDTH-1; i >= d; i --)
            field[l][i] = field[l][i-d];
        for (; i >= 0; i --) field[l][i] = 0;
    }
    if (d < 0) { /* to the left */
        for (i = 0; i < FIELDWIDTH+d; i ++)
            field[l][i] = field[l][i-d];
        for (; i < FIELDWIDTH; i ++) field[l][i] = 0;
    }
    /* if d == 0 do nothing */
}

/* returns a random block */
static int
tetrinet_getrandomblock (TetrinetObject *obj, void) // FIXME: should be in tetris.c
{
    int i = 0, n = randomnum (100);
    while (n >= obj->blockfreq[i]) i ++;
    return i;
}

int
tetrinet_timeout_duration (TetrinetObject *obj)
{
    return obj->level<=100 ? 1005-(obj->level*10) : 5;
}


/*****************/
/* tetrisy stuff */
/*****************/
static void tetrinet_settimeout (int duration);
static int tetrinet_timeout (void);
static void tetrinet_solidify (void);
static void tetrinet_nextblock (void);
static int tetrinet_nextblocktimeout (void);
static int tetrinet_removelines (void);
static int tetrinet_removelinestimeout (void);

static void
tetrinet_startgame (TetrinetObject *obj)
{
  int i;
  
  obj->linecount = obj->slines = obj->llines = 0;
  obj->level = obj->initiallevel;
  
  for (i = 0; i <= 6; i ++)
    obj->playerlevels[i] = obj->initiallevel;
  for (i = 0; i <= 6; i ++)
    if (obj->playernames[i][0]) obj->playerplaying[i] = TRUE;
  
  tetrinet_updatelevels (obj);
  obj->paused = FALSE;
  obj->specialblocknum = 0;
  obj->ingame = TRUE;
  clearallfields (obj);
  
  if (!obj->spectating)
  {
    obj->playing = TRUE;
    
    nextblock = tetrinet_getrandomblock (obj);
    nextorient = tetris_randomorient (nextblock);
    obj->next_block = tetris_get_block (nextblock, nextorient);
    
    tetris_addlines (obj, obj->initialstackheight, 1);
    tetrinet_sendfield (obj, 1);
    tetrinet_nextblock (obj);
  }
}

void
tetrinet_pause_game (TetrinetObject *obj)
{
    obj->paused = TRUE;
}

void
tetrinet_resume_game (TetrinetObject *obj)
{
    obj->paused = FALSE;
}

void
tetrinet_playerlost (TetrinetObject *obj)
{
    int x, y;
    char buf[11];
    FIELD field;
    
    obj->playing = FALSE;
    /* fix up the display */
    for (y = 0; y < FIELDHEIGHT; y ++)
        for (x = 0; x < FIELDWIDTH; x ++)
            field[y][x] = randomnum(5) + 1;
    tetrinet_updatefield (obj, field);
//    fields_drawfield (playerfield(obj, obj->playernum), obj->fields[obj->playernum]);
//    fields_drawnextblock (tetrinet_blankblock);
    /* send field */
    tetrinet_sendfield (obj, 1);
    /* post message */
    sprintf (buf, "%d", playernum);
    client_outmessage (obj, OUT_PLAYERLOST, buf);
    /* make a sound */
//    sound_playsound (S_YOULOSE);
    /* end timeout thingies */
/*    if (movedowntimeout)
      gtk_timeout_remove (movedowntimeout);
    if (nextblocktimeout)
    gtk_timeout_remove (nextblocktimeout);*/
    
    movedowntimeout = nextblocktimeout = 0;
    tetris_makeblock (obj, -1, 0);
}

/* Returns TRUE if you're the winner (the GUI should play the S_YOUWIN sound) */
int
tetrinet_do_endgame (TetrinetObject *obj) // Library function
{
    int i, c = 0;

    obj->ingame = obj->playing = FALSE;
/*    if (movedowntimeout)
      gtk_timeout_remove (movedowntimeout);
    if (nextblocktimeout)
    gtk_timeout_remove (nextblocktimeout);
    movedowntimeout = nextblocktimeout = 0;*/
    tetris_makeblock (obj, -1, 0);
    /* don't clear messages when game ends */
    /*
    fields_attdefclear ();
    fields_gmsgclear ();
    */
    
    obj->specialblocknum = 0;
    for (i = 1; i <= 6; i ++)
        if (obj->playerplaying[i]) c ++;
    if (obj->playing && obj->playercount > 1 && c == 1)
      return TRUE;
    else
      return FALSE;
}

void
tetrinet_updatelevels (TetrinetObject *obj) // Library function
{
    int c = 0, t = 0, i;
    if (obj->levelaverage) {
        /* average levels */
        for (i = 1; i <= 6; i ++) {
            if (obj->playerplaying[i]) {
                c ++;
                t += obj->playerlevels[i];
            }
        }
        if(c) obj->level = t / c;
//        fields_setactivelevel (level);
    }
//    fields_setlevel (obj->playerlevels[bigfieldnum]);
}


int
tetrinet_next_block_delay (TetrinetObject *obj)
{
  if (obj->gamemode == TETRINET_MODE_TETRIFAST)
    return 0;
  else
    return 1000;
}

int
tetrinet_do_nextblock (TetrinetObject *obj) // Library function
{
  if (tetris_makeblock (obj, nextblock, nextorient))
  {
    /* player died */
    return TRUE;
  }
  nextblock = tetrinet_getrandomblock (obj);
  nextorient = tetris_randomorient (nextblock);
  tetris_drawcurrentblock (obj);

  return FALSE;
}

int
tetrinet_removelines (TetrinetObject *obj) // Library function
{
    char buf[256];
    int lines, i, j, sound = -1, slcount;
    
    lines = tetris_removelines (obj, buf);
    if (lines) {
        obj->linecount += lines;
        obj->slines += lines;
        obj->llines += lines;
//        fields_setlines (obj->linecount);
        /* save specials */
        for (i = 0; i < lines; i ++)
            for (j = 0; buf[j]; j ++)
                tetrinet_addspecial (obj, buf[j]);
//        fields_drawspecials (); 
        /* add specials to field */
        slcount = obj->slines / obj->speciallines;
        obj->slines %= obj->speciallines;
        tetrinet_addsbtofield (obj, obj->specialcount * slcount);
        /* work out what noise to make */
        if (lines == 4) sound = S_TETRIS;
        else if (buf[0]) sound = S_SPECIALLINE;
        else sound = S_LINECLEAR;
        /* increment level */
        if (obj->llines >= obj->linesperlevel) {
            char buf[32];
            while (obj->llines >= obj->linesperlevel) {
                obj->playerlevels[obj->playernum] += obj->levelinc;
                obj->llines -= obj->linesperlevel;
            }
            /* tell everybody else */
            sprintf (buf, "%d %d",
                     obj->playernum, obj->playerlevels[obj->playernum]);
            client_outmessage (obj, OUT_LVL, buf);
            tetrinet_updatelevels (obj);
        }
        /* lines to everyone if in classic mode */
        if (classicmode) {
            int sbnum;
            switch (lines) {
            case 2: sbnum = S_ADDALL1; break;
            case 3: sbnum = S_ADDALL2; break;
            case 4: sbnum = S_ADDALL4; break;
            default: goto endremovelines;
            }
            sprintf (buf, "%i %s %i",
                     0, tetrinet_sbinfo[sbnum].id, obj->playernum);
            client_outmessage (obj, OUT_SB, buf);
            tetrinet_dospecial (obj->playernum, 0, sbnum);
        }
    endremovelines:
        /* end of if */ ;
    }
    /* give it a little delay in drawing */
//    gtk_timeout_add (40, (GtkFunction)tetrinet_removelinestimeout, NULL);
    return sound;
}
/*
int
tetrinet_removelinestimeout (void)
{
    tetris_drawcurrentblock ();
    return FALSE;
}
*/
/* called when the player uses a special */
void
tetrinet_specialkey (TetrinetObject *obj, int pnum) // Library function
{
    char buf[64];
    int sbnum, i;

    if (obj->specialblocknum == 0) return;

    if (pnum != -1 && !obj->playerplaying[pnum]) return;

    /* find which block it is */
    for (sbnum = 0; tetrinet_sbinfo[sbnum].id; sbnum ++)
        if (tetrinet_sbinfo[sbnum].block == obj->specialblocks[0]) break;

    /* remove it from the specials bar */
    for (i = 1; i < obj->specialblocknum; i ++)
        obj->specialblocks[i-1] = obj->specialblocks[i];
    obj->specialblocknum --;
/*    fields_drawspecials ();
    if (obj->specialblocknum > 0)
        fields_setspeciallabel (obj->specialblocks[0]);
    else
    fields_setspeciallabel (-1);*/

    /* just discarding a block, no need to say anything */
    if (pnum == -1) return;

    /* send it out */
    sprintf (buf, "%i %s %i", pnum, tetrinet_sbinfo[sbnum].id, obj->playernum);
    client_outmessage (obj, OUT_SB, buf);

    tetrinet_dospecial (obj->playernum, pnum, sbnum);
}

/******************/
/* misc functions */
/******************/

void
tetrinet_checkmoderatorstatus (TetrinetObject *obj) // Library function
{
    int i;
    
    /* find the lowest numbered player, or 0 if no players exist */
    for (i = 1; i <= 6; i ++)
        if (obj->playernames[i][0]) break;
    if (i > 6) i = 0;
    if (!obj->spectating && (i == obj->playernum)) obj->moderator = TRUE;
    else obj->moderator = FALSE;
//    commands_checkstate ();
    if (obj->moderatornum != i) {
        obj->moderatornum = i;
//        moderatorupdate (0);
    }
}

char
tetrinet_translateblock (char c) // Library function
{
    int i;
    
    for (i = 0; tetrinet_block_chars[i]; i ++)
        if (c == tetrinet_block_chars[i]) return i;
    return 0;
}

void
tetrinet_clearallfields (TetrinetObject *obj) // Library function
{
    int i;
    
    memset (obj->fields, 0, 7 * FIELDHEIGHT * FIELDWIDTH);
/*    for (i = 1; i <= 6; i ++)
      fields_drawfield (playerfield(i), obj->fields[i]);*/
}

void
tetrinet_speclist_clear (TetrinetObject *obj) // Library function
{
    obj->spectatorcount = 0;
}

void
tetrinet_speclist_add (TetrinetObject *obj, char *name) // Library function
{
    int p, i;
    
    if (obj->spectatorcount == (sizeof(obj->spectatorlist) / sizeof(obj->spectatorlist[0])))
      return;
    
    for (p = 0; p < obj->spectatorcount; p++)
        if (strcasecmp (name, obj->spectatorlist[p]) < 0) break;
    for (i = obj->spectatorcount; i > p; i--)
        GTET_O_STRCPY (obj->spectatorlist[i], obj->spectatorlist[i-1]);
    GTET_O_STRCPY (obj->spectatorlist[p], name);
    obj->spectatorcount ++;
}

void
tetrinet_speclist_remove (TetrinetObject *obj, char *name) // Library function
{
    int i;
    for (i = 0; i < obj->spectatorcount; i ++) {
        if (strcmp(name, obj->spectatorlist[i]) == 0) {
            for (; i < obj->spectatorcount-1; i++)
                GTET_O_STRCPY (obj->spectatorlist[i], obj->spectatorlist[i+1]);
            obj->spectatorcount --;
            return;
        }
    }
}

void
tetrinet_remove_player (TetrinetObject *obj, int pnum)
{
  obj->playernames[pnum][0] = 0;
  obj->teamnames[pnum][0] = 0;
  obj->playerplaying[pnum] = 0;  
}

