/*
 *  search_old_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 1999 Deutscher Wetterdienst (DWD),
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
 **   search_old_files - searches in all user directories for old
 **                      files
 **
 ** SYNOPSIS
 **   void search_old_files(void)
 **
 ** DESCRIPTION
 **   This function checks the user directory for any old files.
 **   Old depends on the value of OLD_FILE_TIME. If it discovers
 **   files older then OLD_FILE_TIME it will report this in the
 **   system log. When _DELETE_OLD_FILES is defined these files will
 **   be deleted.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   09.06.1997 H.Kiehl Created
 **   21.02.1998 H.Kiehl Added delete logging.
 **                      Search queue directories as well.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* remove(), NULL                     */
#include <string.h>                /* strcpy(), strerror()               */
#include <time.h>                  /* time()                             */
#include <sys/types.h>
#include <sys/stat.h>              /* stat(), S_ISREG()                  */
#include <dirent.h>                /* opendir(), closedir(), readdir(),  */
                                   /* DIR, struct dirent                 */
#ifdef _DELETE_LOG
#include <unistd.h>                /* write()                            */
#endif
#include <errno.h>
#include "amgdefs.h"

extern int                        sys_log_fd,
                                  no_of_dirs,
                                  no_of_hosts;
extern struct directory_entry     *de;
extern struct filetransfer_status *fsa;
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif


/*######################### search_old_files() ##########################*/
void
search_old_files(void)
{
   int           i,
                 junk_files = NO,
                 file_counter;
   time_t        diff_time,
                 now;
   char          *work_ptr,
                 tmp_dir[MAX_PATH_LENGTH];
   unsigned int  file_size;
   struct stat   stat_buf;
   DIR           *dp;
   struct dirent *p_dir;

   for (i = 0; i < no_of_dirs; i++)
   {
      (void)strcpy(tmp_dir, de[i].dir);

      if ((dp = opendir(tmp_dir)) == NULL)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Can't access directory %s : %s (%s %d)\n",
                   tmp_dir, strerror(errno), __FILE__, __LINE__);
      }
      else
      {
         file_counter = 0;
         file_size    = 0;
         time(&now);

         work_ptr = tmp_dir + strlen(tmp_dir);
         *work_ptr++ = '/';
         *work_ptr = '\0';

         errno = 0;
         while ((p_dir = readdir(dp)) != NULL)
         {
            /* Ignore "." and ".." */
            if (((p_dir->d_name[0] == '.') && (p_dir->d_name[1] == '\0')) ||
                ((p_dir->d_name[0] == '.') && (p_dir->d_name[1] == '.') &&
                (p_dir->d_name[2] == '\0')))
            {
               continue;
            }

            (void)strcpy(work_ptr, p_dir->d_name);
            if (stat(tmp_dir, &stat_buf) < 0)
            {
               /*
                * Since this is a very low priority function lets not
                * always report when we fail to stat() a file. Maybe the
                * the user wants to keep some files.
                */
               continue;
            }

            /* Sure it is a normal file? */
            if (S_ISREG(stat_buf.st_mode))
            {
               diff_time = now - stat_buf.st_mtime;
               if (diff_time > de[i].old_file_time)
               {
                  if ((de[i].remove_flag == YES) ||
                      (p_dir->d_name[0] == '.') ||
                      (stat_buf.st_size == 0))
                  {
                     if (remove(tmp_dir) == -1)
                     {
                        (void)rec(sys_log_fd, WARN_SIGN,
                                  "Failed to remove %s : %s (%s %d)\n",
                                  tmp_dir, strerror(errno), __FILE__, __LINE__);
                     }
                     else
                     {
#ifdef _DELETE_LOG
                        size_t dl_real_size;

                        (void)strcpy(dl.file_name, p_dir->d_name);
                        (void)sprintf(dl.host_name, "%-*s %x",
                                      MAX_HOSTNAME_LENGTH,
                                      "-",
                                      AGE_INPUT);
                        *dl.file_size = stat_buf.st_size;
                        *dl.job_number = de[i].dir_no;
                        *dl.file_name_length = strlen(p_dir->d_name);
                        dl_real_size = *dl.file_name_length + dl.size +
                                       sprintf((dl.file_name + *dl.file_name_length + 1),
                                               "dir_check() >%ld",
                                               diff_time);
                        if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                        {
                           (void)rec(sys_log_fd, ERROR_SIGN,
                                     "write() error : %s (%s %d)\n",
                                     strerror(errno), __FILE__, __LINE__);
                        }
#endif /* _DELETE_LOG */
                        file_counter++;
                        file_size += stat_buf.st_size;

                        if (de[i].remove_flag != YES)
                        {
                           junk_files = YES;
                        }
                     }
                  }
                  else
                  {
                     file_counter++;
                     file_size += stat_buf.st_size;
                  }
               }
            }
                 /*
                  * Search queue directories for old files!
                  */
            else if ((de[i].remove_flag == YES) &&
                     (S_ISDIR(stat_buf.st_mode)) &&
                     (p_dir->d_name[0] == '.'))
                 {
                    int pos;

                    if ((pos = get_position(fsa,
                                            &p_dir->d_name[1],
                                            no_of_hosts)) != INCORRECT)
                    {
                       DIR *dp_2;

                       if ((dp_2 = opendir(tmp_dir)) == NULL)
                       {
                          (void)rec(sys_log_fd, WARN_SIGN,
                                    "Can't access directory %s : %s (%s %d)\n",
                                    tmp_dir, strerror(errno),
                                    __FILE__, __LINE__);
                       }
                       else
                       {
                          char *work_ptr_2;

                          work_ptr_2 = tmp_dir + strlen(tmp_dir);
                          *work_ptr_2++ = '/';
                          *work_ptr_2 = '\0';

                          errno = 0;
                          while ((p_dir = readdir(dp_2)) != NULL)
                          {
                             /* Ignore "." and ".." */
                             if (p_dir->d_name[0] == '.')
                             {
                                continue;
                             }

                             (void)strcpy(work_ptr_2, p_dir->d_name);
                             if (stat(tmp_dir, &stat_buf) < 0)
                             {
                                /*
                                 * Since this is a very low priority function lets not
                                 * always report when we fail to stat() a file. Maybe the
                                 * the user wants to keep some files.
                                 */
                                continue;
                             }

                             /* Sure it is a normal file? */
                             if (S_ISREG(stat_buf.st_mode))
                             {
                                diff_time = now - stat_buf.st_mtime;
                                if (diff_time > de[i].old_file_time)
                                {
                                   if (de[i].remove_flag == YES)
                                   {
                                      if (remove(tmp_dir) == -1)
                                      {
                                         (void)rec(sys_log_fd, WARN_SIGN,
                                                   "Failed to remove %s : %s (%s %d)\n",
                                                   tmp_dir, strerror(errno), __FILE__, __LINE__);
                                      }
                                      else
                                      {
#ifdef _DELETE_LOG
                                         size_t dl_real_size;

                                         (void)strcpy(dl.file_name, p_dir->d_name);
                                         (void)sprintf(dl.host_name, "%-*s %x",
                                                       MAX_HOSTNAME_LENGTH,
                                                       fsa[pos].host_dsp_name,
                                                       AGE_INPUT);
                                         *dl.file_size = stat_buf.st_size;
                                         *dl.job_number = de[i].dir_no;
                                         *dl.file_name_length = strlen(p_dir->d_name);
                                         dl_real_size = *dl.file_name_length + dl.size +
                                                        sprintf((dl.file_name + *dl.file_name_length + 1),
                                                                "dir_check() >%ld",
                                                                diff_time);
                                         if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                                         {
                                            (void)rec(sys_log_fd, ERROR_SIGN,
                                                      "write() error : %s (%s %d)\n",
                                                      strerror(errno), __FILE__, __LINE__);
                                         }
#endif /* _DELETE_LOG */
                                         file_counter++;
                                         file_size += stat_buf.st_size;
                                      }
                                   }
                                   else
                                   {
                                      file_counter++;
                                      file_size += stat_buf.st_size;
                                   }
                                }
                             }
                             errno = 0;
                          } /* while ((p_dir = readdir(dp_2)) != NULL) */

                          if (errno)
                          {
                             (void)rec(sys_log_fd, ERROR_SIGN,
                                       "Could not readdir() %s : %s (%s %d)\n",
                                       tmp_dir, strerror(errno),
                                       __FILE__, __LINE__);
                          }

                          /* Don't forget to close the directory */
                          if (closedir(dp_2) < 0)
                          {
                             (void)rec(sys_log_fd, ERROR_SIGN,
                                       "Could not close directory %s : %s (%s %d)\n",
                                       tmp_dir, strerror(errno),
                                       __FILE__, __LINE__);
                          }
                       }
                    }
                 }
            errno = 0;
         }

         /*
          * NOTE: The ENOENT is when it catches a file that is just
          *       being renamed (lock DOT).
          */
         if ((errno) && (errno != ENOENT))
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Could not readdir() %s : %s (%s %d)\n",
                      tmp_dir, strerror(errno), __FILE__, __LINE__);
         }

         /* Don't forget to close the directory */
         if (closedir(dp) < 0)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Could not close directory %s : %s (%s %d)\n",
                      tmp_dir, strerror(errno), __FILE__, __LINE__);
         }

         /* Remove file name from directory name. */
         *(work_ptr - 1) = '\0';

         /* Tell user there are old files in this directory. */
         if ((file_counter > 0) && (de[i].report_flag == YES) &&
             (de[i].remove_flag == NO) && (junk_files == NO))
         {
            if (file_size >= 1073741824)
            {
               (void)rec(sys_log_fd, WARN_SIGN,
                         "There are %d (%d GBytes) old (>%dh) files in %s\n",
                         file_counter, file_size / 1073741824,
                         de[i].old_file_time / 3600, tmp_dir);
            }
            else if (file_size >= 1048576)
                 {
                    (void)rec(sys_log_fd, WARN_SIGN,
                              "There are %d (%d MBytes) old (>%dh) files in %s\n",
                              file_counter, file_size / 1048576,
                              de[i].old_file_time / 3600, tmp_dir);
                 }
            else if (file_size >= 1024)
                 {
                    (void)rec(sys_log_fd, WARN_SIGN,
                              "There are %d (%d KBytes) old (>%dh) files in %s\n",
                              file_counter, file_size / 1024,
                              de[i].old_file_time / 3600, tmp_dir);
                 }
                 else
                 {
                    (void)rec(sys_log_fd, WARN_SIGN,
                              "There are %d (%d Bytes) old (>%dh) files in %s\n",
                              file_counter, file_size,
                              de[i].old_file_time / 3600, tmp_dir);
                 }
         }
      }
   }

   return;
}