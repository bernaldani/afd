/*
 *  sf_wmo.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998, 1999 Deutscher Wetterdienst (DWD),
 *                           Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   sf_wmo - send files via TCP according WMO regulations
 **
 ** SYNOPSIS
 **   sf_wmo [options]
 **
 **   options
 **       --version               - Version
 **       -w directory            - the working directory of the
 **                                 AFD
 **
 ** DESCRIPTION
 **   sf_wmo sends the given files to the defined recipient via TCP
 **   according WMO regulations.
 **
 **   In the message file will be the data it needs about the
 **   remote host in the following format:
 **       [destination]
 **       <scheme>://<user>:<password>@<host>:<port>/<url-path>
 **
 **       [options]
 **       <a list of FD options, terminated by a newline>
 **
 **   If the archive flag is set, each file will be archived after it
 **   has been send successful.
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has
 **   occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.05.1998 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                     /* fprintf(), sprintf()           */
#include <string.h>                    /* strcpy(), strcat(), strcmp(),  */
                                       /* strerror()                     */
#include <stdlib.h>                    /* malloc(), free(), abort()      */
#include <ctype.h>                     /* isdigit()                      */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _OUTPUT_LOG
#include <sys/times.h>                 /* times(), struct tms            */
#endif
#include <fcntl.h>
#include <signal.h>                    /* signal()                       */
#include <unistd.h>                    /* remove(), close(), getpid()    */
#include <errno.h>
#include "fddefs.h"
#include "wmodefs.h"
#include "version.h"

/* Global variables */
int                        counter_fd,    /* NOT USED */
                           no_of_hosts,   /* This variable is not used   */
                                          /* in this module.             */
                           rule_pos,
                           fsa_id,
                           fsa_fd = -1,
                           sys_log_fd = STDERR_FILENO,
                           transfer_log_fd = STDERR_FILENO,
                           trans_db_log_fd = STDERR_FILENO,
                           amg_flag = NO,
                           timeout_flag;
#ifndef _NO_MMAP
off_t                      fsa_size;
#endif
off_t                      *file_size_buffer = NULL;
long                       connect_timeout;
char                       host_deleted = NO,
                           err_msg_dir[MAX_PATH_LENGTH],
                           *p_work_dir,
                           tr_hostname[MAX_HOSTNAME_LENGTH + 1],
                           line_buffer[4096],
                           *file_name_buffer = NULL;
struct filetransfer_status *fsa;
struct job                 db;
struct rule                *rule;
#ifdef _DELETE_LOG
struct delete_log          dl;
#endif

#ifdef _WITH_WMO_SUPPORT
/* Local functions */
static void sf_wmo_exit(void),
            sig_bus(int),
            sig_segv(int),
            sig_pipe(int),
            sig_kill(int),
            sig_exit(int);
#endif


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
#ifdef _WITH_WMO_SUPPORT
#ifdef _VERIFY_FSA
   unsigned int     ui_variable;
#endif
   int              j,
                    fd,
                    status,
                    no_of_bytes,
                    loops,
                    rest,
                    files_to_send,
                    files_send = 0,
#ifdef _BURST_MODE
                    total_files_send = 0,
                    burst_counter = 0,
#endif
                    blocksize,
                    wmo_counter_fd = -1;
   off_t            lock_offset;
#ifdef _OUTPUT_LOG
   int              ol_fd = -1;
   unsigned int     *ol_job_number = NULL;
   char             *ol_data = NULL,
                    *ol_file_name = NULL;
   unsigned short   *ol_file_name_length;
   off_t            *ol_file_size = NULL;
   size_t           ol_size,
                    ol_real_size;
   clock_t          end_time,
                    start_time = 0,
                    *ol_transfer_time = NULL;
   struct tms       tmsdummy;
#endif
   off_t            *p_file_size_buffer;
   char             *p_file_name_buffer,
#ifdef _BURST_MODE
                    search_for_files = NO,
#endif
                    *buffer,
                    fullname[MAX_PATH_LENGTH],
                    file_path[MAX_PATH_LENGTH],
                    work_dir[MAX_PATH_LENGTH];
   struct stat      stat_buf;
   struct job       *p_db;
#ifdef SA_FULLDUMP
   struct sigaction sact;
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
      (void)rec(sys_log_fd, FATAL_SIGN, "sigaction() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#endif

   /* Do some cleanups when we exit */
   if (atexit(sf_wmo_exit) != 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not register exit function : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Initialise variables */
   p_work_dir = work_dir;
   init_sf(argc, argv, file_path, &blocksize, &files_to_send, WMO);
   p_db = &db;

   if ((signal(SIGINT, sig_kill) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR) ||
       (signal(SIGPIPE, sig_pipe) == SIG_ERR))
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "signal() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Set WMO timeout value */
   connect_timeout = fsa[(int)db.position].transfer_timeout;

#ifdef _OUTPUT_LOG
   if (db.output_log == YES)
   {
      output_log_ptrs(&ol_fd,                /* File descriptor to fifo */
                      &ol_job_number,
                      &ol_data,              /* Pointer to buffer       */
                      &ol_file_name,
                      &ol_file_name_length,
                      &ol_file_size,
                      &ol_size,
                      &ol_transfer_time,
                      db.host_alias,
                      WMO);
   }
#endif

   timeout_flag = OFF;

   /* Now determine the real hostname. */
   if (db.toggle_host == YES)
   {
      if (fsa[(int)db.position].host_toggle == HOST_ONE)
      {
         (void)strcpy(db.hostname,
                      fsa[(int)db.position].real_hostname[HOST_TWO - 1]);
      }
      else
      {
         (void)strcpy(db.hostname,
                      fsa[(int)db.position].real_hostname[HOST_ONE - 1]);
      }
   }
   else
   {
      (void)strcpy(db.hostname,
                   fsa[(int)db.position].real_hostname[(int)(fsa[(int)db.position].host_toggle - 1)]);
   }

   /* Connect to remote WMO-server */
   if ((status = wmo_connect(db.hostname, db.port)) != SUCCESS)
   {
      if (fsa[(int)db.position].debug == YES)
      {
         if (timeout_flag == OFF)
         {
            (void)rec(trans_db_log_fd, ERROR_SIGN,
                      "%-*s[%d]: Could not connect to %s at port %d (%d). (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname,
                      (int)db.job_no, db.hostname,
                      db.port, status, __FILE__, __LINE__);
         }
         else
         {
            (void)rec(trans_db_log_fd, ERROR_SIGN,
                      "%-*s[%d]: Could not connect to %s at port %d due to timeout. (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname,
                      (int)db.job_no, db.hostname,
                      db.port, __FILE__, __LINE__);
         }
      }
      if (timeout_flag == OFF)
      {
         /*
          * The wmo_connect() function wrote to the log file if status
          * is INCORRECT. Thus it is not necessary to do it here again.
          */
      }
      else
      {
         (void)rec(transfer_log_fd, ERROR_SIGN,
                   "%-*s[%d]: Failed to connect to port %d due to timeout. #%d (%s %d)\n",
                   MAX_HOSTNAME_LENGTH, tr_hostname,
                   (int)db.job_no, db.port, db.job_id,
                   __FILE__, __LINE__);
      }
      reset_fsa(p_db, YES, NO_OF_FILES_VAR | CONNECT_STATUS_VAR);
      exit(CONNECT_ERROR);
   }
   else
   {
      if (fsa[(int)db.position].debug == YES)
      {
         (void)rec(trans_db_log_fd, INFO_SIGN,
                   "%-*s[%d]: Connected to port %d. (%s %d)\n",
                   MAX_HOSTNAME_LENGTH, tr_hostname,
                   (int)db.job_no, db.port, __FILE__, __LINE__);
      }
   }

   /* Inform FSA that we have finished connecting and */
   /* will now start to transfer data.                */
   if (host_deleted == NO)
   {
      lock_offset = (char *)&fsa[(int)db.position] - (char *)fsa;
      rlock_region(fsa_fd, lock_offset);
      if (check_fsa() == YES)
      {
         if ((db.position = get_position(fsa, db.host_alias, no_of_hosts)) == INCORRECT)
         {
            host_deleted = YES;
         }
         else
         {
            lock_offset = (char *)&fsa[(int)db.position] - (char *)fsa;
            rlock_region(fsa_fd, lock_offset);
         }
      }
      if (host_deleted == NO)
      {
         fsa[(int)db.position].job_status[(int)db.job_no].connect_status = WMO_ACTIVE;
         fsa[(int)db.position].job_status[(int)db.job_no].no_of_files = files_to_send;

         /* Number of connections */
         lock_region_w(fsa_fd, (char *)&fsa[(int)db.position].connections - (char *)fsa);
         fsa[(int)db.position].connections += 1;
         unlock_region(fsa_fd, (char *)&fsa[(int)db.position].connections - (char *)fsa);
         unlock_region(fsa_fd, lock_offset);
      }
   }

   /* Allocate buffer to read data from the source file. */
   if ((buffer = malloc(blocksize + 1 + 4 /* for bulletin end */)) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "malloc() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(ALLOC_ERROR);
   }

   if (db.special_flag & WITH_SEQUENCE_NUMBER)
   {
      char counter_file_name[MAX_FILENAME_LENGTH];

      (void)sprintf(counter_file_name, "/%s.%d", db.host_alias, db.port);

      if ((wmo_counter_fd = open_counter_file(counter_file_name)) < 0)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to open counter file %s. (%s %d)\n",
                   counter_file_name, __FILE__, __LINE__);
      }
   }

#ifdef _BURST_MODE
   do
   {
      if (search_for_files == YES)
      {
         off_t file_size_to_send = 0;

         lock_offset = (char *)&fsa[(int)db.position] - (char *)fsa;
         rlock_region(fsa_fd, lock_offset);
         if (check_fsa() == YES)
         {
            if ((db.position = get_position(fsa, db.host_alias, no_of_hosts)) == INCORRECT)
            {
               host_deleted = YES;
               lock_offset = -1;
            }
            else
            {
               lock_offset = (char *)&fsa[(int)db.position] - (char *)fsa;
               rlock_region(fsa_fd, lock_offset);
               lock_region_w(fsa_fd, (char *)&fsa[(int)db.position].job_status[(int)db.job_no].job_id - (char *)fsa);
            }
         }
         if ((files_to_send = get_file_names(file_path, &file_size_to_send)) < 1)
         {
            /*
             * With age limit it can happen that files_to_send is zero.
             * Though very unlikely.
             */
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "Hmmm. Burst counter = %d and files_to_send = %d [%s]. How is this possible? AAarrgghhhhh.... (%s %d)\n",
                      fsa[(int)db.position].job_status[(int)db.job_no].burst_counter,
                      files_to_send, file_path,
                      __FILE__, __LINE__);
            fsa[(int)db.position].job_status[(int)db.job_no].burst_counter = 0;
            if (lock_offset != -1)
            {
               unlock_region(fsa_fd, lock_offset);
            }
            break;
         }
         burst_counter = fsa[(int)db.position].job_status[(int)db.job_no].burst_counter;
         unlock_region(fsa_fd, (char *)&fsa[(int)db.position].job_status[(int)db.job_no].job_id - (char *)fsa);

         total_files_send += files_send;

         /* Tell user we are bursting */
         if (host_deleted == NO)
         {
            if (check_fsa() == YES)
            {
               if ((db.position = get_position(fsa, db.host_alias, no_of_hosts)) == INCORRECT)
               {
                  host_deleted = YES;
                  lock_offset = -1;
               }
               else
               {
                  lock_offset = (char *)&fsa[(int)db.position] - (char *)fsa;
                  rlock_region(fsa_fd, lock_offset);
               }
            }
            if (host_deleted == NO)
            {
               fsa[(int)db.position].job_status[(int)db.job_no].connect_status = WMO_BURST_TRANSFER_ACTIVE;
               fsa[(int)db.position].job_status[(int)db.job_no].no_of_files = fsa[(int)db.position].job_status[(int)db.job_no].no_of_files_done + files_to_send;
               fsa[(int)db.position].job_status[(int)db.job_no].file_size = fsa[(int)db.position].job_status[(int)db.job_no].file_size_done + file_size_to_send;
            }
            if (fsa[(int)db.position].debug == YES)
            {
               (void)rec(trans_db_log_fd, INFO_SIGN,
                         "%-*s[%d]: Bursting. (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname,
                         (int)db.job_no, __FILE__, __LINE__);
            }
         }

         if (lock_offset != -1)
         {
            unlock_region(fsa_fd, lock_offset);
         }
      }
#endif

      /* Send all files */
      p_file_name_buffer = file_name_buffer;
      p_file_size_buffer = file_size_buffer;
      for (files_send = 0; files_send < files_to_send; files_send++)
      {
         (void)sprintf(fullname, "%s/%s", file_path, p_file_name_buffer);

         if (*p_file_size_buffer > 0)
         {
            int end_length = 0,
                header_length = 0,
                length_type_indicator = 10;

            /* Write status to FSA? */
            if (host_deleted == NO)
            {
#ifdef _SAVE_FSA_WRITE
               lock_offset = (char *)&fsa[(int)db.position] - (char *)fsa;
               rlock_region(fsa_fd, lock_offset);
#endif
               if (check_fsa() == YES)
               {
                  if ((db.position = get_position(fsa, db.host_alias, no_of_hosts)) == INCORRECT)
                  {
                     host_deleted = YES;
                  }
#ifdef _SAVE_FSA_WRITE
                  else
                  {
                     lock_offset = (char *)&fsa[(int)db.position] - (char *)fsa;
                     rlock_region(fsa_fd, lock_offset);
                  }
#endif
               }
               if (host_deleted == NO)
               {
                  fsa[(int)db.position].job_status[(int)db.job_no].file_size_in_use = *p_file_size_buffer;
                  (void)strcpy(fsa[(int)db.position].job_status[(int)db.job_no].file_name_in_use,
                               p_file_name_buffer);
#ifdef _SAVE_FSA_WRITE
                  unlock_region(fsa_fd, lock_offset);
#endif
               }
            }

            /* Open local file */
            if ((fd = open(fullname, O_RDONLY)) == -1)
            {
               if (fsa[(int)db.position].debug == YES)
               {
                  (void)rec(trans_db_log_fd, INFO_SIGN,
                            "%-*s[%d]: Failed to open local file %s (%s %d)\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname,
                            (int)db.job_no, fullname, __FILE__, __LINE__);
               }
               (void)rec(transfer_log_fd, INFO_SIGN,
                         "%-*s[%d]: %d Bytes send in %d file(s).\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         fsa[(int)db.position].job_status[(int)db.job_no].file_size_done,
                         fsa[(int)db.position].job_status[(int)db.job_no].no_of_files_done);
               wmo_quit();
               reset_fsa(p_db, YES, (CONNECT_STATUS_VAR |
                                    NO_OF_FILES_VAR |
                                    NO_OF_FILES_DONE_VAR |
                                    FILE_SIZE_DONE_VAR |
                                    FILE_SIZE_IN_USE_VAR |
                                    FILE_SIZE_IN_USE_DONE_VAR |
                                    FILE_NAME_IN_USE_VAR));
               exit(OPEN_LOCAL_ERROR);
            }
            if (fsa[(int)db.position].debug == YES)
            {
               (void)rec(trans_db_log_fd, INFO_SIGN,
                         "%-*s[%d]: Open local file %s (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname,
                         (int)db.job_no, fullname, __FILE__, __LINE__);
            }

#ifdef _OUTPUT_LOG
            if (db.output_log == YES)
            {
               start_time = times(&tmsdummy);
            }
#endif

            /*
             * When the contents does not contain a bulletin header
             * it must be stored in the file name.
             */
            if (db.special_flag & FILE_NAME_IS_HEADER)
            {
               char *ptr = p_file_name_buffer;

               buffer[length_type_indicator] = 1; /* SOH */
               buffer[length_type_indicator + 1] = '\015'; /* CR */
               buffer[length_type_indicator + 2] = '\015'; /* CR */
               buffer[length_type_indicator + 3] = '\012'; /* LF */
               header_length = 4;

               if (wmo_counter_fd > 0)
               {
                  int wmo_counter;

                  if ((wmo_counter = next_wmo_counter(wmo_counter_fd)) < 0)
                  {
                     if (close(wmo_counter_fd) == -1)
                     {
                        (void)rec(sys_log_fd, DEBUG_SIGN,
                                  "close() error : %s (%s %d)\n",
                                  strerror(errno), __FILE__, __LINE__);
                     }
                     wmo_counter_fd = -1;
                     (void)rec(sys_log_fd, ERROR_SIGN,
                               "Failed to get next WMO counter. (%s %d)\n",
                               __FILE__, __LINE__);
                  }
                  else
                  {
                     if (wmo_counter < 10)
                     {
                        buffer[length_type_indicator + header_length] = '0';
                        buffer[length_type_indicator + header_length + 1] = '0';
                        buffer[length_type_indicator + header_length + 2] = wmo_counter + '0';
                     }
                     else if (wmo_counter < 100)
                          {
                             buffer[length_type_indicator + header_length] = '0';
                             buffer[length_type_indicator + header_length + 1] = (wmo_counter / 10) + '0';
                             buffer[length_type_indicator + header_length + 2] = (wmo_counter % 10) + '0';
                          }
                          else if (wmo_counter < 1000)
                          {
                             buffer[length_type_indicator + header_length] = ((wmo_counter / 100) % 10) + '0';
                             buffer[length_type_indicator + header_length + 1] = ((wmo_counter / 10) % 10) + '0';
                             buffer[length_type_indicator + header_length + 2] = (wmo_counter % 10) + '0';
                          }
                     header_length += 3;
                     buffer[length_type_indicator + header_length] = '\015'; /* CR */
                     header_length++;
                     buffer[length_type_indicator + header_length] = '\015'; /* CR */
                     header_length++;
                     buffer[length_type_indicator + header_length] = '\012'; /* LF */
                     header_length++;
                  }
               } /* if (wmo_counter_fd > 0) */

               for (;;)
               {
                  while ((*ptr != '_') && (*ptr != '-') &&
                         (*ptr != ' ') && (*ptr != '\0'))
                  {
                     buffer[length_type_indicator + header_length] = *ptr;
                     header_length++; ptr++;
                  }
                  if (*ptr == '\0')
                  {
                     break;
                  }
                  else
                  {
                     buffer[length_type_indicator + header_length] = ' ';
                     header_length++; ptr++;
                  }
               } /* for (;;) */
               buffer[length_type_indicator + header_length] = '\015'; /* CR */
               header_length++;
               buffer[length_type_indicator + header_length] = '\015'; /* CR */
               header_length++;
               buffer[length_type_indicator + header_length] = '\012'; /* LF */
               header_length++;
               end_length = 4;
            }

            /* Read (local) and write (remote) file */
            no_of_bytes = 0;
            loops = (length_type_indicator + header_length + *p_file_size_buffer) / blocksize;
            rest = (length_type_indicator + header_length + *p_file_size_buffer) % blocksize;

            if ((db.special_flag & FILE_NAME_IS_HEADER) && (rest == 0))
            {
               loops--;
               rest = blocksize;
            }

            /* Write length and type indicator. */
            (void)sprintf(buffer, "%08lu",
                          (unsigned long)(*p_file_size_buffer + header_length + end_length));
            if (db.transfer_mode == 'I')
            {
               buffer[length_type_indicator - 2] = 'B';
               buffer[length_type_indicator - 1] = 'I';
            }
            else if (db.transfer_mode == 'A')
                 {
                    buffer[length_type_indicator - 2] = 'A';
                    buffer[length_type_indicator - 1] = 'N';
                 }
                 else
                 {
                    buffer[length_type_indicator - 2] = 'F';
                    buffer[length_type_indicator - 1] = 'X';
                 }

            for (;;)
            {
               for (j = 0; j < loops; j++)
               {
#ifdef _SIMULATE_SLOW_TRANSFER
                  (void)sleep(2);
#endif
                  if ((status = read(fd, (buffer + length_type_indicator + header_length),
                                     (blocksize - length_type_indicator - header_length))) != (blocksize - length_type_indicator - header_length))
                  {
                     if (fsa[(int)db.position].debug == YES)
                     {
                        (void)rec(trans_db_log_fd, ERROR_SIGN,
                                  "%-*s[%d]: Could not read local file %s [%d] : %s (%s %d)\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  (int)db.job_no, fullname, status,
                                  strerror(errno), __FILE__, __LINE__);
                        (void)rec(trans_db_log_fd, ERROR_SIGN,
                                  "%-*s[%d]: blocksize = %d  length_type_indicator = %d   header_length = %d\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  (int)db.job_no, blocksize,
                                  length_type_indicator, header_length);
                     }
                     (void)rec(transfer_log_fd, ERROR_SIGN,
                               "%-*s[%d]: Could not read local file %s [%d] : %s #%d (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname,
                               (int)db.job_no, fullname, status,
                               strerror(errno), db.job_id,
                                __FILE__, __LINE__);
                     (void)rec(transfer_log_fd, INFO_SIGN,
                               "%-*s[%d]: %d Bytes send in %d file(s).\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                               fsa[(int)db.position].job_status[(int)db.job_no].file_size_done,
                               fsa[(int)db.position].job_status[(int)db.job_no].no_of_files_done);
                     wmo_quit();
                     reset_fsa(p_db, YES, (CONNECT_STATUS_VAR |
                                          NO_OF_FILES_VAR |
                                          FILE_SIZE_DONE_VAR |
                                          FILE_SIZE_IN_USE_VAR |
                                          FILE_SIZE_IN_USE_DONE_VAR |
                                          FILE_NAME_IN_USE_VAR));
                     exit(READ_LOCAL_ERROR);
                  }
                  if (wmo_write(buffer, blocksize) < 0)
                  {
                     if (fsa[(int)db.position].debug == YES)
                     {
                        if (timeout_flag == OFF)
                        {
                           (void)rec(trans_db_log_fd, ERROR_SIGN,
                                     "%-*s[%d]: Failed to write to remote port %d (%s %d)\n",
                                     MAX_HOSTNAME_LENGTH, tr_hostname,
                                     (int)db.job_no, db.port,
                                     __FILE__, __LINE__);
                        }
                        else
                        {
                           (void)rec(trans_db_log_fd, ERROR_SIGN,
                                     "%-*s[%d]: Failed to write to remote port %d due to timeout. (%s %d)\n",
                                     MAX_HOSTNAME_LENGTH, tr_hostname,
                                     (int)db.job_no, db.port,
                                     __FILE__, __LINE__);
                        }
                     }
                     (void)rec(transfer_log_fd, INFO_SIGN,
                               "%-*s[%d]: %d Bytes send in %d file(s).\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                               fsa[(int)db.position].job_status[(int)db.job_no].file_size_done,
                               fsa[(int)db.position].job_status[(int)db.job_no].no_of_files_done);
                     if (timeout_flag == OFF)
                     {
                        (void)rec(transfer_log_fd, ERROR_SIGN,
                                  "%-*s[%d]: Failed to write to remote port %d #%d (%s %d)\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  (int)db.job_no, db.port,
                                  db.job_id, __FILE__, __LINE__);
                     }
                     else
                     {
                        (void)rec(transfer_log_fd, ERROR_SIGN,
                                  "%-*s[%d]: Failed to write to remote port %d due to timeout. #%d (%s %d)\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  (int)db.job_no, db.port,
                                  db.job_id, __FILE__, __LINE__);
                     }
                     wmo_quit();
                     reset_fsa(p_db, YES, (CONNECT_STATUS_VAR |
                                          NO_OF_FILES_VAR |
                                          NO_OF_FILES_DONE_VAR |
                                          FILE_SIZE_DONE_VAR |
                                          FILE_SIZE_IN_USE_VAR |
                                          FILE_SIZE_IN_USE_DONE_VAR |
                                          FILE_NAME_IN_USE_VAR));
                     exit(WRITE_REMOTE_ERROR);
                  }

                  no_of_bytes += blocksize;

                  if (host_deleted == NO)
                  {
#ifdef _SAVE_FSA_WRITE
                     lock_offset = (char *)&fsa[(int)db.position] - (char *)fsa;
                     rlock_region(fsa_fd, lock_offset);
#endif
                     if (check_fsa() == YES)
                     {
                        if ((db.position = get_position(fsa, db.host_alias, no_of_hosts)) == INCORRECT)
                        {
                           host_deleted = YES;
                        }
#ifdef _SAVE_FSA_WRITE
                        else
                        {
                           lock_offset = (char *)&fsa[(int)db.position] - (char *)fsa;
                           rlock_region(fsa_fd, lock_offset);
                        }
#endif
                     }
                     if (host_deleted == NO)
                     {
                        fsa[(int)db.position].job_status[(int)db.job_no].file_size_in_use_done = no_of_bytes;
                        fsa[(int)db.position].job_status[(int)db.job_no].file_size_done += blocksize;
                        fsa[(int)db.position].job_status[(int)db.job_no].bytes_send += blocksize;
#ifdef _SAVE_FSA_WRITE
                        unlock_region(fsa_fd, lock_offset);
#endif
                     }
                  }
                  if (length_type_indicator > 0)
                  {
                     length_type_indicator = 0;
                     header_length = 0;
                  }
               } /* for (j = 0; j < loops; j++) */

               if (rest > 0)
               {
                  if ((status = read(fd,
                                     (buffer + length_type_indicator + header_length),
                                     (rest - length_type_indicator - header_length))) != (rest - length_type_indicator - header_length))
                  {
                     if (fsa[(int)db.position].debug == YES)
                     {
                        (void)rec(trans_db_log_fd, ERROR_SIGN,
                                  "%-*s[%d]: Could not read local file %s [%d] : %s (%s %d)\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  (int)db.job_no, fullname, status,
                                  strerror(errno), __FILE__, __LINE__);
                        (void)rec(trans_db_log_fd, ERROR_SIGN,
                                  "%-*s[%d]: blocksize = %d  length_type_indicator = %d   header_length = %d\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  (int)db.job_no, blocksize,
                                  length_type_indicator, header_length);
                     }
                     (void)rec(transfer_log_fd, ERROR_SIGN,
                               "%-*s[%d]: Could not read local file %s [%d] : %s #%d (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname,
                               (int)db.job_no, fullname, status,
                               strerror(errno), db.job_id, __FILE__, __LINE__);
                     (void)rec(transfer_log_fd, INFO_SIGN,
                               "%-*s[%d]: %d Bytes send in %d file(s).\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                               fsa[(int)db.position].job_status[(int)db.job_no].file_size_done,
                               fsa[(int)db.position].job_status[(int)db.job_no].no_of_files_done);
                     wmo_quit();
                     reset_fsa(p_db, YES, (CONNECT_STATUS_VAR |
                                          NO_OF_FILES_VAR |
                                          NO_OF_FILES_DONE_VAR |
                                          FILE_SIZE_DONE_VAR |
                                          FILE_SIZE_IN_USE_VAR |
                                          FILE_SIZE_IN_USE_DONE_VAR |
                                          FILE_NAME_IN_USE_VAR));
                     exit(READ_LOCAL_ERROR);
                  }
                  if (end_length == 4)
                  {
                     buffer[rest] = '\015';
                     buffer[rest + 1] = '\015';
                     buffer[rest + 2] = '\012';
                     buffer[rest + 3] = 3;  /* ETX */
                  }
                  if (wmo_write(buffer, rest + end_length) < 0)
                  {
                     if (fsa[(int)db.position].debug == YES)
                     {
                        if (timeout_flag == OFF)
                        {
                           (void)rec(trans_db_log_fd, ERROR_SIGN,
                                     "%-*s[%d]: Failed to write to remote port %d (%s %d)\n",
                                     MAX_HOSTNAME_LENGTH, tr_hostname,
                                     (int)db.job_no, db.port,
                                     __FILE__, __LINE__);
                        }
                        else
                        {
                           (void)rec(trans_db_log_fd, ERROR_SIGN,
                                     "%-*s[%d]: Failed to write to remote port %d due to timeout. (%s %d)\n",
                                     MAX_HOSTNAME_LENGTH, tr_hostname,
                                     (int)db.job_no, db.port,
                                     __FILE__, __LINE__);
                        }
                     }
                     (void)rec(transfer_log_fd, INFO_SIGN,
                               "%-*s[%d]: %d Bytes send in %d file(s).\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                               fsa[(int)db.position].job_status[(int)db.job_no].file_size_done,
                               fsa[(int)db.position].job_status[(int)db.job_no].no_of_files_done);
                     if (timeout_flag == OFF)
                     {
                        (void)rec(transfer_log_fd, ERROR_SIGN,
                                  "%-*s[%d]: Failed to write to remote port %d #%d (%s %d)\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  (int)db.job_no, db.port,
                                  db.job_id, __FILE__, __LINE__);
                     }
                     else
                     {
                        (void)rec(transfer_log_fd, ERROR_SIGN,
                                  "%-*s[%d]: Failed to write to remote port %d due to timeout. #%d (%s %d)\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  (int)db.job_no, db.port,
                                  db.job_id, __FILE__, __LINE__);
                     }
                     wmo_quit();
                     reset_fsa(p_db, YES, (CONNECT_STATUS_VAR |
                                          NO_OF_FILES_VAR |
                                          NO_OF_FILES_DONE_VAR |
                                          FILE_SIZE_DONE_VAR |
                                          FILE_SIZE_IN_USE_VAR |
                                          FILE_SIZE_IN_USE_DONE_VAR |
                                          FILE_NAME_IN_USE_VAR));
                     exit(WRITE_REMOTE_ERROR);
                  }

                  no_of_bytes += rest + end_length;

                  if (host_deleted == NO)
                  {
#ifdef _SAVE_FSA_WRITE
                     lock_offset = (char *)&fsa[(int)db.position] - (char *)fsa;
                     rlock_region(fsa_fd, lock_offset);
#endif
                     if (check_fsa() == YES)
                     {
                        if ((db.position = get_position(fsa, db.host_alias, no_of_hosts)) == INCORRECT)
                        {
                           host_deleted = YES;
                        }
#ifdef _SAVE_FSA_WRITE
                        else
                        {
                           lock_offset = (char *)&fsa[(int)db.position] - (char *)fsa;
                           rlock_region(fsa_fd, lock_offset);
                        }
#endif
                     }
                     if (host_deleted == NO)
                     {
                        fsa[(int)db.position].job_status[(int)db.job_no].file_size_in_use_done = no_of_bytes;
                        fsa[(int)db.position].job_status[(int)db.job_no].file_size_done += rest;
                        fsa[(int)db.position].job_status[(int)db.job_no].bytes_send += rest;
#ifdef _SAVE_FSA_WRITE
                        unlock_region(fsa_fd, lock_offset);
#endif
                     }
                  }
               }

               /*
                * Since there are always some users sending files to the
                * AFD not in dot notation, lets check here if this is really
                * the EOF.
                * If not lets continue so long until we hopefully have reached
                * the EOF.
                * NOTE: This is NOT a fool proof way. There must be a better
                *       way!
                */
               if (fstat(fd, &stat_buf) == -1)
               {
                  (void)rec(transfer_log_fd, DEBUG_SIGN,
                            "Hmmm. Failed to stat() %s : %s (%s %d)\n",
                            fullname, strerror(errno),
                            __FILE__, __LINE__);
                  break;
               }
               else
               {
                  if (stat_buf.st_size > *p_file_size_buffer)
                  {
                     loops = (stat_buf.st_size - *p_file_size_buffer) / blocksize;
                     rest = (stat_buf.st_size - *p_file_size_buffer) % blocksize;
                     *p_file_size_buffer = stat_buf.st_size;

                     /*
                      * Give a warning in the system log, so some action
                      * can be taken against the originator.
                      */
                     (void)rec(sys_log_fd, WARN_SIGN,
                               "File %s for host %s was DEFINITELY NOT send in dot notation. (%s %d)\n",
                               p_file_name_buffer,
                               fsa[(int)db.position].host_dsp_name,
                               __FILE__, __LINE__);
                  }
                  else
                  {
                     break;
                  }
               }
            } /* for (;;) */

            if (db.special_flag & WMO_CHECK_ACKNOWLEDGE)
            {
               int ret;

               if ((ret = wmo_check_reply()) == INCORRECT)
               {
                  if (fsa[(int)db.position].debug == YES)
                  {
                     if (timeout_flag == OFF)
                     {
                        (void)rec(trans_db_log_fd, ERROR_SIGN,
                                  "%-*s[%d]: Failed to receive reply from port %d (%s %d)\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  (int)db.job_no, db.port,
                                  __FILE__, __LINE__);
                     }
                     else
                     {
                        (void)rec(trans_db_log_fd, ERROR_SIGN,
                                  "%-*s[%d]: Failed to receive reply from remote port %d due to timeout. (%s %d)\n",
                                  MAX_HOSTNAME_LENGTH, tr_hostname,
                                  (int)db.job_no, db.port,
                                  __FILE__, __LINE__);
                     }
                  }
                  (void)rec(transfer_log_fd, INFO_SIGN,
                            "%-*s[%d]: %d Bytes send in %d file(s).\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                            fsa[(int)db.position].job_status[(int)db.job_no].file_size_done,
                            fsa[(int)db.position].job_status[(int)db.job_no].no_of_files_done);
                  if (timeout_flag == OFF)
                  {
                     (void)rec(transfer_log_fd, ERROR_SIGN,
                               "%-*s[%d]: Failed to receive reply from remote port %d #%d (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname,
                               (int)db.job_no, db.port,
                               db.job_id, __FILE__, __LINE__);
                  }
                  else
                  {
                     (void)rec(transfer_log_fd, ERROR_SIGN,
                               "%-*s[%d]: Failed to receive reply from remote port %d due to timeout. #%d (%s %d)\n",
                               MAX_HOSTNAME_LENGTH, tr_hostname,
                               (int)db.job_no, db.port,
                               db.job_id, __FILE__, __LINE__);
                  }
                  wmo_quit();
                  reset_fsa(p_db, YES, (CONNECT_STATUS_VAR |
                                       NO_OF_FILES_VAR |
                                       NO_OF_FILES_DONE_VAR |
                                       FILE_SIZE_DONE_VAR |
                                       FILE_SIZE_IN_USE_VAR |
                                       FILE_SIZE_IN_USE_DONE_VAR |
                                       FILE_NAME_IN_USE_VAR));
                  exit(CHECK_REPLY_ERROR);
               }
               else if (ret == NEGATIV_ACKNOWLEDGE)
                    {
                       (void)rec(transfer_log_fd, ERROR_SIGN,
                                 "%-*s[%d]: Received negative acknowledge from remote site for file %s. #%d (%s %d)\n",
                                 MAX_HOSTNAME_LENGTH, tr_hostname,
                                 (int)db.job_no, p_file_name_buffer,
                                 db.job_id, __FILE__, __LINE__);
                    }
            }

#ifdef _OUTPUT_LOG
            if (db.output_log == YES)
            {
               end_time = times(&tmsdummy);
            }
#endif

            /* Close local file */
            if (close(fd) == -1)
            {
               (void)rec(transfer_log_fd, WARN_SIGN,
                         "%-*s[%d]: Failed to close() local file %s : %s (%s %d)\n",
                         MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                         p_file_name_buffer, strerror(errno),
                         __FILE__, __LINE__);
               /*
                * Since we usually do not send more then 100 files and
                * sf_wmo() will exit(), there is no point in stopping
                * the transmission.
                */
            }
         }
         else
         {
            (void)rec(transfer_log_fd, WARN_SIGN,
                      "%-*s[%d]: File %s is of zero length, ignoring. (%s %d)\n",
                      MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                      p_file_name_buffer, __FILE__, __LINE__);
         }

         /* Update FSA, one file transmitted. */
         if (host_deleted == NO)
         {
            lock_offset = (char *)&fsa[(int)db.position] - (char *)fsa;
            rlock_region(fsa_fd, lock_offset);

            /* Before we read from the FSA lets make */
            /* sure that it is NOT stale!            */
            if (check_fsa() == YES)
            {
               if ((db.position = get_position(fsa, db.host_alias, no_of_hosts)) == INCORRECT)
               {
                  host_deleted = YES;
               }
               else
               {
                  lock_offset = (char *)&fsa[(int)db.position] - (char *)fsa;
                  rlock_region(fsa_fd, lock_offset);
               }
            }
            if (host_deleted == NO)
            {
               fsa[(int)db.position].job_status[(int)db.job_no].file_name_in_use[0] = '\0';
               fsa[(int)db.position].job_status[(int)db.job_no].no_of_files_done++;
               fsa[(int)db.position].job_status[(int)db.job_no].file_size_in_use = 0;
               fsa[(int)db.position].job_status[(int)db.job_no].file_size_in_use_done = 0;

               /* Total file counter */
               lock_region_w(fsa_fd, (char *)&fsa[(int)db.position].total_file_counter - (char *)fsa);
               fsa[(int)db.position].total_file_counter -= 1;
#ifdef _VERIFY_FSA
               if (fsa[(int)db.position].total_file_counter < 0)
               {
                  (void)rec(sys_log_fd, DEBUG_SIGN,
                            "Total file counter for host %s less then zero. Correcting to %d. (%s %d)\n",
                            fsa[(int)db.position].host_dsp_name,
                            files_to_send - (files_send + 1),
                            __FILE__, __LINE__);
                  fsa[(int)db.position].total_file_counter = files_to_send - (files_send + 1);
               }
#endif

               /* Total file size */
#ifdef _VERIFY_FSA
               ui_variable = fsa[(int)db.position].total_file_size;
#endif
               fsa[(int)db.position].total_file_size -= stat_buf.st_size;
#ifdef _VERIFY_FSA
               if (fsa[(int)db.position].total_file_size > ui_variable)
               {
                  int   k;
                  off_t *tmp_ptr = p_file_size_buffer;

                  tmp_ptr++;
                  fsa[(int)db.position].total_file_size = 0;
                  for (k = (files_send + 1); k < files_to_send; k++)
                  {     
                     fsa[(int)db.position].total_file_size += *tmp_ptr;
                  }

                  (void)rec(sys_log_fd, DEBUG_SIGN,
                            "Total file size for host %s overflowed. Correcting to %lu. (%s %d)\n",
                            fsa[(int)db.position].host_dsp_name,
                            fsa[(int)db.position].total_file_size,
                            __FILE__, __LINE__);
               }
               else if ((fsa[(int)db.position].total_file_counter == 0) &&
                        (fsa[(int)db.position].total_file_size > 0))
                    {
                       (void)rec(sys_log_fd, DEBUG_SIGN,
                                 "fc for host %s is zero but fs is not zero. Correcting. (%s %d)\n",
                                 fsa[(int)db.position].host_dsp_name,
                                 __FILE__, __LINE__);
                       fsa[(int)db.position].total_file_size = 0;
                    }
#endif
               unlock_region(fsa_fd, (char *)&fsa[(int)db.position].total_file_counter - (char *)fsa);

               /* File counter done */
               lock_region_w(fsa_fd, (char *)&fsa[(int)db.position].file_counter_done - (char *)fsa);
               fsa[(int)db.position].file_counter_done += 1;
               unlock_region(fsa_fd, (char *)&fsa[(int)db.position].file_counter_done - (char *)fsa);

               /* Number of bytes send */
               lock_region_w(fsa_fd, (char *)&fsa[(int)db.position].bytes_send - (char *)fsa);
               fsa[(int)db.position].bytes_send += stat_buf.st_size;
               unlock_region(fsa_fd, (char *)&fsa[(int)db.position].bytes_send - (char *)fsa);
               unlock_region(fsa_fd, lock_offset);
            }
         }

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
            if (archive_file(file_path, p_file_name_buffer, p_db) < 0)
            {
               if (fsa[(int)db.position].debug == YES)
               {
                  (void)rec(trans_db_log_fd, ERROR_SIGN,
                            "%-*s[%d]: Failed to archive file %s (%s %d)\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname,
                            (int)db.job_no, p_file_name_buffer,
                            __FILE__, __LINE__);
               }

#ifdef _OUTPUT_LOG
               if (db.output_log == YES)
               {
                  (void)strcpy(ol_file_name, p_file_name_buffer);
                  *ol_file_size = *p_file_size_buffer;
                  *ol_job_number = fsa[(int)db.position].job_status[(int)db.job_no].job_id;
                  *ol_transfer_time = end_time - start_time;
                  *ol_file_name_length = 0;
                  ol_real_size = strlen(p_file_name_buffer) + ol_size;
                  if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
                  {
                     (void)rec(sys_log_fd, ERROR_SIGN,
                               "write() error : %s (%s %d)\n",
                               strerror(errno), __FILE__, __LINE__);
                  }
               }
#endif
            }
            else
            {
               if (fsa[(int)db.position].debug == YES)
               {
                  (void)rec(trans_db_log_fd, INFO_SIGN,
                            "%-*s[%d]: Archived file %s (%s %d)\n",
                            MAX_HOSTNAME_LENGTH, tr_hostname,
                            (int)db.job_no, p_file_name_buffer,
                            __FILE__, __LINE__);
               }

#ifdef _OUTPUT_LOG
               if (db.output_log == YES)
               {
                  (void)strcpy(ol_file_name, p_file_name_buffer);
                  *ol_file_name_length = (unsigned short)strlen(ol_file_name);
                  (void)strcpy(&ol_file_name[*ol_file_name_length + 1],
                               &db.archive_dir[db.archive_offset]);
                  *ol_file_size = *p_file_size_buffer;
                  *ol_job_number = fsa[(int)db.position].job_status[(int)db.job_no].job_id;
                  *ol_transfer_time = end_time - start_time;
                  ol_real_size = *ol_file_name_length +
                                 strlen(&ol_file_name[*ol_file_name_length + 1]) +
                                 ol_size;
                  if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
                  {
                     (void)rec(sys_log_fd, ERROR_SIGN,
                               "write() error : %s (%s %d)\n",
                               strerror(errno), __FILE__, __LINE__);
                  }
               }
#endif
            }
         }
         else
         {
            /* Delete the file we just have send */
            if (remove(fullname) == -1)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Could not remove local file %s after sending it successfully : %s (%s %d)\n",
                         fullname, strerror(errno), __FILE__, __LINE__);
            }

#ifdef _OUTPUT_LOG
            if (db.output_log == YES)
            {
               (void)strcpy(ol_file_name, p_file_name_buffer);
               *ol_file_size = *p_file_size_buffer;
               *ol_job_number = fsa[(int)db.position].job_status[(int)db.job_no].job_id;
               *ol_transfer_time = end_time - start_time;
               *ol_file_name_length = 0;
               ol_real_size = strlen(ol_file_name) + ol_size;
               if (write(ol_fd, ol_data, ol_real_size) != ol_real_size)
               {
                  (void)rec(sys_log_fd, ERROR_SIGN,
                            "write() error : %s (%s %d)\n",
                            strerror(errno), __FILE__, __LINE__);
               }
            }
#endif
         }

         /*
          * After each successful transfer set error
          * counter to zero, so that other jobs can be
          * started.
          * Also move all, error entries back to the message
          * and file directories.
          */
         if ((*p_file_size_buffer > 0) &&
             (fsa[(int)db.position].error_counter > 0))
         {
            int  fd,
                 j;
            char fd_wake_up_fifo[MAX_PATH_LENGTH];

            lock_region_w(fsa_fd, (char *)&fsa[(int)db.position].error_counter - (char *)fsa);
            fsa[(int)db.position].error_counter = 0;

            /*
             * Wake up FD!
             */
            (void)sprintf(fd_wake_up_fifo, "%s%s%s", p_work_dir,
                          FIFO_DIR, FD_WAKE_UP_FIFO);
            if ((fd = open(fd_wake_up_fifo, O_RDWR)) == -1)
            {
               (void)rec(sys_log_fd, WARN_SIGN,
                         "Failed to open() FIFO %s : %s (%s %d)\n",
                         fd_wake_up_fifo, strerror(errno),
                         __FILE__, __LINE__);
            }
            else
            {
               char dummy;

               if (write(fd, &dummy, 1) != 1)
               {
                  (void)rec(sys_log_fd, WARN_SIGN,
                            "Failed to write() to FIFO %s : %s (%s %d)\n",
                            fd_wake_up_fifo, strerror(errno),
                            __FILE__, __LINE__);
               }
               if (close(fd) == -1)
               {
                  (void)rec(sys_log_fd, DEBUG_SIGN,
                            "Failed to close() FIFO %s : %s (%s %d)\n",
                            fd_wake_up_fifo, strerror(errno),
                            __FILE__, __LINE__);
               }
            }

            /*
             * Remove the error condition (NOT_WORKING) from all jobs
             * of this host.
             */
            for (j = 0; j < fsa[(int)db.position].allowed_transfers; j++)
            {
               if ((j != db.job_no) &&
                   (fsa[(int)db.position].job_status[j].connect_status == NOT_WORKING))
               {
                  fsa[(int)db.position].job_status[j].connect_status = DISCONNECT;
               }
            }
            unlock_region(fsa_fd, (char *)&fsa[(int)db.position].error_counter - (char *)fsa);

            /*
             * Since we have successfully transmitted a file, no need to
             * have the queue stopped anymore.
             */
            if (fsa[(int)db.position].host_status & AUTO_PAUSE_QUEUE_STAT)
            {
               fsa[(int)db.position].host_status ^= AUTO_PAUSE_QUEUE_STAT;
               (void)rec(sys_log_fd, INFO_SIGN,
                         "Starting queue for %s that was stopped by init_afd. (%s %d)\n",
                         fsa[(int)db.position].host_alias, __FILE__, __LINE__);
            }

            /*
             * Hmmm. This is very dangerous! But let see how it
             * works in practice.
             */
            if (fsa[(int)db.position].host_status & AUTO_PAUSE_QUEUE_LOCK_STAT)
            {
               fsa[(int)db.position].host_status ^= AUTO_PAUSE_QUEUE_LOCK_STAT;
            }
         } /* if (fsa[(int)db.position].error_counter > 0) */

         p_file_name_buffer += MAX_FILENAME_LENGTH;
         p_file_size_buffer++;
      } /* for (files_send = 0; files_send < files_to_send; files_send++) */

#ifdef _BURST_MODE
      search_for_files = YES;
      lock_region_w(fsa_fd, (char *)&fsa[(int)db.position].job_status[(int)db.job_no].job_id - (char *)fsa);
   } while (fsa[(int)db.position].job_status[(int)db.job_no].burst_counter != burst_counter);

   fsa[(int)db.position].job_status[(int)db.job_no].burst_counter = 0;
   total_files_send += files_send;
#endif

   free(buffer);

   (void)sprintf(line_buffer, "%-*s[%d]: %lu Bytes send in %d file(s).",
                 MAX_HOSTNAME_LENGTH, tr_hostname, (int)db.job_no,
                 fsa[(int)db.position].job_status[(int)db.job_no].file_size_done,
                 total_files_send);

#ifdef _BURST_MODE
   if (burst_counter == 1)
   {
      (void)strcat(line_buffer, " [BURST]");
   }
   else if (burst_counter > 1)
        {
           char tmp_buffer[12 + MAX_INT_LENGTH];

           (void)sprintf(tmp_buffer, " [BURST * %d]", burst_counter);
           (void)strcat(line_buffer, tmp_buffer);
        }
#endif
   (void)rec(transfer_log_fd, INFO_SIGN, "%s\n", line_buffer);

   /* Disconnect from remote port */
   wmo_quit();
   if (fsa[(int)db.position].debug == YES)
   {
      (void)rec(trans_db_log_fd, INFO_SIGN,
                "%-*s[%d]: Disconnected from port %d. (%s %d)\n",
                MAX_HOSTNAME_LENGTH, tr_hostname,
                (int)db.job_no, db.port,
                __FILE__, __LINE__);
   }

   /* Inform FSA that we have finished transferring */
   /* data and have disconnected.                   */
   reset_fsa(p_db, NO, (CONNECT_STATUS_VAR | NO_OF_FILES_VAR |
                        NO_OF_FILES_DONE_VAR | FILE_SIZE_DONE_VAR));

   if (wmo_counter_fd > 0)
   {
      if (close(wmo_counter_fd) == -1)
      {
         (void)rec(sys_log_fd, DEBUG_SIGN,
                   "close() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
   }

   /*
    * Remove file directory, but only when all files have
    * been transmitted.
    */
   if ((files_to_send == files_send) || (files_to_send == 0))
   {
      if (rmdir(file_path) < 0)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to remove directory %s : %s (%s %d)\n",
                   file_path, strerror(errno), __FILE__, __LINE__);
      }
   }
   else
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "There are still %d files for %s. Will NOT remove this job! (%s %d)\n",
                files_to_send - files_send, file_path, __FILE__, __LINE__);
   }
#endif /* _WITH_WMO_SUPPORT */

   exit(TRANSFER_SUCCESS);
}


#ifdef _WITH_WMO_SUPPORT
/*+++++++++++++++++++++++++++++ sf_wmo_exit() +++++++++++++++++++++++++++*/
static void
sf_wmo_exit(void)
{
   int  fd;
   char sf_fin_fifo[MAX_PATH_LENGTH];

   reset_fsa((struct job *)&db, NO, FILE_SIZE_VAR);

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
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not open fifo %s : %s (%s %d)\n",
                sf_fin_fifo, strerror(errno), __FILE__, __LINE__);
   }
   else
   {
      pid_t pid = getpid();
#ifdef _FIFO_DEBUG
      char  cmd[2];
#endif
      /* Tell FD we are finished */
#ifdef _FIFO_DEBUG
      cmd[0] = ACKN; cmd[1] = '\0';
      show_fifo_data('W', "sf_fin", cmd, 1, __FILE__, __LINE__);
#endif
      if (write(fd, &pid, sizeof(pid_t)) != sizeof(pid_t))
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "write() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
      (void)close(fd);
   }
   (void)close(sys_log_fd);

   return;
}


/*++++++++++++++++++++++++++++++ sig_pipe() +++++++++++++++++++++++++++++*/
static void
sig_pipe(int signo)
{
   reset_fsa((struct job *)&db, YES, (CONNECT_STATUS_VAR |
                        NO_OF_FILES_VAR | NO_OF_FILES_DONE_VAR |
                        FILE_SIZE_VAR | FILE_SIZE_DONE_VAR |
                        FILE_SIZE_IN_USE_VAR | FILE_SIZE_IN_USE_DONE_VAR |
                        FILE_NAME_IN_USE_VAR | PROC_ID_VAR));
   (void)rec(transfer_log_fd, ERROR_SIGN,
             "%-*s[%d]: Received SIGPIPE. Remote site has closed its socket. #%d (%s %d)\n",
             MAX_HOSTNAME_LENGTH, tr_hostname,
             (int)db.job_no, db.job_id, __FILE__, __LINE__);

   exit(SIG_PIPE_ERROR);
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   reset_fsa((struct job *)&db, YES, (CONNECT_STATUS_VAR |
                        NO_OF_FILES_VAR | NO_OF_FILES_DONE_VAR |
                        FILE_SIZE_VAR | FILE_SIZE_DONE_VAR |
                        FILE_SIZE_IN_USE_VAR | FILE_SIZE_IN_USE_DONE_VAR |
                        FILE_NAME_IN_USE_VAR | PROC_ID_VAR));
   (void)rec(sys_log_fd, DEBUG_SIGN,
             "Aaarrrggh! Received SIGSEGV. Remove the programmer who wrote this! (%s %d)\n",
             __FILE__, __LINE__);
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   reset_fsa((struct job *)&db, YES, (CONNECT_STATUS_VAR |
                        NO_OF_FILES_VAR | NO_OF_FILES_DONE_VAR |
                        FILE_SIZE_VAR | FILE_SIZE_DONE_VAR |
                        FILE_SIZE_IN_USE_VAR | FILE_SIZE_IN_USE_DONE_VAR |
                        FILE_NAME_IN_USE_VAR | PROC_ID_VAR));
   (void)rec(sys_log_fd, DEBUG_SIGN,
             "Uuurrrggh! Received SIGBUS. (%s %d)\n",
             __FILE__, __LINE__);
   abort();
}


/*++++++++++++++++++++++++++++++ sig_kill() +++++++++++++++++++++++++++++*/
static void
sig_kill(int signo)
{
   reset_fsa((struct job *)&db, NO, (CONNECT_STATUS_VAR |
                        NO_OF_FILES_VAR | NO_OF_FILES_DONE_VAR |
                        FILE_SIZE_VAR | FILE_SIZE_DONE_VAR |
                        FILE_SIZE_IN_USE_VAR | FILE_SIZE_IN_USE_DONE_VAR |
                        FILE_NAME_IN_USE_VAR | PROC_ID_VAR));
   exit(GOT_KILLED);
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   reset_fsa((struct job *)&db, YES, (CONNECT_STATUS_VAR |
                        NO_OF_FILES_VAR | NO_OF_FILES_DONE_VAR |
                        FILE_SIZE_VAR | FILE_SIZE_DONE_VAR |
                        FILE_SIZE_IN_USE_VAR | FILE_SIZE_IN_USE_DONE_VAR |
                        FILE_NAME_IN_USE_VAR | PROC_ID_VAR));
   exit(INCORRECT);
}
#endif /* _WITH_WMO_SUPPORT */