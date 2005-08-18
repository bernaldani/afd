/*
 *  read_file_mask.c - Part of AFD, an automatic file distribution
 *                     program.
 *  Copyright (c) 2000 - 2003 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   read_file_mask - reads all file masks from a file
 **
 ** SYNOPSIS
 **   int read_file_mask(char *dir_alias, int *nfg, struct file_mask **fml)
 **
 ** DESCRIPTION
 **   The function read_file_mask reads all file masks from the
 **   file:
 **       $AFD_WORK_DIR/files/incoming/filters/<fra_pos>
 **   It reads the data into a buffer that this function will
 **   create.
 **
 ** RETURN VALUES
 **   Returns SUCCESS when it manages to store the data. Otherwise
 **   INCORRECT is returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   15.08.2000 H.Kiehl Created
 **   28.12.2003 H.Kiehl Made number of file mask unlimited.
 **
 */
DESCR__E_M3

#include <stdio.h>            /* sprintf()                               */
#include <string.h>           /* strerror()                              */
#include <stdlib.h>           /* malloc()                                */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "fddefs.h"

/* External global variables. */
extern char *p_work_dir;


/*########################### read_file_mask() ##########################*/
int
read_file_mask(char *dir_alias, int *nfg, struct file_mask **fml)
{
   int         fd,
               i;
   char        *buffer,
               file_mask_file[MAX_PATH_LENGTH],
               *ptr;
   struct stat stat_buf;

   (void)sprintf(file_mask_file, "%s%s%s%s/%s", p_work_dir, AFD_FILE_DIR,
                 INCOMING_DIR, FILE_MASK_DIR, dir_alias);

   if ((fd = lock_file(file_mask_file, ON)) < 0)
   {
      return(INCORRECT);
   }
   if (fstat(fd, &stat_buf) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to fstat() `%s' : %s",
                 file_mask_file, strerror(errno));
      return(INCORRECT);
   }
   if ((buffer = malloc(stat_buf.st_size)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() %d bytes : %s",
                 stat_buf.st_size, strerror(errno));
      (void)close(fd);
      return(INCORRECT);
   }
   if (read(fd, buffer, stat_buf.st_size) != stat_buf.st_size)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to read() %d Bytes from `%s' : %s",
                 stat_buf.st_size, file_mask_file, strerror(errno));
      (void)close(fd);
      return(INCORRECT);
   }
   ptr = buffer;
   *nfg = *(int *)ptr;
   ptr += sizeof(int);
   if ((*fml = malloc(*nfg * sizeof(struct file_mask))) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 "Failed to malloc() %d bytes : %s",
                 (*nfg * sizeof(struct file_mask)), strerror(errno));
      (void)close(fd);
      return(INCORRECT);
   }

   for (i = 0; i < *nfg; i++)
   {
      (*fml)[i].fc = *(int *)ptr;
      ptr += sizeof(int);
      (*fml)[i].fbl = *(int *)ptr;
      ptr += sizeof(int);
      if (((*fml)[i].file_list = malloc((*fml)[i].fbl)) == NULL)
      {
         int j;

         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to malloc() %d bytes : %s",
                    (*fml)[i].fbl, strerror(errno));
         for (j = 0; j < i; j++)
         {
            free((*fml)[j].file_list);
         }
         free(buffer);
         (void)close(fd);
         return(INCORRECT);
      }
      (void)memcpy((*fml)[i].file_list, ptr, (*fml)[i].fbl);
      ptr += (*fml)[i].fbl;
   }
   free(buffer);
   if (close(fd) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__, "Failed to close() `%s' : %s",
                 file_mask_file, strerror(errno));
   }

   return(SUCCESS);
}
