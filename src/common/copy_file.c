/*
 *  copy_file.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1996 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   copy_file - copies a file from one location to another
 **
 ** SYNOPSIS
 **   int copy_file(char *from, char *to, struct stat *p_stat_buf)
 **
 ** DESCRIPTION
 **   The function copy_file() copies the file 'from' to the file
 **   'to'. The files are copied blockwise using the read()
 **   and write() functions.
 **
 ** RETURN VALUES
 **   SUCCESS when file 'from' was copied successful or else INCORRECT
 **   when it fails.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   14.01.1996 H.Kiehl Created
 **   03.07.2001 H.Kiehl When copying via mmap(), copy in chunks.
 **   17.07.2001 H.Kiehl Removed mmap() stuff, simplifies porting.
 **   02.09.2007 H.Kiehl Added copying via splice().
 **   13.07.2012 H.Kiehl Keep modification and access time of original
 **                      file.
 **
 */
DESCR__E_M3

#include <stdio.h>      /* NULL                                          */
#include <stddef.h>
#include <unistd.h>     /* lseek(), write(), close()                     */
#include <string.h>     /* memcpy(), strerror()                          */
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>     /* malloc(), free()                              */
#include <utime.h>      /* utime()                                       */
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#include <errno.h>

#ifdef WITH_SPLICE_SUPPORT
# ifndef SPLICE_F_MOVE
#  define SPLICE_F_MOVE 0x01
# endif
# ifndef SPLICE_F_MORE
#  define SPLICE_F_MORE 0x04
# endif
#endif


/*############################# copy_file() #############################*/
int
copy_file(char *from, char *to, struct stat *p_stat_buf)
{
   int from_fd,
       ret = SUCCESS;

   /* Open source file. */
#ifdef O_LARGEFILE
   if ((from_fd = open(from, O_RDONLY | O_LARGEFILE)) == -1)
#else
   if ((from_fd = open(from, O_RDONLY)) == -1)
#endif
   {
      system_log(ERROR_SIGN, __FILE__, __LINE__,
                 _("Could not open `%s' for copying : %s"),
                 from, strerror(errno));
      ret = INCORRECT;
   }
   else
   {
      struct stat stat_buf;

      /* Need size and permissions of input file. */
      if ((p_stat_buf == NULL) && (fstat(from_fd, &stat_buf) == -1))
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    _("Could not fstat() `%s' : %s"), from, strerror(errno));
         (void)close(from_fd);
         ret = INCORRECT;
      }
      else
      {
         int to_fd;

         if (p_stat_buf == NULL)
         {
            p_stat_buf = &stat_buf;
         }

         /* Open destination file. */
#ifdef O_LARGEFILE
         if ((to_fd = open(to, O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE,
#else
         if ((to_fd = open(to, O_WRONLY | O_CREAT | O_TRUNC,
#endif
                           p_stat_buf->st_mode)) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       _("Could not open `%s' for copying : %s"),
                       to, strerror(errno));
            ret = INCORRECT;
         }
         else
         {
            struct utimbuf old_time;

            if (p_stat_buf->st_size > 0)
            {
#ifdef WITH_SPLICE_SUPPORT
               int fd_pipe[2];

               if (pipe(fd_pipe) == -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Failed to create pipe for copying : %s"),
                             strerror(errno));
                  ret = INCORRECT;
               }
               else
               {
                  long  bytes_read,
                        bytes_written;
                  off_t bytes_left;

                  bytes_left = p_stat_buf->st_size;
                  while (bytes_left)
                  {
                     if ((bytes_read = splice(from_fd, NULL, fd_pipe[1], NULL,
                                              bytes_left,
                                              SPLICE_F_MOVE | SPLICE_F_MORE)) == -1)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   _("splice() error : %s"), strerror(errno));
                        ret = INCORRECT;
                        break;
                     }
                     bytes_left -= bytes_read;

                     while (bytes_read)
                     {
                        if ((bytes_written = splice(fd_pipe[0], NULL, to_fd,
                                                    NULL, bytes_read,
                                                    SPLICE_F_MOVE | SPLICE_F_MORE)) == -1)
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      _("splice() error : %s"), strerror(errno));
                           ret = INCORRECT;
                           bytes_left = 0;
                           break;
                        }
                        bytes_read -= bytes_written;
                     }
                  }
                  if ((close(fd_pipe[0]) == -1) || (close(fd_pipe[1]) == -1))
                  {
                     system_log(WARN_SIGN, __FILE__, __LINE__,
                                _("Failed to close() pipe : %s"),
                                strerror(errno));
                  }
               }
#else
               char *buffer;

               if ((buffer = malloc(p_stat_buf->st_blksize)) == NULL)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             _("Failed to allocate memory : %s"),
                             strerror(errno));
                  ret = INCORRECT;
               }
               else
               {
                  int bytes_buffered;

                  do
                  {
                     if ((bytes_buffered = read(from_fd, buffer,
                                                p_stat_buf->st_blksize)) == -1)
                     {
                        system_log(ERROR_SIGN, __FILE__, __LINE__,
                                   _("Failed to read() from `%s' : %s"),
                                   from, strerror(errno));
                        ret = INCORRECT;
                        break;
                     }
                     if (bytes_buffered > 0)
                     {
                        if (write(to_fd, buffer, bytes_buffered) != bytes_buffered)
                        {
                           system_log(ERROR_SIGN, __FILE__, __LINE__,
                                      _("Failed to write() to `%s' : %s"),
                                      to, strerror(errno));
                           ret = INCORRECT;
                           break;
                        }
                     }
                  } while (bytes_buffered == p_stat_buf->st_blksize);
                  free(buffer);
               }
#endif /* !WITH_SPLICE_SUPPORT */
            }
            if (close(to_fd) == -1)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          _("Failed to close() `%s' : %s"),
                          to, strerror(errno));
            }

            /* Keep time stamp of the original file. */
            old_time.actime = stat_buf.st_atime;
            old_time.modtime = stat_buf.st_mtime;
            if (utime(to, &old_time) == -1)
            {
               system_log(WARN_SIGN, __FILE__, __LINE__,
                          "Failed to set time of file %s : %s",
                          to, strerror(errno));
            }
         }
      }
      if (close(from_fd) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    _("Failed to close() `%s' : %s"), from, strerror(errno));
      }
   }

   return(ret);
}
