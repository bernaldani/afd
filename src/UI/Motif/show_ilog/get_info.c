/*
 *  get_info.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   void get_info(int item)
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
 **   27.05.1997 H.Kiehl Created
 **   11.02.1998 H.Kiehl Adapted to new message format.
 **   22.09.2003 H.Kiehl get_recipient_only() now gets all recipients.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                   /* strerror()                      */
#include <stdlib.h>                   /* strtoul(), realloc(), free()    */
#include <unistd.h>                   /* close()                         */
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "afd_ctrl.h"
#include "show_ilog.h"

/* Global variables */
unsigned int               *current_jid_list;
int                        no_of_current_jobs;

/* External global variables. */
extern Widget              appshell;
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
static unsigned int        get_all(int);
static void                get_dir_data(int),
                           get_recipient_only(int);


/*############################### get_info() ############################*/
void
get_info(int item)
{
   int i;

   current_jid_list = NULL;
   no_of_current_jobs = 0;
   if ((item != GOT_JOB_ID_DIR_ONLY) &&
       (item != GOT_JOB_ID_DIR_AND_RECIPIENT))
   {
      id.dir_id = get_all(item - 1);
      if (get_current_jid_list() == INCORRECT)
      {
         if (current_jid_list != NULL)
         {
            free(current_jid_list);
         }
         return;
      }
   }

   /*
    * Go through job ID database and find the dir ID.
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
         (void)xrec(appshell, ERROR_DIALOG,
                    "Failed to open() %s : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         if (current_jid_list != NULL)
         {
            free(current_jid_list);
         }
         return;
      }
      if (fstat(jd_fd, &stat_buf) == -1)
      {
         (void)xrec(appshell, ERROR_DIALOG,
                    "Failed to fstat() %s : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         (void)close(jd_fd);
         if (current_jid_list != NULL)
         {
            free(current_jid_list);
         }
         return;
      }
      if (stat_buf.st_size > 0)
      {
         char *ptr;

         if ((ptr = mmap(0, stat_buf.st_size, PROT_READ,
                         MAP_SHARED, jd_fd, 0)) == (caddr_t) -1)
         {
            (void)xrec(appshell, ERROR_DIALOG,
                       "Failed to mmap() to %s : %s (%s %d)",
                       job_id_data_file, strerror(errno), __FILE__, __LINE__);
            (void)close(jd_fd);
            if (current_jid_list != NULL)
            {
               free(current_jid_list);
            }
            return;
         }
         no_of_job_ids = (int *)ptr;
         ptr += AFD_WORD_OFFSET;
         jd = (struct job_id_data *)ptr;
         (void)close(jd_fd);
      }
      else
      {
         (void)xrec(appshell, ERROR_DIALOG,
                    "Job ID database file is empty. (%s %d)",
                    __FILE__, __LINE__);
         (void)close(jd_fd);
         if (current_jid_list != NULL)
         {
            free(current_jid_list);
         }
         return;
      }

      /* Map to directory name buffer. */
      (void)sprintf(job_id_data_file, "%s%s%s", p_work_dir, FIFO_DIR,
                    DIR_NAME_FILE);
      if ((dnb_fd = open(job_id_data_file, O_RDONLY)) == -1)
      {
         (void)xrec(appshell, ERROR_DIALOG,
                    "Failed to open() %s : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         if (current_jid_list != NULL)
         {
            free(current_jid_list);
         }
         return;
      }
      if (fstat(dnb_fd, &stat_buf) == -1)
      {
         (void)xrec(appshell, ERROR_DIALOG,
                    "Failed to fstat() %s : %s (%s %d)",
                    job_id_data_file, strerror(errno), __FILE__, __LINE__);
         (void)close(dnb_fd);
         if (current_jid_list != NULL)
         {
            free(current_jid_list);
         }
         return;
      }
      if (stat_buf.st_size > 0)
      {
         char *ptr;

         if ((ptr = mmap(0, stat_buf.st_size, PROT_READ,
                         MAP_SHARED, dnb_fd, 0)) == (caddr_t) -1)
         {
            (void)xrec(appshell, ERROR_DIALOG,
                       "Failed to mmap() to %s : %s (%s %d)",
                       job_id_data_file, strerror(errno), __FILE__, __LINE__);
            (void)close(dnb_fd);
            if (current_jid_list != NULL)
            {
               free(current_jid_list);
            }
            return;
         }
         no_of_dir_names = (int *)ptr;
         ptr += AFD_WORD_OFFSET;
         dnb = (struct dir_name_buf *)ptr;
         (void)close(dnb_fd);
      }
      else
      {
         (void)xrec(appshell, ERROR_DIALOG,
                    "Job ID database file is empty. (%s %d)",
                    __FILE__, __LINE__);
         (void)close(dnb_fd);
         if (current_jid_list != NULL)
         {
            free(current_jid_list);
         }
         return;
      }
   }

   /* Search through all history files. */
   for (i = 0; i < *no_of_dir_names; i++)
   {
      if (id.dir_id == dnb[i].dir_id)
      {
         if (item == GOT_JOB_ID_DIR_ONLY)
         {
            (void)strcpy(id.dir, dnb[i].dir_name);
         }
         else if (item == GOT_JOB_ID_DIR_AND_RECIPIENT)
              {
                 get_recipient_only(i);
              }
              else
              {
                 get_dir_data(i);
              }

         break;
      }
   } /* for (i = 0; i < *no_of_dir_names; i++) */

   if (current_jid_list != NULL)
   {
      free(current_jid_list);
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
      if (fseek(il[file_no].fp, (long)il[file_no].line_offset[pos],
                SEEK_SET) == -1)
      {
         (void)xrec(appshell, FATAL_DIALOG,
                    "fseek() error : %s (%s %d)\n",
                    strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }

      if (fgets(buffer, MAX_FILENAME_LENGTH + MAX_PATH_LENGTH, il[file_no].fp) == NULL)
      {
         (void)xrec(appshell, WARN_DIALOG,
                    "fgets() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return(INCORRECT);
      }
      ptr = buffer;
      str_hex_number[0] = '0';
      str_hex_number[1] = 'x';
      i = 2;
      while ((*ptr != ' ') && (i < 11))
      {
         str_hex_number[i] = *ptr;
         ptr++; i++;
      }
      str_hex_number[i] = '\0';
      *date = (time_t)strtol(str_hex_number, NULL, 16);
      while (*ptr == ' ')
      {
         ptr++;
      }

      /* Ignore file name */
      while (*ptr != SEPARATOR_CHAR)
      {
         ptr++;
      }
      ptr++;

      /* Get file size */
      i = 2;
      while ((*ptr != SEPARATOR_CHAR) && (i < 23))
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
/* Description: Retrieves the full file name and the  dir number.        */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static unsigned int
get_all(int item)
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

   /* Get the dir ID and file name. */
   if (pos > -1)
   {
      int  i;
      char *ptr,
           buffer[MAX_FILENAME_LENGTH + MAX_PATH_LENGTH],
           str_hex_number[23];

      if (fseek(il[file_no].fp, (long)il[file_no].line_offset[pos],
                SEEK_SET) == -1)
      {
         (void)xrec(appshell, FATAL_DIALOG, "fseek() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return(0);
      }

      if (fgets(buffer, MAX_FILENAME_LENGTH + MAX_PATH_LENGTH,
                il[file_no].fp) == NULL)
      {
         (void)xrec(appshell, FATAL_DIALOG, "fgets() error : %s (%s %d)",
                    strerror(errno), __FILE__, __LINE__);
         return(0);
      }
      ptr = buffer;
      str_hex_number[0] = '0';
      str_hex_number[1] = 'x';
      i = 2;
      while ((*ptr != ' ') && (i < 11))
      {
         str_hex_number[i] = *ptr;
         ptr++; i++;
      }
      str_hex_number[i] = '\0';
      id.arrival_time = (time_t)strtol(str_hex_number, NULL, 16);
      while (*ptr == ' ')
      {
         ptr++;
      }

      /* Store file name. */
      i = 0;
      while (*ptr != SEPARATOR_CHAR)
      {
         id.file_name[i] = *ptr;
         i++; ptr++;
      }
      id.file_name[i] = '\0';
      ptr++;

      /* Away with the file size. */
      i = 2;
      while ((*ptr != SEPARATOR_CHAR) && (i < 23))
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

      /* Get the dir ID. */
      i = 2;
      while ((*ptr != SEPARATOR_CHAR) && (*ptr != '\n'))
      {
         str_hex_number[i] = *ptr;
         ptr++; i++;
      }
      if (*ptr == SEPARATOR_CHAR)
      {
         char *unp;

         str_hex_number[i] = '\0';
         ptr++;
         unp = ptr;
         while (*ptr != '\n')
         {
            ptr++;
         }
         if (*ptr == '\n')
         {
            *ptr = '\0';
            id.unique_number = atoi(unp);
         }
         else
         {
            id.unique_number = -1;
         }
      }
      else
      {
         *ptr = '\0';
      }

      return((unsigned int)strtoul(str_hex_number, NULL, 16));
   }
   else
   {
      return(0);
   }
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
   register int  i,
                 j;
   register char *p_file,
                 *p_tmp;
   int           gotcha;

   (void)strcpy(id.dir, dnb[dir_pos].dir_name);

   get_dir_options(dir_pos, &id.d_o);

   id.count = 0;
   for (i = (*no_of_job_ids - 1); i > -1; i--)
   {
      if (jd[i].dir_id_pos == dir_pos)
      {
         for (j = 0; j < no_of_current_jobs; j++)
         {
            if (current_jid_list[j] == jd[i].job_id)
            {
               int k;

               /* Allocate memory to hold all data. */
               if ((id.count % 10) == 0)
               {
                  size_t new_size;

                  /* Calculate new size */
                  new_size = ((id.count / 10) + 1) * 10 *
                             sizeof(struct db_entry);

                  /* Create or increase the space for the buffer */
                  if ((id.dbe = realloc(id.dbe, new_size)) == (struct db_entry *)NULL)
                  {
                     (void)xrec(appshell, FATAL_DIALOG,
                                "realloc() error : %s (%s %d)",
                                strerror(errno), __FILE__, __LINE__);
                     return;
                  }
               }

               id.dbe[id.count].priority = jd[i].priority;
               get_file_mask_list(jd[i].file_mask_id,
                                  &id.dbe[id.count].no_of_files,
                                  &id.dbe[id.count].files);
               if (id.dbe[id.count].files != NULL)
               {
                  p_file = id.dbe[id.count].files;

                  /*
                   * Only show those entries that really match the current
                   * file name. For this it is necessary to filter the file
                   * names through all the filters.
                   */
                  gotcha = NO;
                  for (k = 0; k < id.dbe[id.count].no_of_files; k++)
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
                     /* Save all AMG (local) options. */
                     id.dbe[id.count].no_of_loptions = jd[i].no_of_loptions;
                     if (id.dbe[id.count].no_of_loptions > 0)
                     {
                        p_tmp = jd[i].loptions;
                        for (k = 0; k < id.dbe[id.count].no_of_loptions; k++)
                        {
                           (void)strcpy(id.dbe[id.count].loptions[k], p_tmp);
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
                           (void)xrec(appshell, FATAL_DIALOG,
                                      "calloc() error : %s (%s %d)",
                                      strerror(errno), __FILE__, __LINE__);
                           return;
                        }
                        (void)memcpy(id.dbe[id.count].soptions, jd[i].soptions,
                                     size);
                        id.dbe[id.count].soptions[size] = '\0';
                     }
                     else
                     {
                        id.dbe[id.count].soptions = NULL;
                     }

                     (void)strcpy(id.dbe[id.count].recipient, jd[i].recipient);
                     id.count++;
                  }
               }
            } /* if (current_jid_list[j] == jd[i].job_id) */
         } /* for (j = 0; j < no_of_current_jobs; j++) */
      } /* if (jd[i].dir_id_pos == dir_pos) */
   } /* for (i = (*no_of_job_ids - 1); i > -1; i--) */

   return;
}


/*+++++++++++++++++++++++++ get_recipient_only() ++++++++++++++++++++++++*/
/*                          --------------------                         */
/* Descriptions: This function gets only the recipient from the          */
/*               AMG history file and copies them into the               */
/*               global structure 'info_data'.                           */
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static void
get_recipient_only(int dir_pos)
{
   register int  i,
                 j;
   register char *ptr;
   int           size,
                 gotcha;
   char          *file_mask_buf,
                 *p_file,
                 recipient[MAX_RECIPIENT_LENGTH];

   (void)strcpy(id.dir, dnb[dir_pos].dir_name);
   size = strlen(id.dir);
   id.dir[size] = ' ';
   id.dir[size + 1] = '\0';

   id.count = 0;
   for (i = 0; i < *no_of_job_ids; i++)
   {
      if (jd[i].dir_id_pos == dir_pos)
      {
         int no_of_file_mask;

         /*
          * Only show those entries that really match the current
          * file name. For this it is necessary to filter the file
          * names through all the filters.
          */
         get_file_mask_list(jd[i].file_mask_id, &no_of_file_mask,
                            &file_mask_buf);
         if (file_mask_buf != NULL)
         {
            gotcha = NO;
            p_file = file_mask_buf;
            for (j = 0; j < no_of_file_mask; j++)
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

                  /* Create or increase the space for the buffer */
                  if ((id.dbe = realloc(id.dbe, new_size)) == (struct db_entry *)NULL)
                  {
                     (void)xrec(appshell, FATAL_DIALOG,
                                "realloc() error : %s (%s %d)",
                                strerror(errno), __FILE__, __LINE__);
                     return;
                  }
               }
               (void)strcpy(recipient, jd[i].recipient);
               ptr = recipient;

               while ((*ptr != '/') && (*ptr != '\0'))
               {
                  if (*ptr == '\\')
                  {
                     ptr++;
                  }
                  ptr++;
               }
               if ((*ptr == '/') && (*(ptr + 1) == '/'))
               {
                  int count = 0;

                  ptr += 2;
                  while ((*ptr != ':') && (*ptr != '@') && (*ptr != '\0'))
                  {
                     if (*ptr == '\\')
                     {
                        ptr++;
                     }
                     id.dbe[id.count].user[count] = *ptr;
                     ptr++; count++;
                  }
                  id.dbe[id.count].user[count] = ' ';
                  id.dbe[id.count].user[count + 1] = '\0';
               }
               else
               {
                  id.dbe[id.count].user[0] = '\0';
               }
               while ((*ptr != '@') && (*ptr != '\0'))
               {
                  if (*ptr == '\\')
                  {
                     ptr++;
                  }
                  ptr++;
               }
               if (*ptr == '@')
               {
                  int j = 0;

                  ptr++;
                  while ((*ptr != '\0') && (*ptr != '/') &&
                         (*ptr != ':') && (*ptr != '.'))
                  {
                     if (*ptr == '\\')
                     {
                        ptr++;
                     }
                     id.dbe[id.count].recipient[j] = *ptr;
                     j++; ptr++;
                  }
                  id.dbe[id.count].recipient[j] = ' ';
                  id.dbe[id.count].recipient[j + 1] = '\0';
               }
               else
               {
                  id.dbe[id.count].recipient[0] = '\0';
               }

               /*
                * Next lets check if the directory is a remote one.
                * If thats the case lets store this as well so it is
                * searchable as well.
                */
               ptr = dnb[jd[i].dir_id_pos].orig_dir_name;
               if ((*ptr != '/') && (*ptr != '~'))
               {
                  while ((*ptr != '/') && (*ptr != '\0'))
                  {
                     if (*ptr == '\\')
                     {
                        ptr++;
                     }
                     ptr++;
                  }
                  if ((*ptr == '/') && (*(ptr + 1) == '/'))
                  {
                     int count = 0;

                     ptr += 2;
                     while ((*ptr != ':') && (*ptr != '@') && (*ptr != '\0') &&
                            (count < (MAX_USER_NAME_LENGTH + 1)))
                     {
                        if (*ptr == '\\')
                        {
                           ptr++;
                        }
                        id.dbe[id.count].dir_url_user[count] = *ptr;
                        ptr++; count++;
                     }
                     id.dbe[id.count].dir_url_user[count] = ' ';
                     id.dbe[id.count].dir_url_user[count + 1] = '\0';
                  }
                  else
                  {
                     id.dbe[id.count].dir_url_user[0] = '\0';
                  }
                  while ((*ptr != '@') && (*ptr != '\0'))
                  {
                     if (*ptr == '\\')
                     {
                        ptr++;
                     }
                     ptr++;
                  }
                  if (*ptr == '@')
                  {
                     int j = 0;

                     ptr++;
                     while ((*ptr != '\0') && (*ptr != '/') &&
                            (*ptr != ':') && (*ptr != '.') &&
                            (j < (MAX_HOSTNAME_LENGTH + 1)))
                     {
                        if (*ptr == '\\')
                        {
                           ptr++;
                        }
                        id.dbe[id.count].dir_url_hostname[j] = *ptr;
                        j++; ptr++;
                     }
                     id.dbe[id.count].dir_url_hostname[j] = ' ';
                     id.dbe[id.count].dir_url_hostname[j + 1] = '\0';
                  }
                  else
                  {
                     id.dbe[id.count].dir_url_hostname[0] = '\0';
                  }
               }
               else
               {
                  id.dbe[id.count].dir_url_hostname[0] = '\0';
                  id.dbe[id.count].dir_url_user[0] = '\0';
               }
               id.count++;
            } /* if (gotcha == YES) */
            free(file_mask_buf);
         }
      }
   }

   return;
}
