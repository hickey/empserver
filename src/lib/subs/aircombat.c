/*
 *  Empire - A multi-player, client/server Internet based war game.
 *  Copyright (C) 1986-2008, Dave Pare, Jeff Bailey, Thomas Ruschak,
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
 *  aircombat.c: Deal with air to air combat
 *
 *  Known contributors to this file:
 *     Dave Pare, 1986
 *     Thomas Ruschak, 1992
 *     Steve McClure, 1996
 *     Markus Armbruster, 2006-2008
 */

#include <config.h>

#include "file.h"
#include "land.h"
#include "map.h"
#include "misc.h"
#include "nat.h"
#include "news.h"
#include "nsc.h"
#include "optlist.h"
#include "path.h"
#include "plane.h"
#include "player.h"
#include "prototypes.h"
#include "sect.h"
#include "ship.h"
#include "xy.h"

#define FLAK_GUN_MAX 14

static int plane_caps(struct emp_qelem *);
static void ac_intercept(struct emp_qelem *, struct emp_qelem *,
			 struct emp_qelem *, natid, coord, coord);
static int all_missiles(struct emp_qelem *);
static void ac_dog(struct plist *, struct plist *);
static void ac_planedamage(struct plist *, natid, int, natid, int,
			   int, char *);
static void ac_doflak(struct emp_qelem *, struct sctstr *);
static void ac_landflak(struct emp_qelem *, coord, coord);
static void ac_shipflak(struct emp_qelem *, coord, coord);
static void ac_fireflak(struct emp_qelem *, natid, int);
static void getilist(struct emp_qelem *, natid);
static int do_evade(struct emp_qelem *, struct emp_qelem *);

void
ac_encounter(struct emp_qelem *bomb_list, struct emp_qelem *esc_list,
	     coord x, coord y, char *path, int mission_flags,
	     int no_air_defense)
{
    int val, non_missiles;
    int rel;
    int dir;
    int nats[MAXNOC];
    int lnats[MAXNOC];
    int gotilist[MAXNOC];
    int unfriendly[MAXNOC];
    int overfly[MAXNOC];
    int flags;
    struct emp_qelem ilist[MAXNOC], *qp;
    char mypath[1024];
    int myp;
    int civ, mil;
    natid plane_owner;
    struct sctstr sect;
    struct shpstr ship;
    struct lndstr land;
    struct nstr_item ni;
    natid cn;
    struct natstr *over, *mynatp;
    struct plist *plp;
    int evaded;
    struct shiplist *head = NULL;
    int changed = 0;
    int intown = 0;		/* Last owner to intercept */
/* We want to only intercept once per sector per owner.  So, if we overfly
   a sector, and then overfly some land units or ships, we don't want to
   potentially intercept 3 times. */

    plp = (struct plist *)bomb_list->q_forw;
    plane_owner = plp->plane.pln_own;

    strncpy(mypath, path, sizeof(mypath));
    myp = 0;

    memset(overfly, 0, sizeof(overfly));
    memset(gotilist, 0, sizeof(gotilist));
    memset(unfriendly, 0, sizeof(unfriendly));
    for (cn = 1; cn < MAXNOC; cn++) {
	if ((mynatp = getnatp(cn)) == 0)
	    continue;
	rel = getrel(mynatp, plane_owner);
	if (rel > HOSTILE)
	    continue;
	if (plane_owner == cn)
	    continue;
	unfriendly[cn]++;
    }
    if (mission_flags & PM_R) {
	flags = plane_caps(bomb_list);
	if (flags & P_S) {
	    PR(plane_owner, "\nSPY Plane report\n");
	    PRdate(plane_owner);
	    sathead();
	} else if (flags & P_A) {
	    PR(plane_owner, "\nAnti-Sub Patrol report\n");
	} else {
	    PR(plane_owner, "\nReconnaissance report\n");
	    PRdate(plane_owner);
	}
    }

    while ((dir = mypath[myp++]) && !QEMPTY(bomb_list)) {
	if ((val = diridx(dir)) == DIR_STOP)
	    break;
	/* XXX using xnorm is probably bad */
	x = xnorm(x + diroff[val][0]);
	y = ynorm(y + diroff[val][1]);
	getsect(x, y, &sect);
	over = getnatp(sect.sct_own);

	if (mission_flags & PM_R) {
	    flags = plane_caps(bomb_list);
	    if (opt_HIDDEN)
		setcont(plane_owner, sect.sct_own, FOUND_FLY);
	    if (sect.sct_type == SCT_WATER) {
		PR(plane_owner, "flying over %s at %s\n",
		   dchr[sect.sct_type].d_name, xyas(x, y, plane_owner));
		if (mission_flags & PM_S)
		    plane_sweep(bomb_list, x, y);
		if (flags & P_A)
		    plane_sona(bomb_list, x, y, &head);
		changed += map_set(plane_owner,
				   sect.sct_x, sect.sct_y,
				   dchr[sect.sct_type].d_mnem, 0);
	    } else if (flags & P_S) {
		satdisp_sect(&sect, flags & P_I ? 10 : 50);
	    } else {
		/* This is borrowed from lookout */
		if (sect.sct_own == plane_owner)
		    PR(plane_owner, "Your ");
		else
		    PR(plane_owner, "%s (#%d) ",
		       cname(sect.sct_own), sect.sct_own);
		PR(plane_owner, "%s", dchr[sect.sct_type].d_name);
		changed += map_set(plane_owner,
				   sect.sct_x, sect.sct_y,
				   dchr[sect.sct_type].d_mnem, 0);
		PR(plane_owner, " %d%% efficient ",
		   (sect.sct_own == plane_owner) ?
		   sect.sct_effic : roundintby((int)sect.sct_effic, 25));
		civ = sect.sct_item[I_CIVIL];
		mil = sect.sct_item[I_MILIT];
		if (civ)
		    PR(plane_owner, "with %s%d civ ",
		       (sect.sct_own == plane_owner) ?
		       "" : "approx ",
		       (sect.sct_own == plane_owner) ?
		       civ : roundintby(civ, 25));
		if (mil)
		    PR(plane_owner, "with %s%d mil ",
		       (sect.sct_own == plane_owner) ?
		       "" : "approx ",
		       (sect.sct_own == plane_owner) ?
		       mil : roundintby(mil, 25));
		PR(plane_owner, "@ %s\n", xyas(x, y, plane_owner));
	    }
	    if (flags & P_S)
		satdisp_units(sect.sct_x, sect.sct_y);
	} else {
	    PR(plane_owner, "flying over %s at %s\n",
	       dchr[sect.sct_type].d_name, xyas(x, y, plane_owner));
	    changed += map_set(plane_owner, sect.sct_x, sect.sct_y,
			       dchr[sect.sct_type].d_mnem, 0);
	}
	if ((rel = getrel(over, plane_owner)) == ALLIED)
	    continue;

	evaded = do_evade(bomb_list, esc_list);

	if (sect.sct_own != 0 && sect.sct_own != plane_owner && !evaded) {
	    /* We only show planes overhead if they didn't
	     * evade radar */
	    overfly[sect.sct_own]++;
	    PR(sect.sct_own, "%s planes spotted over %s\n",
	       cname(plane_owner), xyas(x, y, sect.sct_own));
	    if (opt_HIDDEN)
		setcont(cn, plane_owner, FOUND_FLY);
	}

	if (!evaded) {
	    /* Fire flak */
	    if (unfriendly[sect.sct_own])
		ac_doflak(bomb_list, &sect);
	    /* If bombers left, fire flak from units and ships */
	    if (!QEMPTY(bomb_list))
		ac_landflak(bomb_list, x, y);
	    if (!QEMPTY(bomb_list))
		ac_shipflak(bomb_list, x, y);
	}
	/* mission planes aborted due to flak -- don't send escorts */
	if (QEMPTY(bomb_list))
	    break;
	if (!no_air_defense && !evaded)
	    air_defense(x, y, plane_owner, bomb_list, esc_list);

	if (sect.sct_own == 0 || sect.sct_own == plane_owner)
	    continue;

	if (evaded)
	    continue;

	non_missiles = 0;
	for (qp = bomb_list->q_forw; qp != bomb_list; qp = qp->q_forw) {
	    struct plist *ip = (struct plist *)qp;
	    if (!(plchr[(int)ip->plane.pln_type].pl_flags & P_M))
		non_missiles = 1;
	}

	if (!non_missiles)
	    continue;

	if (unfriendly[sect.sct_own] && !gotilist[sect.sct_own]) {
	    getilist(&ilist[sect.sct_own], sect.sct_own);
	    gotilist[sect.sct_own]++;
	}
	if (rel > HOSTILE)
	    continue;
	ac_intercept(bomb_list, esc_list, &ilist[sect.sct_own],
		     sect.sct_own, x, y);
	intown = sect.sct_own;
    }

    /* Let's report all of the overflights even if aborted */
    for (cn = 1; cn < MAXNOC; cn++) {
	if (plane_owner == cn)
	    continue;
	if (overfly[cn] > 0)
	    nreport(plane_owner, N_OVFLY_SECT, cn, overfly[cn]);
    }
    /* If the map changed, update it */
    if (changed)
	writemap(player->cnum);
    /* Now, if the bomber and escort lists are empty, we are done */
    if (QEMPTY(bomb_list) && QEMPTY(esc_list))
	goto out;

    /* Something made it through */
    /* Go figure out if there are ships in this sector, and who's they are */
    memset(nats, 0, sizeof(nats));
    snxtitem_xy(&ni, EF_SHIP, x, y);
    while (nxtitem(&ni, &ship)) {
	if (mchr[(int)ship.shp_type].m_flags & M_SUB)
	    continue;
	nats[ship.shp_own]++;
    }
    /* Go figure out if there are units in this sector, and who's they are */
    memset(lnats, 0, sizeof(lnats));
    snxtitem_xy(&ni, EF_LAND, x, y);
    while (nxtitem(&ni, &land)) {
	if (land.lnd_ship >= 0 || land.lnd_land >= 0)
	    continue;
	lnats[land.lnd_own]++;
    }

    /* Now, let's make life a little rougher. */
    for (cn = 1; cn < MAXNOC && !QEMPTY(bomb_list); cn++) {
	if (plane_owner == cn)
	    continue;
	intown = -1;
	/* Are there ships owned by this country? */
	if (nats[cn] != 0) {
	    /* Yes. */
	    if (cn != 0) {
		PR(cn, "%s planes spotted over ships in %s\n",
		   cname(plane_owner), xyas(x, y, cn));
		if (opt_HIDDEN)
		    setcont(cn, plane_owner, FOUND_FLY);
	    }
	    if (unfriendly[cn]) {
		/* They are unfriendly too */
		if (!gotilist[cn]) {
		    getilist(&ilist[cn], cn);
		    gotilist[cn]++;
		}
		PR(plane_owner, "Flying over %s ships in %s\n",
		   cname(cn), xyas(x, y, plane_owner));
		/* This makes going for ships in harbors tough */
		if (!evaded) {
		    /* We already fired flak up above.  Now we intercept again if we haven't already */
		    /* Flag that we intercepted */
		    intown = 1;
		    /* And now intercept again */
		    ac_intercept(bomb_list, esc_list, &ilist[cn],
				 cn, x, y);
		}
	    }
	}
	/* Are there units owned by this country? */
	if (lnats[cn] != 0) {
	    /* Yes. */
	    if (cn != 0) {
		PR(cn, "%s planes spotted over land units in %s\n",
		   cname(plane_owner), xyas(x, y, cn));
		if (opt_HIDDEN)
		    setcont(cn, plane_owner, FOUND_FLY);
	    }
	    if (unfriendly[cn]) {
		/* They are unfriendly too */
		if (!gotilist[cn]) {
		    getilist(&ilist[cn], cn);
		    gotilist[cn]++;
		}
		PR(plane_owner, "Flying over %s land units in %s\n",
		   cname(cn), xyas(x, y, plane_owner));
		if (!evaded) {
		    if (intown == -1) {
			/* We haven't intercepted yet, so intercept */
			ac_intercept(bomb_list, esc_list, &ilist[cn],
				     cn, x, y);
		    }
		}
	    }
	}
    }
out:
    free_shiplist(&head);
    for (cn = 1; cn < MAXNOC; cn++) {
	if (gotilist[cn])
	    pln_put(&ilist[cn]);
    }
}

static int
plane_caps(struct emp_qelem *list)
{
    struct emp_qelem *qp;
    struct plist *plp;
    int fl;

    fl = 0;
    for (qp = list->q_forw; qp != list; qp = qp->q_forw) {
	plp = (struct plist *)qp;
	fl |= plp->pcp->pl_flags;
    }

    return fl;
}

static int
count_non_missiles(struct emp_qelem *list)
{
    struct emp_qelem *qp;
    struct plist *plp;
    int att_count = 0;

    /* don't intercept missiles */
    for (qp = list->q_forw; qp != list; qp = qp->q_forw) {
	plp = (struct plist *)qp;
	if (!(plp->pcp->pl_flags & P_M))
	    att_count++;
    }
    return att_count;
}

void
sam_intercept(struct emp_qelem *att_list, struct emp_qelem *def_list,
	      natid def_own, natid plane_owner, coord x, coord y,
	      int delete_missiles)
{
    struct emp_qelem *aqp;
    struct emp_qelem *anext;
    struct emp_qelem *dqp;
    struct emp_qelem *dnext;
    struct plist *aplp;
    struct plist *dplp;
    int first = 1;

    for (aqp = att_list->q_forw,
	 dqp = def_list->q_forw;
	 aqp != att_list && dqp != def_list; aqp = anext) {
	anext = aqp->q_forw;
	aplp = (struct plist *)aqp;
	if (aplp->pcp->pl_flags & P_M)
	    continue;
	if (aplp->pcp->pl_cost < 1000)
	    continue;
	for (; dqp != def_list; dqp = dnext) {
	    dnext = dqp->q_forw;
	    dplp = (struct plist *)dqp;
	    if (!(dplp->pcp->pl_flags & P_M))
		continue;

	    if (dplp->plane.pln_range <
		mapdist(x, y, dplp->plane.pln_x, dplp->plane.pln_y)) {
		emp_remque(dqp);
		free(dqp);
		continue;
	    }
	    if (CANT_HAPPEN(dplp->plane.pln_flags & PLN_LAUNCHED)
		|| mission_pln_equip(dplp, 0, P_F, 0) < 0) {
		emp_remque(dqp);
		free(dqp);
		continue;
	    }
	    dplp->plane.pln_flags |= PLN_LAUNCHED;
	    putplane(dplp->plane.pln_uid, &dplp->plane);
	    if (first) {
		first = 0;
		PR(plane_owner, "%s launches SAMs!\n", cname(def_own));
		PR(def_own, "Launching SAMs at %s planes over %s!\n",
		   cname(plane_owner), xyas(x, y, def_own));
		ac_combat_headers(plane_owner, def_own);
	    }
	    ac_dog(aplp, dplp);
	    dqp = dnext;
	    break;
	}
    }
    if (!first) {
	PR(plane_owner, "\n");
	PR(def_own, "\n");
    }
    if (delete_missiles) {
	for (; dqp != def_list; dqp = dnext) {
	    dnext = dqp->q_forw;
	    dplp = (struct plist *)dqp;
	    if (!(dplp->pcp->pl_flags & P_M))
		continue;
	    emp_remque(dqp);
	    free(dqp);
	    continue;
	}
    }
}

static void
ac_intercept(struct emp_qelem *bomb_list, struct emp_qelem *esc_list,
	     struct emp_qelem *def_list, natid def_own, coord x, coord y)
{
    struct plnstr *pp;
    struct plist *plp;
    int icount;
    struct emp_qelem *next;
    struct emp_qelem *qp;
    struct emp_qelem int_list;
    int att_count;
    natid plane_owner;
    int dist;

    plp = (struct plist *)bomb_list->q_forw;
    plane_owner = plp->plane.pln_own;

    icount = 0;

    sam_intercept(bomb_list, def_list, def_own, plane_owner, x, y, 0);
    sam_intercept(esc_list, def_list, def_own, plane_owner, x, y, 1);
    if (!(att_count = count_non_missiles(bomb_list) +
	  count_non_missiles(esc_list)))
	return;

    emp_initque(&int_list);
    for (qp = def_list->q_forw; qp != def_list; qp = next) {
	next = qp->q_forw;
	plp = (struct plist *)qp;
	pp = &plp->plane;
	/* SAMs interdict separately */
	if (plp->pcp->pl_flags & P_M)
	    continue;
	dist = mapdist(x, y, pp->pln_x, pp->pln_y) * 2;
	if (pp->pln_range < dist)
	    continue;
	if (CANT_HAPPEN(pp->pln_flags & PLN_LAUNCHED)
	    || mission_pln_equip(plp, 0, P_F, 0) < 0) {
	    emp_remque(qp);
	    free(qp);
	    continue;
	}
	/* got one; delete from def_list, add to int_list */
	emp_remque(qp);
	emp_insque(qp, &int_list);
	pp->pln_flags |= PLN_LAUNCHED;
	pp->pln_mobil -= pln_mobcost(dist, pp, P_F);
	putplane(pp->pln_uid, pp);
	icount++;
	if (icount > att_count)
	    break;
    }
    if (icount == 0)
	return;
    PR(plane_owner, "%d %s fighter%s rising to intercept!\n",
       icount, cname(def_own), icount == 1 ? " is" : "s are");
    PR(def_own, "%d fighter%s intercepting %s planes over %s!\n",
       icount, icount == 1 ? " is" : "s are", cname(plane_owner),
       xyas(x, y, def_own));
    ac_combat_headers(plane_owner, def_own);
    ac_airtoair(esc_list, &int_list);
    ac_airtoair(bomb_list, &int_list);
    PR(plane_owner, "\n");
    PR(def_own, "\n");
    pln_put(&int_list);
}

void
ac_combat_headers(natid plane_owner, natid def_own)
{
    PR(plane_owner,
       " %-10.10s %-10.10s  strength int odds  damage           results\n",
       cname(plane_owner), cname(def_own));
    PR(def_own,
       " %-10.10s %-10.10s  strength int odds  damage           results\n",
       cname(def_own), cname(plane_owner));
}

/*
 * air-to-air combat.
 */
void
ac_airtoair(struct emp_qelem *att_list, struct emp_qelem *int_list)
{
    struct plist *attacker;
    struct plist *interceptor;
    struct emp_qelem *att;
    struct emp_qelem *in;
    int nplanes;
    int more_att;
    int more_int;
    struct emp_qelem *att_next;
    struct emp_qelem *in_next;

    att = att_list->q_forw;
    in = int_list->q_forw;
    more_att = 1;
    more_int = 1;
    if (QEMPTY(att_list) || QEMPTY(int_list)) {
	more_att = 0;
	more_int = 0;
    }
    while (more_att || more_int) {
	in_next = in->q_forw;
	att_next = att->q_forw;
	attacker = (struct plist *)att;

	/* skip missiles. If only missiles left, we're done */
	if (plchr[(int)attacker->plane.pln_type].pl_flags & P_M) {
	    att = att_next;
	    if (att == att_list) {
		more_att = 0;
		if (QEMPTY(att_list))
		    more_int = 0;
		else
		    att = att->q_forw;
	    }
	    if (all_missiles(att_list))
		more_att = 0;
	    continue;
	}
	interceptor = (struct plist *)in;
	nplanes = attacker->plane.pln_effic;
	if (nplanes > interceptor->plane.pln_effic)
	    nplanes = interceptor->plane.pln_effic;
	ac_dog(attacker, interceptor);
	in = in_next;
	att = att_next;
	if (att == att_list) {
	    more_att = 0;
	    if (QEMPTY(att_list))
		more_int = 0;
	    else
		att = att->q_forw;
	}
	if (in == int_list) {
	    more_int = 0;
	    if (QEMPTY(int_list))
		more_att = 0;
	    else
		in = in->q_forw;
	}
    }
}

static int
all_missiles(struct emp_qelem *att_list)
{
    struct emp_qelem *qp;
    struct plist *p;

    qp = att_list->q_forw;
    while (qp != att_list) {
	p = (struct plist *)qp;
	if (!(plchr[(int)p->plane.pln_type].pl_flags & P_M))
	    return 0;

	qp = qp->q_forw;
    }
    return 1;
}

static void
ac_dog(struct plist *ap, struct plist *dp)
{
    int att, def;
    double odds;
    int intensity;
    natid att_own, def_own;
    int adam, ddam;
    char mesg[1024];
    char temp[14];

    att_own = ap->plane.pln_own;
    def_own = dp->plane.pln_own;

    PR(att_own, " %3.3s #%-4d  %3.3s #%-4d",
       ap->pcp->pl_name,
       ap->plane.pln_uid, dp->pcp->pl_name, dp->plane.pln_uid);
    if (def_own)
	PR(def_own, " %3.3s #%-4d  %3.3s #%-4d",
	   dp->pcp->pl_name,
	   dp->plane.pln_uid, ap->pcp->pl_name, ap->plane.pln_uid);
    att = pln_att(&ap->plane);
    if (att == 0)
	att = pln_def(&ap->plane);
    att = att * ap->plane.pln_effic / 100;
    att = MAX(att, ap->pcp->pl_def / 2);

    def = pln_def(&dp->plane) * dp->plane.pln_effic / 100;
    def = MAX(def, dp->pcp->pl_def / 2);

    if ((ap->pcp->pl_flags & P_F) && ap->bombs != 0)
	att -= 2;
    if ((dp->pcp->pl_flags & P_F) && dp->bombs != 0)
	def -= 2;
    att += ap->pcp->pl_stealth / 25.0;
    def += dp->pcp->pl_stealth / 25.0;
    if (att < 1) {
	def += 1 - att;
	att = 1;
    }
    if (def < 1) {
	att += 1 - def;
	def = 1;
    }
    odds = ((double)att / ((double)def + (double)att));
    if (odds <= 0.05)
	odds = 0.05;
    intensity = roll(20) + roll(20) + roll(20) + roll(20) + 1;

    PR(att_own, "   %3d/%-3d %3d  %3.2f  ", att, def, intensity, odds);
    PR(def_own, "   %3d/%-3d %3d  %3.2f  ", def, att, intensity, odds);

    adam = 0;
    ddam = 0;
    while ((intensity--) > 0) {

	if (chance(odds)) {
	    ddam += 1;
	    if ((dp->plane.pln_effic - ddam) < PLANE_MINEFF)
		intensity = 0;
	} else {
	    adam += 1;
	    if ((ap->plane.pln_effic - adam) < PLANE_MINEFF)
		intensity = 0;
	}
    }

    if (dp->pcp->pl_flags & P_M)
	ddam = 100;

    PR(att_own, "%3d/%-3d", adam, ddam);
    PR(def_own, "%3d/%-3d", ddam, adam);
    ac_planedamage(ap, def_own, adam, def_own, 1, 0, mesg);
    strncpy(temp, mesg, 14);
    ac_planedamage(dp, att_own, ddam, att_own, 1, 0, mesg);
    PR(att_own, "%-13.13s %-13.13s\n", temp, mesg);
    PR(def_own, "%-13.13s %-13.13s\n", mesg, temp);

    if (opt_HIDDEN) {
	setcont(att_own, def_own, FOUND_FLY);
	setcont(def_own, att_own, FOUND_FLY);
    }
}

/*
 * zap plane associated with plp.
 * Damaging country is "from", damage is "dam".
 * def_own is the country on the other side of the conflict from the plane
 * owner. The only time def_own != from is when the interceptor is getting
 * damaged.
 *
 * NOTE: This routine removes the appropriate plane element from the
 * queue if it gets destroyed.  That means that the caller must assume
 * that the current queue pointer is invalid on return from the ac_planedamage
 * call.  (this has caused bugs in the past)
 */
static void
ac_planedamage(struct plist *plp, natid from, int dam, natid other,
	       int checkabort, int show, char *mesg)
{
    struct plnstr *pp;
    int disp;
    char dmess[255];
    int eff;
    natid plane_owner;

    disp = 0;
    pp = &plp->plane;
    plane_owner = pp->pln_own;
    eff = pp->pln_effic;
    sprintf(dmess, " no damage");
    if (dam <= 0) {
	strcpy(mesg, dmess);
	return;
    }
    memset(dmess, 0, sizeof(dmess));
    eff -= dam;
    if (eff < 0)
	eff = 0;
    if (eff < PLANE_MINEFF) {
	sprintf(dmess, " shot down");
	disp = 1;
    } else if (eff < 80 && chance((80 - eff) / 100.0) && checkabort) {
	sprintf(dmess, " aborted @%2d%%", eff);
	disp = 2;
    } else if (show == 0) {
	sprintf(dmess, " cleared");
    }

    if ((plp->pcp->pl_flags & P_M) == 0) {
	if (show) {
	    PR(plane_owner, "    %s %s takes %d%s.\n",
	       cname(pp->pln_own), prplane(pp), dam, dmess);
	    if (other)
		PR(other, "    %s %s takes %d%s.\n",
		   cname(pp->pln_own), prplane(pp), dam, dmess);
	}
    }
    if (show && checkabort == 1) {
	PR(plane_owner, "\n");
	if (other)
	    PR(other, "\n");
    }

    pp->pln_effic = eff;
    pp->pln_mobil -= MIN(32 + pp->pln_mobil, dam / 2);
    if (disp) {
	if (disp == 1 && from != 0 && (plp->pcp->pl_flags & P_M) == 0)
	    nreport(from, N_DOWN_PLANE, pp->pln_own, 1);
	pln_put1(plp);
    } else
	putplane(pp->pln_uid, pp);
    strcpy(mesg, dmess);
}

static void
ac_doflak(struct emp_qelem *list, struct sctstr *from)
{
    int shell;
    int gun;
    natid plane_owner;
    struct plist *plp;

    plp = (struct plist *)list->q_forw;
    plane_owner = plp->plane.pln_own;

    gun = MIN(FLAK_GUN_MAX, from->sct_item[I_GUN]);
    shell = from->sct_item[I_SHELL];
    if (gun > shell * 2) {
	shell += supply_commod(from->sct_own, from->sct_x, from->sct_y,
			       I_SHELL, (gun + 1) / 2 - shell);
	from->sct_item[I_SHELL] = shell;
	putsect(from);
    }
    if (gun > shell * 2)
	gun = shell * 2;

    gun = roundavg(tfact(from->sct_own, 2.0 * gun));
    if (gun > 0) {
	PR(plane_owner, "firing %d flak guns in %s...\n",
	   gun, xyas(from->sct_x, from->sct_y, plane_owner));
	if (from->sct_own != 0)
	    PR(from->sct_own, "firing %d flak guns in %s...\n",
	       gun, xyas(from->sct_x, from->sct_y, from->sct_own));
	ac_fireflak(list, from->sct_own, gun);
    }
}

static void
ac_shipflak(struct emp_qelem *list, coord x, coord y)
{
    struct nstr_item ni;
    struct shpstr ship;
    struct mchrstr *mcp;
    double flak, total, ngun;
    int gun, shell;
    int rel;
    struct plist *plp;
    natid plane_owner;
    natid from;
    int nats[MAXNOC];

    plp = (struct plist *)list->q_forw;
    plane_owner = plp->plane.pln_own;

    memset(nats, 0, sizeof(nats));
    total = ngun = 0;
    snxtitem_xy(&ni, EF_SHIP, x, y);
    while (!QEMPTY(list) && nxtitem(&ni, &ship)) {
	if (ship.shp_own == 0 || ship.shp_own == plane_owner)
	    continue;
	mcp = &mchr[(int)ship.shp_type];
	if (mcp->m_flags & M_SUB)
	    continue;
	rel = getrel(getnatp(ship.shp_own), plane_owner);
	if (rel > HOSTILE)
	    continue;
	shell = 0;
	gun = shp_usable_guns(&ship);
	if (gun) {
	    shell = ship.shp_item[I_SHELL];
	    if (shell <= 0) {
		shell = supply_commod(ship.shp_own, ship.shp_x, ship.shp_y,
				      I_SHELL, 1);
		ship.shp_item[I_SHELL] = shell;
		putship(ship.shp_uid, &ship);
	    }
	}
	if (gun == 0 || shell == 0)
	    continue;
	flak = gun * (ship.shp_effic / 100.0);
	ngun += flak;
	total += techfact(ship.shp_tech, flak * 2.0);

	if (!nats[ship.shp_own]) {
	    /* First time here, print the message */
	    PR(ship.shp_own, "%s planes spotted over ships in %s\n",
	       cname(plane_owner), xyas(x, y, ship.shp_own));
	    PR(plane_owner, "Flying over %s ships in %s\n",
	       cname(ship.shp_own), xyas(x, y, plane_owner));
	    nats[ship.shp_own] = 1;
	}
	PR(ship.shp_own, "firing %.0f flak guns from %s...\n",
	   flak, prship(&ship));
	from = ship.shp_own;
    }

    /* Limit to FLAK_GUN_MAX guns of average tech factor */
    if (ngun > FLAK_GUN_MAX)
	total *= FLAK_GUN_MAX / ngun;

    gun = roundavg(total);
    if (gun > 0) {
	PR(plane_owner, "Flak!  Ships firing %d flak guns...\n", gun);
	ac_fireflak(list, from, gun);
    }
}

static void
ac_landflak(struct emp_qelem *list, coord x, coord y)
{
    struct nstr_item ni;
    struct lndstr land;
    struct lchrstr *lcp;
    double flak, total, ngun;
    int aaf, gun;
    int rel;
    struct plist *plp;
    natid plane_owner;
    natid from;
    int nats[MAXNOC];

    plp = (struct plist *)list->q_forw;
    plane_owner = plp->plane.pln_own;

    memset(nats, 0, sizeof(nats));
    total = ngun = 0;
    snxtitem_xy(&ni, EF_LAND, x, y);
    while (!QEMPTY(list) && nxtitem(&ni, &land)) {
	if (land.lnd_own == 0 || land.lnd_own == plane_owner)
	    continue;
	lcp = &lchr[(int)land.lnd_type];
	aaf = lnd_aaf(&land);
	if ((lcp->l_flags & L_FLAK) == 0 || aaf == 0)
	    continue;
	if (land.lnd_ship >= 0 || land.lnd_land >= 0)
	    continue;
	rel = getrel(getnatp(land.lnd_own), plane_owner);
	if (rel > HOSTILE)
	    continue;
	flak = aaf * 1.5 * land.lnd_effic / 100.0;
	ngun += flak;
	total += techfact(land.lnd_tech, flak * 2.0);

	if (!nats[land.lnd_own]) {
	    /* First time here, print the message */
	    PR(land.lnd_own, "%s planes spotted over land units in %s\n",
	       cname(plane_owner), xyas(x, y, land.lnd_own));
	    PR(plane_owner, "Flying over %s land units in %s\n",
	       cname(land.lnd_own), xyas(x, y, plane_owner));
	    nats[land.lnd_own] = 1;
	}
	PR(land.lnd_own, "firing flak guns from unit %s (aa rating %d)\n",
	   prland(&land), aaf);
	from = land.lnd_own;
    }

    /* Limit to FLAK_GUN_MAX guns of average tech factor */
    if (ngun > FLAK_GUN_MAX)
	total *= FLAK_GUN_MAX / ngun;

    gun = roundavg(total);
    if (gun > 0) {
	PR(plane_owner, "Flak!  Land units firing %d flak guns...\n", gun);
	ac_fireflak(list, from, gun);
    }
}

/*
 * Called from shipflak, landflak, and doflak.
 */
static void
ac_fireflak(struct emp_qelem *list, natid from, int guns)
{
    struct plist *plp;
    int n;
    struct emp_qelem *qp;
    struct emp_qelem *next;
    char msg[255];

    plp = (struct plist *)list->q_forw;

    for (qp = list->q_forw; qp != list; qp = next) {
	next = qp->q_forw;
	plp = (struct plist *)qp;
	n = ac_flak_dam(guns, pln_def(&plp->plane), plp->pcp->pl_flags);
	ac_planedamage(plp, from, n, 0, 2, 1, msg);
    }
}

/*
 * Calculate flak damage
 */
int
ac_flak_dam(int guns, int def, int pl_flags)
{
    int flak, dam;
    float mult;
    /*				   <-7      -7     -6     -5     -4 */
    static float flaktable[18] = { 0.132f, 0.20f, 0.20f, 0.25f, 0.30f,
    /*    -3     -2     -1      0     +1     +2     +3     +4 */
	 0.35f, 0.40f, 0.45f, 0.50f, 0.50f, 0.55f, 0.60f, 0.65f,
    /*    +5    +6     +7     +8    >+8 */
	 0.70f,0.75f, 0.80f, 0.85f, 1.1305f };
    enum { FLAK_MAX = sizeof(flaktable)/sizeof(flaktable[0]) - 1 };

    flak = guns - def;
    if ((pl_flags & P_T) == 0)
	flak--;
    if (pl_flags & P_X)
	flak -= 2;
    if (pl_flags & P_H)
	flak -= 1;

    if (flak > 8)
	mult = flaktable[FLAK_MAX];
    else if (flak < -7)
	mult = flaktable[0];
    else {
	flak += 8;
	mult = flaktable[flak];
    }
    mult *= flakscale;
    dam = (int)((roll(8) + 2) * mult);
    if (dam > 100)
	dam = 100;
    return dam;
}

/*
 * Get a list of planes available for interception duties.
 */
static void
getilist(struct emp_qelem *list, natid own)
{
    struct plchrstr *pcp;
    struct plnstr plane;
    struct nstr_item ni;
    struct plist *ip;

    emp_initque(list);
    snxtitem_all(&ni, EF_PLANE);
    while (nxtitem(&ni, &plane)) {
	if (plane.pln_own != own)
	    continue;
	pcp = &plchr[(int)plane.pln_type];
	if ((pcp->pl_flags & P_F) == 0)
	    continue;
	if (plane.pln_flags & PLN_LAUNCHED)
	    continue;
	if (plane.pln_mission != 0)
	    continue;
	if (plane.pln_mobil <= 0)
	    continue;
	if (plane.pln_effic < 40)
	    continue;
	if (!pln_airbase_ok(&plane, 0, 0))
	    continue;
	/* got one! */
	ip = malloc(sizeof(*ip));
	ip->bombs = 0;
	ip->misc = 0;
	ip->pcp = &plchr[(int)plane.pln_type];
	ip->plane = plane;
	emp_insque(&ip->queue, list);
    }
}

static int
do_evade(struct emp_qelem *bomb_list, struct emp_qelem *esc_list)
{
    struct emp_qelem *qp;
    double evade;
    struct plist *plp;

    evade = 100.0;
    for (qp = bomb_list->q_forw; qp != bomb_list; qp = qp->q_forw) {
	plp = (struct plist *)qp;
	if (evade > plp->pcp->pl_stealth / 100.0)
	    evade = plp->pcp->pl_stealth / 100.0;
    }
    for (qp = esc_list->q_forw; qp != esc_list; qp = qp->q_forw) {
	plp = (struct plist *)qp;
	if (evade > plp->pcp->pl_stealth / 100.0)
	    evade = plp->pcp->pl_stealth / 100.0;
    }

    if (chance(evade))
	return 1;

    return 0;
}
