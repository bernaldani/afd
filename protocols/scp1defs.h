/*
 *  scp1defs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __scp1defs_h
#define __scp1defs_h

#define DEFAULT_SSH_PORT   22
#define SSH_COMMAND        "ssh"

/* Function prototypes */
extern int  scp1_connect(char *, int, char *, char *, char *),
            scp1_close_file(void),
            scp1_cmd_connect(char *, int, char *, char *, char *),
            scp1_open_file(char *, off_t, mode_t),
            scp1_write(char *, int),
            scp1_chmod(char *, char *),
            scp1_move(char *, char *);
extern void scp1_quit(void);

#endif /* __scp1defs_h */