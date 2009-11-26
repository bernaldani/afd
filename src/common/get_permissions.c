/*
 *  get_permissions.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2009 Deutscher Wetterdienst (DWD),
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

DESCR__S_M3
/*
 ** NAME
 **   get_permissions - retrieves permissions from the AFD_USER_FILE
 **                     for the calling user
 **
 ** SYNOPSIS
 **   int get_permissions(char **perm_buffer, char *fake_user)
 **
 ** DESCRIPTION
 **   This function returns a list of permissions in 'perm_buffer'.
 **   To store the permissions it needs to allocate memory for
 **   which the caller is responsible to free.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it either fails to stat()/open()/read()
 **   from the AFD_USER_FILE. SUCCESS is returned when the calling
 **   user is found in AFD_USER_FILE and a list of permissions is
 **   stored in 'perm_buffer'. When the calling user could not be found,
 **   NONE is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   06.05.1997 H.Kiehl Created
 **   05.11.2003 H.Kiehl Ensure that we do check the full username.
 **   06.03.2006 H.Kiehl Do not allow all permissions if permission
 **                      file exist, but is not readable to user.
 **   04.12.2008 H.Kiehl When reading afd.users file check if we need
 **                      to convert from a dos style file to unix.
 **
 */
DESCR__E_M3

#include <stdio.h>              /* NULL                                  */
#include <string.h>             /* strcat(), strcpy(), strerror()        */
#include <stdlib.h>             /* getenv(), calloc(), free()            */
#include <unistd.h>             /* read(), close(), getuid()             */
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>                /* getpwuid()                            */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>

/* External global variables. */
extern char *p_work_dir;


/*########################## get_permissions() ##########################*/
int
get_permissions(char **perm_buffer, char *fake_user)
{
   int         fd,
               ret = SUCCESS;
   char        *buffer,
               user[256],
               afd_user_file[MAX_PATH_LENGTH];
   struct stat stat_buf;

   user[0] = '\n';
   if (fake_user[0] == '\0')
   {
      struct passwd *pwd;

      if ((pwd = getpwuid(getuid())) != NULL)
      {
         (void)strcpy(&user[1], pwd->pw_name);
      }
      else
      {
         (void)strcpy(&user[1], "unknown");
      }
   }
   else
   {
      (void)strcpy(&user[1], fake_user);
   }

   (void)strcpy(afd_user_file, p_work_dir);
   (void)strcat(afd_user_file, ETC_DIR);
   (void)strcat(afd_user_file, AFD_USER_FILE);
   if ((fd = open(afd_user_file, O_RDONLY)) != -1)
   {
      if (fstat(fd, &stat_buf) != -1)
      {
         if (stat_buf.st_size < 1048576)
         {
            /* Create two buffers, one to scribble around 'buffer' the other */
            /* is returned to the calling process.                           */
            if (((buffer = malloc(stat_buf.st_size + 2)) != NULL) &&
                ((*perm_buffer = calloc(stat_buf.st_size, sizeof(char))) != NULL))
            {
               off_t   bytes_buffered;
#ifdef HAVE_GETLINE
               ssize_t n;
               size_t  line_buffer_length;
#else
               size_t  n;
#endif
               char    *read_ptr;
               FILE    *fp;

               if ((fp = fdopen(fd, "r")) == NULL)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Failed to read() `%s'. Permission control deactivated!!! : %s"),
                             afd_user_file, strerror(errno));
                  free(buffer); free(*perm_buffer);
                  ret = INCORRECT;
               }
               bytes_buffered = 0;
               read_ptr = buffer + 1;
#ifdef HAVE_GETLINE
               line_buffer_length = stat_buf.st_size + 1;
               while ((n = getline(&read_ptr, &line_buffer_length, fp)) != -1)
#else
               while (fgets(read_ptr, MAX_LINE_LENGTH, fp) != NULL)
#endif
               {
#ifndef HAVE_GETLINE
                  n = strlen(read_ptr);
#endif
                  if (n > 1)
                  {
                     if (*(read_ptr + n - 2) == 13)
                     {
                        *(read_ptr + n - 2) = 10;
                        n--;
                     }
                  }
                  read_ptr += n;
#ifdef HAVE_GETLINE
                  line_buffer_length -= n;
#endif
                  bytes_buffered += n;
               }
               buffer[bytes_buffered] = '\0';
            }
            else
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to allocate memory. Permission control deactivated!!! : %s"),
                          strerror(errno));
               ret = INCORRECT;
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("The function get_permissions() was not made to handle large file."));
            ret = NONE;
         }
      }
      else
      {
         if (errno == ENOENT)
         {
            /*
             * If there is no AFD_USER_FILE, or we fail to open
             * it because it does not exist, lets allow everything.
             */
            ret = INCORRECT;
         }
         else
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("Failed to fstat() permission file `%s' : %s"),
                       afd_user_file, strerror(errno));
            if (errno == EACCES)
            {
               ret = NO_ACCESS;
            }
            else
            {
               ret = NONE;
            }
         }
      }

      if (close(fd) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("close() error : %s"), strerror(errno));
      }
   }
   else
   {
      if (errno == ENOENT)
      {
         /*
          * If there is no AFD_USER_FILE, or we fail to open
          * it because it does not exist, lets allow everything.
          */
         ret = INCORRECT;
      }
      else
      {
         if (errno == EACCES)
         {
            ret = NO_ACCESS;
         }
         else
         {
            ret = NONE;
         }
      }
   }

   if (ret == SUCCESS)
   {
      char *ptr;

      buffer[0] = '\n';
      ptr = buffer;
      do
      {
         if (((ptr = posi(ptr, user)) != NULL) &&
             ((*(ptr - 1) == ' ') || (*(ptr - 1) == '\t')))
         {
            break;
         }
      } while (ptr != NULL);

      if ((ptr = posi(buffer, user)) != NULL)
      {
         int  i = 0;

         do
         {
            while ((*ptr == ' ') || (*ptr == '\t'))
            {
               ptr++;
            }
            while ((*ptr != '\n') && (*ptr != '\0'))
            {
               *(*perm_buffer + i) = *ptr;
               i++; ptr++;
            }
            if (((ptr + 1) - buffer) >= stat_buf.st_size)
            {
               break;
            }
            ptr++;
         } while ((*ptr == ' ') || (*ptr == '\t'));

         *(*perm_buffer + i) = '\0';
      }
      else
      {
         free(*perm_buffer);
         ret = NONE;
      }
      free(buffer);
   }

   /*
    * The caller is responsible to free 'perm_buffer'!
    */
   return(ret);
}
