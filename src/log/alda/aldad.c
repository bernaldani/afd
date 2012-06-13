/*
 *  aldad.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2009 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   aldad - AFD log data analyser daemon
 **
 ** SYNOPSIS
 **   aldad [options]
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   17.01.2009 H.Kiehl Created
 **
 */
DESCR__E_M3


#include <stdio.h>
#include <stdlib.h>               /* realloc(), free(), atexit()         */
#include <time.h>                 /* time()                              */
#include <sys/types.h>
#include <sys/stat.h>             /* stat()                              */
#include <sys/wait.h>             /* waitpid()                           */
#include <signal.h>               /* signal(), kill()                    */
#include <unistd.h>               /* fork()                              */
#include <errno.h>
#include "aldadefs.h"
#include "version.h"

/* Global variables. */
int                    no_of_process,
                       sys_log_fd = STDERR_FILENO;
char                   afd_config_file[MAX_PATH_LENGTH],
                       *p_work_dir;
struct aldad_proc_list *apl = NULL;
const char             *sys_log_name = SYSTEM_LOG_FIFO;

/* Local function prototypes. */
static pid_t           make_process(char *);
static void            aldad_exit(void),
                       sig_segv(int),
                       sig_bus(int),
                       sig_exit(int),
                       zombie_check(void);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ aldad $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   time_t next_stat_time,
          now,
          old_st_mtime;
   char   work_dir[MAX_PATH_LENGTH];

   /* Evaluate input arguments. */
   CHECK_FOR_VERSION(argc, argv);
   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }

   /* Initialize variables. */
   p_work_dir = work_dir;
   next_stat_time = 0L;
   old_st_mtime = 0L;
   no_of_process = 0;
   (void)sprintf(afd_config_file, "%s%s%s",
                 p_work_dir, ETC_DIR, AFD_CONFIG_FILE);

   /* Do some cleanups when we exit. */
   if (atexit(aldad_exit) != 0)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Could not register exit function : %s", strerror(errno));
      exit(INCORRECT);
   }
   if ((signal(SIGINT, sig_exit) == SIG_ERR) ||
       (signal(SIGQUIT, sig_exit) == SIG_ERR) ||
       (signal(SIGTERM, sig_exit) == SIG_ERR) ||
       (signal(SIGSEGV, sig_segv) == SIG_ERR) ||
       (signal(SIGBUS, sig_bus) == SIG_ERR) ||
       (signal(SIGHUP, SIG_IGN) == SIG_ERR))
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 _("Could not set signal handler : %s"), strerror(errno));
      exit(INCORRECT);
   }
   system_log(INFO_SIGN, NULL, 0, "Started %s.", ALDAD);

   /* Lets determine what log files we need to search. */
   for (;;)
   {
      now = time(NULL);
      if (next_stat_time < now)
      {
         struct stat stat_buf;

         next_stat_time = now + STAT_INTERVAL;
         if (stat(afd_config_file, &stat_buf) == 0)
         {
            if (stat_buf.st_mtime != old_st_mtime)
            {
               int  i;
               char *buffer,
                    tmp_aldad[MAX_PATH_LENGTH + 1];

               for (i = 0; i < no_of_process; i++)
               {
                  apl[i].in_list = NO;
               }

               old_st_mtime = stat_buf.st_mtime;
               if ((eaccess(afd_config_file, F_OK) == 0) &&
                   (read_file_no_cr(afd_config_file, &buffer) != INCORRECT))
               {
                  int  gotcha;
                  char *ptr = buffer;

                  /*
                   * Read all alda daemons entries.
                   */
                  do
                  {
                     if ((ptr = get_definition(ptr, ALDA_DAEMON_DEF, tmp_aldad,
                                               MAX_PATH_LENGTH)) != NULL)
                     {
                        /* First check if we already have this in our list. */
                        gotcha = NO;
                        for (i = 0; i < no_of_process; i++)
                        {
                           if (strcmp(apl[i].parameters, tmp_aldad) == 0)
                           {
                              apl[i].in_list = YES;
                              gotcha = YES;
                              break;
                           }
                        }
                        if (gotcha == NO)
                        {
                           no_of_process++;
                           if ((apl = realloc(apl,
                                              (no_of_process * sizeof(struct aldad_proc_list)))) == NULL)
                           {
                              system_log(FATAL_SIGN, __FILE__, __LINE__,
                                         "Failed to realloc() memory : %s",
                                         strerror(errno));
                              exit(INCORRECT);
                           }
                           if ((apl[no_of_process - 1].parameters = malloc((strlen(tmp_aldad) + 1))) == NULL)
                           {
                              system_log(FATAL_SIGN, __FILE__, __LINE__,
                                         "Failed to malloc() memory : %s",
                                         strerror(errno));
                              exit(INCORRECT);
                           }
                           (void)strcpy(apl[no_of_process - 1].parameters,
                                        tmp_aldad);
                           apl[no_of_process - 1].in_list = YES;
                           if ((apl[no_of_process - 1].pid = make_process(apl[no_of_process - 1].parameters)) == 0)
                           {
                              system_log(ERROR_SIGN, __FILE__, __LINE__,
                                         "Failed to start aldad process with the following parameters : %s",
                                         apl[no_of_process - 1].parameters);
                              free(apl[no_of_process - 1].parameters);
                              no_of_process--;
                              if ((apl = realloc(apl,
                                                 (no_of_process * sizeof(struct aldad_proc_list)))) == NULL)
                              {
                                 system_log(FATAL_SIGN, __FILE__, __LINE__,
                                            "Failed to realloc() memory : %s",
                                            strerror(errno));
                                 exit(INCORRECT);
                              }
                           }
                        }
                     }
                  } while (ptr != NULL);

                  for (i = 0; i < no_of_process; i++)
                  {
                     if (apl[i].in_list == NO)
                     {
                        if (kill(apl[i].pid, SIGINT) == -1)
                        {
                           system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                                      "Failed to kill() process %d with parameters %s",
#else
                                      "Failed to kill() process %lld with parameters %s",
#endif
                                      (pri_pid_t)apl[i].pid, apl[i].parameters);
                        }
                        else
                        {
                           free(apl[i].parameters);
                           if (i < no_of_process)
                           {
                              (void)memmove(&apl[i], &apl[i + 1],
                                            ((no_of_process - i) * sizeof(struct aldad_proc_list)));
                           }
                           no_of_process--;
                           i--;
                           if ((apl = realloc(apl,
                                              (no_of_process * sizeof(struct aldad_proc_list)))) == NULL)
                           {
                              system_log(FATAL_SIGN, __FILE__, __LINE__,
                                         "Failed to realloc() memory : %s",
                                         strerror(errno));
                              exit(INCORRECT);
                           }
                        }
                     }
                  }

                  free(buffer);
               }
            }
         }
      }
      zombie_check();

      sleep(5L);
   }

   exit(SUCCESS);
}


/*++++++++++++++++++++++++++++ make_process() +++++++++++++++++++++++++++*/
static pid_t
make_process(char *parameters)
{
   pid_t proc_id;

   switch (proc_id = fork())
   {
      case -1: /* Could not generate process. */
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Could not create a new process : %s",
                    strerror(errno));
         exit(INCORRECT);

      case  0: /* Child process. */
         {
            int  length,
                 work_dir_length;
            char *cmd;

            work_dir_length = strlen(p_work_dir);
            length = 5 + 3 + work_dir_length + 1 + 3 + strlen(parameters) + 1;
            if ((cmd = malloc(length)) == NULL)
            {
               system_log(FATAL_SIGN, __FILE__, __LINE__,
                          "Failed to malloc() %d bytes : %s",
                          length, strerror(errno));
               exit(INCORRECT);
            }
            cmd[0] = 'a';
            cmd[1] = 'l';
            cmd[2] = 'd';
            cmd[3] = 'a';
            cmd[4] = ' ';
            cmd[5] = '-';
            cmd[6] = 'w';
            cmd[7] = ' ';
            (void)strcpy(&cmd[8], p_work_dir);
            cmd[8 + work_dir_length] = ' ';
            cmd[9 + work_dir_length] = '-';
            cmd[10 + work_dir_length] = 'C';
            cmd[11 + work_dir_length] = ' ';
            (void)strcpy(&cmd[12 + work_dir_length], parameters);
#ifdef _DEBUG
            system_log(DEBUG_SIGN, NULL, 0, "aldad: %s", cmd);
#endif
            (void)execl("/bin/sh", "sh", "-c", cmd, (char *)0);
         }
         exit(SUCCESS);

      default: /* Parent process. */
         return(proc_id);
   }

   return(INCORRECT);
}


/*++++++++++++++++++++++++++++ zombie_check() +++++++++++++++++++++++++++*/
static void
zombie_check(void)
{
   int exit_status,
       i,
       status;

   for (i = 0; i < no_of_process; i++)
   {
      if (waitpid(apl[i].pid, &status, WNOHANG) > 0)
      {
         if (WIFEXITED(status))
         {
            exit_status = WEXITSTATUS(status);
            if (exit_status != 0)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Alda log process (%s) died, return code is %d",
                          apl[i].parameters, exit_status);
            }
            free(apl[i].parameters);
            if (i < (no_of_process - 1))
            {
               (void)memmove(&apl[i], &apl[i + 1],
                             ((no_of_process - i) * sizeof(struct aldad_proc_list)));
            }
            no_of_process--;
            i--;
            if (no_of_process == 0)
            {
               free(apl);
               apl = NULL;
            }
            else
            {
               if ((apl = realloc(apl,
                                  (no_of_process * sizeof(struct aldad_proc_list)))) == NULL)
               {
                  system_log(FATAL_SIGN, __FILE__, __LINE__,
                             "Failed to realloc() memory : %s",
                             strerror(errno));
                  exit(INCORRECT);
               }
            }
         }
         else if (WIFSIGNALED(status))
              {
                 /* Termination caused by signal. */
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Alda log process (%s) terminated by signal %d.",
                            apl[i].parameters, WTERMSIG(status));
                 free(apl[i].parameters);
                 if (i < no_of_process)
                 {
                    (void)memmove(&apl[i], &apl[i + 1],
                                  ((no_of_process - i) * sizeof(struct aldad_proc_list)));
                 }
                 no_of_process--;
                 i--;
                 if (no_of_process == 0)
                 {
                    free(apl);
                    apl = NULL;
                 }
                 else
                 {
                    if ((apl = realloc(apl,
                                       (no_of_process * sizeof(struct aldad_proc_list)))) == NULL)
                    {
                       system_log(FATAL_SIGN, __FILE__, __LINE__,
                                  "Failed to realloc() memory : %s",
                                  strerror(errno));
                       exit(INCORRECT);
                    }
                 }
              }
         else if (WIFSTOPPED(status))
              {
                 /* Child has been stopped. */
                 system_log(WARN_SIGN, __FILE__, __LINE__,
                            "Alda log process (%s) received STOP signal.",
                            apl[i].parameters);
              }
      }
   }

   return;
}


/*+++++++++++++++++++++++++++++ aldad_exit() ++++++++++++++++++++++++++++*/
static void                                                                
aldad_exit(void)
{
   int i;

   system_log(INFO_SIGN, NULL, 0, "Stopped %s.", ALDAD);

   /* Kill all jobs that where started. */
   for (i = 0; i < no_of_process; i++)
   {
      if (kill(apl[i].pid, SIGINT) < 0)
      {
         if (errno != ESRCH)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
#if SIZEOF_PID_T == 4
                       "Failed to kill process alda with pid %d : %s",
#else
                       "Failed to kill process alda with pid %lld : %s",
#endif
                       (pri_pid_t)apl[i].pid, strerror(errno));
         }
      }
   }

   return;
}


/*++++++++++++++++++++++++++++++ sig_segv() +++++++++++++++++++++++++++++*/
static void
sig_segv(int signo)
{
   system_log(FATAL_SIGN, __FILE__, __LINE__,
              "Aaarrrggh! Received SIGSEGV.");
   aldad_exit();
   abort();
}


/*++++++++++++++++++++++++++++++ sig_bus() ++++++++++++++++++++++++++++++*/
static void
sig_bus(int signo)
{
   system_log(FATAL_SIGN, __FILE__, __LINE__, "Uuurrrggh! Received SIGBUS.");
   aldad_exit();
   abort();
}


/*++++++++++++++++++++++++++++++ sig_exit() +++++++++++++++++++++++++++++*/
static void
sig_exit(int signo)
{
   exit(INCORRECT);
}
