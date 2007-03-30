/*
 *  link_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   link_files - generates hard links of all user files that
 **                match a certain pattern
 **
 ** SYNOPSIS
 **   int link_files(char                   *src_file_path,
 **                  char                   *dest_file_path,
 **                  time_t                 current_time,
 **                  struct directory_entry *p_de,
 **                  struct instant_db      *p_db,
 **                  time_t                 *creation_time,
 **                  unsigned int           *split_job_counter,
 **                  int                    unique_number,
 **                  int                    pos_in_fm,
 **                  int                    no_of_files,
 **                  char                   *unique_name,
 **                  off_t                  *file_size_linked)
 **
 ** DESCRIPTION
 **   The function link_files() generates hard links from all files in
 **   'src_file_path' to the AFD file directory. To keep files of each
 **   job unique it has to create a unique directory name. This name is
 **   later also used to create the message name.
 **
 ** RETURN VALUES
 **   Returns INCORRECT when it fails to link the user files. Otherwise
 **   it returns the number of files that are linked, the total size of
 **   all files 'file_size_linked' and the unique name 'unique_name'
 **   that was generated.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.10.1995 H.Kiehl Created
 **   19.06.1997 H.Kiehl Copy file when exec option is set.
 **   18.10.1997 H.Kiehl Introduced inverse filtering.
 **   26.10.1997 H.Kiehl If disk is full do not give up.
 **   16.04.2001 H.Kiehl Buffer the file names we link.
 **   13.07.2003 H.Kiehl Don't link files that are older then the value
 **                      specified in 'age-limit' option.
 **   17.10.2004 H.Kiehl Put files directly to the outgoing directory
 **                      when there are no local options.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* sprintf()                          */
#include <string.h>                /* strcpy(), strlen(), strerror()     */
#include <time.h>                  /* time()                             */
#ifndef _WITH_PTHREAD
#include <stdlib.h>                /* realloc()                          */
#endif
#include <sys/types.h>
#include <unistd.h>                /* link()                             */
#include <errno.h>
#include "amgdefs.h"

/* External global variables */
#ifndef _WITH_PTHREAD
extern off_t             *file_size_pool;
extern time_t            *file_mtime_pool;
extern char              *file_name_buffer,
                         **file_name_pool;
#endif
#ifdef _DELETE_LOG
extern struct delete_log dl;
#endif


/*########################### link_files() ##############################*/
int
link_files(char                   *src_file_path,
           char                   *dest_file_path,
           time_t                 current_time,
#ifdef _WITH_PTHREAD
           off_t                  *file_size_pool,
           time_t                 *file_mtime_pool,
           char                   **file_name_pool,
#endif
           struct directory_entry *p_de,
           struct instant_db      *p_db,
           time_t                 *creation_time,
           unsigned int           *split_job_counter,
           int                    unique_number,
           int                    pos_in_fm,
           int                    no_of_files,
           char                   *unique_name, /* Storage to return unique name. */
           off_t                  *file_size_linked)
{
   int          files_linked = 0,
                retstat;
   register int i,
                j;
   time_t       pmatch_time;
   char         *p_src,
                *p_dest = NULL,
                *p_dest_end = NULL;

   *file_size_linked = 0;
   p_src = src_file_path + strlen(src_file_path);

   for (i = 0; i < no_of_files; i++)
   {
      for (j = 0; j < p_de->fme[pos_in_fm].nfm; j++)
      {
         if (p_de->paused_dir == NULL)
         {
            pmatch_time = current_time;
         }
         else
         {
            pmatch_time = file_mtime_pool[i];
         }
         if ((retstat = pmatch(p_de->fme[pos_in_fm].file_mask[j],
                               file_name_pool[i], &pmatch_time)) == 0)
         {
            time_t diff_time = current_time - file_mtime_pool[i];

            if (diff_time < 0)
            {
               diff_time = 0;
            }
            if ((p_db->age_limit > 0) && (diff_time > p_db->age_limit))
            {
#ifdef _DELETE_LOG
               size_t dl_real_size;

               (void)strcpy(dl.file_name, file_name_pool[i]);
               (void)sprintf(dl.host_name, "%-*s %x",
                             MAX_HOSTNAME_LENGTH, p_db->host_alias, AGE_INPUT);
               *dl.file_size = file_size_pool[i];
               *dl.job_number = p_de->dir_id;
               *dl.file_name_length = strlen(file_name_pool[i]);
               dl_real_size = *dl.file_name_length + dl.size +
                              sprintf((dl.file_name + *dl.file_name_length + 1),
#if SIZEOF_TIME_T == 4
                                      "%s >%ld (%s %d)", DIR_CHECK,
#else
                                      "%s >%lld (%s %d)", DIR_CHECK,
#endif
                                      (pri_time_t)diff_time, 
                                      __FILE__, __LINE__);
               if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "write() error : %s", strerror(errno));
               }
#endif /* _DELETE_LOG */
               if (p_de->flag & RENAME_ONE_JOB_ONLY)
               {
                  (void)strcpy(p_src, file_name_pool[i]);
                  if (unlink(src_file_path) == -1)
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                "Failed to unlink() file `%s' : %s",
                                src_file_path, strerror(errno));
                  }
               }
            }
            else
            {
               /* Only create a unique name and the corresponding */
               /* directory when we have found a file that is to  */
               /* be distributed.                                 */
               if (p_dest == NULL)
               {
                  *creation_time = current_time;
                  if (p_db->loptions != NULL)
                  {
                     /* Create a new message name and directory. */
                     if (create_name(dest_file_path, p_db->priority,
                                     *creation_time, p_db->job_id,
                                     split_job_counter,
                                     &unique_number, unique_name, -1) < 0)
                     {
                        if (errno == ENOSPC)
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      "DISK FULL!!! Will retry in %d second interval.",
                                      DISK_FULL_RESCAN_TIME);

                           while (errno == ENOSPC)
                           {
                              (void)sleep(DISK_FULL_RESCAN_TIME);
                              *creation_time = time(NULL);
                              errno = 0;
                              if (create_name(dest_file_path, p_db->priority,
                                              *creation_time, p_db->job_id,
                                              split_job_counter,
                                              &unique_number, unique_name, -1) < 0)
                              {
                                 if (errno != ENOSPC)
                                 {
                                    system_log(FATAL_SIGN, __FILE__, __LINE__,
                                               "Failed to create a unique name : %s",
                                               strerror(errno));
                                    exit(INCORRECT);
                                 }
                              }
                           }

                           system_log(INFO_SIGN, __FILE__, __LINE__,
                                      "Continuing after disk was full.");
                        }
                        else
                        {
                           system_log(FATAL_SIGN, __FILE__, __LINE__,
                                      "Failed to create a unique name : %s",
                                      strerror(errno));
                           exit(INCORRECT);
                        }
                     }
                     p_dest_end = dest_file_path + strlen(dest_file_path);
                     if (*(p_dest_end - 1) != '/')
                     {
                        *p_dest_end = '/';
                        p_dest_end++;
                     }
                     (void)strcpy(p_dest_end, unique_name);
                     p_dest = p_dest_end + strlen(unique_name);
                     *(p_dest++) = '/';
                     *p_dest = '\0';
                  }
                  else
                  {
                     int dir_no;

                     if ((dir_no = get_dir_number(dest_file_path, p_db->job_id,
                                                  NULL)) == INCORRECT)
                     {
                        if (p_dest_end != NULL)
                        {
                           *p_dest_end = '\0';
                        }
                        if (p_src != NULL)
                        {
                           *p_src = '\0';
                        }
                        return(INCORRECT);
                     }
                     p_dest_end = dest_file_path + strlen(dest_file_path);
                     if (*(p_dest_end - 1) == '/')
                     {
                        p_dest_end--;
                     }
#if SIZEOF_TIME_T == 4
                     (void)sprintf(unique_name, "%x/%x/%lx_%x_%x",
#else
                     (void)sprintf(unique_name, "%x/%x/%llx_%x_%x",
#endif
                                   p_db->job_id, dir_no,
                                   (pri_time_t)*creation_time,
                                   unique_number, *split_job_counter);
                     p_dest = p_dest_end +
                              sprintf(p_dest_end, "/%s/", unique_name);
                     if (mkdir(dest_file_path, DIR_MODE) == -1)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   "Failed to create directory %s : %s",
                                   dest_file_path, strerror(errno));
                        if (p_dest_end != NULL)
                        {
                           *p_dest_end = '\0';
                        }
                        if (p_src != NULL)
                        {
                           *p_src = '\0';
                        }
                        return(INCORRECT);
                     }
                  }
               }

               (void)strcpy(p_src, file_name_pool[i]);
               (void)strcpy(p_dest, file_name_pool[i]);

               /* rename/link/copy the file */
               if (p_de->flag & RENAME_ONE_JOB_ONLY)
               {
                  if ((retstat = rename(src_file_path, dest_file_path)) == -1)
                  {
                     int gotcha = NO,
                         k;

                     /*
                      * It can happen that when we copied/renamed the file
                      * from its source directory into our internal pool
                      * directory, that we have copied/renamed the same
                      * file twice, overwritting one. But in our array
                      * file_name_pool[] we still have both listed. Search
                      * through the array and see if this is the case, then
                      * we can savely ignore this error message.
                      */
                     if (i > 0)
                     {
                        for (k = 0; k < i; k++)
                        {
                           if (CHECK_STRCMP(file_name_pool[i],
                                            file_name_pool[k]) == 0)
                           {
                              system_log(DEBUG_SIGN, NULL, 0,
                                         "File %s has been picked up more then once while scanning input directory %s [%s %x]\n",
                                         file_name_pool[i], p_de->dir,
                                         p_de->alias, p_de->dir_id);
                              gotcha = YES;
                              break;
                           }
                        }
                     }
                     if (gotcha == NO)
                     {
                        system_log(WARN_SIGN, __FILE__, __LINE__,
                                   "Failed to rename() file %s to %s : %s",
                                   src_file_path, dest_file_path,
                                   strerror(errno));
                        if (errno == ENOENT)
                        {
                           char whats_gone[40];

                           whats_gone[0] = '\0';
                           if (eaccess(src_file_path, R_OK) != 0)
                           {
                              (void)strcat(whats_gone, "src file");
                           }
                           if (eaccess(dest_file_path, R_OK) != 0)
                           {
                              (void)strcat(whats_gone, ", dst file");
                           }
                           *p_src = '\0';
                           *p_dest = '\0';
                           if (eaccess(src_file_path, R_OK) != 0)
                           {
                              (void)strcat(whats_gone, ", src dir");
                           }
                           if (eaccess(dest_file_path, R_OK) != 0)
                           {
                              (void)strcat(whats_gone, ", dst dir");
                           }
                           system_log(DEBUG_SIGN, NULL, 0, "%s is not there",
                                      whats_gone);
                        }
                     }
                  }
               }
               else if (p_db->lfs & DO_NOT_LINK_FILES)
                    {
                       if ((retstat = copy_file(src_file_path, dest_file_path,
                                                NULL)) < 0)
                       {
                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                     "Failed to copy file %s to %s",
                                     src_file_path, dest_file_path);
                       }
                    }
                    else /* Just link() the files. */
                    {
                       if ((retstat = link(src_file_path, dest_file_path)) == -1)
                       {
                          if (errno == ENOSPC)
                          {
                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                        "DISK FULL!!! Will retry in %d second interval.",
                                        DISK_FULL_RESCAN_TIME);

                             while (errno == ENOSPC)
                             {
                                (void)sleep(DISK_FULL_RESCAN_TIME);
                                errno = 0;
                                if (link(src_file_path, dest_file_path) < 0)
                                {
                                   if (errno != ENOSPC)
                                   {
                                      system_log(WARN_SIGN, __FILE__, __LINE__,
                                                 "Failed to link file %s to %s : %s",
                                                 src_file_path, dest_file_path,
                                                 strerror(errno));
                                      break;
                                   }
                                }
                             }

                             system_log(INFO_SIGN, __FILE__, __LINE__,
                                        "Continuing after disk was full.");
                          }
                          else
                          {
                             system_log(WARN_SIGN, __FILE__, __LINE__,
                                        "Failed to link file %s to %s : %s",
                                        src_file_path, dest_file_path,
                                        strerror(errno));
                          }
                       }
                    }

               if (retstat != -1)
               {
#ifndef _WITH_PTHREAD
                  if ((files_linked % FILE_NAME_STEP_SIZE) == 0)
                  {
                     size_t new_size;

                     /* Calculate new size of file name buffer */
                     new_size = ((files_linked / FILE_NAME_STEP_SIZE) + 1) * FILE_NAME_STEP_SIZE * MAX_FILENAME_LENGTH;

                     /* Increase the space for the file name buffer */
                     if ((file_name_buffer = realloc(file_name_buffer, new_size)) == NULL)
                     {
                        system_log(FATAL_SIGN, __FILE__, __LINE__,
                                   "Could not realloc() memory : %s",
                                   strerror(errno));
                        exit(INCORRECT);
                     }
                  }
                  (void)strcpy((file_name_buffer + (files_linked * MAX_FILENAME_LENGTH)), file_name_pool[i]);
#endif /* !_WITH_PTHREAD */
                  files_linked++;
                  *file_size_linked += file_size_pool[i];
               }
            }

            /*
             * Since the file is already in the file directory
             * no need to test any further filters.
             */
            break;
         }
         else if (retstat == 1)
              {
                 /*
                  * This file is definitely NOT wanted, no matter what the
                  * following filters say.
                  */
                 break;
              }
      } /* for (j = 0; j < p_de->fme[pos_in_fm].nfm; j++) */
   } /* for (i = 0; i < no_of_files; i++) */

   /* Keep source and destination directories clean so */
   /* that other functions can work with them.         */
   if (p_dest_end != NULL)
   {
      *p_dest_end = '\0';
   }
   if (p_src != NULL)
   {
      *p_src = '\0';
   }

   return(files_linked);
}