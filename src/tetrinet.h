#ifndef __LIBTETRINET_TETRINET_H__
#define __LIBTETRINET_TETRINET_H__

#ifdef HAVE_CONFIG_H
# include "../config.h"
#endif

#include <unistd.h>
#include <stdio.h>
#include <ctype.h>


//#include "config.h"
//#include "fields.h"
//#include "partyline.h"
//#include "misc.h"
//#include "dialogs.h"
//#include "sound.h"

#define TETRINET_DOWNDELAY 100
#define TETRINET_PARTYLINEDELAY1 100
#define TETRINET_PARTYLINEDELAY2 200
#define TETRINET_MAX_PLAYERS 7
#define TETRINET_FIELDWIDTH 12
#define TETRINET_FIELDHEIGHT 22

typedef char TETRINET_FIELD[TETRINET_FIELDHEIGHT][TETRINET_FIELDWIDTH];
typedef char TETRIS_BLOCK[4][4];

typedef enum
{
  TETRINET_SPECIAL_ADDALL1,
  TETRINET_SPECIAL_ADDALL2,
  TETRINET_SPECIAL_ADDALL4,
  TETRINET_SPECIAL_ADDLINE,
  TETRINET_SPECIAL_CLEARLINE,
  TETRINET_SPECIAL_NUKEFIELD,
  TETRINET_SPECIAL_CLEARBLOCKS,
  TETRINET_SPECIAL_SWITCH,
  TETRINET_SPECIAL_CLEARSPECIAL,
  TETRINET_SPECIAL_GRAVITY,
  TETRINET_SPECIAL_BLOCKQUAKE,
  TETRINET_SPECIAL_BLOCKBOMB,
  TETRINET_SPECIAL_BLOCKNUM
} TetrinetSpecialBlock;

typedef enum
{
  TETRINET_MODE_ORIGINAL,
  TETRINET_MODE_TETRIFAST
} TetrinetMode;

typedef enum
{
  TETRINET_RIGHT, 
  TETRINET_LEFT, 
  TETRINET_ROTRIGHT, 
  TETRINET_ROTLEFT,
  TETRINET_DOWN,
  TETRINET_DROP,
  TETRINET_DISCARD,
  TETRINET_GAMEMSG,
  TETRINET_SPECIAL1,
  TETRINET_SPECIAL2,
  TETRINET_SPECIAL3, 
  TETRINET_SPECIAL4,
  TETRINET_SPECIAL5,
  TETRINET_SPECIAL6,
/* not a key but the number of configurable keys */
  TETRINET_TOTAL_KEYS
} TetrinetKey;

typedef struct
{
  int team;
  char *name;
  int score;
} TetrinetWinlistItem;

typedef struct
{
  int item_count;
  TetrinetWinlistItem *items;
} TetrinetWinlist;

typedef struct
{
  int from;
  int to;
  int type;
} TetrinetSpecial;

typedef struct
{
  char *name;
  char *info;
} TetrinetSpectator;

typedef struct
{
  char *name;
  char *text;
  int action;
} TetrinetMessage;

struct TetrinetSpecialInfo
{
    char *id;
    int block;
    char *info;
};

/* FIXME: this should be translated */
struct TetrinetSpecialInfo tetrinet_sbinfo[] = {
    {"cs1", -1, "1 Line Added"},
    {"cs2", -1, "2 Lines Added"},
    {"cs4", -1, "4 Lines Added"},
    {"a",   6,  "Add Line"},
    {"c",   7,  "Clear Line"},
    {"n",   8,  "Nuke Field"},
    {"r",   9,  "Clear Random"},
    {"s",  10,  "Switch Fields"},
    {"b",  11,  "Clear Specials"},
    {"g",  12,  "Block Gravity"},
    {"q",  13,  "Blockquake"},
    {"o",  14,  "Block Bomb"},
    {0, 0, 0}
};

typedef struct
{
  int magic;            /* Magic number, it is set to 0xdeadbeef when a Tetrinet object is created */
  int sock;             /* Socket descriptor */
  
  int gamemode;         /* It is one of TETRINET_MODE */
  int playernum;        /* Number of the player in the game */
  char *team;           /* Name of the team to which the player belongs */
  char *nick;           /* Nickname of the player in this server */
  char specpasswd[128]; /* Spectator password */
  char *server;         /* Tetrinet Server */
  
  int ingame;           /* TRUE if the channel is in the middle of a game */
  int playing;          /* TRUE if the player is playing the game */
  int paused;           /* TRUE if the game is paused */
  
  char specialblocks[256]; /* This array contains the special blocks gathered by the player */
  int specialblocknum;     /* FIXME */
  
/* all the game state variables goes here */
  char playernames[TETRINET_MAX_PLAYERS][128]; /* The name of all the players in the channel */
  char teamnames[TETRINET_MAX_PLAYERS][128];   /* The name of the teams of all the players in the channel */
  int playerlevels[TETRINET_MAX_PLAYERS];      /* The levels of the players in the channel */
  int playerplaying[TETRINET_MAX_PLAYERS];     /* TRUE if the player is playing the game, FALSE if not (he has lost or he is new) */
  int playercount;                             /* Number of players in this channel */
  
  char spectatorlist[128][128]; /* Names of the spectators */
  int spectatorcount;           /* Number of spectators in this channel */
  int moderatornum;             /* FIXME: How many moderators are in the game */
  
  int list_issued; /* this will have the number of /list commands sent and waiting for answer */

  TETRINET_FIELD fields[7]; /* The fields of the game */
  TETRINET_FIELD sentfield; /* The field that the server thinks we have */
  TETRIS_BLOCK next_block;  /* The next block */

  int moderator;  /* are we the moderator ? TRUE : FALSE */
  int spectating; /* are we spectating ? TRUE : FALSE */
  int tetrix;     /* are we connected to a tetrix server ? TRUE : FALSE */

  int linecount; /* FIXME */
  int level;     /* FIXME */
  int slines;    /* FIXME */
  int llines;    /* FIXME: lines still to be used for incrementing level, etc */

/* game options from the server */
  int initialstackheight; /* height of random crap */
  int initiallevel;       /* speed speed level */
  int linesperlevel;      /* number of lines before you go up a level */
  int levelinc;           /* amount you go level up, after every linesperlevel */
  int speciallines;       /* ratio of lines needed for special blocks */
  int specialcount;       /* multiplier for special blocks to add */
  int specialcapacity;    /* max number of specials you can have */
  int levelaverage;       /* flag: should we average the levels across all players */
  int classicmode;        /* bitflag: does everyone get lines of blocks when you
                           * 2x, 3x or tetris */

/* These variables identify the block which is falling */
  int current_block_num;    /* which block */
  int current_block_orient; /* block orientation*/
  int current_block_x;
  int current_block_y; /* current location of block */

/* these are actually cumulative frequency counts */
  int blockfreq[7];   /* FIXME */
  int specialfreq[9]; /* FIXME */
} TetrinetObject;

static char tetrinet_block_chars[] = "012345acnrsbgqo";

TETRIS_BLOCK tetrinet_blankblock =
{ {0,0,0,0}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0} };

#include "tetris.h"
#include "client.h"

TetrinetObject *
tetrinet_object_new (char *server, char *nick, char *team);

void
tetrinet_object_set_nickname (TetrinetObject *obj, char *name);

void
tetrinet_object_set_teamname (TetrinetObject *obj, char *name);

void
tetrinet_object_set_server (TetrinetObject *obj, char *name);

void
tetrinet_object_destroy (TetrinetObject *obj);

/* Returns TRUE if it's a confirmation of connection (GUI should be updated) */
int
tetrinet_do_playernum (TetrinetObject *obj, char *data);

int
tetrinet_do_playerjoin (TetrinetObject *obj, char *data);

int
tetrinet_do_f (TetrinetObject *obj, char *data);

void
tetrinet_special_destroy (TetrinetSpecial *sp);

TetrinetSpecial*
tetrinet_special_new (int from, int to, int sbnum);

TetrinetSpecial*
tetrinet_do_special (TetrinetObject *obj, char *data);

int
tetrinet_do_playerleave (TetrinetObject *obj, char *data);

int
tetrinet_do_teamname (TetrinetObject *obj, char *data);

int
tetrinet_do_playerline (TetrinetObject *obj, char *data, char **line);

int
tetrinet_do_playerlost (TetrinetObject *obj, char *data);

int
tetrinet_do_playerwon (TetrinetObject *obj, char *data);

int
tetrinet_do_playerkick (TetrinetObject *obj, char *data);

void
tetrinet_do_newgame (TetrinetObject *obj, char *data);

void
tetrinet_do_ingame (TetrinetObject *obj, char *data);

int
tetrinet_do_pause (TetrinetObject *obj, char *data);

char *
tetrinet_do_gmsg (TetrinetObject *obj, char *data);

TetrinetWinlist *
tetrinet_do_winlist (TetrinetObject *obj, char *data);

void
tetrinet_winlist_add (TetrinetWinlist *list, int team, char *name, int score);

TetrinetWinlist *
tetrinet_winlist_new ();

void
tetrinet_winlist_destroy (TetrinetWinlist *list);

void
tetrinet_do_lvl (TetrinetObject *obj, char *data);

char *
tetrinet_do_speclist (TetrinetObject *obj, char *data);

TetrinetSpectator *
tetrinet_spectator_new (char *name, char *info);

void
tetrinet_spectator_destroy (TetrinetSpectator *spec);

TetrinetSpectator *
tetrinet_do_specleave (TetrinetObject *obj, char *data);

TetrinetSpectator *
tetrinet_do_specjoin (TetrinetObject *obj, char *data);

TetrinetMessage *
tetrinet_message_new (char *name, char *text, int action);

void
tetrinet_message_destroy (TetrinetMessage *message);

TetrinetMessage *
tetrinet_do_smsg (TetrinetObject *obj, char *data);

void
tetrinet_disconnect (TetrinetObject *obj);

void
tetrinet_remove_player (TetrinetObject *obj, int pnum);

TetrinetMessage *
tetrinet_playerline (TetrinetObject *obj, const char *text);

char *
tetrinet_changeteam (TetrinetObject *obj, const char *newteam);

void
tetrinet_sendfield (TetrinetObject *obj, int reset);

/* This function returns the falling speed of the current block */
int
tetrinet_block_fall_delay (TetrinetObject *obj);

void
tetrinet_pause_game (TetrinetObject *obj);

void
tetrinet_resume_game (TetrinetObject *obj);

void
tetrinet_playerlost (TetrinetObject *obj); // Not for the API

int
tetrinet_timeout_duration (TetrinetObject *obj);

void
tetrinet_updatelevels (TetrinetObject *obj);

int
tetrinet_removelines (TetrinetObject *obj);

void
tetrinet_specialkey (TetrinetObject *obj, int pnum);

#endif
