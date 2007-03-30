/*
 *  check_burst_2.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2001 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_burst_2 - checks if FD still has jobs in the queue
 **
 ** SYNOPSIS
 **   int check_burst_2(char         *file_path,
 **                     int          *files_to_send,
 **                     unsigned int *values_changed)
 **
 ** DESCRIPTION
 **   The function check_burst_2() checks if FD has jobs in the queue
 **   for this host. If so it gets the new job name and if it is in
 **   the error directory via a fifo created by this function. The
 **   fifo will be removed once it has the data.
 **
 **   The structure of data send via the fifo will be as follows:
 **             char in_error_dir
 **             char msg_name[MAX_MSG_NAME_LENGTH]
 **
 ** RETURN VALUES
 **   Returns NO if FD does not have any job in queue or if an error
 **   has occured. If there is a job in queue YES will be returned
 **   and if the job_id of the current job is not the same it will
 **   fill up the structure job db with the new data.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.05.2001 H.Kiehl Created
 **   22.01.2005 H.Kiehl Check that ports are the some, otherwise don't
 **                      burst.
 **   11.04.2006 H.Kiehl For protocol SCP we must also check if the
 **                      target directories are the same before we do
 **                      a burst.
 **   18.05.2006 H.Kiehl For protocol SFTP users must be the same,
 **                      otherwise a burst is not possible.
 **
 */
DESCR__E_M3

#include <stdio.h>                 /* sprintf()                          */
#include <string.h>                /* strlen(), strcpy()                 */
#include <stdlib.h>                /* atoi(), malloc()                   */
#include <signal.h>
#include <sys/time.h>              /* struct timeval                     */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>                /* read(), write(), close()           */
#include <errno.h>
#include "fddefs.h"
#include "ftpdefs.h"
#include "smtpdefs.h"
#include "ssh_commondefs.h"
#ifdef _WITH_WMO_SUPPORT
#include "wmodefs.h"
#endif

/* External global variables. */
extern int                        no_of_hosts,
                                  *p_no_of_hosts;
extern char                       *p_work_dir;
extern struct filetransfer_status *fsa;
extern struct job                 db;

#ifdef WITH_SIGNAL_WAKEUP
/* Local variables. */
static int                        alarm_triggered;

/* Local function prototypes. */
static void                       sig_alarm(int);

#define WAIT_FOR_FD_REPLY 40
#endif


/*########################### check_burst_2() ###########################*/
int
check_burst_2(char         *file_path,
              int          *files_to_send,
#ifdef _WITH_INTERRUPT_JOB
              int          interrupt,
#endif
              unsigned int *values_changed)
{
   int ret;

#ifdef _WITH_BURST_2
   /*
    * First check if there are any jobs queued for this host.
    */
retry:
   /* It could be that the FSA changed. */
   if ((gsf_check_fsa() == YES) || (db.fsa_pos == INCORRECT))
   {
      /*
       * Host is no longer in FSA, so there is
       * no way we can communicate with FD.
       */
      return(NO);
   }
   ret = NO;
   if ((fsa->jobs_queued > 0) &&
       (fsa->active_transfers == fsa->allowed_transfers))
   {
      struct job *p_new_db = NULL;
      int        fd;
#ifdef WITHOUT_FIFO_RW_SUPPORT
      int        readfd;
#endif
      char       generic_fifo[MAX_PATH_LENGTH];

      (void)strcpy(generic_fifo, p_work_dir);
      (void)strcat(generic_fifo, FIFO_DIR);
      (void)strcat(generic_fifo, SF_FIN_FIFO);
#ifdef WITHOUT_FIFO_RW_SUPPORT
      if (open_fifo_rw(generic_fifo, &readfd, &fd) == -1)
#else
      if ((fd = open(generic_fifo, O_RDWR)) == -1)
#endif
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__, "Failed to open() %s : %s",
                    generic_fifo, strerror(errno));
      }
      else
      {
#ifdef WITH_SIGNAL_WAKEUP
         sigset_t         newmask,
                          oldmask,
                          suspmask;
         struct sigaction newact,
                          oldact_alrm,
                          oldact_usr1;
#endif
         pid_t pid = -db.my_pid;

         fsa->job_status[(int)db.job_no].unique_name[1] = '\0';
         fsa->job_status[(int)db.job_no].unique_name[2] = 4;
#ifdef _WITH_INTERRUPT_JOB
         if (interrupt == YES)
         {
            fsa->job_status[(int)db.job_no].unique_name[3] = 4;
         }
#endif
#ifdef WITH_SIGNAL_WAKEUP
         newact.sa_handler = sig_alarm;
         sigemptyset(&newact.sa_mask);
         newact.sa_flags = 0;
         if ((sigaction(SIGALRM, &newact, &oldact_alrm) < 0) ||
             (sigaction(SIGUSR1, &newact, &oldact_usr1) < 0))
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to establish a signal handler for SIGUSR1 and/or SIGALRM : %s",
                       strerror(errno));
            return(NO);
         }
         sigemptyset(&newmask);
         sigaddset(&newmask, SIGALRM);
         sigaddset(&newmask, SIGUSR1);
         if (sigprocmask(SIG_BLOCK, &newmask, &oldmask) < 0)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "sigprocmask() error : %s", strerror(errno));
         }
#endif
         if (write(fd, &pid, sizeof(pid_t)) != sizeof(pid_t))
         {
#ifdef WITH_SIGNAL_WAKEUP
            if ((sigaction(SIGUSR1, &oldact_usr1, NULL) < 0) ||
                (sigaction(SIGALRM, &oldact_alrm, NULL) < 0))
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to restablish a signal handler for SIGUSR1 and/or SIGALRM : %s",
                          strerror(errno));
            }
            if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "sigprocmask() error : %s", strerror(errno));
            }
#endif
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "write() error : %s", strerror(errno));
         }
         else
         {
#ifdef WITH_SIGNAL_WAKEUP
            unsigned int sleep_time = 0;

            alarm_triggered = NO;
            (void)alarm(WAIT_FOR_FD_REPLY);
            suspmask = oldmask;
            sigdelset(&suspmask, SIGALRM);
            sigdelset(&suspmask, SIGUSR1);
            sigsuspend(&suspmask);
            sleep_time = alarm(0);
            if ((sigaction(SIGUSR1, &oldact_usr1, NULL) < 0) ||
                (sigaction(SIGALRM, &oldact_alrm, NULL) < 0))
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to restablish a signal handler for SIGUSR1 and/or SIGALRM : %s",
                          strerror(errno));
            }
            if (sigprocmask(SIG_SETMASK, &oldmask, NULL) < 0)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "sigprocmask() error : %s", strerror(errno));
            }

            /* It could be that the FSA changed. */
            if ((gsf_check_fsa() == YES) && (db.fsa_pos == INCORRECT))
            {
               /*
                * Host is no longer in FSA, so there is
                * no way we can communicate with FD.
                */
# ifdef WITHOUT_FIFO_RW_SUPPORT
               (void)close(readfd);
# endif
               (void)close(fd);
               return(NO);
            }
#else
            unsigned long sleep_time = 0L;

            do
            {
               /* It could be that the FSA changed. */
               if ((gsf_check_fsa() == YES) && (db.fsa_pos == INCORRECT))
               {
                  /*
                   * Host is no longer in FSA, so there is
                   * no way we can communicate with FD.
                   */
# ifdef WITHOUT_FIFO_RW_SUPPORT
                  (void)close(readfd);
# endif
                  (void)close(fd);
                  return(NO);
               }
               if (fsa->job_status[(int)db.job_no].unique_name[1] == '\0')
               {
                  my_usleep(10000L);
                  sleep_time += 10000L;
               }
               else
               {
                  break;
               }
            } while (sleep_time < 40000000L); /* Wait 40 seconds. */
#endif

            if ((fsa->job_status[(int)db.job_no].unique_name[1] != '\0') &&
                (fsa->job_status[(int)db.job_no].unique_name[0] != '\0'))
            {
               (void)memcpy(db.msg_name, 
                            fsa->job_status[(int)db.job_no].unique_name,
                            MAX_MSG_NAME_LENGTH);
               if (fsa->job_status[(int)db.job_no].job_id != db.job_id)
               {
                  db.job_id = fsa->job_status[(int)db.job_no].job_id;
                  if ((p_new_db = malloc(sizeof(struct job))) == NULL)
                  {
                     system_log(ERROR_SIGN, __FILE__, __LINE__,
                                "malloc() error : %s", strerror(errno));
                  }
                  else
                  {
                     char msg_name[MAX_PATH_LENGTH];

                     if (fsa->protocol_options & FTP_IGNORE_BIN)
                     {
                        p_new_db->transfer_mode     = 'N';
                     }
                     else
                     {
                        p_new_db->transfer_mode     = DEFAULT_TRANSFER_MODE;
                     }
                     p_new_db->special_ptr          = NULL;
                     p_new_db->subject              = NULL;
#ifdef _WITH_TRANS_EXEC
                     p_new_db->trans_exec_cmd       = NULL;
                     p_new_db->trans_exec_timeout   = DEFAULT_EXEC_TIMEOUT;
                     p_new_db->set_trans_exec_lock  = NO;
#endif
                     p_new_db->special_flag         = 0;
                     p_new_db->mode_flag            = 0;
                     p_new_db->archive_time         = DEFAULT_ARCHIVE_TIME;
                     if ((fsa->job_status[(int)db.job_no].file_name_in_use[0] == '\0') &&
                         (fsa->job_status[(int)db.job_no].file_name_in_use[1] == 1))
                     {
                        p_new_db->retries           = (unsigned int)atoi(&fsa->job_status[(int)db.job_no].file_name_in_use[1]);
                        if (p_new_db->retries > 0)
                        {
                           p_new_db->special_flag |= OLD_ERROR_JOB;
                        }
                     }
                     else
                     {
                        p_new_db->retries           = 0;
                     }
                     p_new_db->age_limit            = DEFAULT_AGE_LIMIT;
#ifdef _OUTPUT_LOG
                     p_new_db->output_log           = YES;
#endif
                     p_new_db->lock                 = DEFAULT_LOCK;
                     p_new_db->smtp_server[0]       = '\0';
                     p_new_db->chmod_str[0]         = '\0';
                     p_new_db->trans_rename_rule[0] = '\0';
                     p_new_db->user_rename_rule[0]  = '\0';
                     p_new_db->rename_file_busy     = '\0';
                     p_new_db->no_of_restart_files  = 0;
                     p_new_db->restart_file         = NULL;
                     p_new_db->user_id              = -1;
                     p_new_db->group_id             = -1;
#ifdef WITH_DUP_CHECK
                     p_new_db->dup_check_flag       = fsa->dup_check_flag;
                     p_new_db->dup_check_timeout    = fsa->dup_check_timeout;
                     p_new_db->crc_id               = fsa->host_id;
#endif
#ifdef WITH_SSL
                     p_new_db->auth                 = NO;
#endif
                     p_new_db->ssh_protocol         = 0;
                     if (db.protocol & FTP_FLAG)
                     {
                        p_new_db->port = DEFAULT_FTP_PORT;
                     }
                     else if (db.protocol & SFTP_FLAG)
                          {
                             p_new_db->port = DEFAULT_SSH_PORT;
                          }
#ifdef _WITH_SCP_SUPPORT
                     else if (db.protocol & SCP_FLAG)
                          {
                             p_new_db->port = DEFAULT_SSH_PORT;
                             p_new_db->chmod = FILE_MODE;
                          }
#endif
#ifdef _WITH_WMO_SUPPORT
                     else if (db.protocol & WMO_FLAG)
                          {
                             p_new_db->port = DEFAULT_WMO_PORT;
                          }
#endif
                     else if (db.protocol & SMTP_FLAG)
                          {
                             p_new_db->port = DEFAULT_SMTP_PORT;
                          }
                          else
                          {
                             p_new_db->port = -1;
                          }
                     (void)strcpy(p_new_db->lock_notation, DOT_NOTATION);
                     (void)sprintf(msg_name, "%s%s/%x",
                                   p_work_dir, AFD_MSG_DIR, db.job_id);
                     if (*(unsigned char *)((char *)p_no_of_hosts + AFD_FEATURE_FLAG_OFFSET_START) & ENABLE_CREATE_TARGET_DIR)
                     {
                        p_new_db->special_flag |= CREATE_TARGET_DIR;
                     }

                     /*
                      * NOTE: We must set protocol for eval_message()
                      *       otherwise some values are NOT set!
                      */
                     p_new_db->protocol = db.protocol;
                     if (eval_message(msg_name, p_new_db) < 0)
                     {
                        free(p_new_db);
                        p_new_db = NULL;
                     }
                     else
                     {
                        /*
                         * Ports must be the same!
                         */
                        if ((p_new_db->port != db.port) ||
#ifdef _WITH_SCP_SUPPORT
                            ((db.protocol & SCP_FLAG) &&
                             (strcmp(p_new_db->target_dir, db.target_dir) != 0)) ||
#endif
#ifdef WITH_SSL
                            ((db.auth == NO) && (p_new_db->auth != NO)) ||
                            ((db.auth != NO) && (p_new_db->auth == NO)) ||
#endif
                            ((db.protocol & SFTP_FLAG) &&
                             (strcmp(p_new_db->user, db.user) != 0)))
                        {
                           free(p_new_db);
                           p_new_db = NULL;
                           ret = NEITHER;
                        }
                        else
                        {
                           if ((p_new_db->protocol & FTP_FLAG) &&
                               (p_new_db->mode_flag == 0))
                           {
                              if (fsa->protocol_options & FTP_PASSIVE_MODE)
                              {
                                 p_new_db->mode_flag = PASSIVE_MODE;
                                 if (fsa->protocol_options & FTP_EXTENDED_MODE)
                                 {
                                    (void)strcpy(p_new_db->mode_str,
                                                 "extended passive");
                                 }
                                 else
                                 {
                                    if (fsa->protocol_options & FTP_ALLOW_DATA_REDIRECT)
                                    {
                                       (void)strcpy(p_new_db->mode_str,
                                                    "passive (with redirect)");
                                    }
                                    else
                                    {
                                       (void)strcpy(p_new_db->mode_str,
                                                    "passive");
                                    }
                                 }
                              }
                              else
                              {
                                 p_new_db->mode_flag = ACTIVE_MODE;
                                 if (fsa->protocol_options & FTP_EXTENDED_MODE)
                                 {
                                    (void)strcpy(p_new_db->mode_str,
                                                 "extended active");
                                 }
                                 else
                                 {
                                    (void)strcpy(p_new_db->mode_str, "active");
                                 }
                              }
                              if (fsa->protocol_options & FTP_EXTENDED_MODE)
                              {
                                 p_new_db->mode_flag |= EXTENDED_MODE;
                              }
                           }
#ifdef WITH_ERROR_QUEUE
                           if ((fsa->host_status & ERROR_QUEUE_SET) &&
                               (check_error_queue(db.job_id, -1) == 1))
                           {
                              p_new_db->special_flag |= IN_ERROR_QUEUE;
                           }
#endif
                           ret = YES;
                        }
                     }
                  }
               }
               else
               {
                  ret = YES;
               }
            }
            else
            {
#ifdef WITH_SIGNAL_WAKEUP
               if ((sleep_time >= WAIT_FOR_FD_REPLY) && (alarm_triggered == YES))
               {
                  fsa->job_status[(int)db.job_no].unique_name[2] = 1;
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Hmmm, FD had no message for <%s> [%u sec]!",
                             fsa->host_alias, sleep_time);
               }
#else
               if (sleep_time >= 40000000L)
               {
                  fsa->job_status[(int)db.job_no].unique_name[2] = 1;
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Hmmm, failed to get a message from FD for <%s> after 40 seconds!",
                             fsa->host_alias);
               }
# ifdef _DEBUG_BURST2
               else
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Hmmm, FD had no message for <%s> [%lu msec]!",
                             fsa->host_alias, sleep_time);
               }
# endif
#endif
            }
         }
#ifdef WITHOUT_FIFO_RW_SUPPORT
         if (close(readfd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "close() error : %s", strerror(errno));
         }
#endif
         if (close(fd) == -1)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "close() error : %s", strerror(errno));
         }
      }

      if (ret == YES)
      {
         *files_to_send = init_sf_burst2(p_new_db, file_path, values_changed);
         if (*files_to_send < 1)
         {
            goto retry;
         }
      }
   }
#endif /* _WITH_BURST_2 */

   return(ret);
}


#ifdef WITH_SIGNAL_WAKEUP
/*+++++++++++++++++++++++++++++++ sig_alarm() +++++++++++++++++++++++++++*/
static void                                                                
sig_alarm(int signo)
{
   if (signo == SIGALRM)
   {
      alarm_triggered = YES;
   }
   return; /* Return to wakeup sigsuspend(). */
}
#endif