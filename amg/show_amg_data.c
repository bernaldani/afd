/*
 *  show_amg_data.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1995 - 2002 Deutscher Wetterdienst (DWD),
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
 **   show_amg_data - shows contents of the AMG data
 **
 ** SYNOPSIS
 **   void show_amg_data(FILE *output)
 **
 ** DESCRIPTION
 **   The function show_amg_data() is used for debugging only. It shows
 **   the contents of the AMG data. The contents is written to the
 **   file descriptor 'output'.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   10.10.1995 H.Kiehl Created
 **   16.05.2002 H.Kiehl Removed all shared memory stuff and renamed
 **                      to show_amg_data.
 **
 */
DESCR__E_M3

#include <stdio.h>              /* fprintf()                             */
#include <string.h>             /* strcpy(), memcpy(), strerror(), free()*/
#include <stdlib.h>             /* calloc()                              */
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _NO_MMAP
#include <sys/mman.h>           /* mmap(), munmap()                      */
#endif
#include <unistd.h>
#include <fcntl.h>              /* open()                                */
#include <errno.h>
#include "amgdefs.h"

/* Global variables. */
int  sys_log_fd = STDOUT_FILENO;
char *p_work_dir;

/* Local function prototypes. */
static void show_amg_data(FILE *, char *, off_t);


/*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ main() $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
int
main(int argc, char *argv[])
{
   int         fd;
   char        amg_data_file[MAX_PATH_LENGTH],
               *p_mmap,
               work_dir[MAX_PATH_LENGTH];
   FILE        *fp;
   struct stat stat_buf;

   if (get_afd_path(&argc, argv, work_dir) < 0)
   {
      exit(1);
   }
   p_work_dir = work_dir;
   if (argc != 2)
   {
      (void)fprintf(stderr, "Usage: %s <output filename>\n", argv[0]);
      exit(1);
   }
   if ((fp = fopen(argv[1], "w")) == NULL)
   {
      (void)fprintf(stderr, "Failed to fopen() %s : %s\n",
                    argv[1], strerror(errno));
      exit(1);
   }
   (void)sprintf(amg_data_file, "%s%s%s", p_work_dir, FIFO_DIR, AMG_DATA_FILE);
   if ((fd = open(amg_data_file, O_RDWR)) == -1)
   {
      (void)fprintf(stderr, "Failed to open() %s : %s\n",
                    amg_data_file, strerror(errno));
      exit(1);
   }
   if (fstat(fd, &stat_buf) == -1)
   {
      (void)fprintf(stderr, "Failed to fstat() %s : %s\n",
                    amg_data_file, strerror(errno));
      exit(1);
   }
#ifdef _NO_MMAP
   if ((p_mmap = mmap_emu(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                          MAP_SHARED, amg_data_file, 0)) == (caddr_t) -1)
#else
   if ((p_mmap = mmap(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                      MAP_SHARED, fd, 0)) == (caddr_t) -1)
#endif
   {
      (void)fprintf(stderr, "Failed to mmap() %s : %s\n",
                    amg_data_file, strerror(errno));
      exit(1);
   }
   if (close(fd) == -1)
   {
      (void)fprintf(stderr, "Failed to close() %s : %s\n",
                    amg_data_file, strerror(errno));
   }
   show_amg_data(fp, p_mmap, stat_buf.st_size);
   fclose(fp);
#ifdef _NO_MMAP
   if (munmap_emu((void *)p_mmap) == -1)
#else
   if (munmap(p_mmap, stat_buf.st_size) == -1)
#endif
   {
      (void)fprintf(stderr, "Failed to munmap() %s : %s\n",
                    amg_data_file, strerror(errno));
   }
   exit(0);
}


/*########################### show_amg_data() ###########################*/
static void
show_amg_data(FILE *output, char *p_mmap, off_t data_length)
{
   int            i,
                  j,
                  k,
                  no_of_jobs,
                  value,
                  size;
   char           option[MAX_OPTION_LENGTH],
                  *ptr = NULL,
                  *tmp_ptr = NULL,
                  *p_offset = NULL;
   struct p_array *p_ptr;

   /* Show what is stored in the AMG data file. */
   (void)fprintf(output, "\n\n====================> Contents of AMG data file <===================\n");
   (void)fprintf(output, "                      =========================\n");

   if (data_length > 0)
   {
      ptr = p_mmap;

      /* First get the no of jobs */
      no_of_jobs = *(int *)ptr;
      ptr += sizeof(int);

      /* Create and copy pointer array */
      size = no_of_jobs * sizeof(struct p_array);
      if ((tmp_ptr = calloc(no_of_jobs, sizeof(struct p_array))) == NULL)
      {
         (void)fprintf(stderr, "ERROR   : Could not allocate memory : %s (%s %d)\n",
                       strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      p_ptr = (struct p_array *)tmp_ptr;
      memcpy(p_ptr, ptr, size);
      p_offset = ptr + size;

      /* Now show each job */
      for (j = 0; j < no_of_jobs; j++)
      {
         /* Show directory, priority and job type */
         (void)fprintf(output, "Directory          : %s\n", (int)(p_ptr[j].ptr[1]) + p_offset);
         (void)fprintf(output, "Alias name         : %s\n", (int)(p_ptr[j].ptr[2]) + p_offset);
         (void)fprintf(output, "Priority           : %c\n", *((int)(p_ptr[j].ptr[0]) + p_offset));

         /* Show files to be send */
         value = atoi((int)(p_ptr[j].ptr[3]) + p_offset);
         ptr = (int)(p_ptr[j].ptr[4]) + p_offset;
         for (k = 0; k < value; k++)
         {
            (void)fprintf(output, "File            %3d: %s\n", k + 1, ptr);
            while (*(ptr++) != '\0')
               ;
         }

         /* Show recipient */
         (void)fprintf(output, "Recipient          : %s\n", (int)(p_ptr[j].ptr[9]) + p_offset);

         /* Show local options */
         value = atoi((int)(p_ptr[j].ptr[5]) + p_offset);
         ptr = (int)(p_ptr[j].ptr[6]) + p_offset;
         for (k = 0; k < value; k++)
         {
            i = 0;
            while ((*ptr != '\0') && (*ptr != '\n'))
            {
               option[i] = *ptr;
               ptr++; i++;
            }
            ptr++;
            option[i] = '\0';
            (void)fprintf(output, "Local option    %3d: %s\n", k + 1, option);
         }

         /* Show standard options */
         value = atoi((int)(p_ptr[j].ptr[7]) + p_offset);
         ptr = (int)(p_ptr[j].ptr[8]) + p_offset;
         for (k = 0; k < value; k++)
         {
            i = 0;
            while ((*ptr != '\0') && (*ptr != '\n'))
            {
               option[i] = *ptr;
               ptr++; i++;
            }
            ptr++;
            option[i] = '\0';
            (void)fprintf(output, "Standard option %3d: %s\n", k + 1, option);
         }

         (void)fprintf(output, ">------------------------------------------------------------------------<\n\n");
      }

      /* Free memory for pointer array */
      free(tmp_ptr);
   }
   else
   {
      (void)fprintf(output, "\n                >>>>>>>>>> NO DATA <<<<<<<<<<\n\n");
   }

   return;
}