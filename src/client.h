#ifndef __LIBTETRINET_CLIENT_H__
#define __LIBTETRINET_CLIENT_H__

/* inmsgs are messages coming from the server */
enum TetrinetInmsg {
    TETRINET_IN_UNUSED,
    TETRINET_IN_CONNECT, TETRINET_IN_DISCONNECT, TETRINET_IN_CONNECTERROR,
    TETRINET_IN_PLAYERNUM, TETRINET_IN_PLAYERJOIN, TETRINET_IN_PLAYERLEAVE, TETRINET_IN_KICK,
    TETRINET_IN_TEAM,
    TETRINET_IN_PLINE, TETRINET_IN_PLINEACT,
    TETRINET_IN_PLAYERLOST, TETRINET_IN_PLAYERWON,
    TETRINET_IN_NEWGAME, TETRINET_IN_INGAME, TETRINET_IN_PAUSE, TETRINET_IN_ENDGAME,
    TETRINET_IN_F, TETRINET_IN_SB, TETRINET_IN_LVL, TETRINET_IN_GMSG,
    TETRINET_IN_WINLIST,
    TETRINET_IN_SPECJOIN, TETRINET_IN_SPECLEAVE, TETRINET_IN_SPECLIST, TETRINET_IN_SMSG, TETRINET_IN_SACT
};

/* outmsgs are messages going out to the server */
enum TetrinetOutmsg {
    TETRINET_OUT_UNUSED,
    TETRINET_OUT_DISCONNECT, TETRINET_OUT_CONNECTED,
    TETRINET_OUT_TEAM,
    TETRINET_OUT_PLINE, TETRINET_OUT_PLINEACT,
    TETRINET_OUT_PLAYERLOST,
    TETRINET_OUT_F, TETRINET_OUT_SB, TETRINET_OUT_LVL, TETRINET_OUT_GMSG,
    TETRINET_OUT_STARTGAME, TETRINET_OUT_PAUSE,
    TETRINET_OUT_VERSION
};

typedef struct
{
  int msgtype;
  char *data;
} TetrinetServerMsg;

#define TETRINET_PORT 31457
#define TETRINET_SPECPORT 31458

#define TETRINET_ERR_RESOLV  -1
#define TETRINET_ERR_CONNECT -2
#define TETRINET_ERR_SOCKET  -3

/* structures and arrays for message translation */

struct inmsgt {
    enum TetrinetInmsg num;
    char *str;
};

struct outmsgt {
    enum TetrinetOutmsg num;
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

#include "tetrinet.h"

/* functions for connecting and disconnecting */
extern void tetrinet_client_init (TetrinetObject *obj);
extern void tetrinet_disconnect (TetrinetObject *obj);

/* for sending stuff back and forth */
extern void tetrinet_outmessage (TetrinetObject *obj, unsigned int msgtype, char *str);
extern TetrinetServerMsg * tetrinet_inmessage (char *str);

#endif
