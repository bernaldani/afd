/*
 *  reset_values.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2004, 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   reset_values - resets total_file_counter + total_file_size in FSA
 **
 ** SYNOPSIS
 **   void reset_values(int   files_retrieved,
 **                     off_t file_size_retrieved,
 **                     int   files_to_retrieve,
 **                     off_t file_size_to_retrieve)
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
 **   24.04.2004 H.Kiehl Created
 **
 */
DESCR__E_M3

#include "fddefs.h"

/* Global variables */
extern int                        fsa_fd;
extern struct filetransfer_status *fsa;
extern struct job                 db;


/*############################ reset_values() ###########################*/
void
reset_values(int   files_retrieved,
             off_t file_size_retrieved,
             int   files_to_retrieve,
             off_t file_size_to_retrieve)
{
   if (((files_retrieved < files_to_retrieve) ||
        (file_size_retrieved < file_size_to_retrieve)) &&
       (db.fsa_pos != INCORRECT))
   {
      (void)gsf_check_fsa();
      if (db.fsa_pos != INCORRECT)
      {
#ifdef LOCK_DEBUG
         lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
         lock_region_w(fsa_fd, db.lock_offset + LOCK_TFC);
#endif
         if (files_retrieved < files_to_retrieve)
         {
            fsa->total_file_counter -= (files_to_retrieve - files_retrieved);
#ifdef _VERIFY_FSA
            if (fsa->total_file_counter < 0)
            {
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Total file counter for host <%s> less then zero. Correcting to 0.",
                          fsa->host_dsp_name);
               fsa->total_file_counter = 0;
            }
#endif
         }

         if (file_size_retrieved < file_size_to_retrieve)
         {
            fsa->total_file_size -= (file_size_to_retrieve - file_size_retrieved);
#ifdef _VERIFY_FSA
            if (fsa->total_file_size < 0)
            {
               fsa->total_file_size = 0;
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
                          "Total file size for host <%s> overflowed. Correcting to 0.",
                          fsa->host_dsp_name);
            }
            else if ((fsa->total_file_counter == 0) &&
                     (fsa->total_file_size > 0))
                 {
                    system_log(DEBUG_SIGN, __FILE__, __LINE__,
                               "fc for host <%s> is zero but fs is not zero. Correcting.",
                               fsa->host_dsp_name);
                    fsa->total_file_size = 0;
                 }
#endif
         }
#ifdef LOCK_DEBUG
         unlock_region(fsa_fd, db.lock_offset + LOCK_TFC, __FILE__, __LINE__);
#else
         unlock_region(fsa_fd, db.lock_offset + LOCK_TFC);
#endif
      }
   }
   return;
}
