/*
 *  Empire - A multi-player, client/server Internet based war game.
 *  Copyright (C) 1986-2000, Dave Pare, Jeff Bailey, Thomas Ruschak,
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
 *  See the "LEGAL", "LICENSE", "CREDITS" and "README" files for all the
 *  related information and legal notices. It is expected that any future
 *  projects/authors will amend these files as needed.
 *
 *  ---
 *
 *  shark.c: Transfer a loan by buying it out
 * 
 *  Known contributors to this file:
 *     Pat Loney, 1992
 *     Steve McClure, 1996-2000
 */

#include <math.h>
#include "misc.h"
#include "var.h"
#include "file.h"
#include "sect.h"
#include "item.h"
#include "loan.h"
#include "news.h"
#include "nat.h"
#include "xy.h"
#include "player.h"
#include "commands.h"
#include "optlist.h"

int
shark(void)
{
    register int arg;
    time_t now;
    char *p;
    struct lonstr loan;
    struct natstr *natp;
    struct natstr *oldie;
    long rdur;
    long xdur;
    double rate;
    double owed;
    long payment;
    s_char buf[1024];

    if (!opt_LOANS) {
	pr("Loans are not enabled.\n");
	return RET_FAIL;
    }
    p = getstarg(player->argp[1], "Transfer which loan #: ", buf);
    if (p == 0)
	return RET_SYN;
    if (*p == 0)
	return RET_SYN;
    arg = atoi(p);
    if (arg < 0)
	return RET_SYN;
    /* Check if it's a valid loan to shark.  That means, is it a valid loan,
       not owed to this player, with a valid duration and it's been signed. */
    if (!getloan(arg, &loan) || (loan.l_loner == player->cnum) ||
	(loan.l_ldur == 0) || (loan.l_status != LS_SIGNED)) {
	pr("Invalid loan\n");
	return RET_FAIL;
    }
    /* If we got here, we check to see if it's been defaulted on. */
    (void)time(&now);
    /*
     * split duration now - l_lastpay into regular (up to l_duedate)
     * and extended (beyond l_duedate)
     */
    rdur = loan.l_duedate - loan.l_lastpay;
    xdur = now - loan.l_duedate;
    if (rdur < 0) {
	xdur += rdur;
	rdur = 0;
    }
    if (xdur <= 0) {
	pr("There has been no default on loan %d\n", arg);
	return RET_FAIL;
    }

    rate = loan.l_irate / (loan.l_ldur * 8.64e6);

    owed = ((rdur * rate) + (xdur * rate * 2.0) + 1.0);
    if (((1 << 30) / owed) < loan.l_amtdue)
	owed = (1 << 30);
    else
	owed *= loan.l_amtdue;
    pr("That loan is worth $%.2f.\n", owed);
    natp = getnatp(player->cnum);
    payment = owed * (1.0 + loan.l_irate / 100.0);
    if (payment > natp->nat_money - 100.0) {
	pr("You do not have enough to cover that loan\n");
	return RET_FAIL;
    } else {
	wu(0, loan.l_lonee,
	   "%s bought loan #%d.  You now owe him!\n",
	   cname(player->cnum), arg);
	wu(0, loan.l_loner,
	   "%s bought loan #%d out from under you for %ld\n",
	   cname(player->cnum), arg, payment);
	pr("You now own loan #%d.  Go break some legs.\n", arg);
    }
/*	NAT_DELTA(natp->nat_money, loan.l_loner, payment);*/
    oldie = getnatp(loan.l_loner);
    oldie->nat_money += payment;
    player->dolcost += payment;
    loan.l_loner = player->cnum;
    putloan(arg, &loan);
    return RET_OK;
}
