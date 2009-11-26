/*
 *  get_dir_number.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2005 - 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_dir_number - gets a directory number
 **
 ** SYNOPSIS
 **   int get_dir_number(char *directory, unsigned int id, long *dirs_left)
 **
 ** DESCRIPTION
 **   The function get_dir_number() looks in 'directory' for a
 **   free directory. If it does not find one it tries to create
 **   a new one. It starts from zero to the maximum number
 **   of links that may be created in a directory.
 **
 ** RETURN VALUES
 **   Returns a directory number where files may be stored. Otherwise
 **   INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.03.2005 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* strcpy(), strcat(), strerror()  */
#include <unistd.h>                   /* mkdir(), pathconf()             */
#include <sys/types.h>
#include <sys/stat.h>                 /* stat()                          */
#include <errno.h>

/* Local variables. */
static long link_max = 0L;


/*########################### get_dir_number() ##########################*/
int
get_dir_number(char *directory, unsigned int id, long *dirs_left)
{
   int         i;
   size_t      length;
   char        fulldir[MAX_PATH_LENGTH];
   struct stat stat_buf;

   if (link_max == 0L)
   {
      if ((link_max = pathconf(directory, _PC_LINK_MAX)) == -1)
      {
#ifdef REDUCED_LINK_MAX
         link_max = REDUCED_LINK_MAX;
#else
         link_max = _POSIX_LINK_MAX;
#endif
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    _("pathconf() error for _PC_LINK_MAX : %s"),
                    strerror(errno));
      }
   }
   length = strlen(directory);
   (void)memcpy(fulldir, directory, length);
   if (fulldir[length - 1] != '/')
   {
      fulldir[length] = '/';
      length++;
   }

   for (i = 0; i < link_max; i++)
   {
      (void)sprintf(&fulldir[length], "%x/%x", id, i);
      if (stat(fulldir, &stat_buf) == -1)
      {
         if (errno == ENOENT)
         {
            if (stat(directory, &stat_buf) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to stat() `%s' : %s"),
                          directory, strerror(errno));
               return(INCORRECT);
            }

            (void)sprintf(&fulldir[length], "%x", id);
            errno = 0;
            if ((stat(fulldir, &stat_buf) == -1) && (errno != ENOENT))
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to stat() `%s' : %s"),
                          fulldir, strerror(errno));
               return(INCORRECT);
            }
            if (errno == ENOENT)
            {
               if (mkdir(fulldir, DIR_MODE) < 0)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Failed to mkdir() `%s' : %s"),
                             fulldir, strerror(errno));
                  return(INCORRECT);
               }
               else
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             _("Hmm, created directory `%s'"), fulldir);
               }
            }
            (void)sprintf(&fulldir[length], "%x/%x", id, i);
            if (mkdir(fulldir, DIR_MODE) < 0)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to mkdir() `%s' : %s"),
                          fulldir, strerror(errno));
               return(INCORRECT);
            }
            if (dirs_left != NULL)
            {
               if (link_max > 10000L)
               {
                  *dirs_left = 10000L;
               }
               else
               {
                  *dirs_left = link_max;
               }
            }
            return(i);
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to stat() `%s' : %s"),
                       fulldir, strerror(errno));
            return(INCORRECT);
         }
      }
      else if (stat_buf.st_nlink < link_max)
           {
              if (dirs_left != NULL)
              {
                 *dirs_left = link_max - stat_buf.st_nlink;
                 if (*dirs_left > 10000L)
                 {
                    *dirs_left = 10000L;
                 }
              }
              return(i);
           }
   }

   system_log(ERROR_SIGN, __FILE__, __LINE__,
              _("Directory `%s/%x' is full (%ld). Unable to create new jobs for it."),
              directory, id, link_max);
   return(INCORRECT);
}
