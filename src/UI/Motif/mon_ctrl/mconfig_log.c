/*
 *  mconfig_log.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2004 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   mconfig_log - log configuration entries to system log
 **
 ** SYNOPSIS
 **   void mconfig_log(int type, char *sign, char *fmt, ...)
 **
 ** DESCRIPTION
 **   This function logs all configuration options to MON_LOG.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   28.03.2004 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <stdarg.h>                   /* va_start(), va_end()            */
#include <time.h>                     /* time(), localtime()             */
#include <sys/types.h>
#include <unistd.h>                   /* write()                         */
#include <fcntl.h>
#include <errno.h>
#include "mon_ctrl.h"
#include "x_common_defs.h"

/* External global variables */
extern int  mon_log_fd,
            sys_log_fd;
extern char *profile,
            *p_work_dir,
            user[];


/*############################ mconfig_log() ############################*/
void
mconfig_log(int type, char *sign, char *fmt, ...)
{
   int       *p_fd;
   size_t    length;
   time_t    tvalue;
   char      buf[MAX_LINE_LENGTH];
   va_list   ap;
   struct tm *p_ts;

   if (type == SYS_LOG)
   {
      p_fd = &sys_log_fd;
   }
   else
   {
      p_fd = &mon_log_fd;
   }

   /*
    * Only open sys_log_fd to MON_LOG when it is STDERR_FILENO.
    * If it is STDOUT_FILENO it is an X application and here we do
    * NOT wish to write to MON_SYS_LOG or MON_LOG.
    */
   if ((*p_fd == STDERR_FILENO) && (p_work_dir != NULL))
   {
      char log_fifo[MAX_PATH_LENGTH];

      (void)strcpy(log_fifo, p_work_dir);
      (void)strcat(log_fifo, FIFO_DIR);
      if (type == SYS_LOG)
      {
         (void)strcat(log_fifo, MON_SYS_LOG_FIFO);
      }
      else
      {
         (void)strcat(log_fifo, MON_LOG_FIFO);
      }
      if ((*p_fd = open(log_fifo, O_RDWR)) == -1)
      {
         if (errno == ENOENT)
         {
            if ((make_fifo(log_fifo) == SUCCESS) &&
                ((*p_fd = open(log_fifo, O_RDWR)) == -1))
            {
               (void)fprintf(stderr,
                             "WARNING : Could not open fifo %s : %s (%s %d)\n",
                             log_fifo, strerror(errno), __FILE__, __LINE__);
            }
         }
         else
         {
            (void)fprintf(stderr,
                          "WARNING : Could not open fifo %s : %s (%s %d)\n",
                          log_fifo, strerror(errno), __FILE__, __LINE__);
         }
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
   length += vsprintf(&buf[length], fmt, ap);
   va_end(ap);

   if (profile != NULL)
   {
      length += sprintf(&buf[length], " (%s <%s>)\n", profile, user);
   }
   else
   {
      length += sprintf(&buf[length], " (%s)\n", user);
   }

   (void)write(*p_fd, buf, length);

   return;
}
