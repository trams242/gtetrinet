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

#include <string.h>

#include "tetrinet.h"
#include "tetris.h"

static int blockobstructed (FIELD field, int block, int orient, int bx, int by);
static int obstructed (FIELD field, int x, int y);
static void placeblock (FIELD field, int block, int orient, int bx, int by);

FIELD *
tetris_drawcurrentblock (TetrinetObject *obj)
{
    FIELD *field;

    field = (FIELD*) malloc (sizeof (FIELD));
    
    tetris_copyfield (*field, obj->fields[obj->playernum]);
    if (obj->current_block_num >= 0)
      placeblock (*field, obj->current_block_num, obj->current_block_orient, obj->current_block_x, obj->current_block_y);

    return field;
}

int
tetris_makeblock (TetrinetObject *obj, int block, int orient)
{
    obj->current_block_num = block;
    obj->current_block_orient = orient;
    obj->current_block_x = TETRINET_FIELDWIDTH/2-2;
    obj->current_block_y = 0;

    if (block >= 0 &&
        blockobstructed (obj->fields[obj->playernum], obj->current_block_num,
                         obj->current_block_orient, obj->current_block_x, obj->current_block_y))
    {
        /* player is dead */
        tetrinet_playerlost (obj);
        obj->current_block_num = -1;
        return TRUE;
    }
    else
      return FALSE;
}

/* returns -1 if block solidifies, 0 otherwise */
int
tetris_blockdown (TetrinetObject *obj)
{
    if (obj->current_block_num < 0) return 0;
    /* move the block down one */
    if (blockobstructed (obj->fields[obj->playernum], obj->current_block_num,
                         obj->current_block_orient, obj->current_block_x, obj->current_block_y + 1))
    {
        /* cant move down */
#ifdef DEBUG
        printf ("blockobstructed: %d %d\n", obj->current_block_x, obj->current_block_y);
#endif
        return -1;
    }
    else {
        obj->current_block_y ++;
        return 0;
    }
}

void
tetris_blockmove (TetrinetObject *obj, int dir)
{
    if (obj->current_block_num < 0) return;
    if (blockobstructed (obj->fields[obj->playernum], obj->current_block_num,
                         obj->current_block_orient, obj->current_block_x + dir, obj->current_block_y))
    /* do nothing */;
    else
      obj->current_block_x += dir;
}

void
tetris_blockrotate (TetrinetObject *obj, int dir)
{
    int neworient = obj->current_block_orient + dir;
    
    if (obj->current_block_num < 0)
      return;
    
    if (neworient >= tetris_blockcount[obj->current_block_num])
      neworient = 0;
    
    if (neworient < 0)
      neworient = tetris_blockcount[obj->current_block_num] - 1;
    
    switch (blockobstructed (obj->fields[obj->playernum], obj->current_block_num,
                             neworient, obj->current_block_x, obj->current_block_y))
    {
    case 1: return; /* cant rotate if obstructed by blocks */
    case 2: /* obstructed by sides - move block away if possible */
    {
      int shifts[4] = {1, -1, 2, -2};
      int i;
      
      for (i = 0; i < 4; i ++) {
        if (!blockobstructed (obj->fields[obj->playernum], obj->current_block_num,
                              neworient, obj->current_block_x + shifts[i],
                              obj->current_block_y))
        {
          obj->current_block_x += shifts[i];
          goto end;
        }
      }
      return; /* unsuccessful */
    }
    }
end:
    obj->current_block_orient = neworient;
}

void
tetris_blockdrop (TetrinetObject *obj)
{
    if (obj->current_block_num < 0) return;
    while (tetris_blockdown (obj) == 0);
}

void
tetris_addlines (TetrinetObject *obj, int count, int type)
{
    int x, y, i;
    FIELD field;
    
    tetris_copyfield (field, obj->fields[obj->playernum]);
    for (i = 0; i < count; i ++) {
        /* check top row */
        for (x = 0; x < TETRINET_FIELDWIDTH; x ++) {
            if (field[0][x]) {
                /* player is dead */
                tetrinet_playerlost (obj);
                return;
            }
        }
        /* move everything up one */
        for (y = 0; y < TETRINET_FIELDHEIGHT - 1; y ++) {
            for (x = 0; x < TETRINET_FIELDWIDTH; x ++)
                field[y][x] = field[y+1][x];
        }
        /* generate a random line with spaces in it */
        switch (type) {
        case 1: /* addline lines */
            /* ### This is how the original tetrinet seems to do an add line */
            for (x = 0; x < TETRINET_FIELDWIDTH; x ++)
                field[TETRINET_FIELDHEIGHT-1][x] = tetris_randomnum (6);
            field[TETRINET_FIELDHEIGHT-1][tetris_randomnum (TETRINET_FIELDWIDTH)] = 0;
            /* ### Corrected by Pihvi */
            break;
        case 2: /* classicmode lines */
            /* fill up the line */
            for (x = 0; x < TETRINET_FIELDWIDTH; x ++)
                field[TETRINET_FIELDHEIGHT-1][x] = tetris_randomnum(5) + 1;
            /* add a single space */
            field[TETRINET_FIELDHEIGHT-1][tetris_randomnum(TETRINET_FIELDWIDTH)] = 0;
            break;
        }
    }
    tetrinet_updatefield (obj, field);
}

/* this function removes full lines */
int
tetris_removelines (TetrinetObject *obj, char *specials)
{
    int x, y, o, c = 0, i;
    FIELD field;
    
    if (!obj->playing) return 0;
    tetris_copyfield (field, obj->fields[obj->playernum]);
    /* remove full lines */
    for (y = 0; y < TETRINET_FIELDHEIGHT; y ++) {
        o = 0;
        /* count holes */
        for (x = 0; x < TETRINET_FIELDWIDTH; x ++)
            if (field[y][x] == 0) o ++;
        if (o) continue; /* if holes */
        /* no holes */
        /* increment line count */
        c ++;
        /* grab specials */
        if (specials)
            for (x = 0; x < TETRINET_FIELDWIDTH; x ++)
                if (field[y][x] > 5)
                    *specials++ = field[y][x];
        /* move field down */
        for (i = y-1; i >= 0; i --)
            for (x = 0; x < TETRINET_FIELDWIDTH; x ++)
                field[i+1][x] = field[i][x];
        /* clear top line */
        for (x = 0; x < TETRINET_FIELDWIDTH; x ++)
            field[0][x] = 0;
    }
    if (specials) *specials = 0; /* null terminate */
    if (c) tetrinet_updatefield (obj, field);
    return c;
}

void
tetris_solidify (TetrinetObject *obj)
{
    FIELD field;
    
    tetris_copyfield (field, obj->fields[obj->playernum]);
    
    if (obj->current_block_num < 0)
      return;
    
    if (blockobstructed (field, obj->current_block_num, obj->current_block_orient, obj->current_block_x, obj->current_block_y)) {
        /* move block up until we get a free spot */
        for (obj->current_block_y --; obj->current_block_y >= 0; obj->current_block_y --)
            if (!blockobstructed (field, obj->current_block_num, obj->current_block_orient, obj->current_block_x, obj->current_block_y))
            {
                placeblock (field, obj->current_block_num, obj->current_block_orient, obj->current_block_x, obj->current_block_y);
                break;
            }
        if (blocky < 0) {
            /* no space - player has lost */
            tetrinet_playerlost (obj);
            obj->current_block_num = -1;
            return;
        }
    }
    else {
        placeblock (field, obj->current_block_num, obj->current_block_orient, obj->current_block_x, obj->current_block_y);
    }
    tetrinet_updatefield (obj, field);
    obj->current_block_num = -1;
}

void
tetris_copyfield (FIELD dest, FIELD src)
{
    memcpy ((void *)dest, (void *)src, TETRINET_FIELDHEIGHT * TETRINET_FIELDWIDTH);
}

int
tetris_randomorient (int block)
{
    return tetris_randomnum (tetris_blockcount[block]);
}

TETRISBLOCK_P
tetris_getblock (int block, int orient)
{
    return tetris_blocks[block][orient];
}

static int
blockobstructed (FIELD field, int block, int orient, int bx, int by)
{
    int x, y, side = 0;
    for (y = 0; y < 4; y ++)
        for (x = 0; x < 4; x ++)
            if (blocks[block][orient][y][x]) {
                switch (obstructed (field, bx+x, by+y)) {
                case 0: continue;
                case 1: return 1;
                case 2: side = 2;
                }
            }
    return side;
}

static int
obstructed (FIELD field, int x, int y)
{
    if (x < 0) return 2;
    if (x >= TETRINET_FIELDWIDTH) return 2;
    if (y < 0) return 1;
    if (y >= TETRINET_FIELDHEIGHT) return 1;
    if (field[y][x]) return 1;
    return 0;
}

static void
placeblock (FIELD field, int block, int orient, int bx, int by)
{
    int x, y;
    for (y = 0; y < 4; y ++)
        for (x = 0; x < 4; x ++) {
            if (tetris_blocks[block][orient][y][x])
                field[y+by][x+bx] = tetris_blocks[block][orient][y][x];
        }
}

/* returns a random number in the range 0 to n-1 --
 * Note both n==0 and n==1 always return 0 */
int
tetris_randomnum (int n)
{
    return (float)n*rand()/(RAND_MAX+1.0);
}

