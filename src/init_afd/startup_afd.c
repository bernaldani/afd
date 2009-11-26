/*
 *  startup_afd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2007, 2008 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   startup_afd - starts init_afd process
 **
 ** SYNOPSIS
 **   int startup_afd(void)
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
 **   31.03.2007 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>            /* fprintf()                               */
#include <string.h>
#include <stdlib.h>           /* exit()                                  */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>         /* struct timeval                          */
#include <fcntl.h>            /* O_RDWR, O_WRONLY, etc                   */
#include <unistd.h>           /* select(), unlink()                      */
#include <errno.h>
#include "version.h"

/* External global variables. */
#ifdef AFDBENCH_CONFIG
extern int  pause_dir_check;
#endif
extern char *p_work_dir;


/*############################ startup_afd() ############################*/
int
startup_afd(void)
{
   int            gotcha = NO,
                  readfd,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                  writefd,
#endif
                  status;
   fd_set         rset;
   char           buffer[2],
                  probe_only_fifo[MAX_PATH_LENGTH];
   struct timeval timeout;
   struct stat    stat_buf_fifo;

   (void)strcpy(probe_only_fifo, p_work_dir);
   (void)strcat(probe_only_fifo, FIFO_DIR);
   (void)strcat(probe_only_fifo, PROBE_ONLY_FIFO);
   if ((stat(probe_only_fifo, &stat_buf_fifo) == -1) ||
       (!S_ISFIFO(stat_buf_fifo.st_mode)))
   {
      if (make_fifo(probe_only_fifo) < 0)
      {
         (void)fprintf(stderr,
                       _("Could not create fifo `%s'. (%s %d)\n"),
                       probe_only_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(probe_only_fifo, &readfd, &writefd) == -1)
#else
   if ((readfd = coe_open(probe_only_fifo, O_RDWR)) == -1)
#endif
   {
      (void)fprintf(stderr,
                    _("Could not open fifo `%s' : %s (%s %d)\n"),
                    probe_only_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Start AFD. */
   switch (fork())
   {
      case -1 : /* Could not generate process. */
         (void)fprintf(stderr,
                       _("Could not create a new process : %s (%s %d)\n"),
                       strerror(errno),  __FILE__, __LINE__);
         return(NO);

      case  0 : /* Child process. */
#ifdef AFDBENCH_CONFIG
         if (pause_dir_check == YES)
         {
            if (execlp(AFD, AFD, WORK_DIR_ID, p_work_dir, "-A",
                       (char *) 0) < 0)
            {
               (void)fprintf(stderr,
                             _("ERROR   : Failed to execute %s : %s (%s %d)\n"),
                             AFD, strerror(errno), __FILE__, __LINE__);
               exit(1);
            }
         }
         else
         {
            if (execlp(AFD, AFD, WORK_DIR_ID, p_work_dir,
                       (char *) 0) < 0)
            {
               (void)fprintf(stderr,
                             _("ERROR   : Failed to execute %s : %s (%s %d)\n"),
                             AFD, strerror(errno), __FILE__, __LINE__);
               exit(1);
            }
         }
#else
         if (execlp(AFD, AFD, WORK_DIR_ID, p_work_dir,
                    (char *) 0) < 0)
         {
            (void)fprintf(stderr,
                          _("ERROR   : Failed to execute %s : %s (%s %d)\n"),
                          AFD, strerror(errno), __FILE__, __LINE__);
            exit(1);
         }
#endif
         exit(0);

      default : /* Parent process. */
         break;
   }

   /* Now lets wait for the AFD to have finished creating */
   /* FSA (Filetransfer Status Area).                     */
   FD_ZERO(&rset);
   FD_SET(readfd, &rset);
   timeout.tv_usec = 0;
   timeout.tv_sec = 30L;

   /* Wait for message x seconds and then continue. */
   status = select(readfd + 1, &rset, NULL, NULL, &timeout);

   if (status == 0)
   {
      /* No answer from the other AFD. Lets assume it */
      /* not able to startup properly.                */
      (void)fprintf(stderr, _("%s does not reply. (%s %d)\n"),
                    AFD, __FILE__, __LINE__);
      exit(INCORRECT);
   }
   else if (FD_ISSET(readfd, &rset))
        {
           int n;

           if ((n = read(readfd, buffer, 1)) > 0)
           {
              if (buffer[0] == ACKN)
              {
                 gotcha = YES;
              }
              else
              {
                 (void)fprintf(stderr,
                               _("Reading garbage from fifo `%s'. (%s %d)\n"),
                               probe_only_fifo,  __FILE__, __LINE__);
                 exit(INCORRECT);
              }
           }
           else if (n < 0)
                {
                   (void)fprintf(stderr, _("read() error : %s (%s %d)\n"),
                                 strerror(errno),  __FILE__, __LINE__);
                   exit(INCORRECT);
                }
        }
        else if (status < 0)
             {
                (void)fprintf(stderr, _("select() error : %s (%s %d)\n"),
                              strerror(errno),  __FILE__, __LINE__);
                exit(INCORRECT);
             }
             else
             {
                (void)fprintf(stderr,
                              _("Unknown condition. Maybe you can tell what's going on here. (%s %d)\n"),
                              __FILE__, __LINE__);
                exit(INCORRECT);
             }

   (void)close(readfd);
#ifdef WITHOUT_FIFO_RW_SUPPORT
   (void)close(writefd);
#endif

   return(gotcha);
}
