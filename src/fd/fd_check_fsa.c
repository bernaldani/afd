/*
 *  fd_check_fsa.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2002, 2003 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   fd_check_fsa - check if FSA has been updated
 **
 ** SYNOPSIS
 **   int fd_check_fsa(void)
 **
 ** DESCRIPTION
 **   This function checks if the FSA (Filetransfer Status Area)
 **   which is a memory mapped area is still in use. If not
 **   it will wait for AMG to finish every state of rereading the
 **   DIR_CONFIG and then detach from the old memory area and attach
 **   to the new one with the function fsa_attach().
 **
 ** RETURN VALUES
 **   Returns NO if the FSA is still in use. Returns YES if a
 **   new FSA has been created. It will then also return new
 **   values for 'fsa_id' and 'no_of_hosts'.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   05.04.2002 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                      /* strerror()                   */
#include <stdlib.h>                      /* exit()                       */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
#include <sys/mman.h>                    /* munmap()                     */
#endif
#include <errno.h>
#include "fddefs.h"

/* #define DEBUG_WAIT_LOOP */

/* External global variables */
extern int                        fsa_id;
#ifdef _WITH_BURST_2
extern int                        no_of_hosts;
#endif /* _WITH_BURST_2 */
#ifdef HAVE_MMAP
extern off_t                      fsa_size;
#endif
extern char                       str_fsa_id[];
extern struct filetransfer_status *fsa;
extern struct afd_status          *p_afd_status;


/*############################ fd_check_fsa() ###########################*/
int
fd_check_fsa(void)
{
   if (fsa != NULL)
   {
      int  i, j,
           gotcha = NO,
           loops = 0;
      char *ptr;

      if ((p_afd_status->amg_jobs & FD_WAITING) == 0)
      {
         p_afd_status->amg_jobs ^= FD_WAITING;
      }
      do
      {
         if ((p_afd_status->amg_jobs & REREADING_DIR_CONFIG) == 0)
         {
            gotcha = YES;
            break;
         }
#ifdef _WITH_BURST_2
         else
         {
            /*
             * No sf_xxx or gf_xxx process may wait for FD to
             * check its queue to see if it does have a job. Otherwise
             * we will have a deadlock because AMG will try to lock
             * the whole FSA while sf_xxx will hold part of it.
             */
            for (i = 0; i < no_of_hosts; i++)
            {
               if (fsa[i].active_transfers > 0)
               {
                  for (j = 0; j < fsa[i].allowed_transfers; j++)
                  {
                     if ((fsa[i].job_status[j].unique_name[1] == '\0') &&
                         (fsa[i].job_status[j].unique_name[2] == 4))
                     {
                        fsa[i].job_status[j].unique_name[0] = '\0';
                        fsa[i].job_status[j].unique_name[1] = 1;
                     }
                  }
               }
            }
         }
#endif /* _WITH_BURST_2 */
         (void)my_usleep(100000L);
      } while (loops++ < WAIT_LOOPS);
      p_afd_status->amg_jobs ^= FD_WAITING;
      if (gotcha == NO)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Hmmm, AMG does not reset REREADING_DIR_CONFIG flag!");
      }
#ifdef DEBUG_WAIT_LOOP
      else
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Got reset of REREADING_DIR_CONFIG flag after %d loops (%8.3fs).",
                    loops, (float)((float)loops / 10.0));
      }
#endif /* DEBUG_WAIT_LOOP */

      ptr = (char *)fsa;
      ptr -= AFD_WORD_OFFSET;
      if (*(int *)ptr == STALE)
      {
#ifdef HAVE_MMAP
         if (munmap(ptr, fsa_size) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to munmap() from FSA [fsa_id = %d fsa_size = %d] : %s",
                       fsa_id, fsa_size, strerror(errno));
         }
#else
         if (munmap_emu(ptr) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to munmap_emu() from FSA (%d) : %s",
                       fsa_id, strerror(errno));
         }
#endif

         if (fsa_attach() < 0)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to attach to FSA.");
            exit(INCORRECT);
         }
         (void)sprintf(str_fsa_id, "%d", fsa_id);

         return(YES);
      }
   }
   return(NO);
}
