/*
 *  sf_loc.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2001 Deutscher Wetterdienst (DWD),
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

DESCR__S_M1
/*
 ** NAME
 **   sf_loc - copies files from one directory to another
 **
 ** SYNOPSIS
 **   sf_loc [--version] [-w <directory>] -m <message-file>
 **
 ** DESCRIPTION
 **   sf_loc is very similar to sf_ftp only that it sends files
 **   locally (i.e. moves/copies files from one directory to another).
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has
 **   occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.03.1996 H.Kiehl Created
 **   27.01.1997 H.Kiehl Include support for output logging.
 **   18.04.1997 H.Kiehl Do a hard link when we are in the same file
 **                      system.
 **   08.05.1997 H.Kiehl Logging archive directory.
 **   12.06.1999 H.Kiehl Added option to change user and group ID.
 **   07.10.1999 H.Kiehl Added option to force a copy.
 **   09.07.2000 H.Kiehl Cleaned up log output to reduce code size.
 **
 */
DESCR__E_M1

#include <stdio.h>                     /* fprintf(), sprintf()           */
#include <string.h>                    /* strcpy(), strcat(), strcmp(),  */
                                       /* strerror()                     */
#include <stdlib.h>                    /* getenv(), abort()              */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>                  /* struct timeval                 */
#ifdef _OUTPUT_LOG
#include <sys/times.h>                 /* times(), struct tms            */
#endif
#include <fcntl.h>
#include <signal.h>                    /* signal()                       */
#include <unistd.h>                    /* unlink(), alarm()              */
#include <errno.h>
#include "fddefs.h"
#include "version.h"

/* Global variables */
int                        counter_fd = -1,
                           exitflag = IS_FAULTY_VAR,
                           no_of_hosts,    /* This variable is not used */
                                           /* in this module.           */
                           trans_rule_pos, /* Not used [init_sf()]      */
                           user_rule_pos,  /* Not used [init_sf()]      */
                           fsa_id,
                           fsa_fd = -1,
                           sys_log_fd = STDERR_FILENO,
                           timeout_flag = OFF,
                           transfer_log_fd = STDERR_FILENO,
                           trans_db_log_fd = STDERR_FILENO,
                           amg_flag = NO;
long                       transfer_timeout; /* Not used [init_sf()]    */
#ifndef _NO_MMAP
off_t                      fsa_size;
#endif
off_t                      *file_size_buffer;
char                       host_deleted = NO,
                           *p_work_dir = NULL,
                           msg_str[1], /* Required by function trans_log() */
                           tr_hostname[MAX_HOSTNAME_LENGTH + 1],
                           *file_name_buffer;
struct filetransfer_status *fsa;
struct job                 db;
struct rule                *rule;
#ifdef _DELETE_LOG                                   
struct delete_log          dl;
#endif

/* Local functions */
static void sf_loc_exit(void),
            sig_bus(int),
            sig_segv(int),
            sig_kill(int),
            sig_exit(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
#ifdef _VERIFY_FSA
   unsigned int     ui_variable;
#endif
   int              i,
                    fd,
                    lfs,                    /* local file system */
                    files_to_send = 0;
#ifdef _WITH_BURST_2
   unsigned int     burst_2_counter = 0;
#endif
   off_t            lock_offset,
                    *p_file_size_buffer;
   char             *ptr,
                    *p_if_name,
                    *p_ff_name,
                    *p_source_file,
                    *p_to_name,
                    *p_file_name_buffer,
                    file_name[MAX_FILENAME_LENGTH],
                    lockfile[MAX_PATH_LENGTH],
                    if_name[MAX_PATH_LENGTH],
                    ff_name[MAX_PATH_LENGTH],
                    file_path[MAX_PATH_LENGTH],
                    source_file[MAX_PATH_LENGTH],
                    work_dir[MAX_PATH_LENGTH];
   struct job       *p_db;
#ifdef SA_FULLDUMP
   struct sigaction sact;
#endif
#ifdef _OUTPUT_LOG
   int              ol_fd = -1;
   unsigned int     *ol_job_number = NULL;
   char             *ol_data = NULL,
                    *ol_file_name = NULL;
   unsigned short   *ol_archive_name_length,
                    *ol_file_name_length;
   off_t            *ol_file_size = NULL;
   size_t           ol_size,
                    ol_real_size;
   clock_t          end_time = 0,
                    start_time = 0,
                    *ol_transfer_time = NULL;
   struct tms       tmsdummy;
#endif

   CHECK_FOR_VERSION(argc, argv);

#ifdef SA_FULLDUMP
   /*
    * When dumping core sure we do a FULL core dump!
    */
   sact.sa_handler = SIG_DFL;
   sact.sa_flags = SA_FULLDUMP;
   sigemptyset(&sact.sa_mask);
   if (sigaction(SIGSEGV, &sact, NULL) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "sigaction() error : %s", strerror(errno));
      exit(INCORRECT);
   }
#endif

   /* Do some cleanups when we exit */
   if (atexit(sf_loc_exit) != 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not register exit function : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Initialise variables */
   p_work_dir = work_dir;
   msg_str[0] = '\0';
   files_to_send = init_sf(argc, argv, file_path, LOC_FLAG);
   p_db = &db;

   if ((signal(SIGINT, sig_kill) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to set signal handlers : %s", strerror(errno));
      exit(INCORRECT);
   }

   /* Inform FSA that we have are ready to copy the files. */
   if (host_deleted == NO)
   {
      if (check_fsa() == YES)
      {
         if ((db.fsa_pos = get_host_position(fsa, db.host_alias,
                                             no_of_hosts)) == INCORRECT)
         {
            host_deleted = YES;
         }
      }
      if (host_deleted == NO)
      {
         fsa[db.fsa_pos].job_status[(int)db.job_no].connect_status = LOC_ACTIVE;
         fsa[db.fsa_pos].job_status[(int)db.job_no].no_of_files = files_to_send;
      }
   }

#ifdef _OUTPUT_LOG
   if (db.output_log == YES)
   {
      output_log_ptrs(&ol_fd, &ol_job_number, &ol_data, &ol_file_name,
                      &ol_file_name_length, &ol_archive_name_length,
                      &ol_file_size, &ol_size, &ol_transfer_time,
                      db.host_alias, LOC);
   }
#endif

#ifdef _WITH_BURST_2
   do
   {
      if (burst_2_counter > 0)
      {
         if (fsa[db.fsa_pos].debug == YES)
         {
            trans_db_log(INFO_SIGN, __FILE__, __LINE__, "Bursting.");
         }
#ifdef _OUTPUT_LOG
         if ((db.output_log == YES) && (ol_data == NULL))
         {
            output_log_ptrs(&ol_fd, &ol_job_number, &ol_data, &ol_file_name,
                            &ol_file_name_length, &ol_archive_name_length,
                            &ol_file_size, &ol_size, &ol_transfer_time,
                            db.host_alias, LOC);
         }
#endif /* _OUTPUT_LOG */
      }
#endif /* _WITH_BURST_2 */
      /* If we send a lockfile, do it now. */
      if (db.lock == LOCKFILE)
      {
         /* Create lock file in directory */
         (void)strcpy(lockfile, db.target_dir);
         (void)strcat(lockfile, "/");
         (void)strcat(lockfile, LOCK_FILENAME);
         if ((fd = open(lockfile, (O_WRONLY | O_CREAT | O_TRUNC),
                        (S_IRUSR | S_IWUSR))) == -1)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Failed to create lock file <%s> : %s",
                      lockfile, strerror(errno));
            exit(WRITE_LOCK_ERROR);
         }
         else
         {
            if (fsa[db.fsa_pos].debug == YES)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                            "Created lockfile to <%s>.", lockfile);
            }
         }
         if (close(fd) == -1)
         {
            trans_log(WARN_SIGN, __FILE__, __LINE__,
                      "Failed to close() <%s> : %s", lockfile, strerror(errno));
         }
      }

      /*
       * Since we do not know if the directory where the file
       * will be moved is in the same file system, lets determine
       * this by comparing the device numbers.
       */
      if ((db.special_flag & FORCE_COPY) == 0)
      {
         struct stat stat_buf;

         if (stat(file_path, &stat_buf) == 0)
         {
            dev_t ldv;               /* local device number (file system) */

            ldv = stat_buf.st_dev;
            if (stat(db.target_dir, &stat_buf) == 0)
            {
               if (stat_buf.st_dev == ldv)
               {
                  lfs = YES;
               }
               else
               {
                  lfs = NO;
               }
            }
            else
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to stat() %s : %s",
                         db.target_dir, strerror(errno));
               exit(STAT_ERROR);
            }
         }
         else
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Failed to stat() %s : %s", file_path, strerror(errno));
            exit(STAT_ERROR);
         }
      }
      else
      {
         lfs = NO;
      }

      /* Prepare pointers and directory name */
      (void)strcpy(source_file, file_path);
      p_source_file = source_file + strlen(source_file);
      *p_source_file++ = '/';
      (void)strcpy(if_name, db.target_dir);
      p_if_name = if_name + strlen(if_name);
      *p_if_name++ = '/';
      *p_if_name = '\0';
      (void)strcpy(ff_name, db.target_dir);
      p_ff_name = ff_name + strlen(ff_name);
      *p_ff_name++ = '/';
      *p_ff_name = '\0';

      if ((db.lock == DOT) || (db.lock == DOT_VMS))
      {
         p_to_name = if_name;
      }
      else
      {
         p_to_name = ff_name;
      }

      /* Copy all files */
      p_file_name_buffer = file_name_buffer;
      p_file_size_buffer = file_size_buffer;
      for (i = 0; i < files_to_send; i++)
      {
         /* Get the the name of the file we want to send next */
         *p_ff_name = '\0';
         (void)strcat(ff_name, p_file_name_buffer);
         (void)strcpy(file_name, p_file_name_buffer);
         if ((db.lock == DOT) || (db.lock == DOT_VMS))
         {
            *p_if_name = '\0';
            (void)strcat(if_name, db.lock_notation);
            (void)strcat(if_name, p_file_name_buffer);
         }
         (void)strcpy(p_source_file, p_file_name_buffer);

         /* Write status to FSA? */
         if (host_deleted == NO)
         {
            if (check_fsa() == YES)
            {
               if ((db.fsa_pos = get_host_position(fsa, db.host_alias, no_of_hosts)) == INCORRECT)
               {
                  host_deleted = YES;
               }
            }
            if (host_deleted == NO)
            {
               fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_in_use = *p_file_size_buffer;
               (void)strcpy(fsa[db.fsa_pos].job_status[(int)db.job_no].file_name_in_use,
                            p_file_name_buffer);
            }
         }

         if (db.trans_rename_rule[0] != '\0')
         {
            register int k;
   
            for (k = 0; k < rule[trans_rule_pos].no_of_rules; k++)
            {
               if (pmatch(rule[trans_rule_pos].filter[k], p_file_name_buffer) == 0)
               {
                  change_name(p_file_name_buffer,
                              rule[trans_rule_pos].filter[k],
                              rule[trans_rule_pos].rename_to[k],
                              p_ff_name);
                  break;
               }
            }
         }

#ifdef _OUTPUT_LOG
         if (db.output_log == YES)
         {
            start_time = times(&tmsdummy);
         }
#endif

         /* Here comes the BIG move .... */
         if (lfs == YES)
         {
            if (link(source_file, p_to_name) == -1)
            {
               if (errno == EEXIST)
               {
                  if ((unlink(p_to_name) == -1) && (errno != ENOENT))
                  {
                     trans_log(ERROR_SIGN, __FILE__, __LINE__,
                               "Failed to unlink() <%s> : %s",
                               p_to_name, strerror(errno));
                     trans_log(INFO_SIGN, NULL, 0,
                               "%d Bytes copied in %d file(s).",
                               fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done,
                               fsa[db.fsa_pos].job_status[(int)db.job_no].no_of_files_done);
                     exit(MOVE_ERROR);
                  }
                  else
                  {
                     if (errno != ENOENT)
                     {
                        trans_log(INFO_SIGN, __FILE__, __LINE__,
                                  "File <%s> did already exist, removed it and linked again.",
                                  p_to_name);
                     }

                     if (link(source_file, p_to_name) == -1)
                     {
                        trans_log(ERROR_SIGN, __FILE__, __LINE__,
                                  "Failed to link file <%s> to <%s> : %s",
                                  source_file, p_to_name, strerror(errno));
                        trans_log(INFO_SIGN, NULL, 0,
                                  "%d Bytes copied in %d file(s).",
                                  fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done,
                                  fsa[db.fsa_pos].job_status[(int)db.job_no].no_of_files_done);
                        exit(MOVE_ERROR);
                     }
                  }
               }
               else
               {
                  trans_log(ERROR_SIGN, __FILE__, __LINE__,
                            "Failed to link file <%s> to <%s> : %s",
                            source_file, p_to_name, strerror(errno));
                  trans_log(INFO_SIGN, NULL, 0,
                            "%d Bytes copied in %d file(s).",
                            fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done,
                            fsa[db.fsa_pos].job_status[(int)db.job_no].no_of_files_done);
                  exit(MOVE_ERROR);
               }
            }
            else
            {
               if (fsa[db.fsa_pos].debug == YES)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                               "Linked file <%s> to <%s>.",
                               source_file, p_to_name);
               }
            }
         }
         else
         {
            if (copy_file(source_file, p_to_name, NULL) < 0)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to copy file <%s> to <%s> : %s",
                         source_file, p_to_name, strerror(errno));
               trans_log(INFO_SIGN, NULL, 0,
                         "%d Bytes copied in %d file(s).",
                         fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done,
                         fsa[db.fsa_pos].job_status[(int)db.job_no].no_of_files_done);
               exit(MOVE_ERROR);
            }
            else
            {
               if (fsa[db.fsa_pos].debug == YES)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                               "Copied file <%s> to <%s>.",
                               source_file, p_to_name);
               }
            }
         }

         if ((db.lock == DOT) || (db.lock == DOT_VMS))
         {
            if (db.lock == DOT_VMS)
            {
               (void)strcat(ff_name, DOT_NOTATION);
            }
            if (rename(if_name, ff_name) == -1)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to rename() file <%s> to <%s> : %s",
                         if_name, ff_name, strerror(errno));
               trans_log(INFO_SIGN, NULL, 0, "%d Bytes copied in %d file(s).",
                         fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done,
                         fsa[db.fsa_pos].job_status[(int)db.job_no].no_of_files_done);
               exit(RENAME_ERROR);
            }
            else
            {
               if (fsa[db.fsa_pos].debug == YES)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                               "Renamed file <%s> to <%s>.", if_name, ff_name);
               }
            }
            if (db.lock == DOT_VMS)
            {
               /* Take away the dot at the end */
               ptr = ff_name + strlen(ff_name) - 1;
               *ptr = '\0';
            }
         }

#ifdef _OUTPUT_LOG
         if (db.output_log == YES)
         {
            end_time = times(&tmsdummy);
         }
#endif

         if (db.special_flag & CHANGE_PERMISSION)
         {
            if (chmod(ff_name, db.chmod) == -1)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__,
                         "Failed to chmod() file <%s> : %s",
                         ff_name, strerror(errno));
            }
            else
            {
               if (fsa[db.fsa_pos].debug == YES)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                               "Changed permission of file <%s> to %d",
                               ff_name, db.chmod);
               }
            }
         } /* if (db.special_flag & CHANGE_PERMISSION) */

         if (db.special_flag & CHANGE_UID_GID)
         {
            if (chown(ff_name, db.user_id, db.group_id) == -1)
            {
               trans_log(WARN_SIGN, __FILE__, __LINE__,
                         "Failed to chown() of file <%s> : %s",
                         ff_name, strerror(errno));
            }
            else
            {
               if (fsa[db.fsa_pos].debug == YES)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                               "Changed owner of file <%s> to %d:%d.",
                               ff_name, db.user_id, db.group_id);
               }
            }
         } /* if (db.special_flag & CHANGE_UID_GID) */

         /* Tell FSA we have copied a file !!!! */
         if (host_deleted == NO)
         {
            lock_offset = (char *)&fsa[db.fsa_pos] - (char *)fsa;
            rlock_region(fsa_fd, lock_offset);

            /* Before we read from the FSA lets make */
            /* sure that it is NOT stale!            */
            if (check_fsa() == YES)
            {
               if ((db.fsa_pos = get_host_position(fsa, db.host_alias,
                                                   no_of_hosts)) == INCORRECT)
               {
                  host_deleted = YES;
               }
               else
               {
                  lock_offset = (char *)&fsa[db.fsa_pos] - (char *)fsa;
                  rlock_region(fsa_fd, lock_offset);
               }
            }
            if (host_deleted == NO)
            {
               fsa[db.fsa_pos].job_status[(int)db.job_no].file_name_in_use[0] = '\0';
               fsa[db.fsa_pos].job_status[(int)db.job_no].no_of_files_done++;
               fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done += *p_file_size_buffer;
               fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_in_use = 0;
               fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_in_use_done = 0;

               /* Total file counter */
               lock_region_w(fsa_fd, (char *)&fsa[db.fsa_pos].total_file_counter - (char *)fsa);
               fsa[db.fsa_pos].total_file_counter -= 1;
#ifdef _VERIFY_FSA
               if (fsa[db.fsa_pos].total_file_counter < 0)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Total file counter for host %s less then zero. Correcting to %d.",
                             fsa[db.fsa_pos].host_dsp_name,
                             files_to_send - (i + 1));
                  fsa[db.fsa_pos].total_file_counter = files_to_send - (i + 1);
               }
#endif

               /* Total file size */
#ifdef _VERIFY_FSA
               ui_variable = fsa[db.fsa_pos].total_file_size;
#endif
               fsa[db.fsa_pos].total_file_size -= *p_file_size_buffer;
#ifdef _VERIFY_FSA
               if (fsa[db.fsa_pos].total_file_size > ui_variable)
               {
                  int   k;
                  off_t *tmp_ptr = p_file_size_buffer;

                  tmp_ptr++;
                  fsa[db.fsa_pos].total_file_size = 0;
                  for (k = (i + 1); k < files_to_send; k++)
                  {
                     fsa[db.fsa_pos].total_file_size += *tmp_ptr;
                  }
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Total file size for host %s overflowed. Correcting to %lu.",
                             fsa[db.fsa_pos].host_dsp_name,
                             fsa[db.fsa_pos].total_file_size);
               }
               else if ((fsa[db.fsa_pos].total_file_counter == 0) &&
                        (fsa[db.fsa_pos].total_file_size > 0))
                    {
                       system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                  "fc for host %s is zero but fs is not zero. Correcting.",
                                  fsa[db.fsa_pos].host_dsp_name);
                       fsa[db.fsa_pos].total_file_size = 0;
                    }
#endif

               /* File counter done */
               fsa[db.fsa_pos].file_counter_done += 1;

               /* Number of bytes send */
               fsa[db.fsa_pos].bytes_send += *p_file_size_buffer;
               unlock_region(fsa_fd, (char *)&fsa[db.fsa_pos].total_file_counter - (char *)fsa);
               unlock_region(fsa_fd, lock_offset);
            }
         }

#ifdef _WITH_TRANS_EXEC
         if (db.special_flag & TRANS_EXEC)
         {
            trans_exec(file_path, ff_name, p_file_name_buffer);
         }
#endif /* _WITH_TRANS_EXEC */


         /* Now archive file if necessary */
         if ((db.archive_time > 0) &&
             (p_db->archive_dir[0] != FAILED_TO_CREATE_ARCHIVE_DIR))
         {
            /*
             * By telling the function archive_file() that this
             * is the first time to archive a file for this job
             * (in struct p_db) it does not always have to check
             * whether the directory has been created or not. And
             * we ensure that we do not create duplicate names
             * when adding ARCHIVE_UNIT * db.archive_time to
             * msg_name.
             */
            if (archive_file(file_path, file_name, p_db) < 0)
            {
               trans_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Failed to archive file <%s>", file_name);

               /*
                * NOTE: We _MUST_ delete the file we just send,
                *       else the file directory will run full!
                */
               if (unlink(source_file) == -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Could not unlink() local file <%s> after copying it successfully : %s",
                             source_file, strerror(errno));
               }

#ifdef _OUTPUT_LOG
               if (db.output_log == YES)
               {
                  if (db.trans_rename_rule[0] != '\0')
                  {
                     *ol_file_name_length = (unsigned short)sprintf(ol_file_name,
                                                                    "%s /%s",
                                                                    p_file_name_buffer,
                                                                    ff_name);
                  }
                  else
                  {
                     (void)strcpy(ol_file_name, p_file_name_buffer);
                     *ol_file_name_length = (unsigned short)strlen(ol_file_name);
                  }
                  *ol_file_size = *p_file_size_buffer;
                  *ol_job_number = fsa[db.fsa_pos].job_status[(int)db.job_no].job_id;
                  *ol_transfer_time = end_time - start_time;
                  *ol_archive_name_length = 0;
                  ol_real_size = *ol_file_name_length + ol_size;
                  if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "write() error : %s", strerror(errno));
                  }
               }
#endif
            }
            else
            {
               if (fsa[db.fsa_pos].debug == YES)
               {
                  trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                               "Archived file <%s>.", file_name);
               }

#ifdef _OUTPUT_LOG
               if (db.output_log == YES)
               {
                  if (db.trans_rename_rule[0] != '\0')
                  {
                     *ol_file_name_length = (unsigned short)sprintf(ol_file_name,
                                                                    "%s /%s",
                                                                    p_file_name_buffer,
                                                                    ff_name);
                  }
                  else
                  {
                     (void)strcpy(ol_file_name, p_file_name_buffer);
                     *ol_file_name_length = (unsigned short)strlen(ol_file_name);
                  }
                  (void)strcpy(&ol_file_name[*ol_file_name_length + 1], &db.archive_dir[db.archive_offset]);
                  *ol_file_size = *p_file_size_buffer;
                  *ol_job_number = fsa[db.fsa_pos].job_status[(int)db.job_no].job_id;
                  *ol_transfer_time = end_time - start_time;
                  *ol_archive_name_length = (unsigned short)strlen(&ol_file_name[*ol_file_name_length + 1]);
                  ol_real_size = *ol_file_name_length +
                                 *ol_archive_name_length + 1 + ol_size;
                  if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "write() error : %s", strerror(errno));
                  }
               }
#endif
            }
         }
         else
         {
            /* Delete the file we just have copied */
            if (unlink(source_file) == -1)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Could not unlink() local file %s after copying it successfully : %s",
                          source_file, strerror(errno));
            }

#ifdef _OUTPUT_LOG
            if (db.output_log == YES)
            {
               if (db.trans_rename_rule[0] != '\0')
               {
                  *ol_file_name_length = (unsigned short)sprintf(ol_file_name,
                                                                 "%s /%s",
                                                                 p_file_name_buffer,
                                                                 ff_name);
               }
               else
               {
                  (void)strcpy(ol_file_name, p_file_name_buffer);
                  *ol_file_name_length = (unsigned short)strlen(ol_file_name);
               }
               *ol_file_size = *p_file_size_buffer;
               *ol_job_number = fsa[db.fsa_pos].job_status[(int)db.job_no].job_id;
               *ol_transfer_time = end_time - start_time;
               *ol_archive_name_length = 0;
               ol_real_size = *ol_file_name_length + ol_size;
               if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "write() error : %s", strerror(errno));
               }
            }
#endif
         }

         /*
          * After each successful transfer set error
          * counter to zero, so that other jobs can be
          * started.
          */
         if (fsa[db.fsa_pos].error_counter > 0)
         {
            int  fd,
                 j;
            char fd_wake_up_fifo[MAX_PATH_LENGTH];

            lock_region_w(fsa_fd, (char *)&fsa[db.fsa_pos].error_counter - (char *)fsa);
            fsa[db.fsa_pos].error_counter = 0;

            /*
             * Wake up FD!
             */
            (void)sprintf(fd_wake_up_fifo, "%s%s%s",
                          p_work_dir, FIFO_DIR, FD_WAKE_UP_FIFO);
            if ((fd = open(fd_wake_up_fifo, O_RDWR)) == -1)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to open() FIFO %s : %s",
                          fd_wake_up_fifo, strerror(errno));
            }
            else
            {
               if (write(fd, "", 1) != 1)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Failed to write() to FIFO %s : %s",
                             fd_wake_up_fifo, strerror(errno));
               }
               if (close(fd) == -1)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Failed to close() FIFO %s : %s",
                             fd_wake_up_fifo, strerror(errno));
               }
            }

            /*
             * Remove the error condition (NOT_WORKING) from all jobs
             * of this host.
             */
            for (j = 0; j < fsa[db.fsa_pos].allowed_transfers; j++)
            {
               if ((j != db.job_no) &&
                   (fsa[db.fsa_pos].job_status[j].connect_status == NOT_WORKING))
               {
                  fsa[db.fsa_pos].job_status[j].connect_status = DISCONNECT;
               }
            }
            unlock_region(fsa_fd, (char *)&fsa[db.fsa_pos].error_counter - (char *)fsa);

            /*
             * Since we have successfully transmitted a file, no need to
             * have the queue stopped anymore.
             */
            if (fsa[db.fsa_pos].host_status & AUTO_PAUSE_QUEUE_STAT)
            {
               fsa[db.fsa_pos].host_status ^= AUTO_PAUSE_QUEUE_STAT;
               system_log(INFO_SIGN, __FILE__, __LINE__,
                          "Starting queue for %s that was stopped by init_afd.",
                          fsa[db.fsa_pos].host_alias);
            }
         } /* if (fsa[db.fsa_pos].error_counter > 0) */

         p_file_name_buffer += MAX_FILENAME_LENGTH;
         p_file_size_buffer++;
      } /* for (i = 0; i < files_to_send; i++) */

      /* Do not forget to remove lock file if we have created one */
      if ((db.lock == LOCKFILE) && (fsa[db.fsa_pos].active_transfers == 1))
      {
         if (unlink(lockfile) == -1)
         {
            trans_log(ERROR_SIGN, __FILE__, __LINE__,
                      "Failed to unlink() lock file <%s> : %s",
                      lockfile, strerror(errno));
            trans_log(INFO_SIGN, NULL, 0, "%d Bytes copied in %d file(s).",
                      fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done,
                      fsa[db.fsa_pos].job_status[(int)db.job_no].no_of_files_done);
            exit(REMOVE_LOCKFILE_ERROR);
         }
         else
         {
            if (fsa[db.fsa_pos].debug == YES)
            {
               trans_db_log(INFO_SIGN, __FILE__, __LINE__,
                            "Removed lock file <%s>.", lockfile);
            }
         }
      }

      /*
       * Remove file directory.
       */
      if (rmdir(file_path) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to rmdir() <%s> : %s", file_path, strerror(errno));
      }
#ifdef _WITH_BURST_2
      burst_2_counter++;
   } while (check_burst_2(file_path, &files_to_send, NULL) == YES);
   burst_2_counter--;

   i = sprintf(ff_name, "%-*s[%d]: %lu Bytes copied in %d file(s).",
               MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
               fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done,
               fsa[db.fsa_pos].job_status[(int)db.job_no].no_of_files_done);
   if (burst_2_counter == 1)
   {
      (void)strcat(ff_name, " [BURST]");
   }
   else if (burst_2_counter > 1)
        {
           (void)sprintf(ff_name + i, " [BURST * %u]", burst_2_counter);
        }

   (void)rec(transfer_log_fd, INFO_SIGN, "%s\n", ff_name);
#else

   trans_log(INFO_SIGN, NULL, 0, "%d Bytes copied in %d file(s).",
             fsa[db.fsa_pos].job_status[(int)db.job_no].file_size_done, i);
#endif /* _WITH_BURST_2 */

   exitflag = 0;
   exit(TRANSFER_SUCCESS);
}


/*+++++++++++++++++++++++++++++ sf_loc_exit() +++++++++++++++++++++++++++*/
static void
sf_loc_exit(void)
{
   int  fd;
   char sf_fin_fifo[MAX_PATH_LENGTH];

   reset_fsa((struct job *)&db, exitflag);

   if (db.trans_rename_rule[0] != '\0')
   {
      (void)close(counter_fd);
   }
   if (file_name_buffer != NULL)
   {
      free(file_name_buffer);
   }
   if (file_size_buffer != NULL)
   {
      free(file_size_buffer);
   }

   (void)strcpy(sf_fin_fifo, p_work_dir);
   (void)strcat(sf_fin_fifo, FIFO_DIR);
   (void)strcat(sf_fin_fifo, SF_FIN_FIFO);
   if ((fd = open(sf_fin_fifo, O_RDWR)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not open fifo <%s> : %s", sf_fin_fifo, strerror(errno));
   }
   else
   {
#ifdef _FIFO_DEBUG
      char  cmd[2];

      cmd[0] = ACKN; cmd[1] = '\0';
      show_fifo_data('W', "sf_fin", cmd, 1, __FILE__, __LINE__);
#endif
      /* Tell FD we are finished */
      if (write(fd, &db.my_pid, sizeof(pid_t)) != sizeof(pid_t))
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "write() error : %s", strerror(errno));
      }
      (void)close(fd);
   }
   if (sys_log_fd != STDERR_FILENO)
   {
      (void)close(sys_log_fd);
   }

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   reset_fsa((struct job *)&db, IS_FAULTY_VAR);
   system_log(DEBUG_SIGN, __FILE__, __LINE__,
              "Aaarrrggh! Received SIGSEGV. Remove the programmer who wrote this!");
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   reset_fsa((struct job *)&db, IS_FAULTY_VAR);
   system_log(DEBUG_SIGN, __FILE__, __LINE__, "Uuurrrggh! Received SIGBUS.");
   abort();
}


/*++++++++++++++++++++++++++++++ sig_kill() +++++++++++++++++++++++++++++*/
static void
sig_kill(int signo)
{
   reset_fsa((struct job *)&db, IS_FAULTY_VAR);
   exit(GOT_KILLED);
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   reset_fsa((struct job *)&db, IS_FAULTY_VAR);
   exit(INCORRECT);
}
