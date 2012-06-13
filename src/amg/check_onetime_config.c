/*
 *  check_onetime_config.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2011 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_onetime_config - checks if a onetime config has arrived
 **
 ** SYNOPSIS
 **   void check_onetime_config(void)
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
 **   24.02.2011 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* sprintf()                          */
#include <string.h>                /* strlen(), strcpy(), strerror()     */
#include <stdlib.h>                /* exit(), realloc()                  */
#include <sys/types.h>
#include <sys/stat.h>              /* stat(), S_ISREG()                  */
#include <dirent.h>                /* opendir(), closedir(), readdir(),  */
                                   /* DIR, struct dirent                 */
#include <errno.h>
#include "amgdefs.h"

/* External global variables. */
extern int                   no_of_ot_dir_configs;
extern char                  *p_work_dir;
extern struct dir_config_buf *ot_dcl;

/* Local global variables. */
static char                  *ot_config_dir = NULL,
                             *ot_list_dir = NULL,
                             *p_config_dir,
                             *p_list_dir;
static time_t                last_cscan_time = 0L,
                             last_lscan_time = 0L;


/*####################### check_onetime_config() ########################*/
void
check_onetime_config(void)
{
   off_t       db_size;
   struct stat stat_buf;

   if ((last_cscan_time == 0L) || (last_lscan_time == 0L))
   {
      if ((ot_config_dir == NULL) || (ot_list_dir == NULL))
      {
         size_t length;

         length = strlen(p_work_dir) + AFD_ONETIME_DIR_LENGTH + ETC_DIR_LENGTH +
                  AFD_CONFIG_DIR_LENGTH + 1 + MAX_FILENAME_LENGTH + 1;
         if (ot_config_dir == NULL)
         {
            if ((ot_config_dir = malloc(length)) != NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to malloc() memory : %s", strerror(errno));
               return;
            }
            p_config_dir = ot_config_dir + sprintf(ot_config_dir, "%s%s%s%s/",
                                                   p_work_dir, AFD_ONETIME_DIR,
                                                   ETC_DIR, AFD_CONFIG_DIR);
         }
         if (ot_list_dir == NULL)
         {
            if ((ot_list_dir = malloc(length)) != NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to malloc() memory : %s", strerror(errno));
               return;
            }
            p_list_dir = ot_list_dir + sprintf(ot_list_dir, "%s%s%s%s/",
                                               p_work_dir, AFD_ONETIME_DIR,
                                               ETC_DIR, AFD_LIST_DIR);
         }
      }
   }

   /*
    * Allthough we do this same check at the end of this function we
    * do it here as well in case we add some code that returns without
    * resetting these values at some later stage.
    */
   if ((no_of_ot_dir_configs != 0) && (ot_dcl != NULL))
   {
      int i;

      for (i = 0; i < no_of_ot_dir_configs; i++)
      {
         free(ot_dcl[i].dir_config_file);
      }
      free(ot_dcl);
      ot_dcl = NULL;
      no_of_ot_dir_configs = 0;
   }

   /* Check config directories for changes. */
   if (stat(ot_config_dir, &stat_buf) < 0)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to stat() `%s' : %s", ot_config_dir, strerror(errno));
   }
   else
   {
      if (stat_buf.st_mtime > last_cscan_time)
      {
         DIR *dp;

         if ((dp = opendir(ot_config_dir)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to opendir() `%s' : %s",
                       ot_config_dir, strerror(errno));
         }
         else
         {
            struct dirent *p_dir;

            while ((p_dir = readdir(dp)) != NULL)
            {
               if (p_dir->d_name[0] != '.')
               {
                  (void)strcpy(p_config_dir, p_dir->d_name);
                  if (stat(ot_config_dir, &stat_buf) < 0)
                  {
                     if (errno != ENOENT)
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   _("Can't stat() file `%s' : %s"),
                                   ot_config_dir, strerror(errno));
                     }
                     continue;
                  }

                  /* Sure it is a normal file? */
                  if (S_ISREG(stat_buf.st_mode))
                  {
                     if ((no_of_ot_dir_configs % OT_DC_STEP_SIZE) == 0)
                     {
                        size_t new_size;

                        new_size = ((no_of_ot_dir_configs / OT_DC_STEP_SIZE) + 1) *
                                   OT_DC_STEP_SIZE * sizeof(struct dir_config_buf);

                        if ((ot_dcl = realloc(ot_dcl, new_size)) == NULL)
                        {
                           system_log(FATAL_SIGN, __FILE__, __LINE__,
                                      "Failed to realloc() %d bytes : %s",
                                      new_size, strerror(errno));
                           exit(INCORRECT);
                        }
                     }
                     if ((ot_dcl[no_of_ot_dir_configs].dir_config_file = malloc(strlen(ot_config_dir) + 1)) == NULL)
                     {
                        system_log(FATAL_SIGN, __FILE__, __LINE__,
                                   "Failed to malloc() %d bytes : %s",
                                   strlen(ot_config_dir) + 1, strerror(errno));
                        exit(INCORRECT);
                     }
                     (void)strcpy(ot_dcl[no_of_ot_dir_configs].dir_config_file,
                                  ot_config_dir);
                     ot_dcl[no_of_ot_dir_configs].dc_old_time = stat_buf.st_mtime;
                     ot_dcl[no_of_ot_dir_configs].type = OT_CONFIG_TYPE;
                     db_size += stat_buf.st_size;
                  }
               }
            } /* while ((p_dir = readdir(dp)) != NULL) */

            /* When using readdir() it can happen that it always returns     */
            /* NULL, due to some error. We want to know if this is the case. */
            if (errno == EBADF)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          _("Failed to readdir() `%s' : %s"),
                          ot_config_dir, strerror(errno));
            }

            if (p_config_dir != NULL)
            {
               *p_config_dir = '\0';
            }

            /* Don't forget to close the directory. */
            if (closedir(dp) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed  to closedir() `%s' : %s"),
                          ot_config_dir, strerror(errno));
            }
         }
         last_cscan_time = stat_buf.st_mtime;
      }
   }

   /* Check list directory for changes. */
   if (stat(ot_list_dir, &stat_buf) < 0)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 "Failed to stat() `%s' : %s", ot_list_dir, strerror(errno));
   }
   else
   {
      if (stat_buf.st_mtime > last_lscan_time)
      {
         DIR *dp;

         if ((dp = opendir(ot_list_dir)) == NULL)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to opendir() `%s' : %s",
                       ot_list_dir, strerror(errno));
         }
         else
         {
            struct dirent *p_dir;

            while ((p_dir = readdir(dp)) != NULL)
            {
               if (p_dir->d_name[0] != '.')
               {
                  (void)strcpy(p_list_dir, p_dir->d_name);
                  if (stat(ot_list_dir, &stat_buf) < 0)
                  {
                     if (errno != ENOENT)
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   _("Can't stat() file `%s' : %s"),
                                   ot_list_dir, strerror(errno));
                     }
                     continue;
                  }

                  /* Sure it is a normal file? */
                  if (S_ISREG(stat_buf.st_mode))
                  {
                     if ((no_of_ot_dir_configs % OT_DC_STEP_SIZE) == 0)
                     {
                        size_t new_size;

                        new_size = ((no_of_ot_dir_configs / OT_DC_STEP_SIZE) + 1) *
                                   OT_DC_STEP_SIZE * sizeof(struct dir_config_buf);

                        if ((ot_dcl = realloc(ot_dcl, new_size)) == NULL)
                        {
                           system_log(FATAL_SIGN, __FILE__, __LINE__,
                                      "Failed to realloc() %d bytes : %s",
                                      new_size, strerror(errno));
                           exit(INCORRECT);
                        }
                     }
                     if ((ot_dcl[no_of_ot_dir_configs].dir_config_file = malloc(strlen(ot_config_dir) + 1)) == NULL)
                     {
                        system_log(FATAL_SIGN, __FILE__, __LINE__,
                                   "Failed to malloc() %d bytes : %s",
                                   strlen(ot_config_dir) + 1, strerror(errno));
                        exit(INCORRECT);
                     }
                     (void)strcpy(ot_dcl[no_of_ot_dir_configs].dir_config_file,
                                  ot_config_dir);
                     ot_dcl[no_of_ot_dir_configs].dc_old_time = stat_buf.st_mtime;
                     ot_dcl[no_of_ot_dir_configs].type = OT_LIST_TYPE;
                     db_size += stat_buf.st_size;
                  }
               }
            } /* while ((p_dir = readdir(dp)) != NULL) */

            /* When using readdir() it can happen that it always returns     */
            /* NULL, due to some error. We want to know if this is the case. */
            if (errno == EBADF)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          _("Failed to readdir() `%s' : %s"),
                          ot_list_dir, strerror(errno));
            }

            if (p_list_dir != NULL)
            {
               *p_list_dir = '\0';
            }

            /* Don't forget to close the directory. */
            if (closedir(dp) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed  to closedir() `%s' : %s"),
                          ot_list_dir, strerror(errno));
            }
         }
         last_lscan_time = stat_buf.st_mtime;
      }
   }

   if ((no_of_ot_dir_configs > 0) && (db_size > 12))
   {
      if (eval_dir_config(db_size, NULL, YES) != SUCCESS)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not find any valid onetime entries in database %s"),
                    (no_of_ot_dir_configs > 1) ? "files" : "file");
      }
   }

   if ((no_of_ot_dir_configs != 0) && (ot_dcl != NULL))
   {
      int i;

      for (i = 0; i < no_of_ot_dir_configs; i++)
      {
         free(ot_dcl[i].dir_config_file);
      }
      free(ot_dcl);
      ot_dcl = NULL;
      no_of_ot_dir_configs = 0;
   }

   return;
}
