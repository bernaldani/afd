/*
 *  check_inotify_files.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2013, 2014 Holger Kiehl <Holger.Kiehl@dwd.de>
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

DESCR__S_M1
/*
 ** NAME
 **    check_inotify_files - 
 **
 ** SYNOPSIS
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has
 **   occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   20.05.2013 H.Kiehl Created
 **
 */
DESCR__E_M1

/* #define WITH_MEMCHECK */

#include <stdio.h>                 /* fprintf(), sprintf()               */
#include <string.h>                /* strcpy(), memcpy(), strerror(),    */
                                   /* strlen()                           */
#include <stdlib.h>                /* atoi(), malloc(), free(), exit()   */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>              /* struct timeval                     */
#ifdef HAVE_MMAP
# include <sys/mman.h>             /* munmap()                           */
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef WITH_MEMCHECK
# include <mcheck.h>
#endif
#include <unistd.h>               /* sleep(), eaccess(), unlink(),       */
                                  /* write(), close()                    */
#include <errno.h>
#include "amgdefs.h"


/* External global variables. */
extern int                        afd_file_dir_length,
                                  fra_fd;
#ifdef HAVE_HW_CRC32
extern int                        have_hw_crc32;
#endif
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
extern char                       *afd_file_dir;
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

/* Local function prototypes. */
#ifdef _POSIX_SAVED_IDS
static int                        check_sgids(gid_t);
#endif


/*######################## check_inotify_files() ########################*/
int
check_inotify_files(struct inotify_watch_list *p_iwl,
                    struct directory_entry    *p_de,
                    char                      *tmp_file_dir,
                    int                       *unique_number,
                    time_t                    current_time,
                    off_t                     *total_file_size)
{
   int          current_fnl_pos = 0,
                files_copied = 0,
                full_scan = YES,
                i,
                ret,
                set_error_counter = NO; /* Indicator to tell that we */
                                         /* set the fra error_counter */
                                         /* when we are called.       */
   unsigned int split_job_counter = 0;
   char         fullname[MAX_PATH_LENGTH],
                *ptr,
                *work_ptr;
   time_t       diff_time;
#ifdef _INPUT_LOG
   size_t       il_real_size;
#endif
   struct stat  stat_buf;

   (void)strcpy(fullname, p_de->dir);
   work_ptr = fullname + strlen(fullname);
   *work_ptr++ = '/';
   tmp_file_dir[0] = '\0';

   for (i = 0; i < p_iwl->no_of_files; i++)
   {
      (void)strcpy(work_ptr, &p_iwl->file_name[current_fnl_pos]);
      if (stat(fullname, &stat_buf) == -1)
      {
         if (errno != ENOENT)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to stat() file `%s' : %s"),
                       fullname, strerror(errno));
         }
      }
      else
      {
         if (fra[p_de->fra_pos].ignore_file_time != 0)
         {
            diff_time = current_time - stat_buf.st_mtime;
         }

         if (((fra[p_de->fra_pos].ignore_size == -1) ||
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
               if (fra[p_de->fra_pos].dir_flag == ALL_DISABLED)
               {
                  if (unlink(fullname) == -1)
                  {
                     if (errno != ENOENT)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   _("Failed to unlink() file `%s' : %s"),
                                   fullname, strerror(errno));
                     }
                  }
                  else
                  {
#ifdef _DELETE_LOG
                     size_t        dl_real_size;
#endif
#ifdef _DISTRIBUTION_LOG
                     unsigned int  dummy_job_id = 0,
                                   *p_dummy_job_id;
                     unsigned char dummy_proc_cycles = 0;

                     p_dummy_job_id = &dummy_job_id;
                     dis_log(DISABLED_DIS_TYPE, current_time,
                             p_de->dir_id, 0,
                             &p_iwl->file_name[current_fnl_pos],
                             p_iwl->fnl[i], stat_buf.st_size, 1,
                             &p_dummy_job_id, &dummy_proc_cycles, 1);
#endif
#ifdef _DELETE_LOG
                     (void)my_strncpy(dl.file_name,
                                      &p_iwl->file_name[current_fnl_pos],
                                      p_iwl->fnl[i] + 1);
# ifdef HAVE_SNPRINTF
                     (void)snprintf(dl.host_name,
                                    MAX_HOSTNAME_LENGTH + 4 + 1,
# else
                     (void)sprintf(dl.host_name,
# endif
                                   "%-*s %03x",
                                   MAX_HOSTNAME_LENGTH, "-",
                                   DELETE_HOST_DISABLED);
                     *dl.file_size = stat_buf.st_size;
                     *dl.dir_id = p_de->dir_id;
                     *dl.job_id = 0;
                     *dl.input_time = current_time;
                     *dl.split_job_counter = 0;
                     *dl.unique_number = 0;
                     *dl.file_name_length = p_iwl->fnl[i];
                     dl_real_size = *dl.file_name_length + dl.size +
# ifdef HAVE_SNPRINTF
                                    snprintf((dl.file_name + *dl.file_name_length + 1),
                                             MAX_FILENAME_LENGTH + 1,
# else
                                    sprintf((dl.file_name + *dl.file_name_length + 1),
# endif
                                            "%s%c(%s %d)",
                                            DIR_CHECK, SEPARATOR_CHAR,
                                            __FILE__, __LINE__);
                     if (write(dl.fd, dl.data, dl_real_size) != dl_real_size)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   _("write() error : %s"),
                                   strerror(errno));
                     }
#endif
                  }
               }
               else
               {
                  int gotcha;

                  if (p_de->flag & ALL_FILES)
                  {
                     gotcha = YES;
                  }
                  else
                  {
                     int j, k;

                     gotcha = NO;
                     for (j = 0; j < p_de->nfg; j++)
                     {
                        for (k = 0; ((j < p_de->nfg) && (k < p_de->fme[j].nfm)); k++)
                        {
                           if ((ret = pmatch(p_de->fme[j].file_mask[k],
                                             &p_iwl->file_name[current_fnl_pos],
                                             &current_time)) == 0)
                           {
                              gotcha = YES;
                              j = p_de->nfg;
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
                        } /* for (k = 0; ((j < p_de->nfg) && (k < p_de->fme[j].nfm)); k++) */
                     } /* for (j = 0; j < p_de->nfg; j++) */
                  }

                  if (gotcha == YES)
                  {
                     int rl_pos = -1;
#ifdef WITH_DUP_CHECK
                     int is_duplicate = NO;

                     if ((fra[p_de->fra_pos].dup_check_timeout == 0L) ||
                         (((is_duplicate = isdup(fullname, NULL,
                                                 stat_buf.st_size,
                                                 p_de->dir_id,
                                                 fra[p_de->fra_pos].dup_check_timeout,
                                                 fra[p_de->fra_pos].dup_check_flag,
                                                 NO,
# ifdef HAVE_HW_CRC32
                                                 have_hw_crc32,
# endif
                                                 YES, NO)) == NO) ||
                          (((fra[p_de->fra_pos].dup_check_flag & DC_DELETE) == 0) &&
                           ((fra[p_de->fra_pos].dup_check_flag & DC_STORE) == 0))))
                     {
                        if ((is_duplicate == YES) &&
                            (fra[p_de->fra_pos].dup_check_flag & DC_WARN))
                        {
                           receive_log(WARN_SIGN, NULL, 0, current_time,
                                       _("File %s is duplicate."),
                                       &p_iwl->file_name[current_fnl_pos]);
                        }
#endif
                     if ((fra[p_de->fra_pos].fsa_pos != -1) ||
                         (fra[p_de->fra_pos].stupid_mode == YES) ||
                         (fra[p_de->fra_pos].remove == YES) ||
                         ((rl_pos = check_list(p_de, &p_iwl->file_name[current_fnl_pos], &stat_buf)) > -1))
                     {
                        if ((fra[p_de->fra_pos].end_character == -1) ||
                            (fra[p_de->fra_pos].end_character == get_last_char(fullname, stat_buf.st_size)))
                        {
                           int what_done;

                           if (tmp_file_dir[0] == '\0')
                           {
                              (void)strcpy(tmp_file_dir, afd_file_dir);
                              (void)strcpy(tmp_file_dir + afd_file_dir_length,
                                           AFD_TMP_DIR);
                              ptr = tmp_file_dir + afd_file_dir_length + AFD_TMP_DIR_LENGTH;
                              *(ptr++) = '/';
                              *ptr = '\0';

                              /* Create a unique name. */
                              next_counter_no_lock(unique_number,
                                                   MAX_MSG_PER_SEC);
                              if (create_name(tmp_file_dir, NO_PRIORITY,
                                              current_time,
                                              p_de->dir_id,
                                              &split_job_counter, unique_number,
                                              ptr,
                                              MAX_PATH_LENGTH - (ptr - tmp_file_dir),
                                              -1) < 0)
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
                                       next_counter_no_lock(unique_number, MAX_MSG_PER_SEC);
                                       if (create_name(tmp_file_dir,
                                                       NO_PRIORITY,
                                                       current_time,
                                                       p_de->dir_id,
                                                       &split_job_counter,
                                                       unique_number, ptr,
                                                       MAX_PATH_LENGTH - (ptr - tmp_file_dir),
                                                       -1) < 0)
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
                                     * files so we can send data as quickly as possible
                                     * to get more free disk space.
                                     */
                                    full_scan = NO;
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
                           (void)strcpy(ptr, &p_iwl->file_name[current_fnl_pos]);

                           if ((fra[p_de->fra_pos].remove == YES) ||
                               (fra[p_de->fra_pos].protocol != LOC))
                           {
                              if (p_de->flag & IN_SAME_FILESYSTEM)
                              {
                                 ret = move_file(fullname, tmp_file_dir);
                                 what_done = DATA_MOVED;
                              }
                              else
                              {
                                 if ((ret = copy_file(fullname, tmp_file_dir,
                                                      &stat_buf)) == SUCCESS)
                                 {
                                    what_done = DATA_COPIED;
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
                              what_done = DATA_COPIED;
                           }
                           if (ret != SUCCESS)
                           {
                              char reason_str[23];

                              if (errno == ENOENT)
                              {
                                 int  tmp_errno = errno;
                                 char tmp_char = *ptr;

                                 *ptr = '\0';
                                 if ((access(fullname, F_OK) == -1) &&
                                     (errno == ENOENT))
                                 {
                                    (void)strcpy(reason_str, "(source missing) ");
                                 }
                                 else if ((access(tmp_file_dir, F_OK) == -1) &&
                                          (errno == ENOENT))
                                      {
                                         (void)strcpy(reason_str,
                                                      "(destination missing) ");
                                      }
                                      else
                                      {
                                         reason_str[0] = '\0';
                                      }
                                 *ptr = tmp_char;
                                 errno = tmp_errno;
                              }
                              else
                              {
                                 reason_str[0] = '\0';
                              }
                              receive_log(ERROR_SIGN, __FILE__, __LINE__, current_time,
                                          _("Failed (%d) to %s file `%s' to `%s' %s: %s"),
                                          ret,
                                          (what_done == DATA_MOVED) ? "move" : "copy",
                                          fullname, tmp_file_dir, reason_str,
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
                                 event_log(0L, EC_DIR, ET_EXT, EA_ERROR_START,
                                           "%s", p_de->alias);
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
                                 (void)isdup(fullname, NULL,
                                             stat_buf.st_size,
                                             p_de->dir_id,
                                             fra[p_de->fra_pos].dup_check_timeout,
                                             fra[p_de->fra_pos].dup_check_flag,
                                             YES,
# ifdef HAVE_HW_CRC32
                                             have_hw_crc32,
# endif
                                             YES, NO);
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
                              file_length_pool[files_copied] = p_iwl->fnl[i];
                              (void)memcpy(file_name_pool[files_copied],
                                           &p_iwl->file_name[current_fnl_pos],
                                           (size_t)(file_length_pool[files_copied] + 1));
                              file_mtime_pool[files_copied] = stat_buf.st_mtime;
                              file_size_pool[files_copied] = stat_buf.st_size;

#ifdef _INPUT_LOG
                              /* Log the file name in the input log. */
                              (void)memcpy(il_file_name,
                                           &p_iwl->file_name[current_fnl_pos],
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
#endif

                              *total_file_size += stat_buf.st_size;
                              if ((++files_copied >= fra[p_de->fra_pos].max_copied_files) ||
                                  (*total_file_size >= fra[p_de->fra_pos].max_copied_file_size))
                              {
                                 full_scan = NO;
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
                           /* Log the file name in the input log. */
                           (void)strcpy(il_file_name, &p_iwl->file_name[current_fnl_pos]);
                           *il_file_size = stat_buf.st_size;
                           *il_time = current_time;
                           *il_dir_number = p_de->dir_id;
                           *il_unique_number = *unique_number;
                           il_real_size = p_iwl->fnl[i] + il_size;
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
                                 unsigned int  dummy_job_id = 0,
                                               *p_dummy_job_id;
                                 unsigned char dummy_proc_cycles = 0;

                                 p_dummy_job_id = &dummy_job_id;
                                 dis_log(DUPCHECK_DIS_TYPE, current_time,
                                         p_de->dir_id,
                                         *unique_number,
                                         &p_iwl->file_name[current_fnl_pos],
                                         p_iwl->fnl[i], stat_buf.st_size, 1,
                                         &p_dummy_job_id, &dummy_proc_cycles, 1);
# endif
# ifdef _DELETE_LOG
                                 (void)my_strncpy(dl.file_name,
                                                  &p_iwl->file_name[current_fnl_pos],
                                                  p_iwl->fnl[i] + 1);
#  ifdef HAVE_SNPRINTF
                                 (void)snprintf(dl.host_name,
                                                MAX_HOSTNAME_LENGTH + 4 + 1,
#  else
                                 (void)sprintf(dl.host_name,
#  endif
                                               "%-*s %03x",
                                               MAX_HOSTNAME_LENGTH, "-",
                                               DUP_INPUT);
                                 *dl.file_size = stat_buf.st_size;
                                 *dl.dir_id = p_de->dir_id;
                                 *dl.job_id = 0;
                                 *dl.input_time = current_time;
                                 *dl.split_job_counter = split_job_counter;
                                 *dl.unique_number = *unique_number;
                                 *dl.file_name_length = p_iwl->fnl[i];
                                 dl_real_size = *dl.file_name_length + dl.size +
#  ifdef HAVE_SNPRINTF
                                                snprintf((dl.file_name + *dl.file_name_length + 1),
                                                         MAX_FILENAME_LENGTH + 1,
#  else
                                                sprintf((dl.file_name + *dl.file_name_length + 1),
#  endif
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
                              }
                           }
                           else if (fra[p_de->fra_pos].dup_check_flag & DC_STORE)
                                {
                                   char *p_end,
                                        save_dir[MAX_PATH_LENGTH];

                                   p_end = save_dir +
# ifdef HAVE_SNPRINTF
                                           snprintf(save_dir, MAX_PATH_LENGTH,
# else
                                           sprintf(save_dir,
# endif
                                                   "%s%s%s/%x/",
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
                                      (void)strcpy(p_end, &p_iwl->file_name[current_fnl_pos]);
                                      if (rename(fullname, save_dir) == -1)
                                      {
                                         system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                    _("Failed to rename() `%s' to `%s' : %s"),
                                                    fullname, save_dir,
                                                    strerror(errno));
                                         (void)unlink(fullname);
                                      }
                                   }
                                }
                           if (fra[p_de->fra_pos].dup_check_flag & DC_WARN)
                           {
                              receive_log(WARN_SIGN, NULL, 0, current_time,
                                          _("File %s is duplicate."),
                                          &p_iwl->file_name[current_fnl_pos]);
                           }
                        }
                     }
#endif /* WITH_DUP_CHECK */
                  }
                  else /* gotcha == NO */
                  {
                     if (fra[p_de->fra_pos].delete_files_flag & UNKNOWN_FILES)
                     {
                        diff_time = current_time - stat_buf.st_mtime;
                        if ((fra[p_de->fra_pos].unknown_file_time == -2) ||
                            ((diff_time > fra[p_de->fra_pos].unknown_file_time) &&
                             (diff_time > DEFAULT_TRANSFER_TIMEOUT)))
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

                              (void)my_strncpy(dl.file_name,
                                               &p_iwl->file_name[current_fnl_pos],
                                               p_iwl->fnl[i] + 1);
# ifdef HAVE_SNPRINTF
                              (void)snprintf(dl.host_name,
                                             MAX_HOSTNAME_LENGTH + 4 + 1,
# else
                              (void)sprintf(dl.host_name,
# endif
                                            "%-*s %03x",
                                            MAX_HOSTNAME_LENGTH, "-",
                                            DEL_UNKNOWN_FILE);
                              *dl.file_size = stat_buf.st_size;
                              *dl.dir_id = p_de->dir_id;
                              *dl.job_id = 0;
                              *dl.input_time = 0L;
                              *dl.split_job_counter = 0;
                              *dl.unique_number = 0;
                              *dl.file_name_length = p_iwl->fnl[i];
                              dl_real_size = *dl.file_name_length + dl.size +
# ifdef HAVE_SNPRINTF
                                             snprintf((dl.file_name + *dl.file_name_length + 1),
                                                      MAX_FILENAME_LENGTH + 1,
# else
                                             sprintf((dl.file_name + *dl.file_name_length + 1),
# endif
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
                           }
                        }
                     }
                  }
               }
            }
         }
      }
      current_fnl_pos += (p_iwl->fnl[i] + 1);
   } /* for (i = 0; i < p_iwl->no_of_files; i++) */

   /* Reset values in structure. */
   free(p_iwl->file_name);
   free(p_iwl->fnl);
   p_iwl->file_name = NULL;
   p_iwl->fnl = NULL;
   p_iwl->no_of_files = 0;
   p_iwl->cur_fn_length = 0;
   p_iwl->alloc_fn_length = 0;

   /* So that we return only the directory name where */
   /* the files have been stored.                    */
   if (ptr != NULL)
   {
      *ptr = '\0';
   }

#ifdef WITH_DUP_CHECK
   isdup_detach();
#endif

   if (p_de->rl_fd > -1)
   {
      *work_ptr = '\0';
      rm_removed_files(p_de, full_scan, fullname);
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
                       fra[p_de->fra_pos].dir_alias,
                       strerror(errno));
         }
         p_de->rl = NULL;
      }
   }

   if ((files_copied >= fra[p_de->fra_pos].max_copied_files) ||
       (*total_file_size >= fra[p_de->fra_pos].max_copied_file_size))
   {
      if ((fra[p_de->fra_pos].dir_flag & MAX_COPIED) == 0)
      {
         fra[p_de->fra_pos].dir_flag |= MAX_COPIED;
      }
      if ((fra[p_de->fra_pos].dir_flag & INOTIFY_NEEDS_SCAN) == 0)
      {
         fra[p_de->fra_pos].dir_flag |= INOTIFY_NEEDS_SCAN;
      }
   }
   else
   {
      if ((fra[p_de->fra_pos].dir_flag & MAX_COPIED))
      {
         fra[p_de->fra_pos].dir_flag &= ~MAX_COPIED;
      }
   }

   /*
    * With inotify it is to expansive to count the number of files
    * int the directory. So lets always set this to zero.
    */
   if (fra[p_de->fra_pos].files_in_dir > 0)
   {
      fra[p_de->fra_pos].files_in_dir = 0;
   }
   if (fra[p_de->fra_pos].bytes_in_dir > 0)
   {
      fra[p_de->fra_pos].bytes_in_dir = 0;
   }

   if (files_copied > 0)
   {
      fra[p_de->fra_pos].files_received += files_copied;
      fra[p_de->fra_pos].bytes_received += *total_file_size;
      fra[p_de->fra_pos].last_retrieval = current_time;
#ifdef NEW_FRA
      if (fra[p_de->fra_pos].dir_flag & INFO_TIME_REACHED)
      {
         fra[p_de->fra_pos].dir_flag &= ~INFO_TIME_REACHED;
         SET_DIR_STATUS(fra[p_de->fra_pos].dir_flag,
                        current_time,
                        fra[p_de->fra_pos].start_event_handle,
                        fra[p_de->fra_pos].end_event_handle,
                        fra[p_de->fra_pos].dir_status);
         error_action(p_de->alias, "stop", DIR_INFO_ACTION);
         event_log(0L, EC_DIR, ET_AUTO, EA_INFO_TIME_UNSET, "%s",
                   fra[p_de->fra_pos].dir_alias);
      }
#endif
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
                  _("*Received %d files with %ld bytes."),
#else
                  _("*Received %d files with %lld bytes."),
#endif
                  files_copied, (pri_off_t)*total_file_size);
   }
   else
   {
      receive_log(INFO_SIGN, NULL, 0, current_time,
                  _("*Received 0 files with 0 bytes."));
   }

   if ((set_error_counter == NO) &&
       (fra[p_de->fra_pos].error_counter > 0) &&
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
