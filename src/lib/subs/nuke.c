/*
 *  Empire - A multi-player, client/server Internet based war game.
 *  Copyright (C) 1986-2006, Dave Pare, Jeff Bailey, Thomas Ruschak,
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
 *  nuke.c: Nuke post-read and pre-write data massage
 * 
 *  Known contributors to this file:
 *     Dave Pare, 1989
 *     Steve McClure, 1996
 */

#include <config.h>

#include "misc.h"
#include "player.h"
#include "nuke.h"
#include "sect.h"
#include "xy.h"
#include "nsc.h"
#include "file.h"
#include "nat.h"
#include "prototypes.h"

int
nuk_postread(int n, void *ptr)
{
    struct nukstr *np = ptr;
    struct plnstr plane;

    if (np->nuk_uid != n) {
	logerror("nuk_postread: Error - %d != %d, zeroing.\n", np->nuk_uid,
		 n);
	memset(np, 0, sizeof(struct nukstr));
    }

    if (np->nuk_plane >= 0 && np->nuk_own && np->nuk_effic >= 0) {
	if (getplane(np->nuk_plane, &plane)
	    && plane.pln_effic >= PLANE_MINEFF) {
	    if (np->nuk_x != plane.pln_x || np->nuk_y != plane.pln_y) {
		time(&np->nuk_timestamp);
		np->nuk_x = plane.pln_x;
		np->nuk_y = plane.pln_y;
	    }
	}
    }

    player->owner = (player->god || np->nuk_own == player->cnum);
    return 1;
}

int
nuk_prewrite(int n, void *ptr)
{
    struct nukstr *np = ptr;
    struct nukstr nuke;

    if (np->nuk_effic == 0) {
	if (np->nuk_own)
	    makelost(EF_NUKE, np->nuk_own, np->nuk_uid,
		     np->nuk_x, np->nuk_y);
	np->nuk_own = 0;
	np->nuk_effic = 0;
    }

    np->ef_type = EF_NUKE;
    np->nuk_uid = n;

    time(&np->nuk_timestamp);

    getnuke(n, &nuke);

    return 1;
}

void
nuk_init(int n, void *ptr)
{
    struct nukstr *np = ptr;

    np->ef_type = EF_NUKE;
    np->nuk_uid = n;
    np->nuk_own = 0;
}

int
nuk_on_plane(struct nukstr *np, int pluid)
{
    struct nstr_item ni;

    snxtitem_all(&ni, EF_NUKE);
    while (nxtitem(&ni, np)) {
	if (np->nuk_plane == pluid)
	    return np->nuk_uid;
    }
    return -1;
}

char *
prnuke(struct nukstr *np)
{
    return prbuf("%s warhead #%d",
		 nchr[(int)np->nuk_type].n_name, np->nuk_uid);
}
