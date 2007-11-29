/*
 *  recreate_msg.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2003 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   recreate_msg - recreates a message
 **
 ** SYNOPSIS
 **   int recreate_msg(unsigned int job_id)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **   SUCCESS when it managed to recreate the message, otherwise INCORRECT
 **   is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   09.08.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <string.h>          /* strerror()                               */
#include <stdlib.h>          /* exit()                                   */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>        /* mmap(), munmap()                         */
#include <unistd.h>          /* fstat(), read(), close()                 */
#include <fcntl.h>           /* open()                                   */
#include <errno.h>
#include "fddefs.h"

/* External global variables. */
extern char *p_work_dir;


/*########################### recreate_msg() ############################*/
int
recreate_msg(unsigned int job_id)
{
   int                i,
                      jd_fd,
                      *no_of_job_ids,
                      status = INCORRECT;
   char               job_id_data_file[MAX_PATH_LENGTH],
                      *ptr;
   struct stat        stat_buf;
   struct job_id_data *jd;

   (void)sprintf(job_id_data_file, "%s%s%s", p_work_dir, FIFO_DIR,
                 JOB_ID_DATA_FILE);
   if ((jd_fd = open(job_id_data_file, O_RDWR)) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to open() `%s' : %s",
                 job_id_data_file, strerror(errno));
      exit(INCORRECT);
   }
   if (fstat(jd_fd, &stat_buf) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to fstat() `%s' : %s",
                 job_id_data_file, strerror(errno));
      exit(INCORRECT);
   }

   if (stat_buf.st_size > 0)
   {
      if ((ptr = mmap(NULL, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                      MAP_SHARED, jd_fd, 0)) == (caddr_t) -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to mmap() to `%s' : %s",
                    job_id_data_file, strerror(errno));
         exit(INCORRECT);
      }
      no_of_job_ids = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      jd = (struct job_id_data *)ptr;
   }
   else
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "File `%s' is empty! Terminating, don't know what to do :-(",
                 job_id_data_file);
      exit(INCORRECT);
   }
   for (i = 0; i < *no_of_job_ids; i++)
   {
      if (job_id == jd[i].job_id)
      {
         if (jd[i].no_of_soptions > 0)
         {
            status = create_message(job_id, jd[i].recipient, jd[i].soptions);
         }
         else
         {
            status = create_message(job_id, jd[i].recipient, NULL);
         }
         break;
      }
   }

   /* Don't forget to unmap from job_id_data structure. */
   ptr = (char *)jd - AFD_WORD_OFFSET;
   if (munmap(ptr, stat_buf.st_size) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "munmap() error : %s", strerror(errno));
   }
   if (close(jd_fd) == -1)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 "close() error : %s", strerror(errno));
   }

   if (status == SUCCESS)
   {
      system_log(INFO_SIGN, __FILE__, __LINE__,
                "Recreated message for job `%x'.", job_id);
      return(SUCCESS);
   }
   else
   {
      return(INCORRECT);
   }
}
