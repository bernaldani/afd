/*
 *  gen_file.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2004 Deutscher Wetterdienst (DWD),
 *                     Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   gen_file - process that gnerates files at intervals
 **
 ** SYNOPSIS
 **   gen_file <no. of files> <size> <interval> <directory> <file name>
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   13.05.2004 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>              /* fprintf(), printf(), stderr           */
#include <string.h>             /* strcpy(), strerror()                  */
#include <stdlib.h>             /* exit()                                */
#include <sys/types.h>
#include <sys/stat.h>           /* stat(), S_ISDIR()                     */
#include <unistd.h>             /* TDERR_FILENO                          */
#include <time.h>               /* time()                                */
#include <fcntl.h>
#include <errno.h>

#define DEFAULT_BLOCKSIZE 4096


/* Local functions */
static void usage(char *);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ gen_file() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   unsigned int counter = 0;
   int          fd,
                i, j,
                loops,
                no_of_files,
                rest;
   long int     interval;
   off_t        filesize;
   time_t       current_time,
                *p_block;
   char         block[DEFAULT_BLOCKSIZE],
                *dot_ptr,
                *ptr,
                filename[MAX_FILENAME_LENGTH],
                dot_target_dir[MAX_PATH_LENGTH],
                str_number[MAX_INT_LENGTH],
                target_dir[MAX_PATH_LENGTH];

   if (argc == 6)
   {
      no_of_files = atoi(argv[1]);
      filesize = (off_t)strtoul(argv[2], NULL, 10);
      interval = atol(argv[3]);
      (void)strcpy(target_dir, argv[4]);
      (void)strcpy(dot_target_dir, argv[4]);
      (void)strcpy(filename, argv[5]);
   }
   else
   {
      usage(argv[0]);
      exit(0);
   }

   ptr = target_dir + strlen(target_dir);
   *ptr++ = '/';
   (void)strcpy(ptr, filename);
   ptr += strlen(filename);
   *ptr++ = '-';
   dot_ptr = dot_target_dir + strlen(dot_target_dir);
   *dot_ptr++ = '/';
   *dot_ptr++ = '.';
   (void)strcpy(dot_ptr, filename);
   dot_ptr += strlen(filename);
   *dot_ptr++ = '-';
   loops = filesize / DEFAULT_BLOCKSIZE;
   rest = filesize % DEFAULT_BLOCKSIZE;

   for (;;)
   {
      p_block = (time_t *)block;
      current_time = time(NULL);
      while (p_block < (time_t *)&block[DEFAULT_BLOCKSIZE])
      {
         *p_block = current_time;
         p_block++;
      }
      for (i = 0; i < no_of_files; i++)
      {
         (void)sprintf(str_number, "%d", counter);
         (void)strcpy(ptr, str_number);
         (void)strcpy(dot_ptr, str_number);
         if ((fd = open(dot_target_dir, (O_WRONLY | O_CREAT | O_TRUNC),
                        (S_IRUSR | S_IWUSR))) == -1)
         {
            (void)fprintf(stderr, "Failed top open() %s : %s\n",
                          dot_target_dir, strerror(errno));
            exit(INCORRECT);
         }
         for (j = 0; j < loops; j++)
         {
            if (write(fd, block, DEFAULT_BLOCKSIZE) != DEFAULT_BLOCKSIZE)
            {
               (void)fprintf(stderr, "Failed to write() %d bytes : %s\n",
                             DEFAULT_BLOCKSIZE, strerror(errno));
               exit(INCORRECT);
            }
         }
         if (rest)
         {
            if (write(fd, block, rest) != rest)
            {
               (void)fprintf(stderr, "Failed to write() %d bytes : %s\n",
                             rest, strerror(errno));
               exit(INCORRECT);
            }
         }
         if (close(fd) == -1)
         {
            (void)fprintf(stderr, "Failed to close() %s : %s\n",
                          dot_target_dir, strerror(errno));
         }
         if (rename(dot_target_dir, target_dir) == -1)
         {
            (void)fprintf(stderr, "Failed to rename() %s to %s : %s\n",
                          dot_target_dir, target_dir, strerror(errno));
         }
         counter++;
      }
      (void)sleep(interval);
   }

   exit(0);
}


/*+++++++++++++++++++++++++++++ usage() +++++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{
   (void)fprintf(stderr,
                 "Usage : %s <no. of files> <size> <interval> <directory> <file name>\n",
                 progname);
   return;
}
