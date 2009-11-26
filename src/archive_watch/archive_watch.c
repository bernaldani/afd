/*
 *  archive_watch.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2009 Deutscher Wetterdienst (DWD),
 *                            Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   archive_watch - removes old archives
 **
 ** SYNOPSIS
 **   archive_watch
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   19.02.1996 H.Kiehl Created
 **   22.08.1998 H.Kiehl Added some more exit handlers.
 **   12.03.2000 H.Kiehl Check so archive_watch cannot be started twice.
 **
 */
DESCR__E_M1

#include <stdio.h>                 /* fprintf(), fclose(), stderr,       */
#include <string.h>                /* strcpy(), strcat()                 */
#include <stdlib.h>                /* exit()                             */
#include <time.h>                  /* time()                             */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>              /* struct timeval                     */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>                /* O_RDWR, O_RDONLY, O_NONBLOCK, etc  */
#endif
#include <unistd.h>                /* read()                             */
#include <signal.h>                /* signal()                           */
#include <errno.h>
#include "awdefs.h"
#include "version.h"


/* Global variables. */
int          sys_log_fd = STDERR_FILENO;
unsigned int removed_archives = 0,
             removed_files = 0;
char         *p_work_dir;
time_t       current_time;
const char   *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static void  aw_exit(void),
             sig_bus(int),
             sig_exit(int),
             sig_segv(int);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int            n,
                  status,
#ifdef WITHOUT_FIFO_RW_SUPPORT
                  aw_cmd_writefd,
#endif
                  aw_cmd_fd;
   time_t         diff_time,
                  next_report_time,
                  next_rescan_time = 0L,
                  now;
   char           archive_dir[MAX_PATH_LENGTH],
                  aw_cmd_fifo[MAX_PATH_LENGTH],
                  buffer[DEFAULT_BUFFER_SIZE],
                  work_dir[MAX_PATH_LENGTH];
   fd_set         rset;
   struct timeval timeout;
   struct stat    stat_buf;

   CHECK_FOR_VERSION(argc, argv);

   /* First get working directory for the AFD. */
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   else
   {
      char *ptr;

      p_work_dir = work_dir;

      /*
       * Lock archive_watch so no other archive_watch can be started!
       */
      if ((ptr = lock_proc(AW_LOCK_ID, NO)) != NULL)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Process archive_watch already started by %s."), ptr);
         exit(INCORRECT);
      }
   }

   /* Initialize fifo to communicate with AFD. */
   (void)strcpy(aw_cmd_fifo, work_dir);
   (void)strcat(aw_cmd_fifo, FIFO_DIR);
   (void)strcat(aw_cmd_fifo, AW_CMD_FIFO);
   (void)strcpy(archive_dir, work_dir);
   (void)strcat(archive_dir, AFD_ARCHIVE_DIR);

   /* Now lets open the fifo to receive commands from the AFD. */
   if ((stat(aw_cmd_fifo, &stat_buf) < 0) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(aw_cmd_fifo) < 0)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not create fifo `%s'."), aw_cmd_fifo);
         exit(INCORRECT);
      }
   }

#ifdef WITHOUT_FIFO_RW_SUPPORT
   if (open_fifo_rw(aw_cmd_fifo, &aw_cmd_fd, &aw_cmd_writefd) == -1)
#else
   if ((aw_cmd_fd = coe_open(aw_cmd_fifo, O_RDWR)) == -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _(": Could not open fifo `%s' : %s"),
                 aw_cmd_fifo, strerror(errno));
      exit(INCORRECT);
   }

   /* Do some cleanups when we exit. */
   if (atexit(aw_exit) != 0)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not register exit handler : %s"), strerror(errno));
      exit(INCORRECT);
   }
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      system_log(WARN_SIGN, __FILE__, __LINE__,
                 _("Could not set signal handlers : %s"), strerror(errno));
   }

   system_log(INFO_SIGN, NULL, 0, "Starting %s (%s)",
              ARCHIVE_WATCH, PACKAGE_VERSION);

   next_report_time = (time(NULL) / 3600) * 3600 + 3600;
   FD_ZERO(&rset);

   for (;;)
   {
      if (time(&now) >= next_rescan_time)
      {
         next_rescan_time = (now / ARCHIVE_STEP_TIME) *
                            ARCHIVE_STEP_TIME + ARCHIVE_STEP_TIME;
      }

      /* Initialize descriptor set and timeout. */
      FD_SET(aw_cmd_fd, &rset);
      timeout.tv_usec = 0;
      if ((diff_time = (next_rescan_time - now)) < 0)
      {
         diff_time = 0L;
      }
      timeout.tv_sec = diff_time;

      /* Wait for message x seconds and then continue. */
      status = select(aw_cmd_fd + 1, &rset, NULL, NULL, &timeout);

      /* Report every hour how many archives have been deleted. */
      if ((now + diff_time) >= next_report_time)
      {
         next_report_time = ((now + diff_time) / 3600) * 3600 + 3600;
#ifdef _NO_ZERO_DELETION_REPORT
         if ((removed_archives > 0) || (removed_files > 0))
         {
            system_log(INFO_SIGN, NULL, 0,
                      _("Removed %u archives with %u files."),
                      removed_archives, removed_files);
         }
#else
         system_log(INFO_SIGN, NULL, 0,
                   _("Removed %u archives with %u files."),
                   removed_archives, removed_files);
#endif
         removed_archives = removed_files = 0;
      }

      /* Huh? Did we just sleep? */
      if (status == 0)
      {
         /* Lets go to work! */
         current_time = now + diff_time;
         inspect_archive(archive_dir);
      }
      else if (FD_ISSET(aw_cmd_fd, &rset))
           {
              /* Read the message. */
              if ((n = read(aw_cmd_fd, buffer, DEFAULT_BUFFER_SIZE)) > 0)
              {
                 while (n > 0)
                 {
#ifdef _FIFO_DEBUG
                    show_fifo_data('R', "aw_cmd", buffer, n, __FILE__, __LINE__);
#endif
                    if (buffer[0] == STOP)
                    {
                       system_log(INFO_SIGN, NULL, 0,
                                  _("Stopped %s"), ARCHIVE_WATCH);
                       exit(SUCCESS);
                    }
                    else if (buffer[0] == RETRY)
                         {
                            system_log(INFO_SIGN, NULL, 0,
                                       _("Rescaning archive directories."),
                                       ARCHIVE_WATCH);

                            /* Remember to set current_time, since the   */
                            /* function inspect_archive() depends on it. */
                            current_time = time(NULL);
                            inspect_archive(archive_dir);
                         }
                         else
                         {
                            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                                      _("Hmmm..., reading garbage [%d] on fifo `%s'."),
                                      buffer[0], AW_CMD_FIFO);
                         }
                    n--;
                 } /* while (n > 0) */
              }
           }
      else if (status < 0)
           {
              system_log(FATAL_SIGN, __FILE__, __LINE__,
                         _("select() error : %s"), strerror(errno));
              exit(INCORRECT);
           }
           else
           {
              system_log(FATAL_SIGN, __FILE__, __LINE__,
                         _("Huh? Maybe YOU have a clue whats going on here!"));
              exit(INCORRECT);
           }
   } /* for (;;) */

   exit(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ aw_exit() +++++++++++++++++++++++++++++*/
static void
aw_exit(void)
{
   if ((removed_archives > 0) || (removed_files > 0))
   {
      system_log(INFO_SIGN, NULL, 0, _("Removed %u archives with %u files."),
                 removed_archives, removed_files);
   }
   system_log(INFO_SIGN, NULL, 0, "Stopped %s.", ARCHIVE_WATCH);
   (void)close(sys_log_fd);

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   system_log(FATAL_SIGN, __FILE__, __LINE__,
              _("Aaarrrggh! Received SIGSEGV."));
   aw_exit();

   /* Dump core so we know what happened. */
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   system_log(FATAL_SIGN, __FILE__, __LINE__,
              _("Uuurrrggh! Received SIGBUS."));
   aw_exit();

   /* Dump core so we know what happened. */
   abort();
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   exit(INCORRECT);
}
