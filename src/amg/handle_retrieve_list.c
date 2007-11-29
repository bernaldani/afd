/*
 *  handle_retrieve_list.c - Part of AFD, an automatic file distribution
 *                           program.
 *  Copyright (c) 2006, 2007 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   handle_retrieve_list - a set of funtions to handle the local
 **                          retrieve list
 **
 ** SYNOPSIS
 **   int check_list(struct directory_entry *p_de,
 **                  char                   *file,
 **                  struct stat            *p_stat_buf)
 **   int rm_removed_files(struct directory_entry *p_de)
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
 **   21.08.2006 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                /* strcpy(), strerror(), memmove()    */
#include <stdlib.h>                /* malloc(), realloc(), free()        */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"


/* External global variables. */
extern char                       *p_work_dir;
extern struct fileretrieve_status *fra;


/*############################ check_list() #############################*/
int
check_list(struct directory_entry *p_de,
           char                   *file,
           struct stat            *p_stat_buf)
{
   int i;

   if (p_de->rl_fd == -1)
   {
      char        list_file[MAX_PATH_LENGTH],
                  *ptr;
      struct stat stat_buf;

      (void)sprintf(list_file, "%s%s%s%s/%s", p_work_dir, AFD_FILE_DIR,
                    INCOMING_DIR, LS_DATA_DIR, fra[p_de->fra_pos].dir_alias);
      if ((p_de->rl_fd = open(list_file, O_RDWR | O_CREAT, FILE_MODE)) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to open() `%s' : %s",
                    list_file, strerror(errno));
         exit(INCORRECT);
      }
      if (fstat(p_de->rl_fd, &stat_buf) == -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to fstat() `%s' : %s", list_file, strerror(errno));
         exit(INCORRECT);
      }
      if (stat_buf.st_size == 0)
      {
         p_de->rl_size = (RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                         AFD_WORD_OFFSET;
         if (lseek(p_de->rl_fd, p_de->rl_size - 1, SEEK_SET) == -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to lseek() in `%s' : %s",
                       list_file, strerror(errno));
            exit(INCORRECT);
         }
         if (write(p_de->rl_fd, "", 1) != 1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "Failed to write() to `%s' : %s",
                       list_file, strerror(errno));
            exit(INCORRECT);
         }
      }
      else
      {
         p_de->rl_size = stat_buf.st_size;
      }
      if ((ptr = mmap(NULL, p_de->rl_size, (PROT_READ | PROT_WRITE),
                      MAP_SHARED, p_de->rl_fd, 0)) == (caddr_t) -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "Failed to mmap() to `%s' : %s",
                    list_file, strerror(errno));
         exit(INCORRECT);
      }
      p_de->no_of_listed_files = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      p_de->rl = (struct retrieve_list *)ptr;
      if (stat_buf.st_size == 0)
      {
         *p_de->no_of_listed_files = 0;
      }
      else
      {
         if (*p_de->no_of_listed_files < 0)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Hmmm, no_of_listed_files = %d",
                       *p_de->no_of_listed_files);
            *p_de->no_of_listed_files = 0;
         }
         else
         {
            if (((stat_buf.st_size - AFD_WORD_OFFSET) % sizeof(struct retrieve_list)) != 0)
            {
               off_t calc_size;

               calc_size = (((*p_de->no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                            RETRIEVE_LIST_STEP_SIZE *
                            sizeof(struct retrieve_list)) + AFD_WORD_OFFSET;
               system_log(DEBUG_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                          "Hmm, LS data file %s has incorrect size (%ld != %ld), removing it.",
#else
                          "Hmm, LS data file %s has incorrect size (%lld != %lld), removing it.",
#endif
                          list_file, (pri_off_t)stat_buf.st_size,
                          (pri_off_t)calc_size);
               ptr -= AFD_WORD_OFFSET;
               if (munmap(ptr, stat_buf.st_size) == -1)
               {
                  system_log(WARN_SIGN, __FILE__, __LINE__,
                             "Failed to munmap() %s : %s",
                             list_file, strerror(errno));
               }
               if (close(p_de->rl_fd) == -1)
               {
                  system_log(DEBUG_SIGN, __FILE__, __LINE__,
                             "Failed to close() %s : %s",
                             list_file, strerror(errno));
               }
               if ((p_de->rl_fd = open(list_file, O_RDWR | O_CREAT | O_TRUNC,
                                       FILE_MODE)) == -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to open() `%s' : %s",
                             list_file, strerror(errno));
                  exit(INCORRECT);
               }
               p_de->rl_size = (RETRIEVE_LIST_STEP_SIZE *
                                sizeof(struct retrieve_list)) + AFD_WORD_OFFSET;
               if (lseek(p_de->rl_fd, p_de->rl_size - 1, SEEK_SET) == -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to lseek() in `%s' : %s",
                             list_file, strerror(errno));
                  exit(INCORRECT);
               }
               if (write(p_de->rl_fd, "", 1) != 1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to write() to `%s' : %s",
                             list_file, strerror(errno));
                  exit(INCORRECT);
               }
               if ((ptr = mmap(NULL, p_de->rl_size, (PROT_READ | PROT_WRITE),
                               MAP_SHARED, p_de->rl_fd, 0)) == (caddr_t) -1)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to mmap() to `%s' : %s",
                             list_file, strerror(errno));
                  exit(INCORRECT);
               }
               p_de->no_of_listed_files = (int *)ptr;
               ptr += AFD_WORD_OFFSET;
               p_de->rl = (struct retrieve_list *)ptr;
               *p_de->no_of_listed_files = 0;
            }
            for (i = 0; i < *p_de->no_of_listed_files; i++)
            {
               p_de->rl[i].in_list = NO;
            }
         }
      }
   }

   /* Check if this file is in the list. */
   for (i = 0; i < *p_de->no_of_listed_files; i++)
   {
      if (CHECK_STRCMP(p_de->rl[i].file_name, file) == 0)
      {
         p_de->rl[i].in_list = YES;
         if ((fra[p_de->fra_pos].stupid_mode == GET_ONCE_ONLY) &&
             (p_de->rl[i].retrieved == YES))
         {
            return(-1);
         }
         if (p_de->rl[i].file_mtime != p_stat_buf->st_mtime)
         {
            p_de->rl[i].file_mtime = p_stat_buf->st_mtime;
            p_de->rl[i].retrieved = NO;
         }
         p_de->rl[i].got_date = YES;
         if (p_de->rl[i].size != p_stat_buf->st_size)
         {
            p_de->rl[i].size = p_stat_buf->st_size;
            p_de->rl[i].retrieved = NO;
         }
         if (p_de->rl[i].retrieved == NO)
         {
            return(i);
         }
         else
         {
            return(-2);
         }
      }
   } /* for (i = 0; i < *p_de->no_of_listed_files; i++) */

   /* Add this file to the list. */
   if ((*p_de->no_of_listed_files != 0) &&
       ((*p_de->no_of_listed_files % RETRIEVE_LIST_STEP_SIZE) == 0))
   {
      char   list_file[MAX_PATH_LENGTH],
             *ptr;
      size_t new_size = (((*p_de->no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                        RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                        AFD_WORD_OFFSET;

      (void)sprintf(list_file, "%s%s%s%s/%s", p_work_dir, AFD_FILE_DIR,
                    INCOMING_DIR, LS_DATA_DIR, fra[p_de->fra_pos].dir_alias);
      ptr = (char *)p_de->rl - AFD_WORD_OFFSET;
      if ((ptr = mmap_resize(p_de->rl_fd, ptr, new_size)) == (caddr_t) -1)
      {
         system_log(ERROR_SIGN, __FILE__, __LINE__,
                    "mmap_resize() error : %s", strerror(errno));
         exit(INCORRECT);
      }
      p_de->rl_size = new_size;
      p_de->no_of_listed_files = (int *)ptr;
      ptr += AFD_WORD_OFFSET;
      p_de->rl = (struct retrieve_list *)ptr;
      if (*p_de->no_of_listed_files < 0)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Hmmm, no_of_listed_files = %d", *p_de->no_of_listed_files);
         *p_de->no_of_listed_files = 0;
      }
   }
   p_de->rl[*p_de->no_of_listed_files].file_mtime = p_stat_buf->st_mtime;
   p_de->rl[*p_de->no_of_listed_files].got_date = YES;

   (void)strcpy(p_de->rl[*p_de->no_of_listed_files].file_name, file);
   p_de->rl[*p_de->no_of_listed_files].retrieved = NO;
   p_de->rl[*p_de->no_of_listed_files].in_list = YES;
   p_de->rl[*p_de->no_of_listed_files].size = p_stat_buf->st_size;
   (*p_de->no_of_listed_files)++;

   return(*p_de->no_of_listed_files - 1);
}


/*########################## rm_removed_files() #########################*/
void
rm_removed_files(struct directory_entry *p_de)
{
   int    files_removed = 0,
          i;
   size_t move_size;

   for (i = 0; i < (*p_de->no_of_listed_files - files_removed); i++)
   {
      if (p_de->rl[i].in_list == NO)
      {
         int j = i;

         while ((p_de->rl[j].in_list == NO) &&
                (j < (*p_de->no_of_listed_files - files_removed)))
         {
            j++;
         }
         if (j != (*p_de->no_of_listed_files - files_removed))
         {
            move_size = (*p_de->no_of_listed_files - files_removed - j) *
                        sizeof(struct retrieve_list);
            (void)memmove(&p_de->rl[i], &p_de->rl[j], move_size);
         }
         files_removed += (j - i);
      }
   }

   if (files_removed > 0)
   {
      int    current_no_of_listed_files = *p_de->no_of_listed_files;
      size_t new_size,
             old_size;

      *p_de->no_of_listed_files -= files_removed;
      if (*p_de->no_of_listed_files < 0)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Hmmm, no_of_listed_files = %d", *p_de->no_of_listed_files);
         *p_de->no_of_listed_files = 0;
      }
      if (*p_de->no_of_listed_files == 0)
      {
         new_size = (RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                    AFD_WORD_OFFSET;
      }
      else
      {
         new_size = (((*p_de->no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                     RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                    AFD_WORD_OFFSET;
      }
      old_size = (((current_no_of_listed_files / RETRIEVE_LIST_STEP_SIZE) + 1) *
                  RETRIEVE_LIST_STEP_SIZE * sizeof(struct retrieve_list)) +
                 AFD_WORD_OFFSET;

      if (old_size != new_size)
      {
         char *ptr;

         ptr = (char *)p_de->rl - AFD_WORD_OFFSET;
         if ((ptr = mmap_resize(p_de->rl_fd, ptr, new_size)) == (caddr_t) -1)
         {
            system_log(ERROR_SIGN, __FILE__, __LINE__,
                       "mmap_resize() error : %s", strerror(errno));
            exit(INCORRECT);
         }
         p_de->no_of_listed_files = (int *)ptr;
         ptr += AFD_WORD_OFFSET;
         p_de->rl = (struct retrieve_list *)ptr;
         if (*p_de->no_of_listed_files < 0)
         {
            system_log(DEBUG_SIGN, __FILE__, __LINE__,
                       "Hmmm, no_of_listed_files = %d",
                       *p_de->no_of_listed_files);
            *p_de->no_of_listed_files = 0;
         }
         p_de->rl_size = new_size;
      }
   }
   return;
}
