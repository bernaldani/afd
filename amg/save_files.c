/*
 *  save_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 1999 Holger Kiehl <Holger.Kiehl@dwd.de>
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

DESCR__S_M3
/*
 ** NAME
 **   save_files - saves files from user directory
 **
 ** SYNOPSIS
 **   int save_files(char                   *src_path,
 **                  char                   *dest_path,
 **                  struct directory_entry *p_de,
 **                  int                    pos_in_fm,
 **                  int                    no_of_files,
 **                  char                   link_flag)
 **
 ** DESCRIPTION
 **   When the queue has been stopped for a host, this function saves
 **   all files in the user directory into the directory .<hostname>
 **   so that no files are lost for this host.
 **
 ** RETURN VALUES
 **   Returns SUCCESS when all files have been saved. Otherwise
 **   INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   03.03.1996 H.Kiehl Created
 **   15.09.1997 H.Kiehl Remove files when they already do exist.
 **   17.10.1997 H.Kiehl When pool is a different file system files
 **                      should be copied.
 **   18.10.1997 H.Kiehl Introduced inverse filtering.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* sprintf(), remove()                */
#include <string.h>                /* strcmp(), strerror(), strcpy(),    */
                                   /* strcat(), strlen()                 */
#include <sys/types.h>
#include <sys/stat.h>              /* stat(), S_ISDIR()                  */
#include <fcntl.h>                 /* S_IRUSR, ...                       */
#include <unistd.h>                /* link(), mkdir()                    */
#include <errno.h>
#include "amgdefs.h"

/* External global variables */
extern int  sys_log_fd;
#ifndef _WITH_PTHREAD
extern char **file_name_pool;
#endif


/*########################### save_files() ##############################*/
int
save_files(char                   *src_path,
           char                   *dest_path,
#ifdef _WITH_PTHREAD
           char                   **file_name_pool,
#endif
           struct directory_entry *p_de,
           int                    pos_in_fm,
           int                    no_of_files,
           char                   link_flag)
{
   register int i,
                j,
                ret;
   char         *p_src,
                *p_dest;
   struct stat  stat_buf;

   if ((stat(dest_path, &stat_buf) < 0) || (S_ISDIR(stat_buf.st_mode) == 0))
   {
      /*
       * Only the AFD may read and write in this directory!
       */
      if (mkdir(dest_path, (S_IRUSR | S_IWUSR | S_IXUSR)) < 0)
      {
         if (errno != EEXIST)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Could not mkdir() %s to save files : %s (%s %d)\n",
                      dest_path, strerror(errno), __FILE__, __LINE__);
            errno = 0;
            return(INCORRECT);
         }

         /*
          * For now lets assume that another process has created
          * this directory just a little bit faster then this
          * process.
          */
      }
   }

   p_src = src_path + strlen(src_path);
   p_dest = dest_path + strlen(dest_path);
   *p_dest++ = '/';

   for (i = 0; i < no_of_files; i++)
   {
      for (j = 0; j < p_de->fme[pos_in_fm].nfm; j++)
      {
         /*
          * Actually we could just read the source directory
          * and rename all files to the new directory.
          * This however involves several system calls (opendir(),
          * readdir(), closedir()), ie high system load. This
          * we hopefully reduce by using the array file_name_pool
          * and filter() to get the names we need. Let's see
          * how things work.
          */
         if ((ret = filter(p_de->fme[pos_in_fm].file_mask[j], file_name_pool[i])) == 0)
         {
            (void)strcpy(p_src, file_name_pool[i]);
            (void)strcpy(p_dest, file_name_pool[i]);

            if (link_flag & YES)
            {
               if (link(src_path, dest_path) == -1)
               {
                  if (errno == EEXIST)
                  {
                     /*
                      * A file with the same name already exists. Remove
                      * this file and try to link again.
                      */
                     if (remove(dest_path) == -1)
                     {
                        (void)rec(sys_log_fd, WARN_SIGN,
                                  "Failed to remove() file %s : %s (%s %d)\n",
                                  dest_path, strerror(errno), __FILE__, __LINE__);
                        errno = 0;
                     }
                     else
                     {
                        if (link(src_path, dest_path) == -1)
                        {
                           (void)rec(sys_log_fd, WARN_SIGN,
                                     "Failed to link file %s to %s : %s (%s %d)\n",
                                     src_path, dest_path, strerror(errno), __FILE__, __LINE__);
                           errno = 0;
                        }
                     }
                  }
                  else
                  {
                     (void)rec(sys_log_fd, WARN_SIGN,
                               "Failed to link file %s to %s : %s (%s %d)\n",
                               src_path, dest_path, strerror(errno), __FILE__, __LINE__);
                     errno = 0;
                  }
               }
            }
            else
            {
               if (copy_file(src_path, dest_path) < 0)
               {
                  (void)rec(sys_log_fd, WARN_SIGN,
                            "Failed to copy file %s to %s (%s %d)\n",
                            src_path, dest_path, __FILE__, __LINE__);
                  errno = 0;
               }
            }

            /* No need to test any further filters. */
            break;
         }
         else if (ret == 1)
              {
                 /*
                  * This file is definitely NOT wanted, no matter what the
                  * following filters say.
                  */
                 break;
              }
      }
   }

   *(p_dest - 1) = '\0';
   *p_src = '\0';

   return(SUCCESS);
}