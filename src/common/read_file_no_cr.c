/*
 *  read_file_no_cr.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2008 - 2013 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   read_file_no_cr - reads the contents of a file into a buffer
 **                     and remove any CR
 **
 ** SYNOPSIS
 **   off_t read_file_no_cr(char *filename, char **buffer, char *sfile, int sline)
 **
 ** DESCRIPTION
 **   The function reads the contents of the file filename into a
 **   buffer for which it allocates memory. It also removes any
 **   CR at the end of line, so we only have a LF. The caller of
 **   this function is responsible for releasing the memory.
 **
 ** RETURN VALUES
 **   The size of file filename when successful, otherwise INCORRECT
 **   will be returned.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   07.11.2008 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>               /* strlen()                            */
#include <stdlib.h>               /* malloc(), free()                    */
#include <unistd.h>               /* close(), read()                     */
#include <sys/types.h>
#include <sys/stat.h>             /* fstat()                             */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>


/*########################## read_file_no_cr() ##########################*/
off_t
read_file_no_cr(char *filename, char **buffer, char *sfile, int sline)
{
   int         fd;
   off_t       bytes_buffered;
#ifdef HAVE_GETLINE
   ssize_t     n;
   size_t      line_buffer_length;
#else
   size_t      n;
#endif
   char        *read_ptr;
   FILE        *fp;
   struct stat stat_buf;

   /* Open file. */
   if ((fd = open(filename, O_RDONLY)) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not open() `%s' : %s [%s %d]"),
                 filename, strerror(errno), sfile, sline);
      return(INCORRECT);
   }

   /* Get the size we need for the buffer. */
   if (fstat(fd, &stat_buf) == -1)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not fstat() `%s' : %s [%s %d]"),
                 filename, strerror(errno), sfile, sline);
      (void)close(fd);
      return(INCORRECT);
   }

   /* Allocate enough memory for the contents of the file. */
   if ((*buffer = malloc(stat_buf.st_size + 1)) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not malloc() memory : %s [%s %d]"),
                 strerror(errno), sfile, sline);
      (void)close(fd);
      return(INCORRECT);
   }

   if ((fp = fdopen(fd, "r")) == NULL)
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not fdopen() `%s' : %s [%s %d]"),
                 filename, strerror(errno), sfile, sline);
      free(*buffer);
      *buffer = NULL;
      return(INCORRECT);
   }
   bytes_buffered = 0;
   read_ptr = *buffer;

   /* Read file into buffer. */
#ifdef HAVE_GETLINE
   line_buffer_length = stat_buf.st_size + 1;
   while ((n = getline(&read_ptr, &line_buffer_length, fp)) != -1)
#else
   while (fgets(read_ptr, MAX_LINE_LENGTH, fp) != NULL)
#endif
   {
#ifndef HAVE_GETLINE
      n = strlen(read_ptr);
#endif
      if (n > 1)
      {
         if (*(read_ptr + n - 2) == 13)
         {
            *(read_ptr + n - 2) = 10;
            n--;
         }
      }
      read_ptr += n;
#ifdef HAVE_GETLINE
      line_buffer_length -= n;
#endif
      bytes_buffered += n;
   }
   (*buffer)[bytes_buffered] = '\0';

   /* Close input file. */
   if (fclose(fp) == EOF)
   {
      system_log(DEBUG_SIGN, __FILE__, __LINE__,
                 _("fclose() error : %s [%s %d]"),
                 strerror(errno), sfile, sline);
   }

   return(bytes_buffered);
}
