#ifndef __LIBTETRINET_CLIENT_H__
#define __LIBTETRINET_CLIENT_H__

#include "tetrinet.h"

/* inmsgs are messages coming from the server */
enum {
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
} TetrinetInmsg;

/* outmsgs are messages going out to the server */
enum {
    TETRINET_OUT_UNUSED,
    TETRINET_OUT_DISCONNECT, TETRINET_OUT_CONNECTED,
    TETRINET_OUT_TEAM,
    TETRINET_OUT_PLINE, TETRINET_OUT_PLINEACT,
    TETRINET_OUT_PLAYERLOST,
    TETRINET_OUT_F, TETRINET_OUT_SB, TETRINET_OUT_LVL, TETRINET_OUT_GMSG,
    TETRINET_OUT_STARTGAME, TETRINET_OUT_PAUSE,
    TETRINET_OUT_VERSION
} TetrinetOutmsg;

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

/* functions for connecting and disconnecting */
extern void tetrinet_client_init (TetrinetObject *obj);
extern void tetrinet_disconnect (TetrinetObject *obj);

/* for sending stuff back and forth */
extern void tetrinet_outmessage (TetrinetObject *obj, int msgtype, char *str);
extern void tetrinet_inmessage (TetrinetObject *obj, char *str);

#endif
