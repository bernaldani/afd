/*
 *  handle_jid.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2010 - 2012 Holger Kiehl <Holger.Kiehl@dwd.de>
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

DESCR__S_M1
/*
 ** NAME
 **   handle_jid - attaches or detaches to ADL (AFD Directory List)
 **
 ** SYNOPSIS
 **   (void)alloc_jid(char *alias)
 **   (void)dealloc_jid(void)
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
 **   11.02.2010 H.Kiehl Created
 **
 */
DESCR__E_M1

#include <stdio.h>                     /* fprintf()                      */
#include <stdlib.h>                    /* free()                         */
#ifdef WITH_AFD_MON
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>                    /* open()                         */
# include <unistd.h>                   /* read()                         */
#endif
#include <errno.h>
#include "aldadefs.h"
#ifdef WITH_AFD_MON
# include "mondefs.h"
#endif

/* External global variables. */
extern char            *p_work_dir;
extern struct jid_data jidd;


/*############################# alloc_jid() #############################*/
void
alloc_jid(char *alias)
{
#ifdef WITH_AFD_MON
   if (alias == NULL)
   {
      (void)sprintf(jidd.name, "%s%s%s",
                    p_work_dir, FIFO_DIR, JOB_ID_DATA_FILE);
      if (read_job_ids(jidd.name, &jidd.no_of_job_ids, &jidd.jd) == INCORRECT)
      {
         (void)fprintf(stderr, "Failed to read `%s' (%s %d)\n",
                       jidd.name, __FILE__, __LINE__);
         jidd.jd = NULL;
         jidd.name[0] = '\0';
         jidd.prev_pos = -1;
      }
   }
   else
   {
      int fd,
          ret = SUCCESS;

      (void)sprintf(jidd.name, "%s%s%s%s",
                    p_work_dir, FIFO_DIR, AJL_FILE_NAME, alias);

      if ((fd = open(jidd.name, O_RDONLY)) == -1)
      {
         (void)fprintf(stderr, "Failed to open() `%s' : %s (%s %d)\n",
                       jidd.name, strerror(errno), __FILE__, __LINE__);
         ret = INCORRECT;
      }
      else
      {
         struct stat stat_buf;

         if (fstat(fd, &stat_buf) == -1)
         {
            (void)fprintf(stderr, "Failed to fstat() `%s' :  %s (%s %d)\n",
                          jidd.name, strerror(errno), __FILE__, __LINE__);
            ret = INCORRECT;
         }
         else
         {
            if (stat_buf.st_size >= sizeof(struct afd_job_list))
            {
               if ((jidd.jd = malloc(stat_buf.st_size)) == NULL)
               {
                  (void)fprintf(stderr, "malloc() error : %s (%s %d)\n",
                                strerror(errno), __FILE__, __LINE__);
                  ret = INCORRECT;
               }
               else
               {
                  if (read(fd, jidd.jd, stat_buf.st_size) != stat_buf.st_size)
                  {
                     (void)fprintf(stderr,
                                   "Failed to read() from `%s' :  %s (%s %d)\n",
                                   jidd.name, strerror(errno), __FILE__, __LINE__);
                     free(jidd.jd);
                     jidd.jd = NULL;
                     ret = INCORRECT;
                  }
               }
            }
            else
            {
               (void)fprintf(stderr,
                             "`%s' is not large enough to hold any valid data (%s %d)\n",
                             jidd.name, __FILE__, __LINE__);
               ret = INCORRECT;
            }
         }
         if (close(fd) == -1)
         {
            (void)fprintf(stderr, "Failed to close() `%s' :  %s (%s %d)\n",
                          jidd.name, strerror(errno), __FILE__, __LINE__);
         }
      }
      if (ret == INCORRECT)
      {
         jidd.jd = NULL;
         jidd.name[0] = '\0';
         jidd.prev_pos = -1;
      }
   }
#else
   (void)sprintf(jidd.name, "%s%s%s", p_work_dir, FIFO_DIR, JOB_ID_DATA_FILE);
   if (read_job_ids(jidd.name, &jidd.no_of_job_ids, &jidd.jd) == INCORRECT)
   {
      (void)fprintf(stderr, "Failed to read `%s' (%s %d)\n",
                    jidd.name, __FILE__, __LINE__);
      jidd.jd = NULL;
      jidd.name[0] = '\0';
      jidd.prev_pos = -1;
   }
#endif

   return;
}


/*########################### dealloc_jid() #############################*/
void
dealloc_jid(void)
{
   if (jidd.jd != NULL)
   {
      free(jidd.jd);
      jidd.jd = NULL;
      jidd.name[0] = '\0';
      jidd.prev_pos = -1;
   }

   return;
}
