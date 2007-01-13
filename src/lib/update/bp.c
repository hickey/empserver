/*
 *  Empire - A multi-player, client/server Internet based war game.
 *  Copyright (C) 1986-2007, Dave Pare, Jeff Bailey, Thomas Ruschak,
 *                           Ken Stevens, Steve McClure
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
 *
 *  ---
 *
 *  See files README, COPYING and CREDITS in the root of the source
 *  tree for related information and legal notices.  It is expected
 *  that future projects/authors will amend these files as needed.
 *
 *  ---
 *
 *  bp.c: Functions for build pointer (bp) handling
 * 
 *  Known contributors to this file:
 *     Ville Virrankoski, 1996
 *     Markus Armbruster, 2007
 */

#include <config.h>

#include "budg.h"
#include "update.h"

struct bp {
    int val[7];
};

static int bud_key[I_MAX + 2] =
    { 1, 2, 3, 4, 0, 0, 0, 0, 0, 0, 5, 6, 0, 0, 7 };

static struct bp *
bp_ref(struct bp *bp, struct sctstr *sp)
{
    return &bp[sp->sct_x + sp->sct_y * WORLD_X];
}

int
gt_bg_nmbr(struct bp *bp, struct sctstr *sp, i_type comm)
{
    int cm;

    if ((cm = bud_key[comm]) == 0)
	return sp->sct_item[comm];
    return bp_ref(bp, sp)->val[cm - 1];
}

void
pt_bg_nmbr(struct bp *bp, struct sctstr *sp, i_type comm, int amount)
{
    int cm;

    if ((cm = bud_key[comm]) != 0)
	bp_ref(bp, sp)->val[cm - 1] = amount;
}

void
fill_update_array(struct bp *bp, struct sctstr *sp)
{
    int k;
    struct bp *p = bp_ref(bp, sp);
    i_type i;

    for (i = I_NONE + 1; i <= I_MAX; i++) {
	if ((k = bud_key[i]) != 0)
	    p->val[k - 1] = sp->sct_item[i];
    }
    p->val[bud_key[I_MAX + 1] - 1] = sp->sct_avail;
}

struct bp *
alloc_bp(void)
{
    return calloc(WORLD_X * WORLD_Y, sizeof(struct bp));
}
