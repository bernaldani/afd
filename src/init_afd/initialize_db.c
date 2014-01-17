/*
 *  initialize_db.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2011 - 2013 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   initialize_db - initializes the AFD database
 **
 ** SYNOPSIS
 **   void initialize_db(int init_level, int *old_value_list, int dry_run)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   20.10.2011 H.Kiehl Created
 **   04.11.2012 H.Kiehl When deleting data try only delete the data
 **                      that has changed, ie. that what is really
 **                      necessary.
 **
 */
DESCR__E_M3

#include <stdio.h>              /* sprintf(), fprintf()                  */
#include <string.h>             /* strcpy()                              */
#include <unistd.h>             /* unlink()                              */
#include <errno.h>
#include "amgdefs.h"
#include "logdefs.h"
#include "statdefs.h"

#define FSA_ID_FILE_NO              0
#define FRA_ID_FILE_NO              1
#define STATUS_SHMID_FILE_NO        2
#define BLOCK_FILE_NO               3
#define AMG_COUNTER_FILE_NO         4
#define COUNTER_FILE_NO             5
#define MESSAGE_BUF_FILE_NO         6
#define MSG_CACHE_FILE_NO           7
#define MSG_QUEUE_FILE_NO           8
#ifdef WITH_ERROR_QUEUE
# define ERROR_QUEUE_FILE_NO        9
#endif
#define FILE_MASK_FILE_NO           10
#define DC_LIST_FILE_NO             11
#define DIR_NAME_FILE_NO            12
#define JOB_ID_DATA_FILE_NO         13
#define DCPL_FILE_NAME_NO           14
#define PWB_DATA_FILE_NO            15
#define CURRENT_MSG_LIST_FILE_NO    16
#define AMG_DATA_FILE_NO            17
#define AMG_DATA_FILE_TMP_NO        18
#define LOCK_PROC_FILE_NO           19
#define AFD_ACTIVE_FILE_NO          20
#define WINDOW_ID_FILE_NO           21
#define SYSTEM_LOG_FIFO_NO          22
#define EVENT_LOG_FIFO_NO           23
#define RECEIVE_LOG_FIFO_NO         24
#define TRANSFER_LOG_FIFO_NO        25
#define TRANS_DEBUG_LOG_FIFO_NO     26
#define AFD_CMD_FIFO_NO             27
#define AFD_RESP_FIFO_NO            28
#define AMG_CMD_FIFO_NO             29
#define DB_UPDATE_FIFO_NO           30
#define FD_CMD_FIFO_NO              31
#define AW_CMD_FIFO_NO              32
#define IP_FIN_FIFO_NO              33
#ifdef WITH_ONETIME
# define OT_FIN_FIFO_NO             34
#endif
#define SF_FIN_FIFO_NO              35
#define RETRY_FD_FIFO_NO            36
#define FD_DELETE_FIFO_NO           37
#define FD_WAKE_UP_FIFO_NO          38
#define TRL_CALC_FIFO_NO            39
#define QUEUE_LIST_READY_FIFO_NO    40
#define QUEUE_LIST_DONE_FIFO_NO     41
#define PROBE_ONLY_FIFO_NO          42
#ifdef _INPUT_LOG
# define INPUT_LOG_FIFO_NO          43
#endif
#ifdef _DISTRIBUTION_LOG
# define DISTRIBUTION_LOG_FIFO_NO   44
#endif
#ifdef _OUTPUT_LOG
# define OUTPUT_LOG_FIFO_NO         45
#endif
#ifdef _DELETE_LOG
# define DELETE_LOG_FIFO_NO         46
#endif
#ifdef _PRODUCTION_LOG
# define PRODUCTION_LOG_FIFO_NO     47
#endif
#define DEL_TIME_JOB_FIFO_NO        48
#define FD_READY_FIFO_NO            49
#define MSG_FIFO_NO                 50
#define DC_CMD_FIFO_NO              51
#define DC_RESP_FIFO_NO             52
#define AFDD_LOG_FIFO_NO            53
#define TYPESIZE_DATA_FILE_NO       54
#define SYSTEM_DATA_FILE_NO         55
#define MAX_FILE_LIST_LENGTH        56

#define FSA_STAT_FILE_ALL_NO        0
#define FRA_STAT_FILE_ALL_NO        1
#define ALTERNATE_FILE_ALL_NO       2
#define DB_UPDATE_REPLY_FIFO_ALL_NO 3
#define NNN_FILE_ALL_NO             4
#define MAX_MFILE_LIST_LENGTH       5

#define AFD_MSG_DIR_FLAG            1
#ifdef WITH_DUP_CHECK
# define CRC_DIR_FLAG               2
#endif
#define FILE_MASK_DIR_FLAG          4
#define LS_DATA_DIR_FLAG            8

/* External global variables. */
extern int  sys_log_fd;
extern char *p_work_dir;

/* Local function prototypes. */
static void delete_fifodir_files(char *, int, char *, char *, int),
            delete_log_files(char *, int, int);


/*########################## initialize_db() ############################*/
void
initialize_db(int init_level, int *old_value_list, int dry_run)
{
   int  delete_dir,
        offset;
   char dirs[MAX_PATH_LENGTH],
        filelistflag[MAX_FILE_LIST_LENGTH],
        mfilelistflag[MAX_MFILE_LIST_LENGTH];

   (void)memset(filelistflag, NO, MAX_FILE_LIST_LENGTH);
   (void)memset(mfilelistflag, NO, MAX_MFILE_LIST_LENGTH);
   if (old_value_list != NULL)
   {
      delete_dir = 0;
      if (old_value_list[0] & MAX_MSG_NAME_LENGTH_NR)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
         filelistflag[MSG_QUEUE_FILE_NO] = YES;
      }
      if (old_value_list[0] & MAX_FILENAME_LENGTH_NR)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
         delete_dir |= LS_DATA_DIR_FLAG;
      }
      if (old_value_list[0] & MAX_HOSTNAME_LENGTH_NR)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
         filelistflag[FRA_ID_FILE_NO] = YES;
         mfilelistflag[FRA_STAT_FILE_ALL_NO] = YES;
         filelistflag[JOB_ID_DATA_FILE_NO] = YES;
      }
      if (old_value_list[0] & MAX_REAL_HOSTNAME_LENGTH_NR)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
         filelistflag[STATUS_SHMID_FILE_NO] = YES;
         filelistflag[PWB_DATA_FILE_NO] = YES;
      }
      if (old_value_list[0] & MAX_PROXY_NAME_LENGTH_NR)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
      }
      if (old_value_list[0] & MAX_TOGGLE_STR_LENGTH_NR)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
      }
      if (old_value_list[0] & ERROR_HISTORY_LENGTH_NR)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
      }
      if (old_value_list[0] & MAX_NO_PARALLEL_JOBS_NR)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
      }
      if (old_value_list[0] & MAX_DIR_ALIAS_LENGTH_NR)
      {
         filelistflag[FRA_ID_FILE_NO] = YES;
         mfilelistflag[FRA_STAT_FILE_ALL_NO] = YES;
      }
      if (old_value_list[0] & MAX_RECIPIENT_LENGTH_NR)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
         filelistflag[JOB_ID_DATA_FILE_NO] = YES;
      }
      if (old_value_list[0] & MAX_WAIT_FOR_LENGTH_NR)
      {
         filelistflag[FRA_ID_FILE_NO] = YES;
         mfilelistflag[FRA_STAT_FILE_ALL_NO] = YES;
      }
      if (old_value_list[0] & MAX_FRA_TIME_ENTRIES_NR)
      {
         filelistflag[FRA_ID_FILE_NO] = YES;
         mfilelistflag[FRA_STAT_FILE_ALL_NO] = YES;
      }
      if (old_value_list[0] & MAX_OPTION_LENGTH_NR)
      {
         filelistflag[JOB_ID_DATA_FILE_NO] = YES;
      }
      if (old_value_list[0] & MAX_PATH_LENGTH_NR)
      {
         filelistflag[DIR_NAME_FILE_NO] = YES;
         filelistflag[DC_LIST_FILE_NO] = YES;
      }
      if (old_value_list[0] & MAX_USER_NAME_LENGTH_NR)
      {
         filelistflag[PWB_DATA_FILE_NO] = YES;
      }
      if ((old_value_list[0] & CHAR_NR) ||
          (old_value_list[0] & INT_NR))
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         filelistflag[FRA_ID_FILE_NO] = YES;
         filelistflag[STATUS_SHMID_FILE_NO] = YES;
         /* No need to delete BLOCK_FILE */
         filelistflag[AMG_COUNTER_FILE_NO] = YES;
         filelistflag[COUNTER_FILE_NO] = YES;
         filelistflag[MESSAGE_BUF_FILE_NO] = YES;
         filelistflag[MSG_CACHE_FILE_NO] = YES;
         filelistflag[MSG_QUEUE_FILE_NO] = YES;
#ifdef WITH_ERROR_QUEUE
         filelistflag[ERROR_QUEUE_FILE_NO] = YES;
#endif
         filelistflag[FILE_MASK_FILE_NO] = YES;
         filelistflag[DC_LIST_FILE_NO] = YES;
         filelistflag[DIR_NAME_FILE_NO] = YES;
         filelistflag[JOB_ID_DATA_FILE_NO] = YES;
         filelistflag[DCPL_FILE_NAME_NO] = YES;
         filelistflag[PWB_DATA_FILE_NO] = YES;
         filelistflag[CURRENT_MSG_LIST_FILE_NO] = YES;
         filelistflag[AMG_DATA_FILE_NO] = YES;
         filelistflag[AMG_DATA_FILE_TMP_NO] = YES;
         filelistflag[LOCK_PROC_FILE_NO] = YES;
         filelistflag[AFD_ACTIVE_FILE_NO] = YES;
         filelistflag[TYPESIZE_DATA_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
         mfilelistflag[FRA_STAT_FILE_ALL_NO] = YES;
         mfilelistflag[ALTERNATE_FILE_ALL_NO] = YES;
         mfilelistflag[NNN_FILE_ALL_NO] = YES;
         delete_dir |= LS_DATA_DIR_FLAG;
      }
      if (old_value_list[0] & OFF_T_NR)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
         filelistflag[MSG_QUEUE_FILE_NO] = YES;
         delete_dir |= LS_DATA_DIR_FLAG;
      }
      if (old_value_list[0] & TIME_T_NR)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
         filelistflag[MSG_QUEUE_FILE_NO] = YES;
         delete_dir |= LS_DATA_DIR_FLAG;
      }
      if (old_value_list[0] & SHORT_NR)
      {
         filelistflag[FRA_ID_FILE_NO] = YES;
         mfilelistflag[FRA_STAT_FILE_ALL_NO] = YES;
      }
#ifdef HAVE_LONG_LONG
      if (old_value_list[0] & LONG_LONG_NR)
      {
         filelistflag[FRA_ID_FILE_NO] = YES;
         mfilelistflag[FRA_STAT_FILE_ALL_NO] = YES;
      }
#endif
      if (old_value_list[0] & PID_T_NR)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
         filelistflag[MSG_QUEUE_FILE_NO] = YES;
      }
   }
   else
   {
      delete_dir = 0;
      if (init_level > 0)
      {
         filelistflag[SYSTEM_LOG_FIFO_NO] = YES;
         filelistflag[EVENT_LOG_FIFO_NO] = YES;
         filelistflag[RECEIVE_LOG_FIFO_NO] = YES;
         filelistflag[TRANSFER_LOG_FIFO_NO] = YES;
         filelistflag[TRANS_DEBUG_LOG_FIFO_NO] = YES;
         filelistflag[AFD_CMD_FIFO_NO] = YES;
         filelistflag[AFD_RESP_FIFO_NO] = YES;
         filelistflag[AMG_CMD_FIFO_NO] = YES;
         filelistflag[DB_UPDATE_FIFO_NO] = YES;
         filelistflag[FD_CMD_FIFO_NO] = YES;
         filelistflag[AW_CMD_FIFO_NO] = YES;
         filelistflag[IP_FIN_FIFO_NO] = YES;
#ifdef WITH_ONETIME
         filelistflag[OT_FIN_FIFO_NO] = YES;
#endif
         filelistflag[SF_FIN_FIFO_NO] = YES;
         filelistflag[RETRY_FD_FIFO_NO] = YES;
         filelistflag[FD_DELETE_FIFO_NO] = YES;
         filelistflag[FD_WAKE_UP_FIFO_NO] = YES;
         filelistflag[TRL_CALC_FIFO_NO] = YES;
         filelistflag[QUEUE_LIST_READY_FIFO_NO] = YES;
         filelistflag[QUEUE_LIST_DONE_FIFO_NO] = YES;
         filelistflag[PROBE_ONLY_FIFO_NO] = YES;
#ifdef _INPUT_LOG
         filelistflag[INPUT_LOG_FIFO_NO] = YES;
#endif
#ifdef _DISTRIBUTION_LOG
         filelistflag[DISTRIBUTION_LOG_FIFO_NO] = YES;
#endif
#ifdef _OUTPUT_LOG
         filelistflag[OUTPUT_LOG_FIFO_NO] = YES;
#endif
#ifdef _DELETE_LOG
         filelistflag[DELETE_LOG_FIFO_NO] = YES;
#endif
#ifdef _PRODUCTION_LOG
         filelistflag[PRODUCTION_LOG_FIFO_NO] = YES;
#endif
         filelistflag[DEL_TIME_JOB_FIFO_NO] = YES;
         filelistflag[FD_READY_FIFO_NO] = YES;
         filelistflag[MSG_FIFO_NO] = YES;
         filelistflag[DC_CMD_FIFO_NO] = YES;
         filelistflag[DC_RESP_FIFO_NO] = YES;
         filelistflag[AFDD_LOG_FIFO_NO] = YES;
         mfilelistflag[DB_UPDATE_REPLY_FIFO_ALL_NO] = YES;
      }
      if (init_level > 1)
      {
         filelistflag[AFD_ACTIVE_FILE_NO] = YES;
         filelistflag[WINDOW_ID_FILE_NO] = YES;
         filelistflag[LOCK_PROC_FILE_NO] = YES;
         filelistflag[DCPL_FILE_NAME_NO] = YES;
      }
      if (init_level > 2)
      {
         filelistflag[FSA_ID_FILE_NO] = YES;
         mfilelistflag[FSA_STAT_FILE_ALL_NO] = YES;
         filelistflag[FRA_ID_FILE_NO] = YES;
         mfilelistflag[FRA_STAT_FILE_ALL_NO] = YES;
         filelistflag[AMG_DATA_FILE_NO] = YES;
         filelistflag[AMG_DATA_FILE_TMP_NO] = YES;
         mfilelistflag[ALTERNATE_FILE_ALL_NO] = YES;
      }
      if (init_level > 3)
      {
         delete_dir |= AFD_MSG_DIR_FLAG | FILE_MASK_DIR_FLAG;
         filelistflag[MESSAGE_BUF_FILE_NO] = YES;
         filelistflag[MSG_CACHE_FILE_NO] = YES;
         filelistflag[MSG_QUEUE_FILE_NO] = YES;
#ifdef WITH_ERROR_QUEUE
         filelistflag[ERROR_QUEUE_FILE_NO] = YES;
#endif
         filelistflag[CURRENT_MSG_LIST_FILE_NO] = YES;
      }
      if (init_level > 4)
      {
         filelistflag[FILE_MASK_FILE_NO] = YES;
         filelistflag[DC_LIST_FILE_NO] = YES;
         filelistflag[DIR_NAME_FILE_NO] = YES;
         filelistflag[JOB_ID_DATA_FILE_NO] = YES;
      }
      if (init_level > 5)
      {
         filelistflag[STATUS_SHMID_FILE_NO] = YES;
      }
      if (init_level > 6)
      {
         filelistflag[BLOCK_FILE_NO] = YES;
         filelistflag[AMG_COUNTER_FILE_NO] = YES;
         filelistflag[COUNTER_FILE_NO] = YES;
         mfilelistflag[NNN_FILE_ALL_NO] = YES;
#ifdef WITH_DUP_CHECK
         delete_dir |= CRC_DIR_FLAG;
#endif
      }
      if (init_level > 7)
      {
         filelistflag[PWB_DATA_FILE_NO] = YES;
         filelistflag[TYPESIZE_DATA_FILE_NO] = YES;
         filelistflag[SYSTEM_DATA_FILE_NO] = YES;
         delete_dir |= LS_DATA_DIR_FLAG;
      }
   }

#ifdef HAVE_SNPRINTF
   offset = snprintf(dirs, MAX_PATH_LENGTH, "%s%s", p_work_dir, FIFO_DIR);
#else
   offset = sprintf(dirs, "%s%s", p_work_dir, FIFO_DIR);
#endif
   delete_fifodir_files(dirs, offset, filelistflag, mfilelistflag, dry_run);
   if (delete_dir & AFD_MSG_DIR_FLAG)
   {
#ifdef HAVE_SNPRINTF
      (void)snprintf(dirs, MAX_PATH_LENGTH, "%s%s", p_work_dir, AFD_MSG_DIR);
#else
      (void)sprintf(dirs, "%s%s", p_work_dir, AFD_MSG_DIR);
#endif
      if (dry_run == YES)
      {
         (void)fprintf(stdout, "rm -rf %s\n", dirs);
      }
      else
      {
         if (rec_rmdir(dirs) == INCORRECT)
         {
            (void)fprintf(stderr,
                           _("WARNING : Failed to delete everything in %s.\n"),
                           dirs);
         }
      }
   }
#ifdef WITH_DUP_CHECK
   if (delete_dir & CRC_DIR_FLAG)
   {
#ifdef HAVE_SNPRINTF
      (void)snprintf(dirs, MAX_PATH_LENGTH, "%s%s%s",
#else
      (void)sprintf(dirs, "%s%s%s",
#endif
                    p_work_dir, AFD_FILE_DIR, CRC_DIR);
      if (dry_run == YES)
      {
         (void)fprintf(stdout, "rm -rf %s\n", dirs);
      }
      else
      {
         if (rec_rmdir(dirs) == INCORRECT)
         {
            (void)fprintf(stderr,
                          _("WARNING : Failed to delete everything in %s.\n"),
                          dirs);
         }
      }
   }
#endif
   if (delete_dir & FILE_MASK_DIR_FLAG)
   {
#ifdef HAVE_SNPRINTF
      (void)snprintf(dirs, MAX_PATH_LENGTH, "%s%s%s%s",
#else
      (void)sprintf(dirs, "%s%s%s%s",
#endif
                    p_work_dir, AFD_FILE_DIR, INCOMING_DIR, FILE_MASK_DIR);
      if (dry_run == YES)
      {
         (void)fprintf(stdout, "rm -rf %s\n", dirs);
      }
      else
      {
         if (rec_rmdir(dirs) == INCORRECT)
         {
            (void)fprintf(stderr,
                          _("WARNING : Failed to delete everything in %s.\n"),
                          dirs);
         }
      }
   }
   if (delete_dir & LS_DATA_DIR_FLAG)
   {
#ifdef HAVE_SNPRINTF
      (void)snprintf(dirs, MAX_PATH_LENGTH, "%s%s%s%s",
#else
      (void)sprintf(dirs, "%s%s%s%s",
#endif
                    p_work_dir, AFD_FILE_DIR, INCOMING_DIR, LS_DATA_DIR);
      if (dry_run == YES)
      {
         (void)fprintf(stdout, "rm -rf %s\n", dirs);
      }
      else
      {
         if (rec_rmdir(dirs) == INCORRECT)
         {
            (void)fprintf(stderr,
                          _("WARNING : Failed to delete everything in %s.\n"),
                          dirs);
         }
      }
   }
   if (init_level > 8)
   {
#ifdef HAVE_SNPRINTF
      (void)snprintf(dirs, MAX_PATH_LENGTH, "%s%s", p_work_dir, AFD_FILE_DIR);
#else
      (void)sprintf(dirs, "%s%s", p_work_dir, AFD_FILE_DIR);
#endif
      if (dry_run == YES)
      {
         (void)fprintf(stdout, "rm -rf %s\n", dirs);
      }
      else
      {
         if (rec_rmdir(dirs) == INCORRECT)
         {
            (void)fprintf(stderr,
                          _("WARNING : Failed to delete everything in %s.\n"),
                          dirs);
         }
      }
#ifdef HAVE_SNPRINTF
      (void)snprintf(dirs, MAX_PATH_LENGTH, "%s%s", p_work_dir, AFD_ARCHIVE_DIR);
#else
      (void)sprintf(dirs, "%s%s", p_work_dir, AFD_ARCHIVE_DIR);
#endif
      if (dry_run == YES)
      {
         (void)fprintf(stdout, "rm -rf %s\n", dirs);
      }
      else
      {
         if (rec_rmdir(dirs) == INCORRECT)
         {
            (void)fprintf(stderr,
                          _("WARNING : Failed to delete everything in %s.\n"),
                          dirs);
         }
      }
#ifdef HAVE_SNPRINTF
      offset = snprintf(dirs, MAX_PATH_LENGTH, "%s%s", p_work_dir, LOG_DIR);
#else
      offset = sprintf(dirs, "%s%s", p_work_dir, LOG_DIR);
#endif
      delete_log_files(dirs, offset, dry_run);
   }

   return;
}


/*+++++++++++++++++++++++++ delete_fifodir_files() ++++++++++++++++++++++*/
static void
delete_fifodir_files(char *fifodir,
                     int  offset,
                     char *filelistflag,
                     char *mfilelistflag,
                     int  dry_run)
{
   int  i,
        tmp_sys_log_fd;
   char *file_ptr,
        *filelist[] =
        {
           FSA_ID_FILE,
           FRA_ID_FILE,
           STATUS_SHMID_FILE,
           BLOCK_FILE,
           AMG_COUNTER_FILE,
           COUNTER_FILE,
           MESSAGE_BUF_FILE,
           MSG_CACHE_FILE,
           MSG_QUEUE_FILE,
#ifdef WITH_ERROR_QUEUE
           ERROR_QUEUE_FILE,
#else
           "",
#endif
           FILE_MASK_FILE,
           DC_LIST_FILE,
           DIR_NAME_FILE,
           JOB_ID_DATA_FILE,
           DCPL_FILE_NAME,
           PWB_DATA_FILE,
           CURRENT_MSG_LIST_FILE,
           AMG_DATA_FILE,
           AMG_DATA_FILE_TMP,
           LOCK_PROC_FILE,
           AFD_ACTIVE_FILE,
           WINDOW_ID_FILE,
           SYSTEM_LOG_FIFO,
           EVENT_LOG_FIFO,
           RECEIVE_LOG_FIFO,
           TRANSFER_LOG_FIFO,
           TRANS_DEBUG_LOG_FIFO,
           AFD_CMD_FIFO,
           AFD_RESP_FIFO,
           AMG_CMD_FIFO,
           DB_UPDATE_FIFO,
           FD_CMD_FIFO,
           AW_CMD_FIFO,
           IP_FIN_FIFO,
#ifdef WITH_ONETIME
           OT_FIN_FIFO,
#else
           "",
#endif
           SF_FIN_FIFO,
           RETRY_FD_FIFO,
           FD_DELETE_FIFO,
           FD_WAKE_UP_FIFO,
           TRL_CALC_FIFO,
           QUEUE_LIST_READY_FIFO,
           QUEUE_LIST_DONE_FIFO,
           PROBE_ONLY_FIFO,
#ifdef _INPUT_LOG
           INPUT_LOG_FIFO,
#else
           "",
#endif
#ifdef _DISTRIBUTION_LOG
           DISTRIBUTION_LOG_FIFO,
#else
           "",
#endif
#ifdef _OUTPUT_LOG
           OUTPUT_LOG_FIFO,
#else
           "",
#endif
#ifdef _DELETE_LOG
           DELETE_LOG_FIFO,
#else
           "",
#endif
#ifdef _PRODUCTION_LOG
           PRODUCTION_LOG_FIFO,
#else
           "",
#endif
           DEL_TIME_JOB_FIFO,
           FD_READY_FIFO,
           MSG_FIFO,
           DC_CMD_FIFO, /* from amgdefs.h */
           DC_RESP_FIFO,
           AFDD_LOG_FIFO,
           TYPESIZE_DATA_FILE,
           SYSTEM_DATA_FILE
        },
        *mfilelist[] =
        {
           FSA_STAT_FILE_ALL,
           FRA_STAT_FILE_ALL,
           ALTERNATE_FILE_ALL,
           DB_UPDATE_REPLY_FIFO_ALL,
           NNN_FILE_ALL
        };

   file_ptr = fifodir + offset;

   /* Delete single files. */
   for (i = 0; i < (sizeof(filelist) / sizeof(*filelist)); i++)
   {
      if (filelistflag[i] == YES)
      {
         (void)strcpy(file_ptr, filelist[i]);
         if (dry_run == YES)
         {
            (void)fprintf(stdout, "rm -f %s\n", fifodir);
         }
         else
         {
            (void)unlink(fifodir);
         }
      }
   }
   *file_ptr = '\0';

   tmp_sys_log_fd = sys_log_fd;
   sys_log_fd = STDOUT_FILENO;

   /* Delete multiple files. */
   for (i = 0; i < (sizeof(mfilelist) / sizeof(*mfilelist)); i++)
   {
      if (mfilelistflag[i] == YES)
      {
         if (dry_run == YES)
         {
            (void)fprintf(stdout, "rm -f %s/%s\n", fifodir, &mfilelist[i][1]);
         }
         else
         {
            (void)remove_files(fifodir, &mfilelist[i][1]);
         }
      }
   }

   sys_log_fd = tmp_sys_log_fd;

   return;
}


/*++++++++++++++++++++++++++++ delete_log_files() +++++++++++++++++++++++*/
static void
delete_log_files(char *logdir, int offset, int dry_run)
{
   int  i,
        tmp_sys_log_fd;
   char *log_ptr,
        *loglist[] =
        {
           "/DAEMON_LOG.init_afd",
           NEW_STATISTIC_FILE,
           NEW_ISTATISTIC_FILE
        },
        *mloglist[] =
        {
           SYSTEM_LOG_NAME_ALL,
           EVENT_LOG_NAME_ALL,
           RECEIVE_LOG_NAME_ALL,
           TRANSFER_LOG_NAME_ALL,
           TRANS_DB_LOG_NAME_ALL,
#ifdef _INPUT_LOG
           INPUT_BUFFER_FILE_ALL,
#endif
#ifdef _DISTRIBUTION_LOG
           DISTRIBUTION_BUFFER_FILE_ALL,
#endif
#ifdef _OUTPUT_LOG
           OUTPUT_BUFFER_FILE_ALL,
#endif
#ifdef _DELETE_LOG
           DELETE_BUFFER_FILE_ALL,
#endif
#ifdef _PRODUCTION_LOG
           PRODUCTION_BUFFER_FILE_ALL,
#endif
           ISTATISTIC_FILE_ALL,
           STATISTIC_FILE_ALL
        };

   log_ptr = logdir + offset;

   /* Delete single files. */
   for (i = 0; i < (sizeof(loglist) / sizeof(*loglist)); i++)
   {
      (void)strcpy(log_ptr, loglist[i]);
      if (dry_run == YES)
      {
         (void)fprintf(stdout, "rm -f %s\n", logdir);
      }
      else
      {
         (void)unlink(logdir);
      }
   }
   *log_ptr = '\0';

   tmp_sys_log_fd = sys_log_fd;
   sys_log_fd = STDOUT_FILENO;

   /* Delete multiple files. */
   for (i = 0; i < (sizeof(mloglist)/sizeof(*mloglist)); i++)
   {
      if (dry_run == YES)
      {
         (void)fprintf(stdout, "rm -f %s/%s\n", logdir, mloglist[i]);
      }
      else
      {
         (void)remove_files(logdir, mloglist[i]);
      }
   }

   sys_log_fd = tmp_sys_log_fd;

   return;
}
