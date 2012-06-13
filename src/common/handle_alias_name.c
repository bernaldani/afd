/*
 *  handle_alias_name.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2009 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   handle_alias_name - set of functions to handle alias names
 **
 ** SYNOPSIS
 **   void get_alias_names(void)
 **
 ** DESCRIPTION
 **   The function get_alias_names() reads the alias name file and stores
 **   the contents in the global array alias_names. The contents of the
 **   alias name file looks as follows:
 **
 **      RZ_  Ha_
 **      type weather
 **
 **   Where RZ_ is the alias name and Ha_ is the name with which we want
 **   to replace the alias name.
 **
 **   The caller is responsible for freeing the memory for alias_names.
 **
 ** RETURN VALUES
 **   None. It will exit with INCORRECT if it fails to allocate memory
 **   or fails to open the alias names file.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.10.2009 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>          /* strcpy(), strerror()                     */
#include <stdlib.h>          /* malloc(), free()                         */
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

/* #define _DEBUG_ALIAS_NAMES */

/* External global variables. */
extern char               *p_work_dir;

/* Local definitions. */
struct alias_names
       {
          char alias_from[MAX_ALIAS_NAME_LENGTH + 1];
          char alias_to[MAX_ALIAS_NAME_LENGTH + 1];
       };

/* Local variables. */
static int                no_of_alias_names;
static struct alias_names *an;


/*########################## get_alias_names() ##########################*/
void
get_alias_names(void)
{
   static time_t last_read = 0;
   static int    first_time = YES;
   char          alias_file[MAX_PATH_LENGTH];
   struct stat   stat_buf;

   (void)sprintf(alias_file, "%s%s/%s", p_work_dir, ETC_DIR, ALIAS_NAME_FILE);
   if (stat(alias_file, &stat_buf) == -1)
   {
      if (errno == ENOENT)
      {
         /*
          * Only tell user once that the rules file is missing. Otherwise
          * it is anoying to constantly receive this message.
          */
         if (first_time == YES)
         {
            system_log(INFO_SIGN, __FILE__, __LINE__,
                       _("There is no alias name file `%s'"), alias_file);
            first_time = NO;
         }
      }
      else
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Failed to stat() `%s' : %s"),
                    alias_file, strerror(errno));
      }
   }
   else
   {
      if (stat_buf.st_mtime != last_read)
      {
         off_t file_size;
         char  *last_ptr,
               *ptr,
               *buffer;

         if (first_time == YES)
         {
            first_time = NO;
         }

         if (last_read != 0)
         {
            /*
             * Since we are rereading the whole rules file again
             * lets release the memory we stored for the previous
             * array of alias_names.
             */
            free(an);
            an = NULL;
            no_of_alias_names = 0;
         }
         last_read = stat_buf.st_mtime;

         if (((file_size = read_file_no_cr(alias_file,
                                           &buffer)) != INCORRECT) &&
             (file_size > 0))
         {
            int data;

            /*
             * Now that we have the contents in the buffer lets first see
             * how many alias names there are in the buffer so we can allocate
             * memory for the alias_names array.
             */
            ptr = buffer;
            last_ptr = 1 + buffer + file_size;
            data = NO;
            do
            {
               if (*ptr == '#')
               {
                  ptr++;
                  while ((*ptr != '\n') && (ptr < last_ptr))
                  {
                     ptr++;
                  }
               }
               else if (*ptr == ' ')
                    {
                       ptr++;
                       while (*ptr == ' ')
                       {
                          ptr++;
                       }
                    }
               else if (*ptr == '\t')
                    {
                       ptr++;
                       while (*ptr == '\t')
                       {
                          ptr++;
                       }
                    }
               else if (*ptr == '\n')
                    {
                       if (data == YES)
                       {
                          no_of_alias_names++;
                          data = NO;
                       }
                       ptr++;
                    }
                    else
                    {
                       data = YES;
                       ptr++;
                    }
            } while (ptr < last_ptr);

            if (no_of_alias_names > 0)
            {
               int  count = 0,
                    i;
               char *to_ptr;

               if ((an = malloc((no_of_alias_names * sizeof(struct alias_names)))) == NULL)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to malloc() %d bytes of memory : %s",
                             (no_of_alias_names * sizeof(struct alias_names)),
                             strerror(errno));
                  no_of_alias_names = 0;
                  return;
               }
               for (i = 0; i < no_of_alias_names; i++)
               {
                  an[i].alias_from[0] = an[i].alias_to[0] = '\0';
               }
               i = 0;

               ptr = buffer;
               to_ptr = &an[0].alias_from[0];
               do
               {
                  if (*ptr == '#')
                  {
                     ptr++;
                     while ((*ptr != '\n') && (ptr < last_ptr))
                     {
                        ptr++;
                     }
                  }
                  else if (*ptr == ' ')
                       {
                          ptr++;
                          while (*ptr == ' ')
                          {
                             ptr++;
                          }
                          if (i > 0)
                          {
                             if (an[count].alias_to[0] == '\0')
                             {
                                an[count].alias_from[i] = '\0';
                                to_ptr = &an[count].alias_to[0];
                             }
                             else
                             {
                                an[count].alias_to[i] = '\0';
                                if ((count + 1) >= no_of_alias_names)
                                {
                                   break;
                                }
                                else
                                {
                                   to_ptr = &an[count + 1].alias_from[0];
                                }
                                while ((*ptr != '\n') && (ptr < last_ptr))
                                {
                                   ptr++;
                                }
                                if (*ptr == '\n')
                                {
                                   ptr++;
                                }
                                count++;
                             }
                             i = 0;
                          }
                       }
                  else if (*ptr == '\t')
                       {
                          ptr++;
                          while (*ptr == '\t')
                          {
                             ptr++;
                          }
                          if (i > 0)
                          {
                             if (an[count].alias_to[0] == '\0')
                             {
                                an[count].alias_from[i] = '\0';
                                to_ptr = &an[count].alias_to[0];
                             }
                             else
                             {
                                an[count].alias_to[i] = '\0';
                                if ((count + 1) >= no_of_alias_names)
                                {
                                   break;
                                }
                                else
                                {
                                   to_ptr = &an[count + 1].alias_from[0];
                                }
                                while ((*ptr != '\n') && (ptr < last_ptr))
                                {
                                   ptr++;
                                }
                                if (*ptr == '\n')
                                {
                                   ptr++;
                                }
                                count++;
                             }
                             i = 0;
                          }
                       }
                  else if (*ptr == '\n')
                       {
                          if (i > 0)
                          {
                             an[count].alias_to[i] = '\0';
                             if ((count + 1) >= no_of_alias_names)
                             {
                                break;
                             }
                             else
                             {
                                to_ptr = &an[count + 1].alias_from[0];
                             }
                             count++;
                          }
                          ptr++;
                       }
                       else
                       {
                          if (i < MAX_ALIAS_NAME_LENGTH)
                          {
                             *to_ptr = *ptr;
                             to_ptr++; i++;
                          }
                          ptr++;
                       }
               } while (ptr < last_ptr);
            } /* if (no_of_alias_names > 0) */

            /* The buffer holding the contents of the rule file */
            /* is no longer needed.                             */
            free(buffer);

#ifdef _DEBUG_ALIAS_NAMES
            {
               register int i;

               for (i = 0; i < no_of_alias_names; i++)
               {
                  system_log(DEBUG_SIGN, NULL, 0, "'%s'  '%s'",
                             an[i].alias_from, an[i].alias_to);
               }
            }
#endif
         }
      } /* if (stat_buf.st_mtime != last_read) */
   }

   return;
}


/*###################### search_insert_alias_name() #####################*/
int
search_insert_alias_name(char *search_str, char *result_str, int max_length)
{
   int i;

   for (i = 0; i < no_of_alias_names; i++)
   {
      if (strcmp(search_str, an[i].alias_from) == 0)
      {
         int j = 0;

         for (j = 0; j < max_length; j++)
         {
            if (an[i].alias_to[j] == '\0')
            {
               return(j);
            }
            result_str[j] = an[i].alias_to[j];
         }
         return(j);
      }
   }

   return(0);
}
