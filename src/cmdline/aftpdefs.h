/*
 *  aftpdefs.h - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2006 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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

#ifndef __aftpdefs_h
#define __aftpdefs_h

#ifndef _STANDALONE_
# include "afddefs.h"
# include "fddefs.h"
#endif
#include "cmdline.h"

#ifdef _STANDALONE_
# include <stdio.h>

/* Ready files for locking on remote site. */
# define _WITH_READY_FILES

# define AFD_WORD_OFFSET            8

# define FTP_CTRL_KEEP_ALIVE_INTERVAL 1200L

# define DOT                        1
# define LOCK_DOT                   "DOT"
# define LOCK_DOT_VMS               "DOT_VMS"
# define LOCK_OFF                   "OFF"
# ifdef _WITH_READY_FILES
#  define READY_FILE_ASCII          "RDYA"
#  define READY_A_FILE              2
#  define READY_FILE_BINARY         "RDYB"
#  define READY_B_FILE              3
# endif
# define AUTO_SIZE_DETECT           -2

# define INFO_SIGN                  "<I>"
# define WARN_SIGN                  "<W>"
# define ERROR_SIGN                 "<E>"
# define FATAL_SIGN                 "<F>"           /* donated by Paul M. */
# define DEBUG_SIGN                 "<D>"

# define FILE_MODE                  (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

/* Definitions for appending */
# define RESTART_FILE_SIZE          204800
# define MAX_SEND_BEFORE_APPEND     204800


/* Function prototypes */
extern off_t read_file(char *, char **buffer);
extern char  *posi(char *, char *);
#endif /* _STANDALONE_ */

#define DEFAULT_AFD_USER     "anonymous"
#define DEFAULT_AFD_PASSWORD "afd@someplace"

#define TRANSFER_MODE        1
#define RETRIEVE_MODE        2
#define TEST_MODE            3

/* Function prototypes. */
extern int init_aftp(int, char **, struct data *);

#endif /* __aftpdefs_h */
