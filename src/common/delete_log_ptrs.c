/*
 *  delete_log_ptrs.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   delete_log_ptrs - initializes and sets data pointers for delete log
 **
 ** SYNOPSIS
 **   void delete_log_ptrs(struct delete *dl)
 **
 ** DESCRIPTION
 **   When process wants to log the files it deleted, it writes the
 **   buffer 'dl->data' to the delete log via a fifo. To do this a set of
 **   pointers have to be prepared which point to the right
 **   place in the buffer 'dl->data'. Once the buffer has been
 **   filled with the necessary data it will look as follows:
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
 **         |   +--------------------------> Unsigned integer holding the
 **         |                                job number.
 **         +------------------------------> File size of type off_t.
 **
 ** RETURN VALUES
 **   When successful it opens the fifo to the delete log and assigns
 **   memory for the buffer 'dl->data'. The following values are being
 **   returned: dl->fd, dl->job_number, dl->data, dl->file_name,
 **             dl->file_name_length, dl->file_size, dl->size.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.02.1998 H.Kiehl Created
 **   11.11.2002 H.Kiehl Create delete log fifo if it does not exist.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <errno.h>

/* External global variables. */
extern char       *p_work_dir;


/*########################### delete_log_ptrs() #########################*/
void
delete_log_ptrs(struct delete_log *dl)
{
   char        delete_log_fifo[MAX_PATH_LENGTH];
   struct stat stat_buf;

   (void)strcpy(delete_log_fifo, p_work_dir);
   (void)strcat(delete_log_fifo, FIFO_DIR);
   (void)strcat(delete_log_fifo, DELETE_LOG_FIFO);
   if ((stat(delete_log_fifo, &stat_buf) < 0) ||
       (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(delete_log_fifo) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to create fifo %s.", delete_log_fifo);
         return;
      }
   }
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(delete_log_fifo, &dl->readfd, &dl->fd) == -1)
#else
   if ((dl->fd = coe_open(delete_log_fifo, O_RDWR)) == -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Could not open fifo <%s> : %s",
                 delete_log_fifo, strerror(errno));
   }
   else
   {
      int offset;

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
       * Create a buffer which we use can use to send our
       * data to the delete log process. The buffer has the
       * following structure:
       *
       * <transfer time><file size><job number><host name>
       * <file name length><file name + archive dir>
       */
      dl->size = offset + offset +
                 MAX_HOSTNAME_LENGTH + 2 + 1 +
                 sizeof(unsigned char) +        /* file name length  */
                 MAX_FILENAME_LENGTH + 1 +      /* local file name   */
                 MAX_FILENAME_LENGTH + 1;       /* User/process name */
      if ((dl->data = calloc(dl->size, sizeof(char))) == NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "calloc() error : %s", strerror(errno));
      }
      else
      {
         dl->size = offset + offset +
                    MAX_HOSTNAME_LENGTH + 2 +
                    sizeof(unsigned char) +
                    1 + 1 + 1;                /* NOTE: + 1 + 1 + 1 is   */
                                              /* for '\0' at end        */
                                              /* of host + file name    */
                                              /* and user/process name. */
         dl->file_size = &(*(off_t *)(dl->data));
         dl->job_number = (unsigned int *)(dl->data + offset);
         dl->host_name = (char *)(dl->data + offset + offset);
         dl->file_name_length = (unsigned char *)(dl->data + offset +
                                                  offset +
                                                  MAX_HOSTNAME_LENGTH + 2 + 1);
         dl->file_name = (char *)(dl->data + offset + offset +
                                  MAX_HOSTNAME_LENGTH + 2 + 1 +
                                  sizeof(unsigned char));
      }
   }

   return;
}
