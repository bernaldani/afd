/*
 *  check_permissions.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2003 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_permissions - checks the file access permissions of AFD
 **
 ** SYNOPSIS
 **   void check_permissions(void)
 **
 ** DESCRIPTION
 **   The function check_permissions checks if all the file access
 **   permissions of important files in fifodir are set correctly.
 **   If not it will print a warning in the SYSTEM_LOG and correct
 **   it.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   30.04.2003 H.Kiehl Created
 **   18.12.2005 H.Kiehl We may NOT use function system_log() to write
 **                      some log entries, we can deadlock because process
 **                      system_log is not running.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>             /* strcpy()                              */
#include <sys/types.h>
#include <sys/stat.h>           /* stat(), chmod()                       */
#include <errno.h>
#include "logdefs.h"

struct check_list
       {
          char   *file_name;
          mode_t full_mode;
          mode_t mode;
       };

/* External global variables. */
extern char *p_work_dir;


/*######################### check_permissions() #########################*/
void
check_permissions(void)
{
   int               i;
   char              fullname[MAX_PATH_LENGTH],
                     *ptr;
   struct stat       stat_buf;
   struct check_list fifodir[] =
                     {
#ifdef GROUP_CAN_WRITE
                        { SYSTEM_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { EVENT_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { RECEIVE_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { TRANSFER_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { TRANS_DEBUG_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { MON_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { AFD_CMD_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { AFD_RESP_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { AMG_CMD_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { DB_UPDATE_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { FD_CMD_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { AW_CMD_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { IP_FIN_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { SF_FIN_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { RETRY_FD_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { FD_DELETE_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { FD_WAKE_UP_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { PROBE_ONLY_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
# ifdef _INPUT_LOG
                        { INPUT_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
# endif
# ifdef _OUTPUT_LOG
                        { OUTPUT_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
# endif
# ifdef _DELETE_LOG
                        { DELETE_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
# endif
# ifdef _PRODUCTION_LOG
                        { PRODUCTION_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
# endif
                        { DEL_TIME_JOB_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { FD_READY_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { STATUS_SHMID_FILE, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { AMG_DATA_FILE, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { AFD_ACTIVE_FILE, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { MSG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { AFDD_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { COUNTER_FILE, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { MESSAGE_BUF_FILE, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { LOCK_PROC_FILE, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { PWB_DATA_FILE, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { AMG_COUNTER_FILE, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { FILE_MASK_FILE, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) },
                        { DC_LIST_FILE, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH) },
#else
                        { SYSTEM_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { EVENT_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { RECEIVE_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { TRANSFER_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { TRANS_DEBUG_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { MON_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { AFD_CMD_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { AFD_RESP_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { AMG_CMD_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { DB_UPDATE_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { FD_CMD_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { AW_CMD_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { IP_FIN_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { SF_FIN_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { RETRY_FD_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { FD_DELETE_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { FD_WAKE_UP_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { PROBE_ONLY_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
# ifdef _INPUT_LOG
                        { INPUT_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
# endif
# ifdef _OUTPUT_LOG
                        { OUTPUT_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
# endif
# ifdef _DELETE_LOG
                        { DELETE_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
# endif
# ifdef _PRODUCTION_LOG
                        { PRODUCTION_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
# endif
                        { DEL_TIME_JOB_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { FD_READY_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { STATUS_SHMID_FILE, (S_IFREG | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { AMG_DATA_FILE, (S_IFREG | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { AFD_ACTIVE_FILE, (S_IFREG | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { MSG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { AFDD_LOG_FIFO, (S_IFIFO | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { COUNTER_FILE, (S_IFREG | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { MESSAGE_BUF_FILE, (S_IFREG | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { LOCK_PROC_FILE, (S_IFREG | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { PWB_DATA_FILE, (S_IFREG | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { AMG_COUNTER_FILE, (S_IFREG | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { FILE_MASK_FILE, (S_IFREG | S_IRUSR | S_IWUSR), (S_IRUSR | S_IWUSR) },
                        { DC_LIST_FILE, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP| S_IROTH), (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) },
#endif /* !GROUP_CAN_WRITE */
                        { DIR_NAME_FILE, (S_IFREG | FILE_MODE), FILE_MODE },
                        { JOB_ID_DATA_FILE, (S_IFREG | FILE_MODE), FILE_MODE },
                        { CURRENT_MSG_LIST_FILE, (S_IFREG | FILE_MODE), FILE_MODE },
                        { FSA_ID_FILE, (S_IFREG | FILE_MODE), FILE_MODE },
                        { FRA_ID_FILE, (S_IFREG | FILE_MODE), FILE_MODE },
                        { MSG_CACHE_FILE, (S_IFREG | FILE_MODE), FILE_MODE },
                        { MSG_QUEUE_FILE, (S_IFREG | FILE_MODE), FILE_MODE },
#ifdef WITH_ERROR_QUEUE
                        { ERROR_QUEUE_FILE, (S_IFREG | FILE_MODE), FILE_MODE },
#endif
                        { NULL, 0, 0 }
                     },
                     logdir[] =
                     {
#ifdef GROUP_CAN_WRITE
                        { "DAEMON_LOG.init_afd", (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH) },
# ifdef _DELETE_LOG
                        { DELETE_BUFFER_FILE, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH) },
# else
                        { "", 0, 0 },
# endif
# ifdef _INPUT_LOG
                        { INPUT_BUFFER_FILE, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH) },
# else
                        { "", 0, 0 },
# endif
# ifdef _OUTPUT_LOG
                        { OUTPUT_BUFFER_FILE, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH) },
# else
                        { "", 0, 0 },
# endif
# ifdef _PRODUCTION_LOG
                        { PRODUCTION_BUFFER_FILE, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH) },
# else
                        { "", 0, 0 },
# endif
                        { RECEIVE_LOG_NAME, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH) },
                        { SYSTEM_LOG_NAME, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH) },
                        { EVENT_LOG_NAME, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH) },
                        { TRANSFER_LOG_NAME, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH) },
                        { TRANS_DB_LOG_NAME, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH) },
#else
                        { "DAEMON_LOG.init_afd", (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH), (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) },
# ifdef _DELETE_LOG
                        { DELETE_BUFFER_FILE, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH), (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) },
# else
                        { "", 0, 0 },
# endif
# ifdef _INPUT_LOG
                        { INPUT_BUFFER_FILE, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH), (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) },
# else
                        { "", 0, 0 },
# endif
# ifdef _OUTPUT_LOG
                        { OUTPUT_BUFFER_FILE, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH), (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) },
# else
                        { "", 0, 0 },
# endif
# ifdef _PRODUCTION_LOG
                        { PRODUCTION_BUFFER_FILE, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH), (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) },
# else
                        { "", 0, 0 },
# endif
                        { RECEIVE_LOG_NAME, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH), (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) },
                        { SYSTEM_LOG_NAME, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH), (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) },
                        { EVENT_LOG_NAME, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH), (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) },
                        { TRANSFER_LOG_NAME, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH), (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) },
                        { TRANS_DB_LOG_NAME, (S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH), (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) },
#endif /* GROUP_CAN_WRITE */
                        { NULL, 0, 0 }
                     };

   ptr = fullname + sprintf(fullname, "%s%s", p_work_dir, FIFO_DIR);
   i = 0;
   do
   {
      (void)strcpy(ptr, fifodir[i].file_name);
      if (stat(fullname, &stat_buf) == -1)
      {
         if (errno != ENOENT)
         {
            (void)fprintf(stdout, "Can't access file %s : %s (%s %d)\n",
                          fullname, strerror(errno), __FILE__, __LINE__);
         }
      }
      else
      {
         if (stat_buf.st_mode != fifodir[i].full_mode)
         {
            (void)fprintf(stdout,
                          "File %s has mode %o, changing to %o. (%s %d)\n",
                          fullname, stat_buf.st_mode, fifodir[i].full_mode,
                          __FILE__, __LINE__);
            if (chmod(fullname, fifodir[i].mode) == -1)
            {
               (void)fprintf(stdout,
                             "Can't change mode to %o for file %s : %s (%s %d)\n",
                             fifodir[i].mode, fullname, strerror(errno),
                             __FILE__, __LINE__);
            }
         }
      }
      i++;
   } while (fifodir[i].file_name != NULL);

   ptr = fullname + sprintf(fullname, "%s%s/", p_work_dir, LOG_DIR);
   i = 0;
   do
   {
      if (logdir[i].file_name[0] != '\0')
      {
         (void)strcpy(ptr, logdir[i].file_name);
         if (i != 0)
         {
            (void)strcat(ptr, "0");
         }
         if (stat(fullname, &stat_buf) == -1)
         {
            if (errno != ENOENT)
            {
               (void)fprintf(stdout, "Can't access file %s : %s (%s %d)\n",
                             fullname, strerror(errno), __FILE__, __LINE__);
            }
         }
         else
         {
            if (stat_buf.st_mode != logdir[i].full_mode)
            {
               (void)fprintf(stdout,
                             "File %s has mode %o, changing to %o. (%s %d)\n",
                             fullname, stat_buf.st_mode, logdir[i].full_mode,
                             __FILE__, __LINE__);
               if (chmod(fullname, logdir[i].mode) == -1)
               {
                  (void)fprintf(stdout,
                                "Can't change mode to %o for file %s : %s (%s %d)\n",
                                logdir[i].mode, fullname, strerror(errno),
                                __FILE__, __LINE__);
               }
            }
         }
      }
      i++;
   } while (logdir[i].file_name != NULL);

   return;
}
