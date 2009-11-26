/*
 *  check_fsa.c - Part of AFD, an automatic file distribution program.
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

DESCR__S_M3
/*
 ** NAME
 **   check_fsa - checks if FSA has been updated
 **
 ** SYNOPSIS
 **   int check_fsa(int passive)
 **
 ** DESCRIPTION
 **   This function checks if the FSA (Filetransfer Status Area)
 **   which is a memory mapped area is still in use. If not
 **   it will detach from the old memory area and attach
 **   to the new one with the function fsa_attach().
 **
 ** RETURN VALUES
 **   Returns NO if the FSA is still in use. Returns YES if a
 **   new FSA has been created. It will then also return new
 **   values for 'fsa_id' and 'no_of_hosts'.
 **
 ** SEE ALSO
 **   fsa_attach(), fsa_attach_passive()
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   18.01.1996 H.Kiehl Created
 **   26.09.1997 H.Kiehl Removed all function parameters.
 **   29.04.2003 H.Kiehl Option to only attach in read only mode (passive).
 **
 */
DESCR__E_M3

#include <stdio.h>                       /* stderr, NULL                 */
#include <string.h>                      /* strerror()                   */
#include <stdlib.h>                      /* exit()                       */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_MMAP
# include <sys/mman.h>                   /* munmap()                     */
#endif
#include <errno.h>

/* Global variables. */
extern int                        fsa_id;
#ifdef HAVE_MMAP
extern off_t                      fsa_size;
#endif
extern char                       *p_work_dir;
extern struct filetransfer_status *fsa;


/*############################ check_fsa() ##############################*/
int
check_fsa(int passive)
{
   if (fsa != NULL)
   {
      char *ptr;

      ptr = (char *)fsa - AFD_WORD_OFFSET;
      if (*(int *)ptr == STALE)
      {
#ifdef HAVE_MMAP
         if (munmap(ptr, fsa_size) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to munmap() from FSA [fsa_id = %d fsa_size = %d] : %s"),
                       fsa_id, fsa_size, strerror(errno));
         }
#else
         if (munmap_emu(ptr) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Failed to munmap_emu() from FSA (%d) : %s"),
                       fsa_id, strerror(errno));
         }
#endif

         if (passive == YES)
         {
            if (fsa_attach_passive() < 0)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Passive attach to FSA failed."));
               exit(INCORRECT);
            }
         }
         else
         {
            if (fsa_attach() < 0)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          _("Failed to attach to FSA."));
               exit(INCORRECT);
            }
         }

         return(YES);
      }
   }

   return(NO);
}
