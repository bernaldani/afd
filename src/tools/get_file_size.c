/*
 *  get_file_size.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2007 Deutscher Wetterdienst (DWD),
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

#include "afddefs.h"

DESCR__S_M1
/*
 ** NAME
 **   get_file_size - gets the size of a file in bytes
 **
 ** SYNOPSIS
 **   get_file_size <file-name 1> [<file-name 2> ... <file-name n>]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.11.1996 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>            /* fprintf()                               */
#include <string.h>           /* strerror()                              */
#include <stdlib.h>           /* exit()                                  */
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int         i;
   off_t       file_size = 0;
   struct stat stat_buf;

   if (argc == 1)
   {
      (void)fprintf(stderr, "Usage: %s <file-name 1> [<file-name 2> ... <file-name n>]\n",
                    argv[0]);
      exit(INCORRECT);
   }

   for (i = 1; i < argc; i++)
   {
      if (stat(argv[i], &stat_buf) < 0)
      {
         (void)fprintf(stderr, "Failed to stat() %s : %s\n",
                       argv[i], strerror(errno));
         continue;
      }

      file_size += stat_buf.st_size;
   }

#if SIZEOF_OFF_T == 4
   (void)fprintf(stdout, "%ld\n", (pri_off_t)file_size);
#else
   (void)fprintf(stdout, "%lld\n", (pri_off_t)file_size);
#endif

   exit(SUCCESS);
}
