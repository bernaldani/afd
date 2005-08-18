/*
 *  attach_buf.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2005 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   attach_buf - maps to a file
 **
 ** SYNOPSIS
 **   void *attach_buf(char   *file,
 **                    int    *fd,
 **                    size_t new_size,
 **                    char   *prog_name,
 **                    mode_t mode,
 **                    int    wait_lock)
 **
 ** DESCRIPTION
 **   The function attach_buf() attaches to the file 'file'. If
 **   the file does not exist, it is created and a binary int
 **   zero is written to the first four bytes.
 **
 ** RETURN VALUES
 **   On success, attach_buf() returns a pointer to the mapped area.
 **   On error, MAP_FAILED (-1) is returned.
 **
 ** SEE ALSO
 **   unmap_data(), mmap_resize()
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   21.01.1998 H.Kiehl Created
 **   30.04.2003 H.Kiehl Added mode parameter.
 **   05.08.2004 H.Kiehl Added wait for lock.
 **
 */
DESCR__E_M3

#include <string.h>
#include <stdlib.h>                 /* exit()                            */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>                  /* open()                            */
#endif
#include <unistd.h>                 /* fstat(), lseek(), write()         */
#include <sys/mman.h>               /* mmap()                            */
#include <errno.h>


/*############################ attach_buf() #############################*/
void *
attach_buf(char   *file,
           int    *fd,
           size_t new_size,
           char   *prog_name,
           mode_t mode,
           int    wait_lock)
{
   struct stat stat_buf;

   /*
    * This buffer has no open file descriptor, so it we do not
    * need to unmap from the area. However this can be the
    * first time that the this process is started and no buffer
    * exists, then we need to create it.
    */
   if ((*fd = coe_open(file, O_RDWR | O_CREAT, mode)) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to open() and create `%s' : %s",
                 file, strerror(errno));
      exit(INCORRECT);
   }
   if (prog_name != NULL)
   {
      /*
       * Lock the file. That way it is not possible for two same
       * processes to run at the same time.
       */
      if (wait_lock == YES)
      {
#ifdef LOCK_DEBUG
         lock_region_w(*fd, 0, __FILE__, __LINE__);
#else
         lock_region_w(*fd, 0);
#endif
      }
      else
      {
#ifdef LOCK_DEBUG
         if (lock_region(*fd, 0, __FILE__, __LINE__) == LOCK_IS_SET)
#else
         if (lock_region(*fd, 0) == LOCK_IS_SET)
#endif
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Another `%s' is already running. Terminating.",
                       prog_name);
            exit(INCORRECT);
         }
      }
   }

   if (fstat(*fd, &stat_buf) == -1)
   {
      system_log(FATAL_SIGN, __FILE__, __LINE__,
                 "Failed to fstat() `%s' : %s", file, strerror(errno));
      exit(INCORRECT);
   }

   if (stat_buf.st_size < new_size)
   {
      int  buf_size = 0,
           i,
           loops,
           rest,
           write_size;
      char buffer[4096];

      if (write(*fd, &buf_size, sizeof(int)) != sizeof(int))
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to write() to `%s' : %s", file, strerror(errno));
         exit(INCORRECT);
      }
      if (lseek(*fd, stat_buf.st_size, SEEK_SET) == -1)
      {
         system_log(FATAL_SIGN, __FILE__, __LINE__,
                    "Failed to lseek() `%s' : %s", file, strerror(errno));
         exit(INCORRECT);
      }
      write_size = new_size - stat_buf.st_size;
      (void)memset(buffer, 0, 4096);
      loops = write_size / 4096;
      rest = write_size % 4096;
      for (i = 0; i < loops; i++)
      {
         if (write(*fd, buffer, 4096) != 4096)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Failed to write() to `%s' : %s", file, strerror(errno));
            exit(INCORRECT);
         }
      }
      if (rest > 0)
      {
         if (write(*fd, buffer, rest) != rest)
         {
            system_log(FATAL_SIGN, __FILE__, __LINE__,
                       "Failed to write() to `%s' : %s", file, strerror(errno));
            exit(INCORRECT);
         }
      }
   }
   else
   {
      new_size = stat_buf.st_size;
   }

   return(mmap(0, new_size, (PROT_READ | PROT_WRITE), MAP_SHARED, *fd, 0));
}
