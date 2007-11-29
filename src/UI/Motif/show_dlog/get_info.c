/*
 *  get_info.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1998 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
extern int                 no_of_log_files;
extern char                *p_work_dir;
extern struct item_list    *il;
extern struct info_data    id;

/* Local variables. */
static int                 *no_of_dir_names,
                           *no_of_job_ids;
static struct job_id_data  *jd = NULL;
static struct dir_name_buf *dnb = NULL;

/* Local function prototypes. */
static unsigned int        get_all(int, char *);
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
         (void)xrec(ERROR_DIALOG, "Failed to open() %s : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         return;
      }
      if (fstat(jd_fd, &stat_buf) == -1)
      {
         (void)xrec(ERROR_DIALOG, "Failed to fstat() %s : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         (void)close(jd_fd);
         return;
      }
      if (stat_buf.st_size > 0)
      {
         char *ptr;

         if ((ptr = mmap(NULL, stat_buf.st_size, PROT_READ,
                         MAP_SHARED, jd_fd, 0)) == (caddr_t) -1)
         {
            (void)xrec(ERROR_DIALOG, "Failed to mmap() to %s : %s (%s %d)",
                       job_id_data_file, strerror(errno), __FILE__, __LINE__);
            (void)close(jd_fd);
            return;
         }
         no_of_job_ids = (int *)ptr;
         ptr += AFD_WORD_OFFSET;
         jd = (struct job_id_data *)ptr;
         (void)close(jd_fd);
      }
      else
      {
         (void)xrec(ERROR_DIALOG, "Job ID database file is empty. (%s %d)",
                    __FILE__, __LINE__);
         (void)close(jd_fd);
         return;
      }

      /* Map to directory name buffer. */
      (void)sprintf(job_id_data_file, "%s%s%s", p_work_dir, FIFO_DIR,
                    DIR_NAME_FILE);
      if ((dnb_fd = open(job_id_data_file, O_RDONLY)) == -1)
      {
         (void)xrec(ERROR_DIALOG, "Failed to open() %s : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         return;
      }
      if (fstat(dnb_fd, &stat_buf) == -1)
      {
         (void)xrec(ERROR_DIALOG, "Failed to fstat() %s : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         (void)close(dnb_fd);
         return;
      }
      if (stat_buf.st_size > 0)
      {
         char *ptr;

         if ((ptr = mmap(NULL, stat_buf.st_size, PROT_READ,
                         MAP_SHARED, dnb_fd, 0)) == (caddr_t) -1)
         {
            (void)xrec(ERROR_DIALOG, "Failed to mmap() to %s : %s (%s %d)",
                       job_id_data_file, strerror(errno), __FILE__, __LINE__);
            (void)close(dnb_fd);
            return;
         }
         no_of_dir_names = (int *)ptr;
         ptr += AFD_WORD_OFFSET;
         dnb = (struct dir_name_buf *)ptr;
         (void)close(dnb_fd);
      }
      else
      {
         (void)xrec(ERROR_DIALOG, "Job ID database file is empty. (%s %d)",
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
               id.dir_id = dnb[i].dir_id;
               (void)sprintf(id.dir_id_str, "%x", id.dir_id);
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
               id.dir_id = jd[i].dir_id;
               (void)sprintf(id.dir_id_str, "%x", id.dir_id);
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
      int  i;
      char *ptr,
           buffer[MAX_FILENAME_LENGTH + MAX_PATH_LENGTH],
           str_hex_number[23];

      /* Go to beginning of line to read complete line. */
      if (fseek(il[file_no].fp,
                (long)(il[file_no].line_offset[pos] - (LOG_DATE_LENGTH + 1 + MAX_HOSTNAME_LENGTH + 3)),
                SEEK_SET) == -1)
      {
         (void)xrec(FATAL_DIALOG, "fseek() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }

      if (fgets(buffer, MAX_FILENAME_LENGTH + MAX_PATH_LENGTH, il[file_no].fp) == NULL)
      {
         (void)xrec(WARN_DIALOG, "fgets() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }
      ptr = buffer;
      str_hex_number[0] = '0';
      str_hex_number[1] = 'x';
      i = 2;
      while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n') && (i < (LOG_DATE_LENGTH + 1)))
      {
         str_hex_number[i] = *ptr;
         ptr++; i++;
      }
      if (*ptr == SEPARATOR_CHAR)
      {
         str_hex_number[i] = '\0';
         *date = (time_t)str2timet(str_hex_number, NULL, 16);
         ptr++;
      }
      else if (i >= (LOG_DATE_LENGTH + 1))
           {
              while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
              {
                 ptr++;
              }
              if (*ptr == SEPARATOR_CHAR)
              {
                 ptr++;
              }
              *date = 0L;
           }

      /* Ignore file name */
      ptr = &buffer[LOG_DATE_LENGTH + 1 + MAX_HOSTNAME_LENGTH + 3];
      while (*ptr != SEPARATOR_CHAR)
      {
         ptr++;
      }
      ptr++;

      /* Get file size. */
      i = 2;
      while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n') && (i < (LOG_DATE_LENGTH + 1)))
      {
         str_hex_number[i] = *ptr;
         ptr++; i++;
      }
      if (*ptr == SEPARATOR_CHAR)
      {
         str_hex_number[i] = '\0';
#ifdef HAVE_STRTOULL
         *file_size = (double)strtoull(str_hex_number, NULL, 16);
#else
         *file_size = (double)strtoul(str_hex_number, NULL, 16);
#endif
      }
      else
      {
         *file_size = 0.0;
      }
   }

   return(SUCCESS);
}


/*+++++++++++++++++++++++++++++++ get_all() +++++++++++++++++++++++++++++*/
/*                                ---------                              */
/* Description: Retrieves the full local file name, remote file name (if */
/*              it exists), job number and if available the additional   */
/*              reasons out of the log file.                             */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static unsigned int
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
           *p_tmp,
           buffer[MAX_FILENAME_LENGTH + MAX_PATH_LENGTH],
           str_hex_number[23];

      *input_id = il[file_no].input_id[pos];
      if (fseek(il[file_no].fp, (long)il[file_no].line_offset[pos], SEEK_SET) == -1)
      {
         (void)xrec(FATAL_DIALOG, "fseek() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
         return(0U);
      }

      if (fgets(buffer, MAX_FILENAME_LENGTH + MAX_PATH_LENGTH, il[file_no].fp) == NULL)
      {
         (void)xrec(WARN_DIALOG, "fgets() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return(0U);
      }
      ptr = buffer;
      i = 0;
      while (*ptr != SEPARATOR_CHAR)
      {
         id.file_name[i] = *ptr;
         i++; ptr++;
      }
      id.file_name[i] = '\0';
      ptr++;

      /* Away with the file size. */
      str_hex_number[0] = '0';
      str_hex_number[1] = 'x';
      i = 2;
      while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n') && (i < 23))
      {
         str_hex_number[i] = *ptr;
         ptr++; i++;
      }
      if (*ptr == SEPARATOR_CHAR)
      {
         str_hex_number[i] = '\0';
#if SIZEOF_OFF_T == 4
         (void)sprintf(id.file_size, "%ld",
#else
         (void)sprintf(id.file_size, "%lld",
#endif
#ifdef HAVE_STRTOULL
                       (pri_off_t)strtoull(str_hex_number, NULL, 16));
#else
                       (pri_off_t)strtoul(str_hex_number, NULL, 16));
#endif
         ptr++;
      }
      else if (i >= 23)
           {
              while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
              {
                 ptr++;
              }
              if (*ptr == SEPARATOR_CHAR)
              {
                 ptr++;
              }
              id.file_size[0] = '0';
              id.file_size[1] = '\0';
           }

      /* Get the job ID. */
      p_tmp = ptr;
      while ((*ptr != '\n') && (*ptr != SEPARATOR_CHAR))
      {
         ptr++;
      }
      if (*ptr == SEPARATOR_CHAR)
      {
         *ptr = '\0';
         ptr++;

         i = 0;
         while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
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

      if (*ptr == SEPARATOR_CHAR)
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

      return((unsigned int)strtoul(p_tmp, NULL, 16));
   }
   else
   {
      return(0U);
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
   register int  i;
   register char *p_tmp;
   int           size;

   id.count = 1;

   /* Create or increase the space for the buffer */
   if ((id.dbe = realloc(id.dbe, sizeof(struct db_entry))) == (struct db_entry *)NULL)
   {
      (void)xrec(FATAL_DIALOG, "realloc() error : %s (%s %d)",
                 strerror(errno), __FILE__, __LINE__);
      return;
   }

   (void)strcpy(id.dir, dnb[p_jd->dir_id_pos].dir_name);
   id.dir_id = p_jd->dir_id;
   (void)sprintf(id.dir_id_str, "%x", id.dir_id);
   get_dir_options(p_jd->dir_id_pos, &id.d_o);
   id.dbe[0].priority = p_jd->priority;
   get_file_mask_list(p_jd->file_mask_id, &id.dbe[0].no_of_files,
                      &id.dbe[0].files);
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
      if ((id.dbe[0].soptions = calloc(size + 1, sizeof(char))) == NULL)
      {
         (void)xrec(FATAL_DIALOG, "calloc() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return;
      }
      (void)memcpy(id.dbe[0].soptions, p_jd->soptions, size);
      id.dbe[0].soptions[size] = '\0';
   }
   else
   {
      id.dbe[0].soptions = NULL;
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
   register char  *p_file,
                  *p_tmp;
   char           *file_mask_buf;
   int            gotcha,
                  no_of_file_masks;

   (void)strcpy(id.dir, dnb[dir_pos].dir_name);
   id.dir_id = dnb[dir_pos].dir_id;
   (void)sprintf(id.dir_id_str, "%x", id.dir_id);
   get_dir_options(dir_pos, &id.d_o);

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

   while ((jd[i].dir_id_pos == dir_pos) && (i < *no_of_job_ids))
   {
      get_file_mask_list(jd[i].file_mask_id, &no_of_file_masks, &file_mask_buf);
      if (file_mask_buf != NULL)
      {
         p_file = file_mask_buf;

         /*
          * Only show those entries that really match the current
          * file name. For this it is necessary to filter the file
          * names through all the filters.
          */
         gotcha = NO;
         for (j = 0; j < no_of_file_masks; j++)
         {
            if (pmatch(p_file, id.file_name, NULL) == 0)
            {
               gotcha = YES;
               break;
            }
            NEXT(p_file);
         }
         if (gotcha == YES)
         {
            /* Allocate memory to hold all data. */
            if ((id.count % 10) == 0)
            {
               size_t new_size;

               /* Calculate new size */
               new_size = ((id.count / 10) + 1) * 10 * sizeof(struct db_entry);

               /* Create or increase the space for the buffer. */
               if ((id.dbe = realloc(id.dbe, new_size)) == (struct db_entry *)NULL)
               {
                  (void)xrec(FATAL_DIALOG, "realloc() error : %s (%s %d)",
                             strerror(errno), __FILE__, __LINE__);
                  free(file_mask_buf);
                  return;
               }
            }

            id.dbe[id.count].priority = jd[i].priority;
            id.dbe[id.count].no_of_files = no_of_file_masks;
            id.dbe[id.count].files = file_mask_buf;

            /* Save all AMG (local) options. */
            id.dbe[id.count].no_of_loptions = jd[i].no_of_loptions;
            if (id.dbe[id.count].no_of_loptions > 0)
            {
               p_tmp = jd[i].loptions;
               for (j = 0; j < id.dbe[id.count].no_of_loptions; j++)
               {
                  (void)strcpy(id.dbe[id.count].loptions[j], p_tmp);
                  NEXT(p_tmp);
               }
            }

            /* Save all FD (standart) options. */
            id.dbe[id.count].no_of_soptions = jd[i].no_of_soptions;
            if (id.dbe[id.count].no_of_soptions > 0)
            {
               size_t size;

               size = strlen(jd[i].soptions);
               if ((id.dbe[id.count].soptions = calloc(size + 1,
                                                       sizeof(char))) == NULL)
               {
                  (void)xrec(FATAL_DIALOG, "calloc() error : %s (%s %d)",
                             strerror(errno), __FILE__, __LINE__);
                  free(file_mask_buf);
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
         else
         {
            free(file_mask_buf);
         }
      }
      i++;
   } /* while (jd[i].dir_id_pos == dir_pos) */

   return;
}
