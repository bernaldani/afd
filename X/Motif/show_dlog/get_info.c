/*
 *  get_info.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   get_info - retrieves information out of the AMG history file
 **
 ** SYNOPSIS
 **   void get_info(int item, char input_id)
 **   int  get_sum_data(int    item,
 **                     time_t *date,
 **                     double *file_size)
 **
 ** DESCRIPTION
 **   This function searches the AMG history file for the job number
 **   of the selected file item. It then fills the structures item_list
 **   and info_data with data from the AMG history file.
 **
 ** RETURN VALUES
 **   None. The function will exit() with INCORRECT when it fails to
 **   allocate memory or fails to seek in the AMG history file.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   22.02.1998 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* strerror()                      */
#include <stdlib.h>                   /* strtoul()                       */
#include <unistd.h>                   /* close()                         */
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "show_dlog.h"
#include "afd_ctrl.h"

/* External global variables. */
extern Widget              toplevel_w;
extern int                 no_of_log_files,
                           sys_log_fd;
extern char                *p_work_dir;
extern struct item_list    *il;
extern struct info_data    id;

/* Local variables. */
static int                 *no_of_dir_names,
                           *no_of_job_ids;
static struct job_id_data  *jd = NULL;
static struct dir_name_buf *dnb = NULL;

/* Local function prototypes. */
static int                 get_all(int, char *);
static void                get_dir_data(int),
                           get_job_data(struct job_id_data *);


/*############################### get_info() ############################*/
void
get_info(int item, char input_id)
{
   int i;

   if ((item != GOT_JOB_ID) && (item != GOT_JOB_ID_DIR_ONLY))
   {
      id.job_no = get_all(item - 1, &input_id);
   }
   id.input_id = input_id;

   /*
    * Go through job ID database and find the job ID.
    */
   if (jd == NULL)
   {
      int         dnb_fd,
                  jd_fd;
      char        job_id_data_file[MAX_PATH_LENGTH];
      struct stat stat_buf;

      /* Map to job ID data file. */
      (void)sprintf(job_id_data_file, "%s%s%s", p_work_dir, FIFO_DIR,
                    JOB_ID_DATA_FILE);
      if ((jd_fd = open(job_id_data_file, O_RDONLY)) == -1)
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Failed to open() %s : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         return;
      }
      if (fstat(jd_fd, &stat_buf) == -1)
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Failed to fstat() %s : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         (void)close(jd_fd);
         return;
      }
      if (stat_buf.st_size > 0)
      {
         char *ptr;

         if ((ptr = mmap(0, stat_buf.st_size, PROT_READ,
                         MAP_SHARED, jd_fd, 0)) == (caddr_t) -1)
         {
            (void)xrec(toplevel_w, ERROR_DIALOG,
                       "Failed to mmap() to %s : %s (%s %d)",
                       job_id_data_file, strerror(errno), __FILE__, __LINE__);
            (void)close(jd_fd);
            return;
         }
         no_of_job_ids = (int *)ptr;
         ptr += AFD_WORD_OFFSET;
         jd = (struct job_id_data *)ptr;
      }
      else
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Job ID database file is empty. (%s %d)",
                    __FILE__, __LINE__);
         (void)close(jd_fd);
         return;
      }

      /* Map to directory name buffer. */
      (void)sprintf(job_id_data_file, "%s%s%s", p_work_dir, FIFO_DIR,
                    DIR_NAME_FILE);
      if ((dnb_fd = open(job_id_data_file, O_RDONLY)) == -1)
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Failed to open() %s : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         return;
      }
      if (fstat(dnb_fd, &stat_buf) == -1)
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Failed to fstat() %s : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         (void)close(dnb_fd);
         return;
      }
      if (stat_buf.st_size > 0)
      {
         char *ptr;

         if ((ptr = mmap(0, stat_buf.st_size, PROT_READ,
                         MAP_SHARED, dnb_fd, 0)) == (caddr_t) -1)
         {
            (void)xrec(toplevel_w, ERROR_DIALOG,
                       "Failed to mmap() to %s : %s (%s %d)",
                       job_id_data_file, strerror(errno),
                       __FILE__, __LINE__);
            (void)close(dnb_fd);
            return;
         }
         no_of_dir_names = (int *)ptr;
         ptr += AFD_WORD_OFFSET;
         dnb = (struct dir_name_buf *)ptr;
      }
      else
      {
         (void)xrec(toplevel_w, ERROR_DIALOG,
                    "Job ID database file is empty. (%s %d)",
                    __FILE__, __LINE__);
         (void)close(dnb_fd);
         return;
      }
   }

   if (input_id == YES)
   {
      for (i = 0; i < *no_of_dir_names; i++)
      {
         if (id.job_no == dnb[i].dir_id)
         {
            if (item == GOT_JOB_ID_DIR_ONLY)
            {
               (void)strcpy(id.dir, dnb[i].dir_name);
            }
            else
            {
               get_dir_data(i);
            }

            return;
         }
      } /* for (i = 0; i < *no_of_dir_names; i++) */
   }
   else
   {
      for (i = 0; i < *no_of_job_ids; i++)
      {
         if (id.job_no == jd[i].job_id)
         {
            if (item == GOT_JOB_ID_DIR_ONLY)
            {
               (void)strcpy(id.dir, dnb[jd[i].dir_id_pos].dir_name);
            }
            else
            {
               get_job_data(&jd[i]);
            }

            return;
         }
      }
   }

   return;
}


/*############################ get_sum_data() ###########################*/
int
get_sum_data(int item, time_t *date, double *file_size)
{
   int  file_no,
        total_no_of_items = 0,
        pos = -1;

   /* Determine log file and position in this log file. */
   for (file_no = 0; file_no < no_of_log_files; file_no++)
   {
      total_no_of_items += il[file_no].no_of_items;

      if (item < total_no_of_items)
      {
         pos = item - (total_no_of_items - il[file_no].no_of_items);
         break;
      }
   }

   /* Get the date, file size and transfer time. */
   if (pos > -1)
   {
      char *ptr,
           *p_start,
           buffer[MAX_FILENAME_LENGTH + MAX_PATH_LENGTH];

      /* Go to beginning of line to read complete line. */
      if (fseek(il[file_no].fp,
                (long)(il[file_no].line_offset[pos] - (11 + MAX_HOSTNAME_LENGTH + 3)),
                SEEK_SET) == -1)
      {
         (void)xrec(toplevel_w, FATAL_DIALOG,
                    "fseek() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }

      if (fgets(buffer, MAX_FILENAME_LENGTH + MAX_PATH_LENGTH, il[file_no].fp) == NULL)
      {
         (void)xrec(toplevel_w, WARN_DIALOG,
                    "fgets() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }
      ptr = buffer;
      while (*ptr != ' ')
      {
         ptr++;
      }
      *ptr = '\0';
      *date = atol(buffer);

      /* Ignore file name */
      ptr = &buffer[11 + MAX_HOSTNAME_LENGTH + 3];
      while (*ptr != ' ')
      {
         ptr++;
      }
      ptr++;

      /* Get file size */
      p_start = ptr;
      while (*ptr != ' ')
      {
         ptr++;
      }
      *ptr = '\0';
      *file_size = strtod(p_start, NULL);
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ get_all() +++++++++++++++++++++++++++++*/
/*                                ---------                              */
/* Description: Retrieves the full local file name, remote file name (if */
/*              it exists), job number and if available the additional   */
/*              reasons out of the log file.                             */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static int
get_all(int item, char *input_id)
{
   int  file_no,
        total_no_of_items = 0,
        pos = -1;

   /* Determine log file and position in this log file. */
   for (file_no = 0; file_no < no_of_log_files; file_no++)
   {
      total_no_of_items += il[file_no].no_of_items;

      if (item < total_no_of_items)
      {
         pos = item - (total_no_of_items - il[file_no].no_of_items);
         break;
      }
   }

   /* Get the job ID and file name. */
   if (pos > -1)
   {
      int  i;
      char *ptr,
           *p_job_id,
           buffer[MAX_FILENAME_LENGTH + MAX_PATH_LENGTH];

      *input_id = il[file_no].input_id[pos];
      if (fseek(il[file_no].fp, (long)il[file_no].line_offset[pos], SEEK_SET) == -1)
      {
         (void)xrec(toplevel_w, FATAL_DIALOG,
                    "fseek() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
         return(0);
      }

      if (fgets(buffer, MAX_FILENAME_LENGTH + MAX_PATH_LENGTH, il[file_no].fp) == NULL)
      {
         (void)xrec(toplevel_w, WARN_DIALOG,
                    "fgets() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return(0);
      }
      ptr = buffer;
      i = 0;
      while (*ptr != ' ')
      {
         id.file_name[i] = *ptr;
         i++; ptr++;
      }
      id.file_name[i] = '\0';
      ptr++;

      /* Away with the file size. */
      while (*ptr != ' ')
      {
         ptr++;
      }
      ptr++;

      /* Get the job ID. */
      p_job_id = ptr;
      while ((*ptr != '\n') && (*ptr != ' '))
      {
         ptr++;
      }
      if (*ptr == ' ')
      {
         *ptr = '\0';
         ptr++;

         i = 0;
         while ((*ptr != ' ') && (*ptr != '\n'))
         {
            id.proc_user[i] = *ptr;
            i++; ptr++;
         }
         id.proc_user[i] = '\0';
      }
      else
      {
         *ptr = '\0';
      }

      if (*ptr == ' ')
      {
         ptr++;
         i = 0;
         while (*ptr != '\n')
         {
            id.extra_reason[i] = *ptr;
            i++; ptr++;
         }
         id.extra_reason[i] = '\0';
      }

      return((unsigned int)strtoul(p_job_id, NULL, 10));
   }
   else
   {
      return(0);
   }
}


/*++++++++++++++++++++++++++++ get_job_data() +++++++++++++++++++++++++++*/
/*                             --------------                            */
/* Descriptions: This function gets all data that was in the             */
/*               AMG history file and copies them into the global        */
/*               structure 'info_data'.                                  */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void
get_job_data(struct job_id_data *p_jd)
{
   register int   i;
   register char  *p_tmp;
   int            size;
   size_t         new_size;

   /* Calculate new size */
   id.count = 1;
   new_size = 1 * sizeof(struct db_entry);

   /* Create or increase the space for the buffer */
   if ((id.dbe = (struct db_entry *)realloc(id.dbe, new_size)) == (struct db_entry *)NULL)
   {
      (void)xrec(toplevel_w, FATAL_DIALOG, "calloc() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }

   (void)strcpy(id.dir, dnb[p_jd->dir_id_pos].dir_name);
   id.dbe[0].priority = p_jd->priority;
   id.dbe[0].no_of_files = p_jd->no_of_files;

   /* Save all file/filter names */
   for (i = 0; i < id.dbe[0].no_of_files; i++)
   {
      (void)strcpy(id.dbe[0].files[i], p_jd->file_list[i]);
   }

   id.dbe[0].no_of_loptions = p_jd->no_of_loptions;

   /* Save all AMG (local) options. */
   if (id.dbe[0].no_of_loptions > 0)
   {
      p_tmp = p_jd->loptions;
      for (i = 0; i < id.dbe[0].no_of_loptions; i++)
      {
         (void)strcpy(id.dbe[0].loptions[i], p_tmp);
         NEXT(p_tmp);
      }
   }

   id.dbe[0].no_of_soptions = p_jd->no_of_soptions;

   /* Save all FD (standart) options. */
   if (id.dbe[0].no_of_soptions > 0)
   {
      size = strlen(p_jd->soptions);
      if ((id.dbe[0].soptions = calloc(size, sizeof(char))) == NULL)
      {
         (void)xrec(toplevel_w, FATAL_DIALOG,
                    "calloc() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }
      (void)memcpy(id.dbe[0].soptions, p_jd->soptions, size);
      id.dbe[0].soptions[size] = '\0';
   }

   (void)strcpy(id.dbe[0].recipient, p_jd->recipient);

   return;
}


/*++++++++++++++++++++++++++++ get_dir_data() +++++++++++++++++++++++++++*/
/*                             --------------                            */
/* Descriptions: This function gets all data that was in the             */
/*               AMG history file and copies them into the global        */
/*               structure 'info_data'.                                  */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void
get_dir_data(int dir_pos)
{
   register int   i,
                  j;
   register char  *p_tmp;
   int            size,
                  gotcha;

   (void)strcpy(id.dir, dnb[dir_pos].dir_name);

   id.count = 0;
   for (i = (*no_of_job_ids - 1); i > -1; i--)
   {
      if (jd[i].dir_id_pos == dir_pos)
      {
         while ((i > -1) && (jd[i].dir_id_pos == dir_pos))
         {
            i--;
         }
         if (jd[i].dir_id_pos != dir_pos)
         {
            i++;
         }
         break;
      }
   }

   while (jd[i].dir_id_pos == dir_pos)
   {
      /* Allocate memory to hold all data. */
      if ((id.count % 10) == 0)
      {
         size_t new_size;

         /* Calculate new size */
         new_size = ((id.count / 10) + 1) * 10 * sizeof(struct db_entry);

         /* Create or increase the space for the buffer */
         if ((id.dbe = (struct db_entry *)realloc(id.dbe, new_size)) == (struct db_entry *)NULL)
         {
            (void)xrec(toplevel_w, FATAL_DIALOG, "calloc() error : %s (%s %d)",
                       strerror(errno), __FILE__, __LINE__);
            return;
         }
      }

      id.dbe[id.count].priority = jd[i].priority;
      id.dbe[id.count].no_of_files = jd[i].no_of_files;

      /* Save all file/filter names */
      for (j = 0; j < id.dbe[id.count].no_of_files; j++)
      {
         (void)strcpy(id.dbe[id.count].files[j], jd[i].file_list[j]);
      }

      /*
       * Only show those entries that really match the current
       * file name. For this it is necessary to filter the file
       * names through all the filters.
       */
      gotcha = NO;
      for (j = 0; j < id.dbe[id.count].no_of_files; j++)
      {
         if (sfilter(id.dbe[id.count].files[j], id.file_name) == 0)
         {
            gotcha = YES;
            break;
         }
      }
      if (gotcha == YES)
      {
         id.dbe[id.count].no_of_loptions = jd[i].no_of_loptions;

         /* Save all AMG (local) options. */
         if (id.dbe[id.count].no_of_loptions > 0)
         {
            p_tmp = jd[i].loptions;
            for (j = 0; j < id.dbe[id.count].no_of_loptions; j++)
            {
               (void)strcpy(id.dbe[id.count].loptions[j], p_tmp);
               NEXT(p_tmp);
            }
         }

         id.dbe[id.count].no_of_soptions = jd[i].no_of_soptions;

         /* Save all FD (standart) options. */
         if (id.dbe[id.count].no_of_soptions > 0)
         {
            size = strlen(jd[i].soptions);
            if ((id.dbe[id.count].soptions = calloc(size,
                                                    sizeof(char))) == NULL)
            {
               (void)xrec(toplevel_w, FATAL_DIALOG,
                          "calloc() error : %s (%s %d)",
                          strerror(errno), __FILE__, __LINE__);
               return;
            }
            (void)memcpy(id.dbe[id.count].soptions, jd[i].soptions, size);
            id.dbe[id.count].soptions[size] = '\0';
         }
         else
         {
            id.dbe[id.count].soptions = NULL;
         }

         (void)strcpy(id.dbe[id.count].recipient, jd[i].recipient);

         id.count++;
      }
      i++;
   } /* while (jd[i].dir_id_pos == dir_pos) */

   return;
}