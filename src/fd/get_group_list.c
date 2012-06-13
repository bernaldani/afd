/*
 *  get_group_list.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 - 2012 Deutscher Wetterdienst (DWD),
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
 **   get_group_list - read list from group file
 **
 ** SYNOPSIS
 **   void get_group_list(char *user, struct job *p_db)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   21.02.2002 H.Kiehl Created
 **   10.03.2012 H.Kiehl We NOT take global struct job. We might be doing
 **                      a burst.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "fddefs.h"

/* External global variables. */
extern char   *p_work_dir;


/*########################### get_group_list() ##########################*/
void
get_group_list(char *user, struct job *p_db)
{
   off_t file_size;
   char  *buffer = NULL,
         group_file[MAX_PATH_LENGTH];

   (void)sprintf(group_file, "%s%s%s", p_work_dir, ETC_DIR, GROUP_FILE);
   if (((file_size = read_file_no_cr(group_file, &buffer)) != INCORRECT) &&
       (file_size > 0))
   {
      int  length;
      char group_id[MAX_USER_NAME_LENGTH + 2 + 1],
           *ptr;

      length = sprintf(group_id, "[%s]", user);
      if ((ptr = lposi(buffer, group_id, length)) == NULL)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to locate group %s in group file.", user);
         p_db->group_list = NULL;
      }
      else
      {
         ptr--;
         while ((*ptr != '\n') && (*ptr != '\0'))
         {
            ptr++;
         }
         if (*ptr == '\n')
         {
            int  max_length = 0;
            char *ptr_start = ptr; /* NOTE: NOT + 1 */

            /*
             * First count the number of groups.
             */
            length = 0;
            p_db->no_listed = 0;
            do
            {
               ptr++;
               if (*ptr == '\\')
               {
                  ptr++;
               }
               else if (*ptr == '#')
                    {
                       while ((*ptr != '\n') && (*ptr != '\0'))
                       {
                          ptr++;
                       }
                       if (length > 0)
                       {
                          if (length > max_length)
                          {
                             max_length = length;
                          }
                          length = 0;
                          p_db->no_listed++;
                       }
                    }
               else if ((*ptr == ' ') || (*ptr == '\t'))
                    {
                       /* Ignore spaces! */;
                    }
                    else
                    {
                       length++;
                       if ((*ptr == '\n') || (*ptr == '\0'))
                       {
                          if (length > max_length)
                          {
                             max_length = length;
                          }
                          length = 0;
                          p_db->no_listed++;
                       }
                    }

               if ((*ptr == '\n') &&
                   ((*(ptr + 1) == '\n') || (*(ptr + 1) == '\0')))
               {
                  break;
               }
            } while ((*ptr != '[') && (*ptr != '\0'));

            if ((p_db->no_listed > 0) && (max_length > 0))
            {
               int counter = 0;

               RT_ARRAY(p_db->group_list, p_db->no_listed, max_length, char);
               ptr = ptr_start;
               length = 0;
               do
               {
                  ptr++;
                  if (*ptr == '\\')
                  {
                     ptr++;
                  }
                  else if (*ptr == '#')
                       {
                          while ((*ptr != '\n') && (*ptr != '\0'))
                          {
                             ptr++;
                          }
                          if (length > 0)
                          {
                             p_db->group_list[counter][length] = '\0';
                             length = 0;
                             counter++;
                          }
                       }
                  else if ((*ptr == ' ') || (*ptr == '\t'))
                       {
                          /* Ignore spaces! */;
                       }
                       else
                       {
                          if ((*ptr == '\n') || (*ptr == '\0'))
                          {
                             p_db->group_list[counter][length] = '\0';
                             length = 0;
                             counter++;
                          }
                          else
                          {
                             p_db->group_list[counter][length] = *ptr;
                             length++;
                          }
                       }
                  if ((*ptr == '\n') &&
                      ((*(ptr + 1) == '\n') || (*(ptr + 1) == '\0')))
                  {
                     break;
                  }
               } while ((*ptr != '[') && (*ptr != '\0'));
            }
            else
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "No group elements found for group %s.", user);
               p_db->group_list = NULL;
            }
         }
         else
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "No group elements found for group %s.", user);
            p_db->group_list = NULL;
         }
      }
      free(buffer);
   }

   return;
}
