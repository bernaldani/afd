/*
 *  delete_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2004 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   delete_log - logs all file names distributed by the AFD.
 **
 ** SYNOPSIS
 **   delete_log [--version][-w <working directory>]
 **
 ** DESCRIPTION
 **   This function reads from the fifo DELETE_LOG_FIFO any file name
 **   that was by any process of the AFD. The data in the fifo has the
 **   following structure:
 **       <FS><JN><HN>\0<FNL><FN>\0<UPN>\0
 **         |   |   |     |    |     |
 **         |   |   |     |    |     +-----> A \0 terminated string of
 **         |   |   |     |    |             the user or process that
 **         |   |   |     |    |             deleted the file.
 **         |   |   |     |    +-----------> \0 terminated string of
 **         |   |   |     |                  the File Name.
 **         |   |   |     +----------------> Unsigned char holding the
 **         |   |   |                        File Name Length.
 **         |   |   +----------------------> \0 terminated string of
 **         |   |                            the Host Name and reason.
 **         |   +--------------------------> Integer holding the
 **         |                                job number.
 **         +------------------------------> File size of type off_t.
 **
 **   This data is then written to the delete log file in the following
 **   format:
 **
 **   863021759  btx      1 dat.txt 9888 46 sf_ftp
 **      |        |       |    |     |   |    |
 **      |        |       |    |     |   +-+  +----+
 **      |        |       |    |     |     |       |
 **   Deletion  Host Deletion File  File  Job   User/process
 **    time     name   type   name  size number that deleted
 **
 ** RETURN VALUES
 **   SUCCESS on normal exit and INCORRECT when an error has occurred.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.01.1998 H.Kiehl Created
 **   07.01.2001 H.Kiehl Build in some checks when fifo buffer overflows.
 **   14.06.2001 H.Kiehl Removed the above unnecessary checks.
 **   13.04.2002 H.Kiehl Added SEPARATOR_CHAR.
 **
 */
DESCR__E_M1

#include <stdio.h>           /* fopen(), fflush()                        */
#include <string.h>          /* strcpy(), strcat(), strerror(), memcpy() */
#include <stdlib.h>          /* malloc()                                 */
#include <time.h>            /* time()                                   */
#include <sys/types.h>       /* fdset                                    */
#include <sys/stat.h>
#include <sys/time.h>        /* struct timeval                           */
#include <unistd.h>          /* fpathconf()                              */
#include <fcntl.h>           /* O_RDWR, open()                           */
#include <signal.h>          /* signal()                                 */
#include <errno.h>
#include "logdefs.h"
#include "version.h"


/* External global variables */
int  sys_log_fd = STDERR_FILENO;
char *p_work_dir = NULL;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
#ifdef _DELETE_LOG
   FILE           *delete_file;
   int            bytes_buffered = 0,
                  log_number = 0,
                  n,
                  length,
                  max_delete_log_files = MAX_DELETE_LOG_FILES,
                  no_of_buffered_writes = 0,
                  offset,
                  check_size,
                  status,
                  log_fd;
   off_t          *file_size;
   time_t         next_file_time,
                  now;
   unsigned int   *job_number;
   long           fifo_size;
   char           *p_end,
                  *fifo_buffer,
                  *p_host_name,
                  *p_file_name,
                  work_dir[MAX_PATH_LENGTH],
                  current_log_file[MAX_PATH_LENGTH],
                  log_file[MAX_PATH_LENGTH];
   unsigned char  *file_name_length;
   fd_set         rset;
   struct timeval timeout;
   struct stat    stat_buf;

   CHECK_FOR_VERSION(argc, argv);

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   else
   {
      char delete_log_fifo[MAX_PATH_LENGTH];

      p_work_dir = work_dir;

      /* Create and open fifos that we need */
      (void)strcpy(delete_log_fifo, work_dir);
      (void)strcat(delete_log_fifo, FIFO_DIR);
      (void)strcat(delete_log_fifo, DELETE_LOG_FIFO);
      if ((log_fd = open(delete_log_fifo, O_RDWR)) == -1)
      {
         if (errno == ENOENT)
         {
            if (make_fifo(delete_log_fifo) == SUCCESS)
            {
               if ((log_fd = open(delete_log_fifo, O_RDWR)) == -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to open() fifo %s : %s",
                             delete_log_fifo, strerror(errno));
                  exit(INCORRECT);
               }
            }
            else
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to create fifo %s.", delete_log_fifo);
               exit(INCORRECT);
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to open() fifo %s : %s",
                       delete_log_fifo, strerror(errno));
            exit(INCORRECT);
         }
      }
   }

   /*
    * Lets determine the largest offset so the 'structure'
    * is aligned correctly.
    */
   offset = sizeof(clock_t);
   if (sizeof(off_t) > offset)
   {
      offset = sizeof(off_t);
   }
   if (sizeof(unsigned int) > offset)
   {
      offset = sizeof(unsigned int);
   }
   
   /*
    * Determine the size of the fifo buffer. Then create a buffer
    * large enough to hold the data from a fifo.
    */
   if ((fifo_size = (int)fpathconf(log_fd, _PC_PIPE_BUF)) < 0)
   {
      /* If we cannot determine the size of the fifo set default value */
      fifo_size = DEFAULT_FIFO_SIZE;
   }
   if (fifo_size < (offset + offset + MAX_HOSTNAME_LENGTH + 2 + 1 +
                    MAX_FILENAME_LENGTH + MAX_FILENAME_LENGTH))
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "Fifo is NOT large enough to ensure atomic writes!");
      fifo_size = offset + offset + MAX_HOSTNAME_LENGTH + 2 + 1 +
                  MAX_FILENAME_LENGTH + MAX_FILENAME_LENGTH;
   }

   /* Now lets allocate memory for the fifo buffer */
   if ((fifo_buffer = malloc((size_t)fifo_size)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not allocate memory for the fifo buffer : %s",
                 strerror(errno));
      exit(INCORRECT);
   }

   /* Get the maximum number of logfiles we keep for history. */
   get_max_log_number(&max_delete_log_files, MAX_DELETE_LOG_FILES_DEF,
                      MAX_DELETE_LOG_FILES);

   /*
    * Set umask so that all log files have the permission 644.
    * If we do not set this here fopen() will create files with
    * permission 666 according to POSIX.1.
    */
   (void)umask(S_IWGRP | S_IWOTH);

   /*
    * Lets open the delete file name buffer. If it does not yet exists
    * create it.
    */
   get_log_number(&log_number,
                  (max_delete_log_files - 1),
                  DELETE_BUFFER_FILE,
                  strlen(DELETE_BUFFER_FILE));
   (void)sprintf(current_log_file, "%s%s/%s0",
                 work_dir, LOG_DIR, DELETE_BUFFER_FILE);
   p_end = log_file;
   p_end += sprintf(log_file, "%s%s/%s",
                    work_dir, LOG_DIR, DELETE_BUFFER_FILE);

   /* Calculate time when we have to start a new file */
   next_file_time = (time(NULL) / SWITCH_FILE_TIME) * SWITCH_FILE_TIME +
                    SWITCH_FILE_TIME;

   /* Is current log file already too old? */
   if (stat(current_log_file, &stat_buf) == 0)
   {
      if (stat_buf.st_mtime < (next_file_time - SWITCH_FILE_TIME))
      {
         if (log_number < (max_delete_log_files - 1))
         {
            log_number++;
         }
         reshuffel_log_files(log_number, log_file, p_end);
      }
   }

   delete_file = open_log_file(current_log_file);

   /* Position pointers in fifo so that we only need to read */
   /* the data as they are in the fifo.                      */
   file_size = (off_t *)fifo_buffer;
   job_number = (unsigned int *)(fifo_buffer + offset);
   p_host_name = (char *)(fifo_buffer + offset + offset);
   file_name_length = (unsigned char *)(fifo_buffer + offset + offset +
                                        MAX_HOSTNAME_LENGTH + 2 + 1);
   p_file_name = (char *)(fifo_buffer + offset + offset + 1 +
                          MAX_HOSTNAME_LENGTH + 2 + 1);
   check_size = offset + offset + MAX_HOSTNAME_LENGTH + 2 +
                sizeof(unsigned char) + 1 + 1 + 1;

   /* Ignore any SIGHUP signal */
   if (signal(SIGHUP, SIG_IGN) == SIG_ERR)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "signal() error : %s", strerror(errno));
   }

   /*
    * Now lets wait for data to be written to the delete log.
    */
   FD_ZERO(&rset);
   for (;;)
   {
      /* Initialise descriptor set and timeout */
      FD_SET(log_fd, &rset);
      timeout.tv_usec = 0L;
      timeout.tv_sec = 3L;

      /* Wait for message x seconds and then continue. */
      status = select(log_fd + 1, &rset, NULL, NULL, &timeout);

      if (status == 0)
      {
         if (no_of_buffered_writes > 0)
         {
            (void)fflush(delete_file);
            no_of_buffered_writes = 0;
         }

         /* Check if we have to create a new log file. */
         if (time(&now) > next_file_time)
         {
            if (log_number < (max_delete_log_files - 1))
            {
               log_number++;
            }
            if (fclose(delete_file) == EOF)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "fclose() error : %s", strerror(errno));
            }
            reshuffel_log_files(log_number, log_file, p_end);
            delete_file = open_log_file(current_log_file);
            next_file_time = (now / SWITCH_FILE_TIME) *
                             SWITCH_FILE_TIME + SWITCH_FILE_TIME;
         }
      }
      else if (FD_ISSET(log_fd, &rset))
           {
              /*
               * It is accurate enough to look at the time once only,
               * even though we are writting in a loop to the delete
               * file.
               */
              now = time(NULL);

              /*
               * Aaaah. Some new data has arrived. Lets write this
               * data to the delete log. The data in the 
               * fifo always has the following format:
               *
               *   <file size><job number><host name><file name>
               *   <user/process>
               */
              if ((n = read(log_fd, &fifo_buffer[bytes_buffered],
                            fifo_size - bytes_buffered)) > 0)
              {
                 n += bytes_buffered;
                 bytes_buffered = 0;
                 do
                 {
                    if ((n < (check_size - 2)) ||
                        (n < (check_size + *file_name_length - 1)))
                    {
                       length = n;
                       bytes_buffered = n;
                    }
                    else
                    {
                       length = check_size + *file_name_length +
                                strlen(&p_file_name[*file_name_length + 1]);
                       if (n < length)
                       {
                          length = n;
                          bytes_buffered = n;
                       }
                       else
                       {
                          (void)fprintf(delete_file, "%-10ld %s%c%s%c%lu%c%d%c%s\n",
                                        now,
                                        p_host_name,
                                        SEPARATOR_CHAR,
                                        p_file_name,
                                        SEPARATOR_CHAR,
                                        (unsigned long)*file_size,
                                        SEPARATOR_CHAR,
                                        *job_number,
                                        SEPARATOR_CHAR,
                                        &p_file_name[*file_name_length + 1]);
                       }
                    }
                    n -= length;
                    if (n > 0)
                    {
                       (void)memmove(fifo_buffer, &fifo_buffer[length], n);
                    }
                    no_of_buffered_writes++;
                 } while (n > 0);

                 if (no_of_buffered_writes > BUFFERED_WRITES_BEFORE_FLUSH_SLOW)
                 {
                    (void)fflush(delete_file);
                    no_of_buffered_writes = 0;
                 }
              }
              else if (n < 0)
                   {
                      system_log(FATAL_SIGN, __FILE__, __LINE__,
                                 "read() error (%d) : %s", n, strerror(errno));
                      exit(INCORRECT);
                   }

              /*
               * Since we can receive a constant stream of data
               * on the fifo, we might never get that select() returns 0.
               * Thus we always have to check if it is time to create
               * a new log file.
               */
              if (now > next_file_time)
              {
                 if (log_number < (max_delete_log_files - 1))
                 {
                    log_number++;
                 }
                 if (fclose(delete_file) == EOF)
                 {
                    system_log(ERROR_SIGN, __FILE__, __LINE__,
                               "fclose() error : %s", strerror(errno));
                 }
                 reshuffel_log_files(log_number, log_file, p_end);
                 delete_file = open_log_file(current_log_file);
                 next_file_time = (now / SWITCH_FILE_TIME) * SWITCH_FILE_TIME +
                                  SWITCH_FILE_TIME;
              }
           }
           else
           {
              system_log(ERROR_SIGN, __FILE__, __LINE__,
                         "Select error : %s", strerror(errno));
              exit(INCORRECT);
           }
   } /* for (;;) */

#endif /* _DELETE_LOG */
   /* Should never come to this point. */
   exit(SUCCESS);
}
