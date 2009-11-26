/*
 *  get_file_mask_list.c - Part of AFD, an automatic file distribution
 *                         program.
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
 **   get_file_mask_list - gets all file masks for given ID
 **
 ** SYNOPSIS
 **   void get_file_mask_list(unsigned int file_mask_id,
 **                           int          *no_of_file_masks,
 **                           char         **files)
 **
 ** DESCRIPTION
 **   This function extracts the file masks for the given file mask ID
 **   and stores them in 'files'.
 **   The caller of this function is responsible in freeing the memory
 **   that is being allocated in 'files'.
 **
 ** RETURN VALUES
 **   When it does not find any file mask, 'files' will be NULL.
 **   Otherwise it returns the file masks in 'files'.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   31.12.2003 H.Kiehl Created
 **   21.04.2005 H.Kiehl Don't lock FILE_MASK_FILE!
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>            /* strerror()                             */
#include <stdlib.h>            /* malloc()                               */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "ui_common_defs.h"

extern char *p_work_dir;


/*########################## get_file_mask_list() #######################*/
void
get_file_mask_list(unsigned int file_mask_id,
                   int          *no_of_file_masks,
                   char         **files)
{
   int  fmd_fd;
   char fmd_file_name[MAX_PATH_LENGTH];

   *files = NULL;
   (void)strcpy(fmd_file_name, p_work_dir);
   (void)strcat(fmd_file_name, FIFO_DIR);
   (void)strcat(fmd_file_name, FILE_MASK_FILE);
   if ((fmd_fd = open(fmd_file_name, O_RDONLY)) == -1)
   {
      (void)xrec(ERROR_DIALOG, "Failed to open() `%s' : %s (%s %d)",
                 fmd_file_name, strerror(errno), __FILE__, __LINE__);
   }
   else
   {
      struct stat stat_buf;

      if (fstat(fmd_fd, &stat_buf) == -1)
      {
         (void)xrec(ERROR_DIALOG, "Failed to fstat() `%s' : %s (%s %d)",
                    fmd_file_name, strerror(errno), __FILE__, __LINE__);
      }
      else
      {
         if (stat_buf.st_size > AFD_WORD_OFFSET)
         {
            char *buffer;

            if ((buffer = malloc(stat_buf.st_size)) == NULL)
            {
#if SIZEOF_OFF_T == 4
               (void)xrec(ERROR_DIALOG, "Failed to malloc() %ld : %s (%s %d)",
#else
               (void)xrec(ERROR_DIALOG, "Failed to malloc() %lld : %s (%s %d)",
#endif
                          (pri_off_t)stat_buf.st_size, strerror(errno),
                          __FILE__, __LINE__);
            }
            else
            {
               if (read(fmd_fd, buffer, stat_buf.st_size) != stat_buf.st_size)
               {
                  (void)xrec(ERROR_DIALOG,
#if SIZEOF_OFF_T == 4
                             "Failed to read() %ld bytes from %s : %s (%s %d)",
#else
                             "Failed to read() %lld bytes from %s : %s (%s %d)",
#endif
                             (pri_off_t)stat_buf.st_size, fmd_file_name,
                             strerror(errno), __FILE__, __LINE__);
               }
               else
               {
                  int  fml_offset,
                       i,
                       mask_offset,
                       *no_of_file_mask_ids,
                       shift_length;
                  char *ptr = buffer;

                  no_of_file_mask_ids = (int *)ptr;
                  ptr += AFD_WORD_OFFSET;
                  fml_offset = sizeof(int) + sizeof(int);
                  mask_offset = fml_offset + sizeof(int) +
                                sizeof(unsigned int) + sizeof(unsigned char);

                  for (i = 0; i < *no_of_file_mask_ids; i++)
                  {
                     if (*(unsigned int *)(ptr + fml_offset +
                                           sizeof(int)) == file_mask_id)
                     {
                        if ((*files = malloc(*(int *)(ptr + fml_offset))) == NULL)
                        {
                           (void)xrec(ERROR_DIALOG,
                                      "Failed to malloc() %d bytes : %s (%s %d)",
                                      *(int *)(ptr + fml_offset),
                                      strerror(errno), __FILE__, __LINE__);
                        }
                        else
                        {
                           *no_of_file_masks = *(int *)ptr;
                           (void)memcpy(*files, (ptr + mask_offset),
                                        *(int *)(ptr + fml_offset));
                        }
                        break;
                     }
                     shift_length = mask_offset + *(int *)(ptr + fml_offset) +
                                    sizeof(char) + *(ptr + mask_offset - 1);
                     if ((ptr + shift_length + fml_offset + sizeof(int) + sizeof(int)) > (buffer + stat_buf.st_size))
                     {
                        system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                                   "Hmm, buffer overflow by %d bytes! This filemask (%s (%ld)) is not correct.",
#else
                                   "Hmm, buffer overflow by %d bytes! This filemask (%s (%lld)) is not correct.",
#endif
                                   (ptr + shift_length + fml_offset + sizeof(int) + sizeof(int)) - (buffer + stat_buf.st_size),
                                   fmd_file_name, (pri_off_t)stat_buf.st_size);
                        break;
                     }
                     else
                     {
                        ptr += shift_length;
                     }
                  }
               }
               free(buffer);
            }
         }
         else
         {
            (void)xrec(ERROR_DIALOG,
                       "File `%s' is not large enough (%d bytes) to contain any valid data.",
                       fmd_file_name, stat_buf.st_size);
         }
      }
      if (close(fmd_fd) == -1)
      {
         (void)xrec(ERROR_DIALOG, "Failed to close() `%s' : %s (%s %d)",
                    fmd_file_name, strerror(errno), __FILE__, __LINE__);
      }
   }
   
   return;
}
