/*
 *  init_fra_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   init_fra_data - initializes all data from the FRA needed by FD
 **
 ** SYNOPSIS
 **   void init_fra_data(void)
 **
 ** DESCRIPTION
 **   Counts the number of retrieve jobs in the FRA and prepares
 **   an array so that these jobs can be accessed faster.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.08.2000 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>           /* strerror()                              */
#include <stdlib.h>           /* realloc(), free()                       */
#include <errno.h>
#include "fddefs.h"

/* External global variables. */
extern int                        no_of_dirs,
                                  no_of_retrieves,
                                  *retrieve_list;
extern struct fileretrieve_status *fra;


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$ init_fra_data() $$$$$$$$$$$$$$$$$$$$$$$$$$$*/
void
init_fra_data(void)
{
   register int i;

   if (retrieve_list != NULL)
   {
      free(retrieve_list);
      retrieve_list = NULL;
      no_of_retrieves = 0;
   }
   for (i = 0; i < no_of_dirs; i++)
   {
      if ((fra[i].protocol == FTP) || (fra[i].protocol == HTTP) ||
          (fra[i].protocol == SFTP))
      {
         if ((no_of_retrieves % 10) == 0)
         {
            size_t new_size = ((no_of_retrieves / 10) + 1) * 10 * sizeof(int);

            if ((retrieve_list = realloc(retrieve_list, new_size)) == (int *)NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "realloc() error [%d bytes] : %s",
                          new_size, strerror(errno));

               no_of_retrieves = 0;
               return;
            }
         }
         retrieve_list[no_of_retrieves] = i;
         no_of_retrieves++;
      }
   }
   return;
}
