/*
 *  mafd.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2006 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   mafd - controls startup and shutdown of AFD_MON
 **
 ** SYNOPSIS
 **   mafd [options]
 **          -a          only start AFD_MON
 **          -c          only check if AFD_MON is active
 **          -C          check if AFD_MON is active, if not start it
 **          -d          only start mon_ctrl dialog
 **          -i          initialize AFD_MON, by deleting fifodir
 **          -I          initialize AFD_MON, by deleting everything except
 **                      for the etc directory
 **          -s          shutdown AFD_MON
 **          -S          silent AFD_MON shutdown
 **          -u[ <user>] fake user
 **          --version   Current version
 **
 ** DESCRIPTION
 **   This program controls the startup or shutdown procedure of
 **   the AFDMON. When starting the following process are being
 **   initiated in this order:
 **
 **          afd_mon         - Monitors all process of the AFD.
 **          mon_log         - Logs all system activities.
 **          
 **
 ** RETURN VALUES
 **   None when successful. Otherwise if no response is received
 **   from the AFD_MON, it will tell the user.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.08.1998 H.Kiehl Created
 **   27.02.2005 H.Kiehl Added options to initialize AFD_MON.
 **   07.04.2006 H.Kiehl When doing initialization only delete AFD_MON
 **                      files, not AFD files.
 **
 */
DESCR__E_M1

#include <stdio.h>            /* fprintf(), remove(), NULL               */
#include <string.h>           /* strcpy(), strcat(), strerror()          */
#include <stdlib.h>           /* exit()                                  */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>         /* struct timeval, FD_SET...               */
#include <signal.h>           /* kill()                                  */
#ifdef HAVE_FCNTL_H
#include <fcntl.h>            /* O_RDWR, O_CREAT, O_WRONLY, etc          */
#endif
#include <unistd.h>           /* execlp(), chdir(), R_OK, X_OK           */
#include <errno.h>
#include "mondefs.h"
#include "logdefs.h"
#include "version.h"
#include "permission.h"

/* Some local definitions */
#define AFD_MON_ONLY            1
#define AFD_MON_CHECK_ONLY      2
#define AFD_MON_CHECK           3
#define MON_CTRL_ONLY           4
#define SHUTDOWN_ONLY           5
#define SILENT_SHUTDOWN_ONLY    6
#define START_BOTH              7
#define AFD_MON_INITIALIZE      8
#define AFD_MON_FULL_INITIALIZE 9

/* External global variables */
int         sys_log_fd = STDERR_FILENO;
char        *p_work_dir,
            mon_active_file[MAX_PATH_LENGTH],
            mon_cmd_fifo[MAX_PATH_LENGTH],
            probe_only_fifo[MAX_PATH_LENGTH];
const char  *sys_log_name = MON_SYS_LOG_FIFO;

/* Local functions */
static void delete_fifodir_files(char *, int),
            delete_log_files(char *, int),
            usage(char *);
static int  check_database(void);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int            fd,
                  initialize_perm,
                  mon_ctrl_perm,
                  n,
                  read_fd,
                  shutdown_perm,
                  start_up,
                  startup_perm,
                  status;
   char           *perm_buffer,
                  work_dir[MAX_PATH_LENGTH],
                  block_file[MAX_PATH_LENGTH],
                  exec_cmd[MAX_PATH_LENGTH],
                  fake_user[MAX_FULL_USER_ID_LENGTH],
                  sys_log_fifo[MAX_PATH_LENGTH],
                  user[MAX_FULL_USER_ID_LENGTH],
                  buffer[2];
   fd_set         rset;
   struct timeval timeout;
   struct stat    stat_buf,
                  stat_buf_fifo;

   CHECK_FOR_VERSION(argc, argv);

   if (get_mon_path(&argc, argv, work_dir) < 0)
   {
      exit(INCORRECT);
   }
   p_work_dir = work_dir;

   check_fake_user(&argc, argv, MON_CONFIG_FILE, fake_user);
   switch (get_permissions(&perm_buffer, fake_user))
   {
      case NONE :
         (void)fprintf(stderr, "%s\n", PERMISSION_DENIED_STR);
         exit(INCORRECT);

      case SUCCESS : /* Lets evaluate the permissions and see what */
                     /* the user may do.                           */
         if ((perm_buffer[0] == 'a') && (perm_buffer[1] == 'l') &&
             (perm_buffer[2] == 'l') &&
             ((perm_buffer[3] == '\0') || (perm_buffer[3] == ' ') ||
              (perm_buffer[3] == '\t')))
         {
            mon_ctrl_perm   = YES;
            shutdown_perm   = YES;
            startup_perm    = YES;
            initialize_perm = YES;
         }
         else
         {
            if (posi(perm_buffer, MON_CTRL_PERM) == NULL)
            {
               mon_ctrl_perm = NO_PERMISSION;
            }
            else
            {
               mon_ctrl_perm = YES;
            }
            if (posi(perm_buffer, MON_SHUTDOWN_PERM) == NULL)
            {
               shutdown_perm = NO_PERMISSION;
            }
            else
            {
               shutdown_perm = YES;
            }
            if (posi(perm_buffer, MON_STARTUP_PERM) == NULL)
            {
               startup_perm = NO_PERMISSION;
            }
            else
            {
               startup_perm = YES;
            }
            if (posi(perm_buffer, INITIALIZE_PERM) == NULL)
            {
               initialize_perm = NO_PERMISSION;
            }
            else
            {
               initialize_perm = YES;
            }
            if (perm_buffer != NULL)
            {
               free(perm_buffer);
            }
         }
         break;

      case INCORRECT : /* Hmm. Something did go wrong. Since we want to */
                       /* be able to disable permission checking let    */
                       /* the user have all permissions.                */
         mon_ctrl_perm   = YES;
         shutdown_perm   = YES;
         startup_perm    = YES;
         initialize_perm = YES;
         break;

      default :
         (void)fprintf(stderr,
                       "Impossible!! Remove the programmer!\n");
         exit(INCORRECT);
   }

   if (argc <= 2)
   {
      if (argc == 2)
      {
         if (strcmp(argv[1], "-a") == 0) /* Start AFD_MON */
         {
            if (startup_perm != YES)
            {
               (void)fprintf(stderr,
                             "You do not have the permission to start the AFD_MON.\n");
               exit(INCORRECT);
            }
            start_up = AFD_MON_ONLY;
         }
         else if (strcmp(argv[1], "-c") == 0) /* Only check if AFD_MON is active */
              {
                 start_up = AFD_MON_CHECK_ONLY;
              }
         else if (strcmp(argv[1], "-C") == 0) /* Only check if AFD_MON is active */
              {
                 if (startup_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  "You do not have the permission to start the AFD_MON.\n");
                    exit(INCORRECT);
                 }
                 start_up = AFD_MON_CHECK;
              }
         else if (strcmp(argv[1], "-d") == 0) /* Start mon_ctrl */
              {
                 if (mon_ctrl_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  "You do not have the permission to start the MON control dialog.\n");
                    exit(INCORRECT);
                 }
                 start_up = MON_CTRL_ONLY;
              }
         else if (strcmp(argv[1], "-i") == 0) /* Initialize AFD_MON. */
              {
                 if (initialize_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  "You do not have the permission to initialize AFD_MON.\n");
                    exit(INCORRECT);
                 }
                 start_up = AFD_MON_INITIALIZE;
              }
         else if (strcmp(argv[1], "-I") == 0) /* Full Initialization AFD_MON. */
              {
                 if (initialize_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  "You do not have the permission to do a full initialization of AFD_MON.\n");
                    exit(INCORRECT);
                 }
                 start_up = AFD_MON_FULL_INITIALIZE;
              }
         else if (strcmp(argv[1], "-s") == 0) /* Shutdown AFD_MON */
              {
                 if (shutdown_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  "You do not have the permission to shutdown the AFD_MON.\n");
                    exit(INCORRECT);
                 }
                 start_up = SHUTDOWN_ONLY;
              }
         else if (strcmp(argv[1], "-S") == 0) /* Silent AFD_MON shutdown */
              {
                 if (shutdown_perm != YES)
                 {
                    (void)fprintf(stderr,
                                  "You do not have the permission to shutdown the AFD_MON.\n");
                    exit(INCORRECT);
                 }
                 start_up = SILENT_SHUTDOWN_ONLY;
              }
         else if ((strcmp(argv[1], "-h") == 0) ||
                  (strcmp(argv[1], "-?") == 0)) /* Show usage */
              {
                 usage(argv[0]);
                 exit(0);
              }
              else
              {
                 usage(argv[0]);
                 exit(1);
              }
      }
      else /* Start AFD_MON and mon_ctrl */
      {
         if ((startup_perm == YES) && (mon_ctrl_perm == YES))
         {
            start_up = START_BOTH;
         }
         else if (startup_perm == YES)
              {
                 start_up = AFD_MON_ONLY;
              }
         else if (mon_ctrl_perm == YES)
              {
                 start_up = MON_CTRL_ONLY;
              }
              else
              {
                 (void)fprintf(stderr,
                               "You do not have enough permissions to use this program.\n");
                 exit(INCORRECT);
              }
      }
   }
   else
   {
      usage(argv[0]);
      exit(1);
   }

   if (chdir(work_dir) < 0)
   {
      (void)fprintf(stderr,
                    "ERROR   : Failed to change directory to %s : %s (%s %d)\n",
                    work_dir, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Initialise variables */
   (void)strcpy(block_file, work_dir);
   (void)strcat(block_file, ETC_DIR);
   (void)strcat(block_file, BLOCK_FILE);
   (void)strcpy(mon_active_file, work_dir);
   (void)strcat(mon_active_file, FIFO_DIR);
   if (check_dir(mon_active_file, R_OK | X_OK) < 0)
   {
      exit(INCORRECT);
   }
   (void)strcpy(sys_log_fifo, mon_active_file);
   (void)strcat(sys_log_fifo, MON_SYS_LOG_FIFO);
   (void)strcpy(mon_cmd_fifo, mon_active_file);
   (void)strcat(mon_cmd_fifo, MON_CMD_FIFO);
   (void)strcpy(probe_only_fifo, mon_active_file);
   (void)strcat(probe_only_fifo, MON_PROBE_ONLY_FIFO);
   (void)strcat(mon_active_file, MON_ACTIVE_FILE);

   if ((stat(sys_log_fifo, &stat_buf) == -1) || (!S_ISFIFO(stat_buf.st_mode)))
   {
      if (make_fifo(sys_log_fifo) < 0)
      {
         (void)fprintf(stderr, "ERROR   : Could not create fifo %s. (%s %d)\n",
                   sys_log_fifo, __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   if ((sys_log_fd = coe_open(sys_log_fifo, O_RDWR)) == -1)
   {
      (void)fprintf(stderr, "ERROR   : Could not open fifo %s : %s (%s %d)\n",
                    sys_log_fifo, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   if ((start_up == SHUTDOWN_ONLY) || (start_up == SILENT_SHUTDOWN_ONLY))
   {
      int   n,
            read_fd;
      pid_t ia_pid;

      /* First get the pid of init_afd before we send the */
      /* shutdown command.                                */
      if ((read_fd = coe_open(mon_active_file, O_RDONLY)) == -1)
      {
         if (errno != ENOENT)
         {
            (void)fprintf(stderr, "Failed to open %s : %s (%s %d)\n",
                          mon_active_file, strerror(errno),
                          __FILE__, __LINE__);
         }
         else
         {
            if (start_up == SHUTDOWN_ONLY)
            {
               (void)fprintf(stderr, "There is no AFD_MON active.\n");
            }
         }
         exit(INCORRECT);
      }
      if ((n = read(read_fd, &ia_pid, sizeof(pid_t))) != sizeof(pid_t))
      {
         if (n == 0)
         {
            (void)fprintf(stderr,
                          "File %s is empty. Unable to determine if AFD_MON is active.\n",
                          mon_active_file);
         }
         else
         {
            (void)fprintf(stderr, "read() error : %s (%s %d)\n",
                          strerror(errno),  __FILE__, __LINE__);
         }
         exit(INCORRECT);
      }
      (void)close(read_fd);

      if (start_up == SHUTDOWN_ONLY)
      {
         (void)fprintf(stdout, "Starting %s shutdown ", AFD_MON);
         fflush(stdout);

         shutdown_mon(NO, fake_user);

         (void)fprintf(stdout, "\nDone!\n");
      }
      else
      {
         shutdown_mon(YES, fake_user);
      }

      exit(0);
   }
   else if (start_up == MON_CTRL_ONLY)
        {
           (void)strcpy(exec_cmd, MON_CTRL);
           if (execlp(exec_cmd, exec_cmd, WORK_DIR_ID, work_dir,
                      (char *) 0) == -1)
           {
              (void)fprintf(stderr,
                            "ERROR   : Failed to execute %s : %s (%s %d)\n",
                            exec_cmd, strerror(errno), __FILE__, __LINE__);
              exit(1);
           }
           exit(0);
        }
   else if (start_up == AFD_MON_ONLY)
        {
           if (check_database() == -1)
           {
              (void)fprintf(stderr,
                            "Cannot read AFD_MON_CONFIG file : %s\nUnable to start AFD_MON.\n",
                            strerror(errno));
              exit(INCORRECT);
           }

           (void)strcpy(exec_cmd, AFD_MON);
           get_user(user, fake_user);
           (void)rec(sys_log_fd, WARN_SIGN,
                     "AFD_MON startup initiated by %s\n", user);
           switch (fork())
           {
              case -1 :

                 /* Could not generate process */
                 (void)rec(sys_log_fd, FATAL_SIGN,
                           "Could not create a new process : %s (%s %d)\n",
                           strerror(errno),  __FILE__, __LINE__);
                 break;

              case  0 :

                 /* Child process */
                 if (execlp(exec_cmd, exec_cmd, WORK_DIR_ID, work_dir,
                            (char *) 0) == -1)
                 {
                    (void)fprintf(stderr,
                                  "ERROR   : Failed to execute %s : %s (%s %d)\n",
                                  exec_cmd, strerror(errno),
                                  __FILE__, __LINE__);
                    exit(1);
                 }
                 exit(0);

              default :

                 /* Parent process */
                 break;
           }
           exit(0);
        }
   else if ((start_up == AFD_MON_CHECK) || (start_up == AFD_MON_CHECK_ONLY))
        {
           if (check_mon(18L) == 1)
           {
              (void)fprintf(stdout, "AFD_MON is active in %s\n", p_work_dir);
              exit(5);
           }
           else if (start_up == AFD_MON_CHECK)
                {
                   if (check_database() == -1)
                   {
                      (void)fprintf(stderr,
                                    "Cannot read AFD_MON_CONFIG file : %s\nUnable to start AFD_MON.\n",
                                    strerror(errno));
                      exit(INCORRECT);
                   }

                   (void)strcpy(exec_cmd, AFD_MON);
                   get_user(user, fake_user);
                   (void)rec(sys_log_fd, WARN_SIGN,
                             "Hmm. AFD_MON is NOT running! Startup initiated by %s\n",
                             user);
                   switch (fork())
                   {
                      case -1 : /* Could not generate process */
                         (void)rec(sys_log_fd, FATAL_SIGN,
                                   "Could not create a new process : %s (%s %d)\n",
                                   strerror(errno),  __FILE__, __LINE__);
                         break;

                      case  0 : /* Child process */
                         if (execlp(exec_cmd, exec_cmd, WORK_DIR_ID, work_dir,
                                    (char *) 0) == -1)
                         {
                            (void)fprintf(stderr,
                                          "ERROR   : Failed to execute %s : %s (%s %d)\n",
                                          exec_cmd, strerror(errno), __FILE__, __LINE__);
                            exit(1);
                         }
                         exit(0);

                      default : /* Parent process */
                         break;
                   }
                }
                else
                {
                   (void)fprintf(stderr, "No AFD_MON active in %s\n",
                                 p_work_dir);
                }

           exit(0);
        }
   else if ((start_up == AFD_MON_INITIALIZE) ||
            (start_up == AFD_MON_FULL_INITIALIZE))
        {
           if (check_mon(18L) == 1)
           {
              (void)fprintf(stderr,
                            "ERROR   : AFD_MON is still active, unable to initialize.\n");
              exit(INCORRECT);
           }
           else
           {
              int  offset;
              char dirs[MAX_PATH_LENGTH];

              offset = sprintf(dirs, "%s%s", p_work_dir, FIFO_DIR);
              delete_fifodir_files(dirs, offset);
              if (start_up == AFD_MON_FULL_INITIALIZE)
              {
                 offset = sprintf(dirs, "%s%s", p_work_dir, LOG_DIR);
                 delete_log_files(dirs, offset);
              }
              exit(SUCCESS);
           }
        }

   /* Create a lock, to ensure that AFD_MON does not get started twice */
   if ((fd = lock_file(sys_log_fifo, ON)) == INCORRECT)
   {
      (void)fprintf(stderr, "Failed to create lock! (%s %d)\n",
                    __FILE__, __LINE__);
      exit(INCORRECT);
   }
   else if (fd == LOCK_IS_SET)
        {
           (void)fprintf(stderr, "Someone else is trying to start the AFD_MON!\n");
           exit(INCORRECT);
        }
   else if (fd == LOCKFILE_NOT_THERE)
        {
           (void)fprintf(stderr, "Lock file `%s' not there.\n", sys_log_fifo);
           exit(INCORRECT);
        }

   /* Is another AFD_MON active in this directory? */
   if (check_mon(10L) == 1)
   {
      /* Unlock, so other users don't get blocked */
      (void)close(fd);

      /* Another AFD_MON is active. Only start mon_ctrl. */
      (void)strcpy(exec_cmd, MON_CTRL);
      if (execlp(exec_cmd, exec_cmd, WORK_DIR_ID, work_dir, (char *) 0) == -1)
      {
         (void)fprintf(stderr,
                       "ERROR   : Failed to execute %s : %s (%s %d)\n",
                       exec_cmd, strerror(errno), __FILE__, __LINE__);
         exit(1);
      }
   }
   else /* Start both */
   {
      if (check_database() == -1)
      {
         (void)fprintf(stderr,
                       "Cannot read AFD_MON_CONFIG file : %s\nUnable to start AFD_MON.\n",
                       strerror(errno));
         exit(INCORRECT);
      }

      if ((stat(probe_only_fifo, &stat_buf_fifo) == -1) ||
          (!S_ISFIFO(stat_buf_fifo.st_mode)))
      {
         if (make_fifo(probe_only_fifo) < 0)
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "Could not create fifo %s. (%s %d)\n",
                      probe_only_fifo, __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }
      if ((read_fd = coe_open(probe_only_fifo, O_RDWR)) == -1)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not open fifo %s : %s (%s %d)\n",
                   probe_only_fifo, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }

      /* Start AFD_MON */
      (void)strcpy(exec_cmd, AFD_MON);
      get_user(user, fake_user);
      (void)rec(sys_log_fd, WARN_SIGN,
                "AFD_MON automatic startup initiated by %s\n", user);
      switch (fork())
      {
         case -1 : /* Could not generate process */
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "Could not create a new process : %s (%s %d)\n",
                      strerror(errno),  __FILE__, __LINE__);
            break;

         case  0 : /* Child process */
            if (execlp(exec_cmd, exec_cmd, WORK_DIR_ID, work_dir,
                       (char *) 0) < 0)
            {
               (void)fprintf(stderr,
                             "ERROR   : Failed to execute %s : %s (%s %d)\n",
                             exec_cmd, strerror(errno), __FILE__, __LINE__);
               exit(1);
            }
            exit(0);

         default : /* Parent process */
            break;
      }

      /* Now lets wait for the AFD_MON to have finished */
      /* creating MSA (Monitor Status Area).            */
      FD_ZERO(&rset);
      FD_SET(read_fd, &rset);
      timeout.tv_usec = 0;
      timeout.tv_sec = 20;

      /* Wait for message x seconds and then continue. */
      status = select(read_fd + 1, &rset, NULL, NULL, &timeout);

      if (status == 0)
      {
         /* No answer from the other AFD_MON. Lets assume it */
         /* not able to startup properly.                    */
         (void)rec(sys_log_fd, FATAL_SIGN, "%s does not reply. (%s %d)\n",
                   AFD_MON, __FILE__, __LINE__);
         exit(INCORRECT);
      }
      else if (FD_ISSET(read_fd, &rset))
           {
              /* Ahhh! Now we can start mon_ctrl */
              if ((n = read(read_fd, buffer, 1)) > 0)
              {
                 if (buffer[0] == ACKN)
                 {
                    /* Unlock, so other users don't get blocked */
                    (void)close(fd);

                    (void)close(read_fd);

                    (void)strcpy(exec_cmd, MON_CTRL);
                    if (execlp(exec_cmd, exec_cmd, WORK_DIR_ID, work_dir,
                               (char *) 0) == -1)
                    {
                       (void)fprintf(stderr, "ERROR   : Failed to execute %s : %s (%s %d)\n",
                                     exec_cmd, strerror(errno), __FILE__, __LINE__);
                       exit(1);
                    }
                 }
                 else
                 {
                    (void)rec(sys_log_fd, FATAL_SIGN,
                              "Reading garbage from fifo %s. (%s %d)\n",
                              probe_only_fifo,  __FILE__, __LINE__);
                    exit(INCORRECT);
                 }
              }
              else if (n < 0)
                   {
                      (void)rec(sys_log_fd, FATAL_SIGN,
                                "read() error : %s (%s %d)\n",
                                strerror(errno),  __FILE__, __LINE__);
                      exit(INCORRECT);
                   }
           }
           else if (status < 0)
                {
                   (void)rec(sys_log_fd, FATAL_SIGN,
                             "Select error : %s (%s %d)\n",
                             strerror(errno),  __FILE__, __LINE__);
                   exit(INCORRECT);
                }
                else
                {
                   (void)rec(sys_log_fd, FATAL_SIGN,
                             "Unknown condition. Maybe you can tell what's going on here. (%s %d)\n",
                             __FILE__, __LINE__);
                   exit(INCORRECT);
                }

      (void)close(read_fd);
   }

   exit(0);
}


/*++++++++++++++++++++++++++++ check_database() +++++++++++++++++++++++++*/
static int
check_database(void)
{
   char db_file[MAX_PATH_LENGTH];

   (void)sprintf(db_file, "%s%s%s",
                 p_work_dir, ETC_DIR, AFD_MON_CONFIG_FILE);

   return(eaccess(db_file, R_OK));
}


/*+++++++++++++++++++++++++ delete_fifodir_files() ++++++++++++++++++++++*/
static void
delete_fifodir_files(char *fifodir, int offset)
{
   int  i,
        tmp_sys_log_fd;
   char *file_ptr,
        *filelist[] =
         {
            MON_ACTIVE_FILE,
            AFD_MON_STATUS_FILE,
            MSA_ID_FILE,
            MON_CMD_FIFO,
            MON_RESP_FIFO,
            MON_PROBE_ONLY_FIFO,
            MON_SYS_LOG_FIFO
         },
        *mfilelist[] =
        {
           MON_STATUS_FILE_ALL,
           RETRY_MON_FIFO_ALL,
           AHL_FILE_NAME_ALL
        };

   file_ptr = fifodir + offset;

   /* Delete single files. */
   for (i = 0; i < (sizeof(filelist) / sizeof(*filelist)); i++)
   {
      (void)strcpy(file_ptr, filelist[i]);
      (void)unlink(fifodir);
   }
   *file_ptr = '\0';

   tmp_sys_log_fd = sys_log_fd;
   sys_log_fd = STDOUT_FILENO;

   /* Delete multiple files. */
   for (i = 0; i < (sizeof(mfilelist) / sizeof(*mfilelist)); i++)
   {
      (void)remove_files(fifodir, &mfilelist[i][1]);
   }

   sys_log_fd = tmp_sys_log_fd;

   return;
}


/*++++++++++++++++++++++++++++ delete_log_files() +++++++++++++++++++++++*/
static void
delete_log_files(char *logdir, int offset)
{
   int  i,
        tmp_sys_log_fd;
   char *log_ptr,
        *loglist[] =
        {
           "/DAEMON_LOG.afd_mon"
        },
        *mloglist[] =
        {
           MON_SYS_LOG_NAME_ALL,
           MON_LOG_NAME_ALL
        };

   log_ptr = logdir + offset;

   /* Delete single files. */
   for (i = 0; i < (sizeof(loglist) / sizeof(*loglist)); i++)
   {
      (void)strcpy(log_ptr, loglist[i]);
      (void)unlink(logdir);
   }
   *log_ptr = '\0';

   tmp_sys_log_fd = sys_log_fd;
   sys_log_fd = STDOUT_FILENO;

   /* Delete multiple files. */
   for (i = 0; i < (sizeof(mloglist)/sizeof(*mloglist)); i++)
   {
      (void)remove_files(logdir, mloglist[i]);
   }

   sys_log_fd = tmp_sys_log_fd;

   return;
}


/*+++++++++++++++++++++++++++++++ usage() +++++++++++++++++++++++++++++++*/
static void
usage(char *progname)
{
   (void)fprintf(stderr, "USAGE: %s [-w <AFD_MON working dir>] [option]\n", progname);
   (void)fprintf(stderr, "              -a          only start AFD_MON\n");
   (void)fprintf(stderr, "              -c          only check if AFD_MON is active\n");
   (void)fprintf(stderr, "              -C          check if AFD_MON is active, if not start it\n");
   (void)fprintf(stderr, "              -d          only start mon_ctrl dialog\n");
   (void)fprintf(stderr, "              -i          initialize AFD_MON, by deleting fifodir\n");
   (void)fprintf(stderr, "              -I          initialize AFD_MON, by deleting everything\n");
   (void)fprintf(stderr, "              -s          shutdown AFD_MON\n");
   (void)fprintf(stderr, "              -S          silent AFD_MON shutdown\n");
   (void)fprintf(stderr, "              -u[ <user>] fake user\n");
   (void)fprintf(stderr, "              --version   Show current version\n");

   return;
}
