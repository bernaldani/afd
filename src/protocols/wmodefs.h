/*
 *  wmodefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998, 1999 Deutscher Wetterdienst (DWD),
 *                           Holger Kiehl <Holger.Kiehl@dwd.de>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __wmodefs_h
#define __wmodefs_h

#define DEFAULT_WMO_PORT   25
#define MAX_WMO_COUNTER    999

/* Function prototypes */
extern int  next_wmo_counter(int),
            wmo_check_reply(void),
            wmo_connect(char *, int),
            wmo_write(char *, int);
extern void wmo_quit(void);

#endif /* __wmodefs_h */
