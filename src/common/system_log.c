/*
 *  system_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   system_log - writes formated log output to transfer debug log
 **
 ** SYNOPSIS
 **   void system_log(char *sign, char *file, int line, char *fmt, ...)
 **
 ** DESCRIPTION
 **   The function system_log() first checks if the fifo sys_log_fd
 **   is already open, if not it will open it and write formated
 **   messages to the SYSTEM_LOG of AFD. The main reason for this
 **   function (instead of using rec()) is to have one less file
 **   descriptor open that are hardly ever used in any of the sf_xxx
 **   process.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.12.2000 H.Kiehl Created
 **   30.01.2009 H.Kiehl Restore errno to what it was when the caller
 **                      called this function.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* memcpy()                        */
#include <stdarg.h>                   /* va_start(), va_end()            */
#include <time.h>                     /* time(), localtime()             */
#ifdef TM_IN_SYS_TIME
# include <sys/time.h>
#endif
#include <sys/types.h>
#include <unistd.h>                   /* write()                         */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>

extern int        sys_log_fd;
extern char       *p_work_dir;
extern const char *sys_log_name;


/*############################ system_log() #############################*/
void
system_log(char *sign, char *file, int line, char *fmt, ...)
{
   int       tmp_errno = errno;
   size_t    length;
   time_t    tvalue;
   char      buf[MAX_LINE_LENGTH + 1];
   va_list   ap;
   struct tm *p_ts;

   /*
    * Only open sys_log_fd to SYSTEM_LOG when it is STDERR_FILENO.
    * If it is STDOUT_FILENO it is an X application and here we do
    * NOT wish to write to SYSTEM_LOG.
    */
   if ((sys_log_fd == STDERR_FILENO) && (p_work_dir != NULL))
   {
#ifdef WITHOUT_FIFO_RW_SUPPORT
      int  readfd;
#endif
      char sys_log_fifo[MAX_PATH_LENGTH];

      (void)strcpy(sys_log_fifo, p_work_dir);
      (void)strcat(sys_log_fifo, FIFO_DIR);
      (void)strcat(sys_log_fifo, sys_log_name);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(sys_log_fifo, &readfd, &sys_log_fd) == -1)
#else
      if ((sys_log_fd = coe_open(sys_log_fifo, O_RDWR)) == -1)
#endif
      {
         if (errno == ENOENT)
         {
            if ((make_fifo(sys_log_fifo) == SUCCESS) &&
#ifdef WITHOUT_FIFO_RW_SUPPORT
                (open_fifo_rw(sys_log_fifo, &readfd, &sys_log_fd) == -1))
#else
                ((sys_log_fd = coe_open(sys_log_fifo, O_RDWR)) == -1))
#endif
            {
               (void)fprintf(stderr,
                             _("WARNING : Could not open fifo %s : %s (%s %d)\n"),
                             sys_log_fifo, strerror(errno), __FILE__, __LINE__);
            }
         }
         else
         {
            (void)fprintf(stderr,
                          _("WARNING : Could not open fifo %s : %s (%s %d)\n"),
                          sys_log_fifo, strerror(errno), __FILE__, __LINE__);
         }
         sys_log_fd = STDERR_FILENO;
      }
   }

   tvalue = time(NULL);
   p_ts    = localtime(&tvalue);
   buf[0]  = (p_ts->tm_mday / 10) + '0';
   buf[1]  = (p_ts->tm_mday % 10) + '0';
   buf[2]  = ' ';
   buf[3]  = (p_ts->tm_hour / 10) + '0';
   buf[4]  = (p_ts->tm_hour % 10) + '0';
   buf[5]  = ':';
   buf[6]  = (p_ts->tm_min / 10) + '0';
   buf[7]  = (p_ts->tm_min % 10) + '0';
   buf[8]  = ':';
   buf[9]  = (p_ts->tm_sec / 10) + '0';
   buf[10] = (p_ts->tm_sec % 10) + '0';
   buf[11] = ' ';
   buf[12] = sign[0];
   buf[13] = sign[1];
   buf[14] = sign[2];
   buf[15] = ' ';
   length = 16;

   va_start(ap, fmt);
#ifdef HAVE_VSNPRINTF
   length += vsnprintf(&buf[length], MAX_LINE_LENGTH - length, fmt, ap);
#else
   length += vsprintf(&buf[length], fmt, ap);
#endif
   va_end(ap);

   if ((file == NULL) || (line == 0))
   {
      buf[length] = '\n';
      length += 1;
   }
   else
   {
#ifdef HAVE_SNPRINTF
      length += snprintf(&buf[length], MAX_LINE_LENGTH - length, " (%s %d)\n",
                         file, line);
#else
      length += sprintf(&buf[length], " (%s %d)\n", file, line);
#endif
   }

   if (write(sys_log_fd, buf, length) == -1)
   {
      (void)fprintf(stderr,
#if SIZEOF_SIZE_T == 4
                    _("ERROR   : Failed to write() %d bytes : %s (%s %d)\n"),
#else
                    _("ERROR   : Failed to write() %lld bytes : %s (%s %d)\n"),
#endif
                    (pri_size_t)length, strerror(errno), __FILE__, __LINE__);
   }
   errno = tmp_errno;

   return;
}
