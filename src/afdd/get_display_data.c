/*
 *  get_display_file.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2007 Deutscher Wetterdienst (DWD),
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
 **   get_display_data - writes the contents of a file to a socket
 **
 ** SYNOPSIS
 **   int get_display_data(char *search_file,
 **                        char *search_string,
 **                        int  no_of_lines,
 **                        int  show_time,
 **                        int  file_no)
 **
 ** DESCRIPTION
 **
 ** RETURN VALUES
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   25.06.1997 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                     /* strerror()                    */
#include <stdlib.h>                     /* malloc(), free()              */
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <unistd.h>                     /* read(), write(), close()      */
#include <errno.h>
#include "afdddefs.h"

/* External global variables */
extern int  cmd_sd,
            sys_log_fd;
extern char *p_work_dir;
extern FILE *p_data;


/*########################## get_display_data() #########################*/
int
get_display_data(char *search_file,
                 char *search_string,
		 int  no_of_lines,
		 int  show_time,
		 int  file_no)
{
   int         from_fd,
	       length;
   size_t      hunk,
               left;
   char        *buffer;
   struct stat stat_buf;

   /* Open source file. */
   length = strlen(search_file);
   (void)sprintf(&search_file[length], "%d", file_no);
   if ((from_fd = open(search_file, O_RDONLY)) < 0)
   {
      (void)fprintf(p_data, "500 Failed to open() %s : %s (%s %d)\r\n",
                       search_file, strerror(errno), __FILE__, __LINE__);
      return(INCORRECT);
   }

   if (fstat(from_fd, &stat_buf) != 0)
   {
      (void)fprintf(p_data, "500 Failed to fstat() %s : %s (%s %d)\r\n",
                    search_file, strerror(errno), __FILE__, __LINE__);
      (void)close(from_fd);
      return(INCORRECT);
   }
   else if (stat_buf.st_size == 0)
        {
           (void)fprintf(p_data, "500 File %s is empty.\r\n", search_file);
           (void)close(from_fd);
           return(SUCCESS);
        }

   left = hunk = stat_buf.st_size;

   if (hunk > HUNK_MAX)
   {
      hunk = HUNK_MAX;
   }

   if ((buffer = malloc(hunk)) == NULL)
   {
      (void)fprintf(p_data, "500 Failed to malloc() memory : %s (%s %d)\r\n",
                    strerror(errno), __FILE__, __LINE__);
      (void)close(from_fd);
      return(INCORRECT);
   }

   (void)fprintf(p_data, "211- Command successful\r\n");
   (void)fflush(p_data);

   while (left > 0)
   {
      if (read(from_fd, buffer, hunk) != hunk)
      {
         (void)fprintf(p_data, "500 Failed to read() %s : %s (%s %d)\r\n",
                       search_file, strerror(errno), __FILE__, __LINE__);
         free(buffer);
         (void)close(from_fd);
         return(INCORRECT);
      }

      if (write(cmd_sd, buffer, hunk) != hunk)
      {
         (void)fprintf(p_data, "520 write() error : %s (%s %d)\r\n",
                       strerror(errno), __FILE__, __LINE__);
         free(buffer);
         (void)close(from_fd);
         return(INCORRECT);
      }
      left -= hunk;
      if (left < hunk)
      {
         hunk = left;
      }
   }

   (void)fprintf(p_data, "200 End of data\n");
   free(buffer);
   if (close(from_fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN,
                "Failed to close() %s : %s (%s %d)\n",
                search_file, strerror(errno), __FILE__, __LINE__);
   }

   return(SUCCESS);
}
