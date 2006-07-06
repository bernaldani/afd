/*
 *  read_afd_stat_db.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2006 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   read_afd_stat_db - maps to AFD output statistic file
 **
 ** SYNOPSIS
 **   void read_afd_stat_db(int no_of_hosts)
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
 **   15.09.1996 H.Kiehl Created
 **   18.10.1997 H.Kiehl Initialise day, hour and second counter.
 **   21.02.2003 H.Kiehl Put in version number at beginning of structure.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>          /* strcpy(), strerror(), memcpy(), memset() */
#include <time.h>            /* time()                                   */
#ifdef TM_IN_SYS_TIME
#include <sys/time.h>        /* struct tm                                */
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>          /* exit(), malloc(), free()                 */
#ifdef HAVE_MMAP
#include <sys/mman.h>        /* mmap(), munmap()                         */
#endif
#include <fcntl.h>
#include <unistd.h>          /* close(), lseek(), write()                */
#include <errno.h>
#include "statdefs.h"

#ifdef HAVE_MMAP
#ifndef MAP_FILE    /* Required for BSD          */
#define MAP_FILE 0  /* All others do not need it */
#endif
#endif

/* Global external variables */
extern int                        sys_log_fd,
                                  lock_fd;
extern size_t                     stat_db_size;
extern char                       statistic_file[MAX_PATH_LENGTH],
                                  new_statistic_file[MAX_PATH_LENGTH];
extern struct afdstat             *stat_db;
extern struct filetransfer_status *fsa;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$ read_afd_stat_db() $$$$$$$$$$$$$$$$$$$$$$$$$*/
void
read_afd_stat_db(int no_of_hosts)
{
   int            old_status_fd = -1,
                  new_status_fd,
                  position,
                  i;
   time_t         now;
   static int     no_of_old_hosts = 0;
   size_t         size = sizeof(struct statistics),
                  old_stat_db_size = 0;
   char           *old_ptr,
                  *ptr;
   struct afdstat *old_stat_db;
   struct stat    stat_buf;
   struct tm      *p_ts;

   /*
    * Check if there is memory with an old database. If not
    * this is the first time and the memory and file have to
    * be created.
    */
   if (stat_db == (struct afdstat *)NULL)
   {
      /*
       * Now see if an old statistics file exists. If it does, mmap
       * to this file, so the data can be used to create the new
       * statistics file. If it does not exist, don't bother, since
       * we only need to initialize all values to zero.
       */
      if ((stat(statistic_file, &stat_buf) < 0) || (stat_buf.st_size == 0))
      {
         if (stat_buf.st_size == 0)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "Hmm..., old output statistic file is empty. (%s %d)\n",
                      __FILE__, __LINE__);
         }
         else
         {
            if (errno != ENOENT)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Failed to stat() %s : %s (%s %d)\n",
                         statistic_file, strerror(errno), __FILE__, __LINE__);
               exit(INCORRECT);
            }
         }
         old_stat_db = NULL;
      }
      else /* An old statistics database file exists */
      {
         int no_of_old_statistic_hosts;  /* Not used. */

         if ((lock_fd = lock_file(statistic_file, OFF)) == LOCK_IS_SET)
         {
            (void)rec(sys_log_fd, WARN_SIGN,
                      "Another process is currently using file %s (%s %d)\n",
                      statistic_file, __FILE__, __LINE__);
            exit(INCORRECT);
         }
         if ((old_status_fd = open(statistic_file, O_RDWR)) < 0)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to open() %s : %s (%s %d)\n",
                      statistic_file, strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }

#ifdef HAVE_MMAP
         if ((old_ptr = mmap(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                             (MAP_FILE | MAP_SHARED),
                             old_status_fd, 0)) == (caddr_t) -1)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Could not mmap() file %s : %s (%s %d)\n",
                      statistic_file, strerror(errno), __FILE__, __LINE__);
            (void)close(old_status_fd);
            exit(INCORRECT);
         }
#else
         if ((old_ptr = malloc(stat_buf.st_size)) == NULL)
         {
            (void)rec(sys_log_fd, ERROR_SIGN, "malloc() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
         if (read(old_status_fd, old_ptr, stat_buf.st_size) != stat_buf.st_size)
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to read() %s : %s (%s %d)\n",
                      statistic_file, strerror(errno), __FILE__, __LINE__);
            free(old_ptr);
            (void)close(old_status_fd);
            exit(INCORRECT);
         }
#endif
         no_of_old_statistic_hosts = *(int *)old_ptr; /* NOT USED */
         old_stat_db = (struct afdstat *)(old_ptr + AFD_WORD_OFFSET);
         old_stat_db_size = stat_buf.st_size;
         no_of_old_hosts = (old_stat_db_size - AFD_WORD_OFFSET) /
                           sizeof(struct afdstat);
      }
   }
   else /* An old statistics database in memory does exist. */
   {
      /*
       * Store the following values so we can unmap the
       * old region later.
       */
      old_stat_db = stat_db;
      old_stat_db_size = stat_db_size;
      old_ptr = (char *)stat_db - AFD_WORD_OFFSET;
   }

   stat_db_size = AFD_WORD_OFFSET + (no_of_hosts * sizeof(struct afdstat));

   /* Create new temporary file. */
   if ((new_status_fd = open(new_statistic_file, (O_RDWR | O_CREAT | O_TRUNC),
                             FILE_MODE)) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not open() %s : %s (%s %d)\n",
                new_statistic_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef HAVE_MMAP
   if (lseek(new_status_fd, stat_db_size - 1, SEEK_SET) == -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not seek() on %s : %s (%s %d)\n",
                new_statistic_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (write(new_status_fd, "", 1) != 1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not write() to %s : %s (%s %d)\n",
                new_statistic_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if ((ptr = mmap(0, stat_db_size, (PROT_READ | PROT_WRITE),
                   (MAP_FILE | MAP_SHARED),
                   new_status_fd, 0)) == (caddr_t) -1)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not mmap() file %s : %s (%s %d)\n",
                new_statistic_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#else
   if ((ptr = malloc(stat_db_size)) == NULL)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "malloc() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#endif
   *(int *)ptr = no_of_hosts;
   *(ptr + sizeof(int) + 1 + 1 + 1) = CURRENT_STAT_VERSION;
   stat_db = (struct afdstat *)(ptr + AFD_WORD_OFFSET);
   (void)memset(stat_db, 0, (stat_db_size - AFD_WORD_OFFSET));

   if ((no_of_old_hosts < 1) && (old_stat_db != NULL))
   {
      (void)rec(sys_log_fd, DEBUG_SIGN,
                "Failed to find any old hosts! [%d %d Bytes] (%s %d)\n",
                no_of_old_hosts, old_stat_db_size, __FILE__, __LINE__);
   }

   /*
    * Now compare the old data with the FSA that was just read.
    */
   now = time(NULL);
   p_ts = gmtime(&now);
   if ((old_stat_db == NULL) || (no_of_old_hosts < 1))
   {
      int j;

      for (i = 0; i < no_of_hosts; i++)
      {
         (void)strcpy(stat_db[i].hostname, fsa[i].host_alias);
         stat_db[i].start_time = now;
         if (p_ts->tm_yday >= DAYS_PER_YEAR)
         {
            stat_db[i].day_counter = 0;
         }
         else
         {
            stat_db[i].day_counter = p_ts->tm_yday;
         }
         stat_db[i].hour_counter = p_ts->tm_hour;
         stat_db[i].sec_counter = ((p_ts->tm_min * 60) + p_ts->tm_sec) / STAT_RESCAN_TIME;
         (void)memset(&stat_db[i].year, 0, (DAYS_PER_YEAR * size));
         (void)memset(&stat_db[i].day, 0, (HOURS_PER_DAY * size));
         (void)memset(&stat_db[i].hour, 0, (SECS_PER_HOUR * size));
         stat_db[i].prev_nfs = fsa[i].file_counter_done;
         for (j = 0; j < MAX_NO_PARALLEL_JOBS; j++)
         {
            stat_db[i].prev_nbs[j] = fsa[i].job_status[j].bytes_send;
         }
         stat_db[i].prev_ne = fsa[i].total_errors;
         stat_db[i].prev_nc = fsa[i].connections;
      }
   }
   else
   {
      for (i = 0; i < no_of_hosts; i++)
      {
         if ((position = locate_host(old_stat_db,
                                     fsa[i].host_alias,
                                     no_of_old_hosts)) < 0)
         {
            int j;

            (void)strcpy(stat_db[i].hostname, fsa[i].host_alias);
            stat_db[i].start_time = now;
            if (p_ts->tm_yday >= DAYS_PER_YEAR)
            {
               stat_db[i].day_counter = 0;
            }
            else
            {
               stat_db[i].day_counter = p_ts->tm_yday;
            }
            stat_db[i].hour_counter = p_ts->tm_hour;
            stat_db[i].sec_counter = ((p_ts->tm_min * 60) + p_ts->tm_sec) / STAT_RESCAN_TIME;
            (void)memset(&stat_db[i].year, 0, (DAYS_PER_YEAR * size));
            (void)memset(&stat_db[i].day, 0, (HOURS_PER_DAY * size));
            (void)memset(&stat_db[i].hour, 0, (SECS_PER_HOUR * size));
            stat_db[i].prev_nfs = fsa[i].file_counter_done;
            for (j = 0; j < MAX_NO_PARALLEL_JOBS; j++)
            {
               stat_db[i].prev_nbs[j] = fsa[i].job_status[j].bytes_send;
            }
            stat_db[i].prev_ne = fsa[i].total_errors;
            stat_db[i].prev_nc = fsa[i].connections;
         }
         else
         {
            (void)memcpy(&stat_db[i], &old_stat_db[position], sizeof(struct afdstat));
         }
      }
   }

   if (old_stat_db != NULL)
   {
#ifdef HAVE_MMAP
      if (munmap(old_ptr, old_stat_db_size) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to munmap() %s : %s (%s %d)\n",
                   statistic_file, strerror(errno), __FILE__, __LINE__);
      }
#else
      free(old_ptr);
#endif
      if (lock_fd > -1)
      {
         if (close(lock_fd) == -1)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                      strerror(errno), __FILE__, __LINE__);
         }
      }
   }

#ifndef HAVE_MMAP
   if (write(new_status_fd, stat_db, stat_db_size) != stat_db_size)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not write() to %s : %s (%s %d)\n",
                new_statistic_file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#endif
   if (close(new_status_fd) == -1)
   {
      (void)rec(sys_log_fd, WARN_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   if (rename(new_statistic_file, statistic_file) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to rename() %s to %s : %s (%s %d)\n",
                new_statistic_file, statistic_file,
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if ((lock_fd = lock_file(statistic_file, OFF)) < 0)
   {
      (void)rec(sys_log_fd, WARN_SIGN,
                "Failed to lock file `%s' [%d] (%s %d)\n",
                statistic_file, lock_fd, __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Store current number of hosts. */
   no_of_old_hosts = no_of_hosts;

   if (old_status_fd != -1)
   {
      if (close(old_status_fd) == -1)
      {
         (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
   }

   return;
}
