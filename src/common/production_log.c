/*
 *  production_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2009 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   production_log - writes production data to the production log fifo
 **
 ** SYNOPSIS
 **   void production_log(time_t       creation_time,
 **                       unsigned int ratio_1,
 **                       unsigned int ratio_2,
 **                       unsigned int unique_number,
 **                       unsigned int split_job_counter,
 **                       unsigned int job_id,
 **                       unsigned int dir_id,
 **                       char         *fmt,
 **                       ...)
 **
 ** DESCRIPTION
 **   When process wants to log the files it changed, it writes them
 **   via a fifo. The data it will write looks as follows:
 **       <ML><RR><UDN>|<DID>|<JID>|<OFN>|<NFL>[|<CMD>]\n
 **         |   |   |     |     |     |     |      |
 **         |   |   |     |     |     |     |      +-------> Command executed.
 **         |   |   |     |     |     |     +--------------> New filename.
 **         |   |   |     |     |     +--------------------> Original File Name.
 **         |   |   |     |     +--------------------------> Job ID.
 **         |   |   |     +--------------------------------> Directory ID.
 **         |   |   +--------------------------------------> Unique ID.
 **         |   +------------------------------------------> Ratio
 **         |                                                relationship.
 **         +----------------------------------------------> The length of this
 **                                                          message of type
 **                                                          unsigned short.
 **
 ** RETURN VALUES
 **   None
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   11.03.2001 H.Kiehl Created
 **   02.10.2004 H.Kiehl Change format and show command executed.
 **   15.01.2008 H.Kiehl Added job ID.
 **   28.03.2008 H.Kiehl Added directory ID.
 **   28.10.2008 H.Kiehl Added ratio relationship.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>
#include <stdarg.h>                   /* va_start(), va_end()            */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>                   /* STDERR_FILENO                   */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>

/* External global variables. */
extern int  production_log_fd;
extern char *p_work_dir;


/*########################## production_log() ###########################*/
void
production_log(time_t       creation_time,
               unsigned int ratio_1,
               unsigned int ratio_2,
               unsigned int unique_number,
               unsigned int split_job_counter,
               unsigned int job_id,
               unsigned int dir_id,
               char         *fmt,
               ...)
{
   size_t  length;
   char    production_buffer[MAX_INT_LENGTH + MAX_PRODUCTION_BUFFER_LENGTH + 1];
   va_list ap;

   if ((production_log_fd == STDERR_FILENO) && (p_work_dir != NULL))
   {
#ifdef WITHOUT_FIFO_RW_SUPPORT
      int  readfd;
#endif
      char production_log_fifo[MAX_PATH_LENGTH];

      (void)strcpy(production_log_fifo, p_work_dir);
      (void)strcat(production_log_fifo, FIFO_DIR);
      (void)strcat(production_log_fifo, PRODUCTION_LOG_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(production_log_fifo, &readfd, &production_log_fd) == -1)
#else
      if ((production_log_fd = coe_open(production_log_fifo, O_RDWR)) == -1)
#endif
      {
         if (errno == ENOENT)
         {
            if (make_fifo(production_log_fifo) != SUCCESS)
            {
               return;
            }
            else
            {
#ifdef WITHOUT_FIFO_RW_SUPPORT
               if (open_fifo_rw(production_log_fifo, &readfd, &production_log_fd) == -1)
#else
               if ((production_log_fd = coe_open(production_log_fifo, O_RDWR)) == -1)
#endif
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Could not open `%s' : %s"),
                             production_log_fifo, strerror(errno));
                  return;
               }
            }
         }
         else
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Could not open `%s' : %s"),
                       production_log_fifo, strerror(errno));
            return;
         }
      }
   }

   length = sizeof(short);
#if SIZEOF_TIME_T == 4
   length += sprintf(&production_buffer[length], "%x:%x%c%lx_%x_%x%c%x%c%x%c",
#else
   length += sprintf(&production_buffer[length], "%x:%x%c%llx_%x_%x%c%x%c%x%c",
#endif
                     ratio_1, ratio_2, SEPARATOR_CHAR,
                     (pri_time_t)creation_time, unique_number,
                     split_job_counter, SEPARATOR_CHAR, dir_id,
                     SEPARATOR_CHAR, job_id, SEPARATOR_CHAR);
   va_start(ap, fmt);
   length += vsprintf(&production_buffer[length], fmt, ap) + 1;
   production_buffer[length] = '\n';
   *(unsigned short *)production_buffer = (unsigned short)length;
   va_end(ap);
   if (write(production_log_fd, production_buffer, length) != length)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__, _("write() error : %s"),
                 strerror(errno));
   }

   return;
}
