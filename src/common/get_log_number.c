/*
 *  get_log_number.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2013 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_log_number - gets the largest log number in log directory
 **
 ** SYNOPSIS
 **   void get_log_number(int  *log_number,
 **                       int  max_log_number,
 **                       char *log_name,
 **                       int  log_name_length,
 **                       char *alias_name)
 **
 ** DESCRIPTION
 **   The function get_log_number() looks in the AFD log directory
 **   for the highest log number of the log file type 'log_name'.
 **   If the log number gets larger than 'max_log_number', these log
 **   files will be removed.
 **
 ** RETURN VALUES
 **   Returns the highest log number 'log_number' it finds in the
 **   AFD log directory.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   03.02.1996 H.Kiehl Created
 **   12.01.1997 H.Kiehl Remove log files with their log number being
 **                      larger or equal to max_log_number.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* fprintf(), sprintf(), rename()     */
#include <string.h>                /* strncmp(), strerror()              */
#include <stdlib.h>                /* atoi()                             */
#include <ctype.h>                 /* isdigit()                          */
#include <sys/types.h>
#include <sys/stat.h>              /* stat(), S_ISREG()                  */
#include <unistd.h>                /* unlink()                           */
#include <dirent.h>                /* readdir(), closedir(), DIR,        */
                                   /* struct dirent                      */
#include <errno.h>

/* Global variables. */
extern char *p_work_dir;


/*########################### get_log_number() ##########################*/
void
get_log_number(int  *log_number,
               int  max_log_number,
               char *log_name,
               int  log_name_length,
               char *alias_name)
{
   int           i,
                 tmp_number;
   char          *ptr,
                 str_number[MAX_INT_LENGTH],
                 fullname[MAX_PATH_LENGTH],
                 log_dir[MAX_PATH_LENGTH];
   struct stat   stat_buf;
   struct dirent *p_dir;
   DIR           *dp;

   /* Initialise log directory. */
   if (alias_name == NULL)
   {
      (void)my_strncpy(log_dir, p_work_dir, MAX_PATH_LENGTH - LOG_DIR_LENGTH);
      (void)strcat(log_dir, LOG_DIR);
   }
   else
   {
#ifdef HAVE_SNPRINTF
      (void)snprintf(log_dir, MAX_PATH_LENGTH, "%s%s/%s",
#else
      (void)sprintf(log_dir, "%s%s/%s",
#endif
                    p_work_dir, RLOG_DIR, alias_name);
   }
   if ((dp = opendir(log_dir)) == NULL)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("Could not opendir() `%s' : %s"), log_dir, strerror(errno));
      exit(INCORRECT);
   }

   errno = 0;
   while ((p_dir = readdir(dp)) != NULL)
   {
      if (p_dir->d_name[0] == '.')
      {
         continue;
      }

      if (strncmp(p_dir->d_name, log_name, log_name_length) == 0)
      {
#ifdef HAVE_SNPRINTF
         (void)snprintf(fullname, MAX_PATH_LENGTH, "%s/%s",
#else
         (void)sprintf(fullname, "%s/%s",
#endif
                       log_dir, p_dir->d_name);
         if (stat(fullname, &stat_buf) < 0)
         {
            if (errno != ENOENT)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          _("Can't access file `%s' : %s"),
                          fullname, strerror(errno));
            }
            continue;
         }

         /* Sure it is a normal file? */
         if (S_ISREG(stat_buf.st_mode))
         {
            ptr = p_dir->d_name;
            ptr += log_name_length;
            if (*ptr != '\0')
            {
               i = 0;
               while (isdigit((int)(*ptr)) != 0)
               {
                  str_number[i] = *ptr;
                  ptr++; i++;
               }
               if (i > 0)
               {
                  str_number[i] = '\0';
                  if ((tmp_number = atoi(str_number)) > *log_number)
                  {
                     if (tmp_number > max_log_number)
                     {
                        if (unlink(fullname) < 0)
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
                                      _("Failed to unlink() `%s' : %s"),
                                      fullname, strerror(errno));
                        }
                        else
                        {
                           system_log(INFO_SIGN, __FILE__, __LINE__,
                                      _("Removing log file `%s'"), fullname);
                        }
                     }
                     else
                     {
                        *log_number = tmp_number;
                     }
                  }
               }
            }
         }
      }
      errno = 0;
   } /*  while (readdir(dp) != NULL) */

   if (errno)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("readdir() error : %s"), strerror(errno));
   }
   if (closedir(dp) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("closedir() error : %s"), strerror(errno));
   }

   return;
}
