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
 *  com.h: Definitions used to parse Empire commands
 * 
 *  Known contributors to this file:
 *  
 */

#ifndef _COM_H_
#define _COM_H_

struct cmndstr {
    s_char *c_form;		/* prototype of command */
    int c_cost;			/* btu cost of command */
    int (*c_addr)(void);	/* core addr of appropriate routine */
    int c_flags;
    int c_permit;		/* who is allowed to "do" this command */
};

#define C_MOD		0x1	/* modifies database */

/* variables associated with this stuff */

extern struct cmndstr player_coms[];

#endif /* _COM_H_ */
