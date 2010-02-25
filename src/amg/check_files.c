/*
 *  check_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2010 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_files - moves all files that are to be distributed
 **                 to a temporary directory
 **
 ** SYNOPSIS
 **   int check_files(struct directory_entry *p_de,
 **                   char                   *src_file_path,
 **                   int                    use_afd_file_dir,
 **                   char                   *tmp_file_dir,
 **                   int                    count_files,
 **                   time_t                 current_time,
 **                   int                    *rescan_dir,
 **                   off_t                  *total_file_size)
 **
 ** DESCRIPTION
 **   The function check_files() searches the directory 'p_de->dir'
 **   for files 'p_de->fme[].file_mask[]'. If it finds them they get
 **   moved to a unique directory of the following format:
 **            nnnnnnnnnn_llll
 **                |       |
 **                |       +-------> Counter which is set to zero
 **                |                 after each second. This
 **                |                 ensures that no message can
 **                |                 have the same name in one
 **                |                 second.
 **                +---------------> creation time in seconds
 **
 **   check_files() will only copy 'max_copied_files' (100) or
 **   'max_copied_file_size' bytes. If there are more files or the
 **   size limit has been reached, these will be handled with the next
 **   call to check_files(). Otherwise it will take too long before files
 **   get transmitted and other directories get their turn. Since local
 **   copying is always faster than copying something to a remote machine,
 **   the AMG will have finished its task before the FD has finished.
 **
 ** RETURN VALUES
 **   Returns the number of files that have been copied and the directory
 **   where the files have been moved in 'tmp_file_dir'. If it fails it
 **   will return INCORRECT.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   02.10.1995 H.Kiehl Created
 **   18.10.1997 H.Kiehl Introduced inverse filtering.
 **   04.03.1999 H.Kiehl When copying files don't only limit the number
 **                      of files, also limit the size that may be copied.
 **   05.06.1999 H.Kiehl Option to check for last character in file.
 **   25.04.2001 H.Kiehl Check if we need to move or copy a file.
 **   27.08.2001 H.Kiehl Extra parameter to enable or disable counting
 **                      of files on input, so we don't count files twice
 **                      when queue is opened.
 **   03.05.2002 H.Kiehl Count total number of files and bytes in directory
 **                      regardless if they are to be retrieved or not.
 **   12.07.2002 H.Kiehl Check if we can already delete any old files that
 **                      have no distribution rule.
 **   15.07.2002 H.Kiehl Option to ignore files which have a certain size.
 **   06.02.2003 H.Kiehl Put max_copied_files and max_copied_file_size into
 **                      FRA.
 **   13.07.2003 H.Kiehl Store mtime of file.
 **   16.08.2003 H.Kiehl Added options to start using the given directory
 **                      when a certain file has arrived and/or a certain
 **                      number of files and/or the files in the directory
 **                      have a certain size.
 **   02.09.2004 H.Kiehl Added option "igore file time".
 **   17.04.2005 H.Kiehl When it is a paused directory do not wait for
 **                      a certain file, file size or make any date check.
 **   01.06.2005 H.Kiehl Build in check for duplicate files.
 **   15.03.2006 H.Kiehl When checking if we have sufficient file permissions
 **                      we must check supplementary groups as well.
 **   29.03.2006 H.Kiehl Support for pattern matching with expanding
 **                      filters.
 **   10.09.2009 H.Kiehl Tell caller of function it needs to rescan directory
 **                      if file size or time has not yet been reached.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* fprintf(), sprintf()               */
#include <string.h>                /* strcmp(), strcpy(), strerror()     */
#include <stdlib.h>                /* exit()                             */
#include <unistd.h>                /* write(), read(), lseek(), close()  */
#ifdef _WITH_PTHREAD
# include <pthread.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>              /* stat(), S_ISREG()                  */
#include <dirent.h>                /* opendir(), closedir(), readdir(),  */
                                   /* DIR, struct dirent                 */
#ifdef HAVE_MMAP
# include <sys/mman.h>
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>
#include "amgdefs.h"

extern int                        afd_file_dir_length,
                                  amg_counter_fd,
                                  fra_fd; /* Needed by ABS_REDUCE_QUEUE()*/
#ifdef _POSIX_SAVED_IDS
extern int                        no_of_sgids;
extern uid_t                      afd_uid;
extern gid_t                      afd_gid,
                                  *afd_sgids;
#endif
#ifdef _INPUT_LOG
extern int                        il_fd,
                                  *il_unique_number;
extern unsigned int               *il_dir_number;
extern size_t                     il_size;
extern off_t                      *il_file_size;
extern time_t                     *il_time;
extern char                       *il_file_name,
                                  *il_data;
#endif /* _INPUT_LOG */
#ifdef WITH_DUP_CHECK
extern char                       *p_work_dir;
#endif
#ifndef _WITH_PTHREAD
extern unsigned int               max_file_buffer;
extern off_t                      *file_size_pool;
extern time_t                     *file_mtime_pool;
extern char                       **file_name_pool;
extern unsigned char              *file_length_pool;
#endif
#ifdef _WITH_PTHREAD
extern pthread_mutex_t            fsa_mutex;
#endif
extern struct fileretrieve_status *fra;
#ifdef _DELETE_LOG
extern struct delete_log          dl;
#endif
#ifdef _DISTRIBUTION_LOG
extern unsigned int               max_jobs_per_file;
# ifndef _WITH_PTHREAD
extern struct file_dist_list      **file_dist_pool;
# endif
#endif
extern char                       *afd_file_dir;

/* Local function prototypes. */
static int                        get_last_char(char *, off_t);
#ifdef _POSIX_SAVED_IDS
static int                        check_sgids(gid_t);
#endif


/*########################### check_files() #############################*/
int
check_files(struct directory_entry *p_de,
            char                   *src_file_path,
            int                    use_afd_file_dir,
            char                   *tmp_file_dir,  /* Return directory   */
                                                   /* where files are    */
                                                   /* stored.            */
            int                    count_files,
            int                    *unique_number,
            time_t                 current_time,
            int                    *rescan_dir,
#ifdef _WITH_PTHREAD
            off_t                  *file_size_pool,
            time_t                 *file_mtime_pool,
            char                   **file_name_pool,
            unsigned char          *file_length_pool,
            struct file_dist_list  **file_dist_pool,
#endif
            off_t                  *total_file_size)
{
   int           files_copied = 0,
                 files_in_dir = 0,
                 i,
                 ret,
                 rl_pos,
                 set_error_counter = NO; /* Indicator to tell that we */
                                         /* set the fra error_counter */
                                         /* when we are called.       */
   unsigned int  split_job_counter = 0;
   off_t         bytes_in_dir = 0;
   time_t        diff_time,
                 pmatch_time;
   char          fullname[MAX_PATH_LENGTH],
                 *ptr = NULL,
                 *work_ptr;
   struct stat   stat_buf;
   DIR           *dp;
   struct dirent *p_dir;
#ifdef _INPUT_LOG
   size_t        il_real_size;
#endif

   (void)strcpy(fullname, src_file_path);
   work_ptr = fullname + strlen(fullname);
   *work_ptr++ = '/';
   *work_ptr = '\0';
   *rescan_dir = NO;

   /*
    * Check if this is the special case when we have a dummy remote dir
    * and its queue is stopped. In this case it is just necessary to
    * move the files to the paused directory which is in tmp_file_dir
    * or visa versa.
    */
   if (use_afd_file_dir == YES)
   {
      tmp_file_dir[0] = '\0';
   }
   else
   {
      if (count_files == PAUSED_REMOTE)
      {
         (void)strcpy(tmp_file_dir, p_de->paused_dir);
         ptr = tmp_file_dir + strlen(tmp_file_dir);
         *ptr = '/';
         ptr++;
         *ptr = '\0';

         /* If remote paused directory does not exist, create it. */
         if ((stat(tmp_file_dir, &stat_buf) < 0) ||
             (S_ISDIR(stat_buf.st_mode) == 0))
         {
            /*
             * Only the AFD may read and write in this directory!
             */
#ifdef GROUP_CAN_WRITE
            if (mkdir(tmp_file_dir, (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP)) < 0)
#else
            if (mkdir(tmp_file_dir, (S_IRUSR | S_IWUSR | S_IXUSR)) < 0)
#endif
            {
               if (errno != EEXIST)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Could not mkdir() `%s' to save files : %s"),
                             tmp_file_dir, strerror(errno));
                  errno = 0;
                  return(INCORRECT);
               }
            }
         }
      }
      else
      {
         (void)strcpy(tmp_file_dir, p_de->dir);
         ptr = tmp_file_dir + strlen(tmp_file_dir);
         *ptr = '/';
         ptr++;
      }
   }

#ifdef _DEBUG
   system_log(DEBUG_SIGN, __FILE__, __LINE__, "Scanning: %s", fullname);
#endif
   if ((dp = opendir(fullname)) == NULL)
   {
      receive_log(ERROR_SIGN, __FILE__, __LINE__, current_time,
                  _("Failed to opendir() `%s' : %s"), fullname, strerror(errno));
      if (fra[p_de->fra_pos].fsa_pos == -1)
      {
         lock_region_w(fra_fd,
#ifdef LOCK_DEBUG
                       (char *)&fra[p_de->fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                       (char *)&fra[p_de->fra_pos].error_counter - (char *)fra);
#endif
         fra[p_de->fra_pos].error_counter += 1;
         if ((fra[p_de->fra_pos].error_counter >= fra[p_de->fra_pos].max_errors) &&
             ((fra[p_de->fra_pos].dir_flag & DIR_ERROR_SET) == 0))
         {
            fra[p_de->fra_pos].dir_flag |= DIR_ERROR_SET;
            SET_DIR_STATUS(fra[p_de->fra_pos].dir_flag,
                           current_time,
                           fra[p_de->fra_pos].start_event_handle,
                           fra[p_de->fra_pos].end_event_handle,
                           fra[p_de->fra_pos].dir_status);
            error_action(p_de->alias, "start", DIR_ERROR_ACTION);
            event_log(0L, EC_DIR, ET_EXT, EA_ERROR_START, "%s", p_de->alias);
         }
         unlock_region(fra_fd,
#ifdef LOCK_DEBUG
                       (char *)&fra[p_de->fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                       (char *)&fra[p_de->fra_pos].error_counter - (char *)fra);
#endif
      }
      return(INCORRECT);
   }

   /*
    * See if we have to wait for a certain file name before we
    * can check this directory.
    */
   if ((fra[p_de->fra_pos].wait_for_filename[0] != '\0') &&
       (count_files != NO))
   {
      int          gotcha = NO;
      unsigned int dummy_files_in_dir = 0;
      off_t        dummy_bytes_in_dir = 0;

      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] != '.')
         {
            (void)strcpy(work_ptr, p_dir->d_name);
            if (stat(fullname, &stat_buf) < 0)
            {
               if (errno != ENOENT)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             _("Can't stat() file `%s' : %s"),
                             fullname, strerror(errno));
               }
               continue;
            }

            if (fra[p_de->fra_pos].ignore_file_time != 0)
            {
               diff_time = current_time - stat_buf.st_mtime;
            }

            /* Sure it is a normal file? */
            if (S_ISREG(stat_buf.st_mode))
            {
               dummy_files_in_dir++;
               dummy_bytes_in_dir += stat_buf.st_size;
               if (((fra[p_de->fra_pos].ignore_size == 0) ||
                    ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_EQUAL) &&
                     (fra[p_de->fra_pos].ignore_size == stat_buf.st_size)) ||
                    ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_LESS_THEN) &&
                     (fra[p_de->fra_pos].ignore_size < stat_buf.st_size)) ||
                    ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_GREATER_THEN) &&
                     (fra[p_de->fra_pos].ignore_size > stat_buf.st_size))) &&
                   ((fra[p_de->fra_pos].ignore_file_time == 0) ||
                     ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_EQUAL) &&
                      (fra[p_de->fra_pos].ignore_file_time == diff_time)) ||
                    ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_LESS_THEN) &&
                     (fra[p_de->fra_pos].ignore_file_time < diff_time)) ||
                    ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_GREATER_THEN) &&
                     (fra[p_de->fra_pos].ignore_file_time > diff_time))))
               {
#ifdef _POSIX_SAVED_IDS
                  if ((stat_buf.st_mode & S_IROTH) ||
                      ((stat_buf.st_gid == afd_gid) && (stat_buf.st_mode & S_IRGRP)) ||
                      ((stat_buf.st_uid == afd_uid) && (stat_buf.st_mode & S_IRUSR)) ||
                      ((stat_buf.st_mode & S_IRGRP) && (no_of_sgids > 0) &&
                       (check_sgids(stat_buf.st_gid) == YES)))
#else
                  if ((eaccess(fullname, R_OK) == 0))
#endif
                  {
                     if ((ret = pmatch(fra[p_de->fra_pos].wait_for_filename,
                                       p_dir->d_name, &current_time)) == 0)
                     {
                        if ((fra[p_de->fra_pos].end_character == -1) ||
                            (fra[p_de->fra_pos].end_character == get_last_char(fullname, stat_buf.st_size)))
                        {
                           gotcha = YES;
                        }
                        else
                        {
                           if (fra[p_de->fra_pos].end_character != -1)
                           {
                              p_de->search_time -= 5;
                           }
                        }
                        break;
                     } /* If file in file mask. */
                  }
               }
            }
         }
      } /* while ((p_dir = readdir(dp)) != NULL) */

      if (gotcha == NO)
      {
         files_in_dir = dummy_files_in_dir;
         bytes_in_dir = dummy_bytes_in_dir;
         goto done;
      }
      else
      {
         rewinddir(dp);
      }
   }

   /*
    * See if we have to wait for a certain number of files or
    * total file size before we can check this directory.
    */
   if (((fra[p_de->fra_pos].accumulate != 0) ||
        (fra[p_de->fra_pos].accumulate_size != 0)) &&
       (count_files != NO))
   {
      int          gotcha = NO;
      unsigned int accumulate = 0,
                   dummy_files_in_dir = 0;
      off_t        accumulate_size = 0,
                   dummy_bytes_in_dir = 0;

      while (((p_dir = readdir(dp)) != NULL) && (gotcha == NO))
      {
         if (p_dir->d_name[0] != '.')
         {
            (void)strcpy(work_ptr, p_dir->d_name);
            if (stat(fullname, &stat_buf) < 0)
            {
               if (errno != ENOENT)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             _("Failed to stat() file `%s' : %s"),
                             fullname, strerror(errno));
               }
               continue;
            }

            if (fra[p_de->fra_pos].ignore_file_time != 0)
            {
               diff_time = current_time - stat_buf.st_mtime;
            }

            /* Sure it is a normal file? */
            if (S_ISREG(stat_buf.st_mode))
            {
               dummy_files_in_dir++;
               dummy_bytes_in_dir += stat_buf.st_size;
               if (((fra[p_de->fra_pos].ignore_size == 0) ||
                    ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_EQUAL) &&
                     (fra[p_de->fra_pos].ignore_size == stat_buf.st_size)) ||
                    ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_LESS_THEN) &&
                     (fra[p_de->fra_pos].ignore_size < stat_buf.st_size)) ||
                    ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_GREATER_THEN) &&
                     (fra[p_de->fra_pos].ignore_size > stat_buf.st_size))) &&
                   ((fra[p_de->fra_pos].ignore_file_time == 0) ||
                     ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_EQUAL) &&
                      (fra[p_de->fra_pos].ignore_file_time == diff_time)) ||
                    ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_LESS_THEN) &&
                     (fra[p_de->fra_pos].ignore_file_time < diff_time)) ||
                    ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_GREATER_THEN) &&
                     (fra[p_de->fra_pos].ignore_file_time > diff_time))))
               {
#ifdef _POSIX_SAVED_IDS
                  if ((stat_buf.st_mode & S_IROTH) ||
                      ((stat_buf.st_gid == afd_gid) && (stat_buf.st_mode & S_IRGRP)) ||
                      ((stat_buf.st_uid == afd_uid) && (stat_buf.st_mode & S_IRUSR)) ||
                      ((stat_buf.st_mode & S_IRGRP) && (no_of_sgids > 0) &&
                       (check_sgids(stat_buf.st_gid) == YES)))
#else
                  if ((eaccess(fullname, R_OK) == 0))
#endif
                  {
                     if (p_de->flag & ALL_FILES)
                     {
                        if ((fra[p_de->fra_pos].fsa_pos != -1) ||
                            (fra[p_de->fra_pos].stupid_mode == YES) ||
                            (fra[p_de->fra_pos].remove == YES) ||
                            (check_list(p_de, p_dir->d_name, &stat_buf) > -1))
                        {
                           if ((fra[p_de->fra_pos].end_character == -1) ||
                               (fra[p_de->fra_pos].end_character == get_last_char(fullname, stat_buf.st_size)))
                           {
                              if (fra[p_de->fra_pos].accumulate != 0)
                              {
                                 accumulate++;
                              }
                              if (fra[p_de->fra_pos].accumulate_size != 0)
                              {
                                 accumulate_size += stat_buf.st_size;
                              }
                              if ((accumulate >= fra[p_de->fra_pos].accumulate) ||
                                  (accumulate_size >= fra[p_de->fra_pos].accumulate_size))
                              {
                                 gotcha = YES;
                              }
                           }
                           else
                           {
                              if (fra[p_de->fra_pos].end_character != -1)
                              {
                                 p_de->search_time -= 5;
                              }
                           }
                        }
                     }
                     else
                     {
                        int j;

                        /* Filter out only those files we need for this directory. */
                        if (p_de->paused_dir == NULL)
                        {
                           pmatch_time = current_time;
                        }
                        else
                        {
                           pmatch_time = stat_buf.st_mtime;
                        }
                        for (i = 0; i < p_de->nfg; i++)
                        {
                           for (j = 0; ((j < p_de->fme[i].nfm) && (i < p_de->nfg)); j++)
                           {
                              if ((ret = pmatch(p_de->fme[i].file_mask[j],
                                                p_dir->d_name, &pmatch_time)) == 0)
                              {
                                 if ((fra[p_de->fra_pos].fsa_pos != -1) ||
                                     (fra[p_de->fra_pos].stupid_mode == YES) ||
                                     (fra[p_de->fra_pos].remove == YES) ||
                                     (check_list(p_de, p_dir->d_name, &stat_buf) > -1))
                                 {
                                    if ((fra[p_de->fra_pos].end_character == -1) ||
                                        (fra[p_de->fra_pos].end_character == get_last_char(fullname, stat_buf.st_size)))
                                    {
                                       if (fra[p_de->fra_pos].accumulate != 0)
                                       {
                                          accumulate++;
                                       }
                                       if (fra[p_de->fra_pos].accumulate_size != 0)
                                       {
                                          accumulate_size += stat_buf.st_size;
                                       }
                                       if ((accumulate >= fra[p_de->fra_pos].accumulate) ||
                                           (accumulate_size >= fra[p_de->fra_pos].accumulate_size))
                                       {
                                          gotcha = YES;
                                       }
                                    }
                                    else
                                    {
                                       if (fra[p_de->fra_pos].end_character != -1)
                                       {
                                          p_de->search_time -= 5;
                                       }
                                    }
                                 }

                                 /*
                                  * Since the file is in the file pool, it is not
                                  * necessary to test all other masks.
                                  */
                                 i = p_de->nfg;
                              } /* If file in file mask. */
                              else if (ret == 1)
                                   {
                                      /*
                                       * This file is definitely NOT wanted, for this
                                       * file group! However, the next file group
                                       * might state that it does want this file. So
                                       * only ignore all entries for this file group!
                                       */
                                      break;
                                   }
                           } /* for (j = 0; ((j < p_de->fme[i].nfm) && (i < p_de->nfg)); j++) */
                        } /* for (i = 0; i < p_de->nfg; i++) */
                     }
                  }
               }
            }
         }
      } /* while ((p_dir = readdir(dp)) != NULL) */

      if (gotcha == NO)
      {
         files_in_dir = dummy_files_in_dir;
         bytes_in_dir = dummy_bytes_in_dir;
         goto done;
      }
      else
      {
         rewinddir(dp);
      }
   }

   if (p_de->flag & ALL_FILES)
   {
      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] == '.')
         {
            errno = 0;
            continue;
         }

         (void)strcpy(work_ptr, p_dir->d_name);
         if (stat(fullname, &stat_buf) < 0)
         {
            if (errno != ENOENT)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          _("Failed to stat() file `%s' : %s"),
                          fullname, strerror(errno));
            }
            continue;
         }

         if (fra[p_de->fra_pos].ignore_file_time != 0)
         {
            diff_time = current_time - stat_buf.st_mtime;
         }

         /* Sure it is a normal file? */
         if (S_ISREG(stat_buf.st_mode))
         {
            files_in_dir++;
            bytes_in_dir += stat_buf.st_size;
            if ((count_files == NO) || /* Paused dir. */
                (((fra[p_de->fra_pos].ignore_size == 0) ||
                  ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_EQUAL) &&
                   (fra[p_de->fra_pos].ignore_size == stat_buf.st_size)) ||
                  ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_LESS_THEN) &&
                   (fra[p_de->fra_pos].ignore_size < stat_buf.st_size)) ||
                  ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_GREATER_THEN) &&
                   (fra[p_de->fra_pos].ignore_size > stat_buf.st_size))) &&
                 ((fra[p_de->fra_pos].ignore_file_time == 0) ||
                   ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_EQUAL) &&
                    (fra[p_de->fra_pos].ignore_file_time == diff_time)) ||
                  ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_LESS_THEN) &&
                   (fra[p_de->fra_pos].ignore_file_time < diff_time)) ||
                  ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_GREATER_THEN) &&
                   (fra[p_de->fra_pos].ignore_file_time > diff_time)))))
            {
#ifdef _POSIX_SAVED_IDS
               if ((stat_buf.st_mode & S_IROTH) ||
                   ((stat_buf.st_gid == afd_gid) && (stat_buf.st_mode & S_IRGRP)) ||
                   ((stat_buf.st_uid == afd_uid) && (stat_buf.st_mode & S_IRUSR)) ||
                   ((stat_buf.st_mode & S_IRGRP) && (no_of_sgids > 0) &&
                    (check_sgids(stat_buf.st_gid) == YES)))
#else
               if ((eaccess(fullname, R_OK) == 0))
#endif
               {
#ifdef WITH_DUP_CHECK
                  int is_duplicate = NO;

                  if ((fra[p_de->fra_pos].dup_check_timeout == 0L) ||
                      (((is_duplicate = isdup(fullname, stat_buf.st_size,
                                              p_de->dir_id,
                                              fra[p_de->fra_pos].dup_check_timeout,
                                              fra[p_de->fra_pos].dup_check_flag, NO)) == NO) ||
                       (((fra[p_de->fra_pos].dup_check_flag & DC_DELETE) == 0) &&
                        ((fra[p_de->fra_pos].dup_check_flag & DC_STORE) == 0))))
                  {
                     if ((is_duplicate == YES) &&
                         (fra[p_de->fra_pos].dup_check_flag & DC_WARN))
                     {
                        receive_log(WARN_SIGN, NULL, 0, current_time,
                                    _("File %s is duplicate."), p_dir->d_name);
                     }
#endif
                  rl_pos = -1;
                  if ((fra[p_de->fra_pos].fsa_pos != -1) ||
                      (fra[p_de->fra_pos].stupid_mode == YES) ||
                      (fra[p_de->fra_pos].remove == YES) ||
                      ((rl_pos = check_list(p_de, p_dir->d_name, &stat_buf)) > -1))
                  {
                     if ((fra[p_de->fra_pos].end_character == -1) ||
                         (fra[p_de->fra_pos].end_character == get_last_char(fullname, stat_buf.st_size)))
                     {
                        if (tmp_file_dir[0] == '\0')
                        {
                           (void)strcpy(tmp_file_dir, afd_file_dir);
                           (void)strcpy(tmp_file_dir + afd_file_dir_length,
                                        AFD_TMP_DIR);
                           ptr = tmp_file_dir + afd_file_dir_length + AFD_TMP_DIR_LENGTH;
                           *(ptr++) = '/';
                           *ptr = '\0';

                           /* Create a unique name. */
                           next_counter_no_lock(unique_number);
                           if (create_name(tmp_file_dir, NO_PRIORITY,
                                           current_time, p_de->dir_id,
                                           &split_job_counter, unique_number,
                                           ptr, -1) < 0)
                           {
                              if (errno == ENOSPC)
                              {
                                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                                            _("DISK FULL!!! Will retry in %d second interval."),
                                            DISK_FULL_RESCAN_TIME);

                                 while (errno == ENOSPC)
                                 {
                                    (void)sleep(DISK_FULL_RESCAN_TIME);
                                    errno = 0;
                                    next_counter_no_lock(unique_number);
                                    if (create_name(tmp_file_dir, NO_PRIORITY,
                                                    current_time, p_de->dir_id,
                                                    &split_job_counter,
                                                    unique_number, ptr, -1) < 0)
                                    {
                                       if (errno != ENOSPC)
                                       {
                                          system_log(FATAL_SIGN, __FILE__, __LINE__,
                                                     _("Failed to create a unique name : %s"),
                                                     strerror(errno));
                                          exit(INCORRECT);
                                       }
                                    }
                                 }
                                 system_log(INFO_SIGN, __FILE__, __LINE__,
                                            _("Continuing after disk was full."));

                                 /*
                                  * If the disk is full, lets stop copying/moving
                                  * files so we can send data as quickly as possible
                                  * to get more free disk space.
                                  */
                                 break;
                              }
                              else
                              {
                                 system_log(FATAL_SIGN, __FILE__, __LINE__,
                                            _("Failed to create a unique name : %s"),
                                            strerror(errno));
                                 exit(INCORRECT);
                              }
                           }

                           while (*ptr != '\0')
                           {
                              ptr++;
                           }
                           *(ptr++) = '/';
                           *ptr = '\0';
                        } /* if (tmp_file_dir[0] == '\0') */

                        /* Generate name for the new file. */
                        (void)strcpy(ptr, p_dir->d_name);

                        if ((fra[p_de->fra_pos].remove == YES) ||
                            (count_files == NO) ||  /* Paused directory. */
                            (fra[p_de->fra_pos].protocol != LOC))
                        {
                           if (p_de->flag & IN_SAME_FILESYSTEM)
                           {
                              ret = move_file(fullname, tmp_file_dir);
                           }
                           else
                           {
                              if ((ret = copy_file(fullname, tmp_file_dir,
                                                   &stat_buf)) == SUCCESS)
                              {
                                 if ((ret = unlink(fullname)) == -1)
                                 {
                                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                                               _("Failed to unlink() file `%s' : %s"),
                                               fullname, strerror(errno));

                                    if (errno == ENOENT)
                                    {
                                       ret = SUCCESS;
                                    }
                                    else
                                    {
                                       /*
                                        * Delete target file otherwise we
                                        * might end up in an endless loop.
                                        */
                                       (void)unlink(tmp_file_dir);
                                    }
                                 }
                              }
                           }
                        }
                        else
                        {
                           /* Leave original files in place. */
                           ret = copy_file(fullname, tmp_file_dir, &stat_buf);
                        }
                        if (ret != SUCCESS)
                        {
                           receive_log(ERROR_SIGN, __FILE__, __LINE__, current_time,
                                       _("Failed to move/copy file `%s' to `%s' : %s"),
                                       fullname, tmp_file_dir, strerror(errno));
                           lock_region_w(fra_fd,
#ifdef LOCK_DEBUG
                                         (char *)&fra[p_de->fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                                         (char *)&fra[p_de->fra_pos].error_counter - (char *)fra);
#endif
                           fra[p_de->fra_pos].error_counter += 1;
                           if ((fra[p_de->fra_pos].error_counter >= fra[p_de->fra_pos].max_errors) &&
                               ((fra[p_de->fra_pos].dir_flag & DIR_ERROR_SET) == 0))
                           {
                              fra[p_de->fra_pos].dir_flag |= DIR_ERROR_SET;
                              SET_DIR_STATUS(fra[p_de->fra_pos].dir_flag,
                                             current_time,
                                             fra[p_de->fra_pos].start_event_handle,
                                             fra[p_de->fra_pos].end_event_handle,
                                             fra[p_de->fra_pos].dir_status);
                              error_action(p_de->alias, "start", DIR_ERROR_ACTION);
                              event_log(0L, EC_DIR, ET_EXT, EA_ERROR_START, "%s", p_de->alias);
                           }
                           unlock_region(fra_fd,
#ifdef LOCK_DEBUG
                                         (char *)&fra[p_de->fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                                         (char *)&fra[p_de->fra_pos].error_counter - (char *)fra);
#endif
                           set_error_counter = YES;

#ifdef WITH_DUP_CHECK
                           /*
                            * We have already stored the CRC value for
                            * this file but failed pick up the file.
                            * So we must remove the CRC value!
                            */
                           if ((fra[p_de->fra_pos].dup_check_timeout > 0L) &&
                               (is_duplicate == NO))
                           {
                              (void)isdup(fullname, stat_buf.st_size,
                                          p_de->dir_id,
                                          fra[p_de->fra_pos].dup_check_timeout,
                                          fra[p_de->fra_pos].dup_check_flag, YES);
                           }
#endif
                        }
                        else
                        {
                           /* Store file name of file that has just been  */
                           /* moved. So one does not always have to walk  */
                           /* with the directory pointer through all      */
                           /* files every time we want to know what files */
                           /* are available. Hope this will reduce the    */
                           /* system time of the process dir_check.       */
#ifndef _WITH_PTHREAD
                           if ((files_copied + 1) > max_file_buffer)
                           {
# ifdef _DISTRIBUTION_LOG
                              int          k, m;
                              size_t       tmp_val;
                              unsigned int prev_max_file_buffer = max_file_buffer;
# endif

                              if ((files_copied + 1) > fra[p_de->fra_pos].max_copied_files)
                              {
                                 system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                            _("Hmmm, files_copied %d is larger then max_copied_files %u."),
                                            files_copied + 1,
                                            fra[p_de->fra_pos].max_copied_files);
                                 max_file_buffer = files_copied + 1;
                              }
                              else
                              {
                                 if ((max_file_buffer + FILE_BUFFER_STEP_SIZE) >= fra[p_de->fra_pos].max_copied_files)
                                 {
                                    max_file_buffer = fra[p_de->fra_pos].max_copied_files;
                                 }
                                 else
                                 {
                                    max_file_buffer += FILE_BUFFER_STEP_SIZE;
                                 }
                              }
                              REALLOC_RT_ARRAY(file_name_pool, max_file_buffer,
                                               MAX_FILENAME_LENGTH, char);
                              if ((file_length_pool = realloc(file_length_pool,
                                                              max_file_buffer * sizeof(unsigned char))) == NULL)
                              {
                                 system_log(FATAL_SIGN, __FILE__, __LINE__,
                                            _("realloc() error : %s"),
                                            strerror(errno));
                                 exit(INCORRECT);
                              }
                              if ((file_mtime_pool = realloc(file_mtime_pool,
                                                             max_file_buffer * sizeof(off_t))) == NULL)
                              {
                                 system_log(FATAL_SIGN, __FILE__, __LINE__,
                                            _("realloc() error : %s"),
                                            strerror(errno));
                                 exit(INCORRECT);
                              }
                              if ((file_size_pool = realloc(file_size_pool,
                                                            max_file_buffer * sizeof(off_t))) == NULL)
                              {
                                 system_log(FATAL_SIGN, __FILE__, __LINE__,
                                            _("realloc() error : %s"),
                                            strerror(errno));
                                 exit(INCORRECT);
                              }
# ifdef _DISTRIBUTION_LOG
#  ifdef RT_ARRAY_STRUCT_WORKING
                              REALLOC_RT_ARRAY(file_dist_pool, max_file_buffer,
                                               NO_OF_DISTRIBUTION_TYPES,
                                               struct file_dist_list);
#  else
                              if ((file_dist_pool = (struct file_dist_list **)realloc(file_dist_pool, max_file_buffer * NO_OF_DISTRIBUTION_TYPES * sizeof(struct file_dist_list *))) == NULL)
                              {
                                 system_log(FATAL_SIGN, __FILE__, __LINE__,
                                            _("realloc() error : %s"),
                                            strerror(errno));
                                 exit(INCORRECT);
                              }
                              if ((file_dist_pool[0] = (struct file_dist_list *)realloc(file_dist_pool[0], max_file_buffer * NO_OF_DISTRIBUTION_TYPES * sizeof(struct file_dist_list))) == NULL)
                              {
                                 system_log(FATAL_SIGN, __FILE__, __LINE__,
                                            _("realloc() error : %s"),
                                            strerror(errno));
                                 exit(INCORRECT);
                              }
                              for (k = 0; k < max_file_buffer; k++)
                              {
                                 file_dist_pool[k] = file_dist_pool[0] + (k * NO_OF_DISTRIBUTION_TYPES);
                              }
#  endif
                              tmp_val = max_jobs_per_file * sizeof(unsigned int);
                              for (k = prev_max_file_buffer; k < max_file_buffer; k++)
                              {
                                 for (m = 0; m < NO_OF_DISTRIBUTION_TYPES; m++)
                                 {
                                    if (((file_dist_pool[k][m].jid_list = malloc(tmp_val)) == NULL) ||
                                        ((file_dist_pool[k][m].proc_cycles = malloc(max_jobs_per_file)) == NULL))
                                    {
                                       system_log(FATAL_SIGN, __FILE__, __LINE__,
                                                  _("malloc() error : %s"),
                                                  strerror(errno));
                                       exit(INCORRECT);
                                    }
                                    file_dist_pool[k][m].no_of_dist = 0;
                                 }
                              }
# endif
                           }
#endif
                           if (rl_pos > -1)
                           {
                              p_de->rl[rl_pos].retrieved = YES;
                           }
                           file_length_pool[files_copied] = strlen(p_dir->d_name);
                           (void)memcpy(file_name_pool[files_copied],
                                        p_dir->d_name,
                                        (size_t)(file_length_pool[files_copied] + 1));
                           file_mtime_pool[files_copied] = stat_buf.st_mtime;
                           file_size_pool[files_copied] = stat_buf.st_size;

#ifdef _INPUT_LOG
                           if ((count_files == YES) ||
                               (count_files == PAUSED_REMOTE))
                           {
                              /* Log the file name in the input log. */
                              (void)memcpy(il_file_name, p_dir->d_name,
                                           (size_t)(file_length_pool[files_copied] + 1));
                              *il_file_size = stat_buf.st_size;
                              *il_time = current_time;
                              *il_dir_number = p_de->dir_id;
                              *il_unique_number = *unique_number;
                              il_real_size = (size_t)file_length_pool[files_copied] + il_size;
                              if (write(il_fd, il_data, il_real_size) != il_real_size)
                              {
                                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                                            _("write() error : %s"),
                                            strerror(errno));

                                 /*
                                  * Since the input log is not critical, no
                                  * need to exit here.
                                  */
                              }
                           }
#endif

                           *total_file_size += stat_buf.st_size;
                           if ((++files_copied >= fra[p_de->fra_pos].max_copied_files) ||
                               (*total_file_size >= fra[p_de->fra_pos].max_copied_file_size))
                           {
                              break;
                           }
                        }
                     }
                     else
                     {
                        if (fra[p_de->fra_pos].end_character != -1)
                        {
                           p_de->search_time -= 5;
                        }
                     }
                  }
#ifdef WITH_DUP_CHECK
                  }
                  else
                  {
                     if (is_duplicate == YES)
                     {
# ifdef _INPUT_LOG
                        if ((count_files == YES) ||
                            (count_files == PAUSED_REMOTE))
                        {
                           /* Log the file name in the input log. */
                           (void)strcpy(il_file_name, p_dir->d_name);
                           *il_file_size = stat_buf.st_size;
                           *il_time = current_time;
                           *il_dir_number = p_de->dir_id;
                           *il_unique_number = *unique_number;
                           il_real_size = strlen(p_dir->d_name) + il_size;
                           if (write(il_fd, il_data, il_real_size) != il_real_size)
                           {
                              system_log(ERROR_SIGN, __FILE__, __LINE__,
                                         _("write() error : %s"),
                                         strerror(errno));

                              /*
                               * Since the input log is not critical, no
                               * need to exit here.
                               */
                           }
                        }
# endif
                        if (fra[p_de->fra_pos].dup_check_flag & DC_DELETE)
                        {
                           if (unlink(fullname) == -1)
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                         _("Failed to unlink() `%s' : %s"),
                                         fullname, strerror(errno));
                           }
                           else
                           {
# ifdef _DELETE_LOG
                              size_t        dl_real_size;
# endif
# ifdef _DISTRIBUTION_LOG
                              unsigned int  dummy_job_id,
                                            *p_dummy_job_id;
                              unsigned char dummy_proc_cycles;

                              dummy_job_id = 0;
                              p_dummy_job_id = &dummy_job_id;
                              dummy_proc_cycles = 0;
                              dis_log(DUPCHECK_DIS_TYPE, current_time,
                                      p_de->dir_id, *unique_number,
                                      p_dir->d_name, strlen(p_dir->d_name),
                                      stat_buf.st_size, 1, &p_dummy_job_id,
                                      &dummy_proc_cycles, 1);
# endif
# ifdef _DELETE_LOG
                              (void)strcpy(dl.file_name, p_dir->d_name);
                              (void)sprintf(dl.host_name, "%-*s %03x",
                                            MAX_HOSTNAME_LENGTH, "-",
                                            DUP_INPUT);
                              *dl.file_size = stat_buf.st_size;
                              *dl.dir_id = p_de->dir_id;
                              *dl.job_id = 0;
                              *dl.input_time = current_time;
                              *dl.split_job_counter = split_job_counter;
                              *dl.unique_number = *unique_number;
#  ifdef _INPUT_LOG
                              if ((count_files == YES) ||
                                  (count_files == PAUSED_REMOTE))
                              {
                                 *dl.file_name_length = il_real_size - il_size;
                              }
                              else
                              {
                                 *dl.file_name_length = strlen(p_dir->d_name);
                              }
#  else
                              *dl.file_name_length = strlen(p_dir->d_name);
#  endif
                              dl_real_size = *dl.file_name_length + dl.size +
                                             sprintf((dl.file_name + *dl.file_name_length + 1),
                                                     "%s%c(%s %d)",
                                                     DIR_CHECK, SEPARATOR_CHAR,
                                                     __FILE__, __LINE__);
                              if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                              {
                                 system_log(ERROR_SIGN, __FILE__, __LINE__,
                                            _("write() error : %s"),
                                            strerror(errno));
                              }
# endif
                              files_in_dir--;
                              bytes_in_dir -= stat_buf.st_size;
                           }
                        }
                        else if (fra[p_de->fra_pos].dup_check_flag & DC_STORE)
                             {
                                char *p_end,
                                     save_dir[MAX_PATH_LENGTH];

                                p_end = save_dir +
                                        sprintf(save_dir, "%s%s%s/%x/",
                                                p_work_dir, AFD_FILE_DIR,
                                                STORE_DIR, p_de->dir_id);
                                if ((mkdir(save_dir, DIR_MODE) == -1) &&
                                    (errno != EEXIST))
                                {
                                   system_log(ERROR_SIGN, __FILE__, __LINE__,
                                              _("Failed to mkdir() `%s' : %s"),
                                              save_dir, strerror(errno));
                                   (void)unlink(fullname);
                                }
                                else
                                {
                                   (void)strcpy(p_end, p_dir->d_name);
                                   if (rename(fullname, save_dir) == -1)
                                   {
                                      system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                 _("Failed to rename() `%s' to `%s' : %s"),
                                                 fullname, save_dir,
                                                 strerror(errno));
                                      (void)unlink(fullname);
                                   }
                                }
                                files_in_dir--;
                                bytes_in_dir -= stat_buf.st_size;
                             }
                        if (fra[p_de->fra_pos].dup_check_flag & DC_WARN)
                        {
                           receive_log(WARN_SIGN, NULL, 0, current_time,
                                       _("File %s is duplicate."), p_dir->d_name);
                        }
                     }
                  }
#endif /* WITH_DUP_CHECK */
               }
            }
            else
            {
               if ((fra[p_de->fra_pos].delete_files_flag & UNKNOWN_FILES) &&
                   ((fra[p_de->fra_pos].ignore_size != 0) ||
                    ((fra[p_de->fra_pos].ignore_file_time != 0) &&
                     ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_GREATER_THEN) ||
                      (fra[p_de->fra_pos].gt_lt_sign & IFTIME_EQUAL)))) &&
                   ((current_time - stat_buf.st_mtime) > fra[p_de->fra_pos].unknown_file_time))
               {
                  if (unlink(fullname) == -1)
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                _("Failed to unlink() `%s' : %s"),
                                fullname, strerror(errno));
                  }
                  else
                  {
#ifdef _DELETE_LOG
                     size_t dl_real_size;

                     (void)strcpy(dl.file_name, p_dir->d_name);
                     (void)sprintf(dl.host_name, "%-*s %03x",
                                   MAX_HOSTNAME_LENGTH, "-",
                                   DEL_UNKNOWN_FILE);
                     *dl.file_size = stat_buf.st_size;
                     *dl.dir_id = p_de->dir_id;
                     *dl.job_id = 0;
                     *dl.input_time = 0L;
                     *dl.split_job_counter = 0;
                     *dl.unique_number = 0;
                     *dl.file_name_length = strlen(p_dir->d_name);
                     dl_real_size = *dl.file_name_length + dl.size +
                                    sprintf((dl.file_name + *dl.file_name_length + 1),
# if SIZEOF_TIME_T == 4
                                            "%s%c>%ld (%s %d)",
# else
                                            "%s%c>%lld (%s %d)",
# endif
                                            DIR_CHECK, SEPARATOR_CHAR,
                                            (pri_time_t)(current_time - stat_buf.st_mtime),
                                            __FILE__, __LINE__);
                     if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   _("write() error : %s"), strerror(errno));
                     }
#endif
                     files_in_dir--;
                     bytes_in_dir -= stat_buf.st_size;
                  }
               }
               else if (((fra[p_de->fra_pos].ignore_file_time != 0) &&
                         (((fra[p_de->fra_pos].gt_lt_sign & IFTIME_LESS_THEN) &&
                           (diff_time <= fra[p_de->fra_pos].ignore_file_time)) ||
                          ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_EQUAL) &&
                           (diff_time < fra[p_de->fra_pos].ignore_file_time)))) ||
                        ((fra[p_de->fra_pos].ignore_size != 0) &&
                         ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_LESS_THEN) ||
                          (fra[p_de->fra_pos].gt_lt_sign & ISIZE_EQUAL)) &&
                         (stat_buf.st_size < fra[p_de->fra_pos].ignore_size)))
                    {
                       *rescan_dir = YES;
                    }
            }
         } /* if (S_ISREG(stat_buf.st_mode)) */
         errno = 0;
      } /* while ((p_dir = readdir(dp)) != NULL) */
   }
   else
   {
      int j;

      while ((p_dir = readdir(dp)) != NULL)
      {
         if (p_dir->d_name[0] == '.')
         {
            errno = 0;
            continue;
         }

         (void)strcpy(work_ptr, p_dir->d_name);
         if (stat(fullname, &stat_buf) < 0)
         {
            if (errno != ENOENT)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          _("Failed to stat() `%s' : %s"),
                          fullname, strerror(errno));
            }
            continue;
         }

         if (fra[p_de->fra_pos].ignore_file_time != 0)
         {
            diff_time = current_time - stat_buf.st_mtime;
         }

         /* Sure it is a normal file? */
         if (S_ISREG(stat_buf.st_mode))
         {
            files_in_dir++;
            bytes_in_dir += stat_buf.st_size;
            if ((count_files == NO) || /* Paused dir. */
                (((fra[p_de->fra_pos].ignore_size == 0) ||
                  ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_EQUAL) &&
                   (fra[p_de->fra_pos].ignore_size == stat_buf.st_size)) ||
                  ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_LESS_THEN) &&
                   (fra[p_de->fra_pos].ignore_size < stat_buf.st_size)) ||
                  ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_GREATER_THEN) &&
                   (fra[p_de->fra_pos].ignore_size > stat_buf.st_size))) &&
                 ((fra[p_de->fra_pos].ignore_file_time == 0) ||
                   ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_EQUAL) &&
                    (fra[p_de->fra_pos].ignore_file_time == diff_time)) ||
                  ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_LESS_THEN) &&
                   (fra[p_de->fra_pos].ignore_file_time < diff_time)) ||
                  ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_GREATER_THEN) &&
                   (fra[p_de->fra_pos].ignore_file_time > diff_time)))))
            {
#ifdef _POSIX_SAVED_IDS
               if ((stat_buf.st_mode & S_IROTH) ||
                   ((stat_buf.st_gid == afd_gid) && (stat_buf.st_mode & S_IRGRP)) ||
                   ((stat_buf.st_uid == afd_uid) && (stat_buf.st_mode & S_IRUSR)) ||
                   ((stat_buf.st_mode & S_IRGRP) && (no_of_sgids > 0) &&
                    (check_sgids(stat_buf.st_gid) == YES)))
#else
               if ((eaccess(fullname, R_OK) == 0))
#endif
               {
                  int gotcha = NO;

                  /* Filter out only those files we need for this directory. */
                  if (p_de->paused_dir == NULL)
                  {
                     pmatch_time = current_time;
                  }
                  else
                  {
                     pmatch_time = stat_buf.st_mtime;
                  }
                  for (i = 0; i < p_de->nfg; i++)
                  {
                     for (j = 0; ((j < p_de->fme[i].nfm) && (i < p_de->nfg)); j++)
                     {
                        if ((ret = pmatch(p_de->fme[i].file_mask[j],
                                          p_dir->d_name, &pmatch_time)) == 0)
                        {
#ifdef WITH_DUP_CHECK
                           int is_duplicate = NO;

                           if ((fra[p_de->fra_pos].dup_check_timeout == 0L) ||
                               (((is_duplicate = isdup(fullname,
                                                       stat_buf.st_size,
                                                       p_de->dir_id,
                                                       fra[p_de->fra_pos].dup_check_timeout,
                                                       fra[p_de->fra_pos].dup_check_flag, NO)) == NO) ||
                                (((fra[p_de->fra_pos].dup_check_flag & DC_DELETE) == 0) &&
                                 ((fra[p_de->fra_pos].dup_check_flag & DC_STORE) == 0))))
                           {
                              if ((is_duplicate == YES) &&
                                  (fra[p_de->fra_pos].dup_check_flag & DC_WARN))
                              {
                                 receive_log(WARN_SIGN, NULL, 0, current_time,
                                             _("File %s is duplicate."),
                                             p_dir->d_name);
                              }
#endif
                           rl_pos = -1;
                           if ((fra[p_de->fra_pos].fsa_pos != -1) ||
                               (fra[p_de->fra_pos].stupid_mode == YES) ||
                               (fra[p_de->fra_pos].remove == YES) ||
                               ((rl_pos = check_list(p_de, p_dir->d_name, &stat_buf)) > -1))
                           {
                              if ((fra[p_de->fra_pos].end_character == -1) ||
                                  (fra[p_de->fra_pos].end_character == get_last_char(fullname, stat_buf.st_size)))
                              {
                                 if (tmp_file_dir[0] == '\0')
                                 {
                                    (void)strcpy(tmp_file_dir, afd_file_dir);
                                    (void)strcpy(tmp_file_dir + afd_file_dir_length, AFD_TMP_DIR);
                                    ptr = tmp_file_dir + afd_file_dir_length + AFD_TMP_DIR_LENGTH;
                                    *(ptr++) = '/';
                                    *ptr = '\0';

                                    /* Create a unique name. */
                                    next_counter_no_lock(unique_number);
                                    if (create_name(tmp_file_dir, NO_PRIORITY,
                                                    current_time, p_de->dir_id,
                                                    &split_job_counter,
                                                    unique_number, ptr, -1) < 0)
                                    {
                                       if (errno == ENOSPC)
                                       {
                                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                     _("DISK FULL!!! Will retry in %d second interval."),
                                                     DISK_FULL_RESCAN_TIME);

                                          while (errno == ENOSPC)
                                          {
                                             (void)sleep(DISK_FULL_RESCAN_TIME);
                                             errno = 0;
                                             next_counter_no_lock(unique_number);
                                             if (create_name(tmp_file_dir,
                                                             NO_PRIORITY,
                                                             current_time,
                                                             p_de->dir_id,
                                                             &split_job_counter,
                                                             unique_number,
                                                             ptr, -1) < 0)
                                             {
                                                if (errno != ENOSPC)
                                                {
                                                   system_log(FATAL_SIGN, __FILE__, __LINE__,
                                                              _("Failed to create a unique name in %s : %s"),
                                                              tmp_file_dir,
                                                              strerror(errno));
                                                   exit(INCORRECT);
                                                }
                                             }
                                          }
                                          system_log(INFO_SIGN, __FILE__, __LINE__,
                                                     _("Continuing after disk was full."));

                                          /*
                                           * If the disk is full, lets stop copying/moving
                                           * files so we can send data as quickly as
                                           * possible to get more free disk space.
                                           */
                                          goto done;
                                       }
                                       else
                                       {
                                          system_log(FATAL_SIGN, __FILE__, __LINE__,
                                                     _("Failed to create a unique name : %s"),
                                                     strerror(errno));
                                          exit(INCORRECT);
                                       }
                                    }

                                    while (*ptr != '\0')
                                    {
                                      ptr++;
                                    }
                                    *(ptr++) = '/';
                                    *ptr = '\0';
                                 }

                                 /* Generate name for the new file. */
                                 (void)strcpy(ptr, p_dir->d_name);
                                 if ((fra[p_de->fra_pos].remove == YES) ||
                                     (count_files == NO) || /* Paused directory. */
                                     (fra[p_de->fra_pos].protocol != LOC))
                                 {
                                    if (p_de->flag & IN_SAME_FILESYSTEM)
                                    {
                                       ret = move_file(fullname, tmp_file_dir);
                                    }
                                    else
                                    {
                                       if ((ret = copy_file(fullname,
                                                            tmp_file_dir,
                                                            &stat_buf)) == SUCCESS)
                                       {
                                          if (unlink(fullname) == -1)
                                          {
                                             system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                        _("Failed to unlink() file `%s' : %s"),
                                                        fullname, strerror(errno));

                                             if (errno == ENOENT)
                                             {
                                                ret = SUCCESS;
                                             }
                                             else
                                             {
                                                /*
                                                 * Delete target file otherwise
                                                 * we might end up in an
                                                 * endless loop.
                                                 */
                                                (void)unlink(tmp_file_dir);
                                             }
                                          }
                                       }
                                    }
                                 }
                                 else
                                 {
                                    /* Leave original files in place. */
                                    ret = copy_file(fullname, tmp_file_dir, &stat_buf);
                                 }
                                 if (ret != SUCCESS)
                                 {
                                    receive_log(ERROR_SIGN, __FILE__, __LINE__, current_time,
                                                _("Failed to move/copy file `%s' to `%s' (%d) : %s"),
                                                fullname, tmp_file_dir, ret,
                                                strerror(errno));
                                    lock_region_w(fra_fd,
#ifdef LOCK_DEBUG
                                                  (char *)&fra[p_de->fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                                                  (char *)&fra[p_de->fra_pos].error_counter - (char *)fra);
#endif
                                    fra[p_de->fra_pos].error_counter += 1;
                                    if ((fra[p_de->fra_pos].error_counter >= fra[p_de->fra_pos].max_errors) &&
                                        ((fra[p_de->fra_pos].dir_flag & DIR_ERROR_SET) == 0))
                                    {
                                       fra[p_de->fra_pos].dir_flag |= DIR_ERROR_SET;
                                       SET_DIR_STATUS(fra[p_de->fra_pos].dir_flag,
                                                      current_time,
                                                      fra[p_de->fra_pos].start_event_handle,
                                                      fra[p_de->fra_pos].end_event_handle,
                                                      fra[p_de->fra_pos].dir_status);
                                       error_action(p_de->alias, "start", DIR_ERROR_ACTION);
                                       event_log(0L, EC_DIR, ET_EXT, EA_ERROR_START, "%s", p_de->alias);
                                    }
                                    unlock_region(fra_fd,
#ifdef LOCK_DEBUG
                                                  (char *)&fra[p_de->fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                                                  (char *)&fra[p_de->fra_pos].error_counter - (char *)fra);
#endif
                                    set_error_counter = YES;

#ifdef WITH_DUP_CHECK
                                    /*
                                     * We have already stored the CRC value for
                                     * this file but failed pick up the file.
                                     * So we must remove the CRC value!
                                     */
                                    if ((fra[p_de->fra_pos].dup_check_timeout > 0L) &&
                                        (is_duplicate == NO))
                                    {
                                       (void)isdup(fullname, stat_buf.st_size,
                                                   p_de->dir_id,
                                                   fra[p_de->fra_pos].dup_check_timeout,
                                                   fra[p_de->fra_pos].dup_check_flag, YES);

                                       /*
                                        * We must set gotcha to yes, otherwise
                                        * the file will be deleted because
                                        * we then think it is something
                                        * not to be distributed.
                                        */
                                       gotcha = YES;
                                    }
#endif
                                 }
                                 else
                                 {
                                    /* Store file name of file that has just been  */
                                    /* moved. So one does not always have to walk  */
                                    /* with the directory pointer through all      */
                                    /* files every time we want to know what files */
                                    /* are available. Hope this will reduce the    */
                                    /* system time of the process dir_check.       */
#ifndef _WITH_PTHREAD
                                    if ((files_copied + 1) > max_file_buffer)
                                    {
# ifdef _DISTRIBUTION_LOG
                                       int          k, m;
                                       size_t       tmp_val;
                                       unsigned int prev_max_file_buffer = max_file_buffer;
# endif

                                       if ((files_copied + 1) > fra[p_de->fra_pos].max_copied_files)
                                       {
                                          system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                                     _("Hmmm, files_copied %d is larger then max_copied_files %u."),
                                                     files_copied + 1,
                                                     fra[p_de->fra_pos].max_copied_files);
                                          max_file_buffer = files_copied + 1;
                                       }
                                       else
                                       {
                                          if ((max_file_buffer + FILE_BUFFER_STEP_SIZE) >= fra[p_de->fra_pos].max_copied_files)
                                          {
                                             max_file_buffer = fra[p_de->fra_pos].max_copied_files;
                                          }
                                          else
                                          {
                                             max_file_buffer += FILE_BUFFER_STEP_SIZE;
                                          }
                                       }
                                       REALLOC_RT_ARRAY(file_name_pool, max_file_buffer,
                                                        MAX_FILENAME_LENGTH, char);
                                       if ((file_length_pool = realloc(file_length_pool,
                                                                       max_file_buffer * sizeof(unsigned char))) == NULL)
                                       {
                                          system_log(FATAL_SIGN, __FILE__, __LINE__,
                                                     _("realloc() error : %s"),
                                                     strerror(errno));
                                          exit(INCORRECT);
                                       }
                                       if ((file_mtime_pool = realloc(file_mtime_pool,
                                                                      max_file_buffer * sizeof(off_t))) == NULL)
                                       {
                                          system_log(FATAL_SIGN, __FILE__, __LINE__,
                                                     _("realloc() error : %s"),
                                                     strerror(errno));
                                          exit(INCORRECT);
                                       }
                                       if ((file_size_pool = realloc(file_size_pool,
                                                                     max_file_buffer * sizeof(off_t))) == NULL)
                                       {
                                          system_log(FATAL_SIGN, __FILE__, __LINE__,
                                                     _("realloc() error : %s"),
                                                     strerror(errno));
                                          exit(INCORRECT);
                                       }
# ifdef _DISTRIBUTION_LOG
#  ifdef RT_ARRAY_STRUCT_WORKING
                                       REALLOC_RT_ARRAY(file_dist_pool, max_file_buffer,
                                                        NO_OF_DISTRIBUTION_TYPES,
                                                        struct file_dist_list);
#  else
                                       if ((file_dist_pool = (struct file_dist_list **)realloc(file_dist_pool, max_file_buffer * NO_OF_DISTRIBUTION_TYPES * sizeof(struct file_dist_list *))) == NULL)
                                       {
                                          system_log(FATAL_SIGN, __FILE__, __LINE__,
                                                     _("realloc() error : %s"),
                                                     strerror(errno));
                                          exit(INCORRECT);
                                       }
                                       if ((file_dist_pool[0] = (struct file_dist_list *)realloc(file_dist_pool[0], max_file_buffer * NO_OF_DISTRIBUTION_TYPES * sizeof(struct file_dist_list))) == NULL)
                                       {
                                          system_log(FATAL_SIGN, __FILE__, __LINE__,
                                                     _("realloc() error : %s"),
                                                     strerror(errno));
                                          exit(INCORRECT);
                                       }
                                       for (k = 0; k < max_file_buffer; k++)
                                       {
                                          file_dist_pool[k] = file_dist_pool[0] + (k * NO_OF_DISTRIBUTION_TYPES);
                                       }
#  endif
                                       tmp_val = max_jobs_per_file * sizeof(unsigned int);
                                       for (k = prev_max_file_buffer; k < max_file_buffer; k++)
                                       {
                                          for (m = 0; m < NO_OF_DISTRIBUTION_TYPES; m++)
                                          {
                                             if (((file_dist_pool[k][m].jid_list = malloc(tmp_val)) == NULL) ||
                                                 ((file_dist_pool[k][m].proc_cycles = malloc(max_jobs_per_file)) == NULL))
                                             {
                                                system_log(FATAL_SIGN, __FILE__, __LINE__,
                                                           _("malloc() error : %s"),
                                                           strerror(errno));
                                                exit(INCORRECT);
                                             }
                                             file_dist_pool[k][m].no_of_dist = 0;
                                          }
                                       }
# endif
                                    }
#endif
                                    file_length_pool[files_copied] = strlen(p_dir->d_name);
                                    (void)memcpy(file_name_pool[files_copied],
                                                 p_dir->d_name,
                                                 (size_t)(file_length_pool[files_copied] + 1));
                                    file_mtime_pool[files_copied] = stat_buf.st_mtime;
                                    file_size_pool[files_copied] = stat_buf.st_size;
                                    if (rl_pos > -1)
                                    {
                                       p_de->rl[rl_pos].retrieved = YES;
                                    }

#ifdef _INPUT_LOG
                                    if ((count_files == YES) ||
                                        (count_files == PAUSED_REMOTE))
                                    {
                                       /* Log the file name in the input log. */
                                       (void)memcpy(il_file_name, p_dir->d_name,
                                                    (size_t)(file_length_pool[files_copied] + 1));
                                       *il_file_size = stat_buf.st_size;
                                       *il_time = current_time;
                                       *il_dir_number = p_de->dir_id;
                                       *il_unique_number = *unique_number;
                                       il_real_size = (size_t)file_length_pool[files_copied] + il_size;
                                       if (write(il_fd, il_data, il_real_size) != il_real_size)
                                       {
                                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                     _("write() error : %s"),
                                                     strerror(errno));

                                          /*
                                           * Since the input log is not critical, no need to
                                           * exit here.
                                           */
                                       }
                                    }
#endif

                                    *total_file_size += stat_buf.st_size;
                                    if ((++files_copied >= fra[p_de->fra_pos].max_copied_files) ||
                                        (*total_file_size >= fra[p_de->fra_pos].max_copied_file_size))
                                    {
                                       goto done;
                                    }
                                    gotcha = YES;
                                 }

                                 /*
                                  * Since the file is in the file pool, it is not
                                  * necessary to test all other masks.
                                  */
                                 i = p_de->nfg;
                              }
                              else
                              {
                                 if (fra[p_de->fra_pos].end_character != -1)
                                 {
                                    p_de->search_time -= 5;
                                 }
                              }
                           }
#ifdef WITH_DUP_CHECK
                           }
                           else
                           {
                              if (is_duplicate == YES)
                              {
# ifdef _INPUT_LOG
                                 if ((count_files == YES) ||
                                     (count_files == PAUSED_REMOTE))
                                 {
                                    /* Log the file name in the input log. */
                                    (void)strcpy(il_file_name, p_dir->d_name);
                                    *il_file_size = stat_buf.st_size;
                                    *il_time = current_time;
                                    *il_dir_number = p_de->dir_id;
                                    *il_unique_number = *unique_number;
                                    il_real_size = strlen(p_dir->d_name) + il_size;
                                    if (write(il_fd, il_data, il_real_size) != il_real_size)
                                    {
                                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                  _("write() error : %s"),
                                                  strerror(errno));

                                       /*
                                        * Since the input log is not critical, no need to
                                        * exit here.
                                        */
                                    }
                                 }
# endif

                                 if (fra[p_de->fra_pos].dup_check_flag & DC_DELETE)
                                 {
                                    if (unlink(fullname) == -1)
                                    {
                                       system_log(WARN_SIGN, __FILE__, __LINE__,
                                                  _("Failed to unlink() `%s' : %s"),
                                                  fullname, strerror(errno));
                                    }
                                    else
                                    {
# ifdef _DELETE_LOG
                                       size_t        dl_real_size;
# endif
# ifdef _DISTRIBUTION_LOG
                                       unsigned int  dummy_job_id,
                                                     *p_dummy_job_id;
                                       unsigned char dummy_proc_cycles;

                                       dummy_job_id = 0;
                                       p_dummy_job_id = &dummy_job_id;
                                       dummy_proc_cycles = 0;
                                       dis_log(DUPCHECK_DIS_TYPE, current_time,
                                               p_de->dir_id, *unique_number,
                                               p_dir->d_name,
                                               strlen(p_dir->d_name),
                                               stat_buf.st_size, 1,
                                               &p_dummy_job_id,
                                               &dummy_proc_cycles, 1);
# endif
# ifdef _DELETE_LOG
                                       (void)strcpy(dl.file_name, p_dir->d_name);
                                       (void)sprintf(dl.host_name, "%-*s %03x",
                                                     MAX_HOSTNAME_LENGTH, "-",
                                                     DUP_INPUT);
                                       *dl.file_size = stat_buf.st_size;
                                       *dl.dir_id = p_de->dir_id;
                                       *dl.job_id = 0;
                                       *dl.input_time = current_time;
                                       *dl.split_job_counter = split_job_counter;
                                       *dl.unique_number = *unique_number;
#  ifdef _INPUT_LOG
                                       if ((count_files == YES) ||
                                           (count_files == PAUSED_REMOTE))
                                       {
                                          *dl.file_name_length = il_real_size - il_size;
                                       }
                                       else
                                       {
                                          *dl.file_name_length = strlen(p_dir->d_name);
                                       }
#  else
                                       *dl.file_name_length = strlen(p_dir->d_name);
#  endif
                                       dl_real_size = *dl.file_name_length + dl.size +
                                                      sprintf((dl.file_name + *dl.file_name_length + 1),
                                                              "%s%c(%s %d)",
                                                              DIR_CHECK,
                                                              SEPARATOR_CHAR,
                                                              __FILE__,
                                                              __LINE__);
                                       if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                                       {
                                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                     _("write() error : %s"),
                                                     strerror(errno));
                                       }
# endif
                                       files_in_dir--;
                                       bytes_in_dir -= stat_buf.st_size;

                                       gotcha = YES;
                                       i = p_de->nfg;
                                    }
                                 }
                                 else if (fra[p_de->fra_pos].dup_check_flag & DC_STORE)
                                      {
                                         char *p_end,
                                              save_dir[MAX_PATH_LENGTH];

                                         p_end = save_dir +
                                                 sprintf(save_dir, "%s%s%s/%x/",
                                                         p_work_dir,
                                                         AFD_FILE_DIR,
                                                         STORE_DIR,
                                                         p_de->dir_id);
                                         if ((mkdir(save_dir, DIR_MODE) == -1) &&
                                             (errno != EEXIST))
                                         {
                                            system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                       _("Failed to mkdir() `%s' : %s"),
                                                       save_dir,
                                                       strerror(errno));
                                            (void)unlink(fullname);
                                         }
                                         else
                                         {
                                            (void)strcpy(p_end, p_dir->d_name);
                                            if (rename(fullname, save_dir) == -1)
                                            {
                                               system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                          _("Failed to rename() `%s' to `%s' : %s"),
                                                          fullname, save_dir,
                                                          strerror(errno));
                                               (void)unlink(fullname);
                                            }
                                         }
                                         files_in_dir--;
                                         bytes_in_dir -= stat_buf.st_size;

                                         gotcha = YES;
                                         i = p_de->nfg;
                                      }
                                 if (fra[p_de->fra_pos].dup_check_flag & DC_WARN)
                                 {
                                    receive_log(WARN_SIGN, NULL, 0, current_time,
                                                _("File %s is duplicate."),
                                                p_dir->d_name);
                                 }
                              }
                           }
#endif /* WITH_DUP_CHECK */
                        } /* If file in file mask. */
                        else if (ret == 1)
                             {
                                /*
                                 * This file is definitely NOT wanted, for this
                                 * file group! However, the next file group
                                 * might state that it does want this file. So
                                 * only ignore all entries for this file group!
                                 */
                                break;
                             }
                     } /* for (j = 0; ((j < p_de->fme[i].nfm) && (i < p_de->nfg)); j++) */
                  } /* for (i = 0; i < p_de->nfg; i++) */

                  if ((gotcha == NO) &&
                      (fra[p_de->fra_pos].delete_files_flag & UNKNOWN_FILES))
                  {
                     diff_time = current_time - stat_buf.st_mtime;
                     if ((diff_time > fra[p_de->fra_pos].unknown_file_time) &&
                         (diff_time > DEFAULT_TRANSFER_TIMEOUT))
                     {
                        if (unlink(fullname) == -1)
                        {
                           if (errno != ENOENT)
                           {
                              system_log(WARN_SIGN, __FILE__, __LINE__,
                                         _("Failed to unlink() `%s' : %s"),
                                         fullname, strerror(errno));
                           }
                        }
                        else
                        {
#ifdef _DELETE_LOG
                           size_t dl_real_size;

                           (void)strcpy(dl.file_name, p_dir->d_name);
                           (void)sprintf(dl.host_name, "%-*s %03x",
                                         MAX_HOSTNAME_LENGTH, "-",
                                         DEL_UNKNOWN_FILE);
                           *dl.file_size = stat_buf.st_size;
                           *dl.dir_id = p_de->dir_id;
                           *dl.job_id = 0;
                           *dl.input_time = 0L;
                           *dl.split_job_counter = 0;
                           *dl.unique_number = 0;
                           *dl.file_name_length = strlen(p_dir->d_name);
                           dl_real_size = *dl.file_name_length + dl.size +
                                          sprintf((dl.file_name + *dl.file_name_length + 1),
# if SIZEOF_TIME_T == 4
                                                  "%s%c>%ld (%s %d)",
# else
                                                  "%s%c>%lld (%s %d)",
# endif
                                                  DIR_CHECK, SEPARATOR_CHAR,
                                                  (pri_time_t)diff_time,
                                                  __FILE__, __LINE__);
                           if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                           {
                              system_log(ERROR_SIGN, __FILE__, __LINE__,
                                         _("write() error : %s"),
                                         strerror(errno));
                           }
#endif
                           files_in_dir--;
                           bytes_in_dir -= stat_buf.st_size;
                        }
                     }
                  }
               }
            }
            else
            {
               if ((fra[p_de->fra_pos].delete_files_flag & UNKNOWN_FILES) &&
                   ((fra[p_de->fra_pos].ignore_size != 0) ||
                    ((fra[p_de->fra_pos].ignore_file_time != 0) &&
                     ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_GREATER_THEN) ||
                      (fra[p_de->fra_pos].gt_lt_sign & IFTIME_EQUAL)))) &&
                   ((current_time - stat_buf.st_mtime) > fra[p_de->fra_pos].unknown_file_time))
               {
                  if (unlink(fullname) == -1)
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                _("Failed to unlink() `%s' : %s"),
                                fullname, strerror(errno));
                  }
                  else
                  {
#ifdef _DELETE_LOG
                     size_t dl_real_size;

                     (void)strcpy(dl.file_name, p_dir->d_name);
                     (void)sprintf(dl.host_name, "%-*s %03x",
                                   MAX_HOSTNAME_LENGTH, "-",
                                   DEL_UNKNOWN_FILE);
                     *dl.file_size = stat_buf.st_size;
                     *dl.dir_id = p_de->dir_id;
                     *dl.job_id = 0;
                     *dl.input_time = 0L;
                     *dl.split_job_counter = 0;
                     *dl.unique_number = 0;
                     *dl.file_name_length = strlen(p_dir->d_name);
                     dl_real_size = *dl.file_name_length + dl.size +
                                    sprintf((dl.file_name + *dl.file_name_length + 1),
# if SIZEOF_TIME_T == 4
                                            "%s%c>%ld (%s %d)",
# else
                                            "%s%c>%lld (%s %d)",
# endif
                                            DIR_CHECK, SEPARATOR_CHAR,
                                            (pri_time_t)(current_time - stat_buf.st_mtime),
                                            __FILE__, __LINE__);
                     if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   _("write() error : %s"), strerror(errno));
                     }
#endif
                     files_in_dir--;
                     bytes_in_dir -= stat_buf.st_size;
                  }
               }
               else if (((fra[p_de->fra_pos].ignore_file_time != 0) &&
                         (((fra[p_de->fra_pos].gt_lt_sign & IFTIME_LESS_THEN) &&
                           (diff_time <= fra[p_de->fra_pos].ignore_file_time)) ||
                          ((fra[p_de->fra_pos].gt_lt_sign & IFTIME_EQUAL) &&
                           (diff_time < fra[p_de->fra_pos].ignore_file_time)))) ||
                        ((fra[p_de->fra_pos].ignore_size != 0) &&
                         ((fra[p_de->fra_pos].gt_lt_sign & ISIZE_LESS_THEN) ||
                          (fra[p_de->fra_pos].gt_lt_sign & ISIZE_EQUAL)) &&
                         (stat_buf.st_size < fra[p_de->fra_pos].ignore_size)))
                    {
                       *rescan_dir = YES;
                    }
            }
         } /* if (S_ISREG(stat_buf.st_mode)) */
         errno = 0;
      } /* while ((p_dir = readdir(dp)) != NULL) */
   }

done:

   /* When using readdir() it can happen that it always returns     */
   /* NULL, due to some error. We want to know if this is the case. */
   if (errno == EBADF)
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 _("Failed to readdir() `%s' : %s"), fullname, strerror(errno));
   }

   /* So that we return only the directory name where */
   /* the files have been stored.                    */
   if (ptr != NULL)
   {
      *ptr = '\0';
   }

   /* Don't forget to close the directory. */
   if (closedir(dp) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Failed  to closedir() `%s' : %s"),
                 src_file_path, strerror(errno));
   }

   if (p_de->rl_fd > -1)
   {
      rm_removed_files(p_de);
      if (close(p_de->rl_fd) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Failed to close() ls_data file for %s : %s"),
                    fra[p_de->fra_pos].dir_alias, strerror(errno));
      }
      p_de->rl_fd = -1;
      if (p_de->rl != NULL)
      {
         ptr = (char *)p_de->rl;
         ptr -= AFD_WORD_OFFSET;
#ifdef HAVE_MMAP
         if (munmap(ptr, p_de->rl_size) == -1)
#else
         if (munmap_emu(ptr) == -1)
#endif
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       _("Failed to munmap() from ls_data file %s : %s"),
                       fra[p_de->fra_pos].dir_alias, strerror(errno));
         }
         p_de->rl = NULL;
      }
   }

#ifdef _WITH_PTHREAD
   if ((ret = pthread_mutex_lock(&fsa_mutex)) != 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "pthread_mutex_lock() error : %s", strerror(ret));
   }
#endif
   if ((files_copied >= fra[p_de->fra_pos].max_copied_files) ||
       (*total_file_size >= fra[p_de->fra_pos].max_copied_file_size))
   {
      if (count_files == YES)
      {
         if (fra[p_de->fra_pos].files_in_dir < files_in_dir)
         {
            fra[p_de->fra_pos].files_in_dir = files_in_dir;
         }
         if (fra[p_de->fra_pos].bytes_in_dir < bytes_in_dir)
         {
            fra[p_de->fra_pos].bytes_in_dir = bytes_in_dir;
         }
      }
      if ((fra[p_de->fra_pos].dir_flag & MAX_COPIED) == 0)
      {
         fra[p_de->fra_pos].dir_flag ^= MAX_COPIED;
      }
   }
   else
   {
      if (count_files == YES)
      {
         fra[p_de->fra_pos].files_in_dir = files_in_dir;
         fra[p_de->fra_pos].bytes_in_dir = bytes_in_dir;
      }
      if ((fra[p_de->fra_pos].dir_flag & MAX_COPIED))
      {
         fra[p_de->fra_pos].dir_flag ^= MAX_COPIED;
      }
   }

   if (files_copied > 0)
   {
      if ((count_files == YES) || (count_files == PAUSED_REMOTE))
      {
         fra[p_de->fra_pos].files_received += files_copied;
         fra[p_de->fra_pos].bytes_received += *total_file_size;
         fra[p_de->fra_pos].last_retrieval = current_time;
         if (fra[p_de->fra_pos].dir_flag & WARN_TIME_REACHED)
         {
            fra[p_de->fra_pos].dir_flag &= ~WARN_TIME_REACHED;
            SET_DIR_STATUS(fra[p_de->fra_pos].dir_flag,
                           current_time,
                           fra[p_de->fra_pos].start_event_handle,
                           fra[p_de->fra_pos].end_event_handle,
                           fra[p_de->fra_pos].dir_status);
            error_action(p_de->alias, "stop", DIR_WARN_ACTION);
            event_log(0L, EC_DIR, ET_AUTO, EA_WARN_TIME_UNSET, "%s",
                      fra[p_de->fra_pos].dir_alias);
         }
         receive_log(INFO_SIGN, NULL, 0, current_time,
#if SIZEOF_OFF_T == 4
                     _("Received %d files with %ld bytes."),
#else
                     _("Received %d files with %lld bytes."),
#endif
                     files_copied, (pri_off_t)*total_file_size);
      }
      else
      {
         ABS_REDUCE_QUEUE(p_de->fra_pos, files_copied, *total_file_size);
      }
   }
#ifdef REPORT_EMPTY_DIR_SCANS
   else
   {
      if ((count_files == YES) || (count_files == PAUSED_REMOTE))
      {
         receive_log(INFO_SIGN, NULL, 0, current_time,
                     _("Received 0 files with 0 bytes."));
      }
   }
#endif
#ifdef _WITH_PTHREAD
   if ((ret = pthread_mutex_unlock(&fsa_mutex)) != 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("pthread_mutex_unlock() error : %s"), strerror(ret));
   }
#endif

   if ((set_error_counter == NO) && (fra[p_de->fra_pos].error_counter > 0) &&
       (fra[p_de->fra_pos].fsa_pos == -1))
   {
      lock_region_w(fra_fd,
#ifdef LOCK_DEBUG
                    (char *)&fra[p_de->fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                    (char *)&fra[p_de->fra_pos].error_counter - (char *)fra);
#endif
      fra[p_de->fra_pos].error_counter = 0;
      if (fra[p_de->fra_pos].dir_flag & DIR_ERROR_SET)
      {
         fra[p_de->fra_pos].dir_flag &= ~DIR_ERROR_SET;
         SET_DIR_STATUS(fra[p_de->fra_pos].dir_flag,
                        current_time,
                        fra[p_de->fra_pos].start_event_handle,
                        fra[p_de->fra_pos].end_event_handle,
                        fra[p_de->fra_pos].dir_status);
         error_action(p_de->alias, "stop", DIR_ERROR_ACTION);
         event_log(0L, EC_DIR, ET_EXT, EA_ERROR_END, "%s", p_de->alias);
      }
      unlock_region(fra_fd,
#ifdef LOCK_DEBUG
                    (char *)&fra[p_de->fra_pos].error_counter - (char *)fra, __FILE__, __LINE__);
#else
                    (char *)&fra[p_de->fra_pos].error_counter - (char *)fra);
#endif
   }

   return(files_copied);
}


/*+++++++++++++++++++++++++++ get_last_char() +++++++++++++++++++++++++++*/
static int
get_last_char(char *file_name, off_t file_size)
{
   int fd,
       last_char = -1;

   if (file_size > 0)
   {
#ifdef O_LARGEFILE
      if ((fd = open(file_name, O_RDONLY | O_LARGEFILE)) != -1)
#else
      if ((fd = open(file_name, O_RDONLY)) != -1)
#endif
      {
         if (lseek(fd, file_size - 1, SEEK_SET) != -1)
         {
            char tmp_char;

            if (read(fd, &tmp_char, 1) == 1)
            {
               last_char = (int)tmp_char;
            }
            else
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          _("Failed to read() last character from `%s' : %s"),
                          file_name, strerror(errno));
            }
         }
         else
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("Failed to lseek() in `%s' : %s"),
                       file_name, strerror(errno));
         }
         if (close(fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       _("Failed to close() `%s' : %s"),
                       file_name, strerror(errno));
         }
      }
   }

   return(last_char);
}


#ifdef _POSIX_SAVED_IDS
/*++++++++++++++++++++++++++++ check_sgids() ++++++++++++++++++++++++++++*/
static int
check_sgids(gid_t file_gid)
{
   int i;

   for (i = 0; i < no_of_sgids; i++)
   {
      if (file_gid == afd_sgids[i])
      {
         return(YES);
      }
   }
   return(NO);
}
#endif
