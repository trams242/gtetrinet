#ifndef __LIBTETRINET_TETRIS_H__
#define __LIBTETRINET_TETRIS_H__

#include "tetrinet.h"

typedef char (*TETRIS_BLOCK_P)[4];

TETRIS_BLOCK tetris_block1[2] = {
    {
        {1,1,1,1},
        {0,0,0,0},
        {0,0,0,0},
        {0,0,0,0}
    }, {
        {0,0,1,0},
        {0,0,1,0},
        {0,0,1,0},
        {0,0,1,0}
    }
};

TETRIS_BLOCK tetris_block2[1] = {
    {
        {0,2,2,0},
        {0,2,2,0},
        {0,0,0,0},
        {0,0,0,0}
    }
};

TETRIS_BLOCK tetris_block3[4] = {
    {
        {0,0,3,0},
        {0,0,3,0},
        {0,3,3,0},
        {0,0,0,0}
    }, {
        {0,3,0,0},
        {0,3,3,3},
        {0,0,0,0},
        {0,0,0,0}
    }, {
        {0,3,3,0},
        {0,3,0,0},
        {0,3,0,0},
        {0,0,0,0}
    }, {
        {0,3,3,3},
        {0,0,0,3},
        {0,0,0,0},
        {0,0,0,0}
    }
};

TETRIS_BLOCK tetris_block4[4] = {
    {
        {0,4,0,0},
        {0,4,0,0},
        {0,4,4,0},
        {0,0,0,0}
    }, {
        {0,4,4,4},
        {0,4,0,0},
        {0,0,0,0},
        {0,0,0,0}
    }, {
        {0,4,4,0},
        {0,0,4,0},
        {0,0,4,0},
        {0,0,0,0}
    }, {
        {0,0,0,4},
        {0,4,4,4},
        {0,0,0,0},
        {0,0,0,0}
    }
};

TETRIS_BLOCK tetris_block5[2] = {
    {
        {0,0,5,0},
        {0,5,5,0},
        {0,5,0,0},
        {0,0,0,0}
    }, {
        {0,5,5,0},
        {0,0,5,5},
        {0,0,0,0},
        {0,0,0,0}
    }
};

TETRIS_BLOCK tetris_block6[2] = {
    {
        {0,1,0,0},
        {0,1,1,0},
        {0,0,1,0},
        {0,0,0,0}
    }, {
        {0,0,1,1},
        {0,1,1,0},
        {0,0,0,0},
        {0,0,0,0}
    }
};

TETRIS_BLOCK tetris_block7[4] = {
    {
        {0,0,2,0},
        {0,2,2,0},
        {0,0,2,0},
        {0,0,0,0}
    }, {
        {0,0,2,0},
        {0,2,2,2},
        {0,0,0,0},
        {0,0,0,0}
    }, {
        {0,2,0,0},
        {0,2,2,0},
        {0,2,0,0},
        {0,0,0,0}
    }, {
        {0,2,2,2},
        {0,0,2,0},
        {0,0,0,0},
        {0,0,0,0}
    }
};

static TETRIS_BLOCK *tetris_blocks[7] =
{
  tetris_block1,
  tetris_block2,
  tetris_block3,
  tetris_block4,
  tetris_block5,
  tetris_block6,
  tetris_block7
};

static int tetris_blockcount[7] = { 2, 1, 4, 4, 2, 2, 4 };

extern void tetris_drawcurrentblock (TetrinetObject *obj);
extern int tetris_makeblock (TetrinetObject *obj, int block, int orient);
extern int tetris_blockdown (TetrinetObject *obj);
extern void tetris_blockmove (TetrinetObject *obj, int dir);
extern void tetris_blockrotate (TetrinetObject *obj, int dir);
extern void tetris_blockdrop (TetrinetObject *obj);
extern void tetris_solidify (TetrinetObject *obj);
extern void tetris_addlines (TetrinetObject *obj, int count, int type);
extern int tetris_removelines (TetrinetObject *obj, char *specials);

extern void tetris_copyfield (TETRINET_FIELD dest, TETRINET_FIELD src);
extern int tetris_randomorient (int block);
extern TETRIS_BLOCK_P tetris_getblock (int block, int orient);
extern int tetris_randomnum (int n);

#endif
