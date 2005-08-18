/*
 *  del_cache.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   del_cache - deletes a single FD cache element
 **
 ** SYNOPSIS
 **   del_cache [-w <AFD work dir>] [--version] <cache pos>
 **
 ** DESCRIPTION
 **   This program is only used for debugging, so if you are not debugging
 **   don't use it!
 **   NOTE: This function may only called when no files are being queued
 **         by FD.
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   09.04.2002 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>                 /* exit(), atoi()                    */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>                  /* open()                            */
#include <unistd.h>                 /* fstat()                           */
#include <sys/mman.h>               /* mmap()                            */
#include <errno.h>
#include "fddefs.h"
#include "version.h"

/* Global variables */
int        sys_log_fd = STDERR_FILENO;
char       *p_work_dir = NULL;
const char *sys_log_name = SYSTEM_LOG_FIFO;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int                  fd,
                        *no_msg_cached;
   char                 file[MAX_PATH_LENGTH],
                        *ptr,
                        work_dir[MAX_PATH_LENGTH];
   struct stat          stat_buf;
   struct msg_cache_buf *mb;

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD */
   if (get_afd_path(&argc, argv, work_dir) < 0) 
   {
      exit(INCORRECT);
   }

   if (argc != 2)
   {
      (void)fprintf(stderr,
                    "Usage: %s [-w <AFD work dir>] [--version] <cache pos>\n",
                    argv[0]);
      exit(INCORRECT);
   }

   (void)sprintf(file, "%s%s%s", work_dir, FIFO_DIR, MSG_CACHE_FILE);
   if ((fd = open(file, O_RDWR)) == -1)
   {
      (void)fprintf(stderr,
                    "Failed to open() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if (fstat(fd, &stat_buf) == -1)
   {
      (void)fprintf(stderr,
                    "Failed to fstat() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if ((ptr = mmap(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                   MAP_SHARED, fd, 0)) == (caddr_t)-1)
   {
      (void)fprintf(stderr,
                    "Failed to mmap() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   no_msg_cached = (int *)ptr;
   ptr += AFD_WORD_OFFSET;
   mb = (struct msg_cache_buf *)ptr;

   if (*no_msg_cached > 0)
   {
      int del_pos = atoi(argv[1]);

      if (del_pos < *no_msg_cached)
      {
         size_t move_size = (*no_msg_cached - 1 - del_pos) *
                            sizeof(struct msg_cache_buf);

         (void)memmove(&mb[del_pos], &mb[del_pos + 1], move_size);
         (*no_msg_cached)--;
      }
      else
      {
         (void)fprintf(stderr,
                       "Delete position (%d) to high, there are only %d elements.\n",
                       del_pos, *no_msg_cached);
      }
   }
   else
   {
      (void)fprintf(stdout, "No messages cached.\n");
   }

   ptr -= AFD_WORD_OFFSET;
   if (munmap(ptr, stat_buf.st_size) == -1)
   {
      (void)fprintf(stderr,
                    "Failed to munmap() %s : %s (%s %d)\n",
                    file, strerror(errno), __FILE__, __LINE__);
   }
   (void)close(fd);

   exit(SUCCESS);
}
