/*
 *  check_fsa_entries.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   check_fsa_entries - checks all FSA entries if they are still
 **                       correct
 **
 ** SYNOPSIS
 **   void check_fsa_entries(void)
 **
 ** DESCRIPTION
 **   The function check_fsa_entries() checks the file counter,
 **   file size, active transfers and error counter of all hosts
 **   in the FSA. This check is only performed when there are
 **   currently no message for the checked host in the message
 **   queue qb.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.06.1998 H.Kiehl Created
 **   07.04.2002 H.Kiehl Added some more checks for the job_status struct.
 **
 */
DESCR__E_M3

#include "fddefs.h"

/* External global variables */
extern int                        no_of_hosts,
                                  *no_msg_queued,
                                  sys_log_fd;
extern struct filetransfer_status *fsa;
extern struct fileretrieve_status *fra;
extern struct queue_buf           *qb;
extern struct msg_cache_buf       *mdb;


/*########################## check_fsa_entries() ########################*/
void
check_fsa_entries(void)
{
   register int gotcha, i, j;

   for (i = 0; i < no_of_hosts; i++)
   {
      gotcha = NO;
      for (j = 0; j < *no_msg_queued; j++)
      {
         if (qb[j].msg_name[0] == '\0')
         {
            if (fra[qb[j].pos].fsa_pos == i)
            {
               gotcha = YES;
               break;
            }
         }
         else
         {
            if (mdb[qb[j].pos].fsa_pos == i)
            {
               gotcha = YES;
               break;
            }
         }
      }

      /*
       * If there are currently no messages stored for this host
       * we can check if the values for file size, number of files,
       * number of active transfers and the error counter in the FSA
       * are still correct.
       */
      if (gotcha == NO)
      {
         if (fsa[i].active_transfers != 0)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "Active transfers for host %s is %d. It should be 0. Correcting. (%s %d)\n",
                      fsa[i].host_dsp_name, fsa[i].active_transfers,
                      __FILE__, __LINE__);
            fsa[i].active_transfers = 0;
         }
         if (fsa[i].total_file_counter != 0)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "File counter for host %s is %d. It should be 0. Correcting. (%s %d)\n",
                      fsa[i].host_dsp_name, fsa[i].total_file_counter,
                      __FILE__, __LINE__);
            fsa[i].total_file_counter = 0;
         }
         if (fsa[i].total_file_size != 0)
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "File size for host %s is %lu. It should be 0. Correcting. (%s %d)\n",
                      fsa[i].host_dsp_name, fsa[i].total_file_size,
                      __FILE__, __LINE__);
            fsa[i].total_file_size = 0;
         }
#ifdef _DO_NOT_RESET_RETRIEVE_JOB
         if ((fsa[i].error_counter != 0) && ((fsa[i].protocol & GET_FTP) == 0))
#else
         if (fsa[i].error_counter != 0)
#endif
         {
            (void)rec(sys_log_fd, DEBUG_SIGN,
                      "Error counter for host %s is %d. It should be 0. Correcting. (%s %d)\n",
                      fsa[i].host_dsp_name, fsa[i].error_counter,
                      __FILE__, __LINE__);
            fsa[i].error_counter = 0;
         }
         for (j = 0; j < fsa[i].allowed_transfers; j++)
         {
            if (fsa[i].job_status[j].connect_status != DISCONNECT)
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "Connect status %d for host %s is %d. It should be %d. Correcting. (%s %d)\n",
                         j, fsa[i].host_dsp_name,
                         fsa[i].job_status[j].connect_status,
                         DISCONNECT, __FILE__, __LINE__);
               fsa[i].job_status[j].connect_status = DISCONNECT;
            }
            if (fsa[i].job_status[j].proc_id != -1)
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "Process ID in job %d for host %s is %d. It should be -1. Correcting. (%s %d)\n",
                         j, fsa[i].host_dsp_name, fsa[i].job_status[j].proc_id,
                         __FILE__, __LINE__);
               fsa[i].job_status[j].proc_id = -1;
            }
#if defined (_BURST_MODE) || defined (_OUTPUT_LOG)
            if (fsa[i].job_status[j].job_id != NO_ID)
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "Job ID in job %d for host %s is %d. It should be %d. Correcting. (%s %d)\n",
                         j, fsa[i].host_dsp_name, fsa[i].job_status[j].job_id,
                         NO_ID, __FILE__, __LINE__);
               fsa[i].job_status[j].job_id = NO_ID;
            }
#endif
         }
      } /* if (gotcha == NO) */
   } /* for (i = 0; i < no_of_hosts; i++) */

   return;
}
