/*
 *  create_fra.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 2000 - 2002 Holger Kiehl <Holger.Kiehl@dwd.de>
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
 **   create_fra - creates the FRA of the AFD
 **
 ** SYNOPSIS
 **   void create_fra(int no_of_dirs)
 **
 ** DESCRIPTION
 **   This function creates the FRA (File Retrieve Area), to which
 **   most process of the AFD will map. The FRA has the following
 **   structure:
 **
 **      <int no_of_dirs><struct fileretrieve_status fra[no_of_dirs]>
 **
 **   A detailed description of the structures fileretrieve_status
 **   can be found in afddefs.h. The variable no_of_dirs is the number
 **   of directories from where the destinations get there data. This
 **   variable can have the value STALE (-1), which will tell all other
 **   process to unmap from this area and map to the new area.
 **
 ** RETURN VALUES
 **   None. Will exit with incorrect if any of the system call will
 **   fail.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   27.03.2000 H.Kiehl Created
 **   05.04.2001 H.Kiehl Fill file with data before it is mapped.
 **   18.04.2001 H.Kiehl Add version number to structure.
 **   03.05.2002 H.Kiehl Added new elements to FRA structure to show
 **                      total number of files in directory.
 **
 */
DESCR__E_M3

#include <stdio.h>
#include <string.h>                 /* strlen(), strcmp(), strcpy(),     */
                                    /* strerror()                        */
#include <stdlib.h>                 /* calloc(), free()                  */
#include <time.h>                   /* time()                            */
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _NO_MMAP
#include <sys/mman.h>               /* mmap(), munmap()                  */
#endif
#include <unistd.h>                 /* read(), write(), close(), lseek() */
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"


/* External global variables */
extern char                       *p_work_dir;
extern int                        fra_id,
                                  fra_fd,
                                  sys_log_fd;
extern off_t                      fra_size;
extern struct dir_data            *dd;
extern struct fileretrieve_status *fra;


/*############################ create_fra() #############################*/
void
create_fra(int no_of_dirs)
{
   int                        i, k,
                              fd,
                              loops,
                              old_fra_fd = -1,
                              old_fra_id,
                              old_no_of_dirs = -1,
                              rest;
   off_t                      old_fra_size = -1;
   char                       buffer[4096],
                              fra_id_file[MAX_PATH_LENGTH],
                              new_fra_stat[MAX_PATH_LENGTH],
                              old_fra_stat[MAX_PATH_LENGTH],
                              *ptr = NULL;
   struct fileretrieve_status *old_fra = NULL;
   struct flock               wlock = {F_WRLCK, SEEK_SET, 0, 1},
                              ulock = {F_UNLCK, SEEK_SET, 0, 1};
   struct stat                stat_buf;

   fra_size = -1;

   /* Initialise all pathnames and file descriptors */
   (void)strcpy(fra_id_file, p_work_dir);
   (void)strcat(fra_id_file, FIFO_DIR);
   (void)strcpy(old_fra_stat, fra_id_file);
   (void)strcat(old_fra_stat, FRA_STAT_FILE);
   (void)strcat(fra_id_file, FRA_ID_FILE);

   /*
    * First just try open the fra_id_file. If this fails create
    * the file and initialise old_fra_id with -1.
    */
   if ((fd = open(fra_id_file, O_RDWR)) > -1)
   {
      /*
       * Lock FRA ID file. If it is already locked
       * wait for it to clear the lock again.
       */
      if (fcntl(fd, F_SETLKW, &wlock) < 0)
      {
         /* Is lock already set or are we setting it again? */
         if ((errno != EACCES) && (errno != EAGAIN))
         {
            (void)rec(sys_log_fd, FATAL_SIGN,
                      "Could not set write lock for %s : %s (%s %d)\n",
                      fra_id_file, strerror(errno), __FILE__, __LINE__);
            exit(INCORRECT);
         }
      }

      /* Read the FRA file ID */
      if (read(fd, &old_fra_id, sizeof(int)) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not read the value of the FRA file ID : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   else
   {
      if ((fd = open(fra_id_file, (O_RDWR | O_CREAT),
                     (S_IRUSR | S_IWUSR))) < 0)
      {
         (void)rec(sys_log_fd, FATAL_SIGN,
                   "Could not open %s : %s (%s %d)\n",
                   fra_id_file, strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
      old_fra_id = -1;
   }

   /*
    * Mark memory mapped region as old, so no process puts
    * any new information into the region after we
    * have copied it into the new region.
    */
   if (old_fra_id > -1)
   {
      /* Attach to old region */
      ptr = old_fra_stat + strlen(old_fra_stat);
      (void)sprintf(ptr, ".%d", old_fra_id);

      /* Get the size of the old FSA file. */
      if (stat(old_fra_stat, &stat_buf) < 0)
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to stat() %s : %s (%s %d)\n",
                   old_fra_stat, strerror(errno), __FILE__, __LINE__);
         old_fra_id = -1;
      }
      else
      {
         if (stat_buf.st_size > 0)
         {
            if ((old_fra_fd = open(old_fra_stat, O_RDWR)) < 0)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Failed to open() %s : %s (%s %d)\n",
                         old_fra_stat, strerror(errno), __FILE__, __LINE__);
               old_fra_id = old_fra_fd = -1;
            }
            else
            {
#ifdef _NO_MMAP
               if ((ptr = mmap_emu(0, stat_buf.st_size,
                                   (PROT_READ | PROT_WRITE),
                                   MAP_SHARED, old_fra_stat, 0)) == (caddr_t) -1)
#else
               if ((ptr = mmap(0, stat_buf.st_size, (PROT_READ | PROT_WRITE),
                               MAP_SHARED, old_fra_fd, 0)) == (caddr_t) -1)
#endif
               {
                  (void)rec(sys_log_fd, ERROR_SIGN,
                            "mmap() error : %s (%s %d)\n",
                            strerror(errno), __FILE__, __LINE__);
                  old_fra_id = -1;
               }
               else
               {
                  if (*(int *)ptr == STALE)
                  {
                     (void)rec(sys_log_fd, WARN_SIGN,
                               "FRA in %s is stale! Ignoring this FRA. (%s %d)\n",
                               old_fra_stat, __FILE__, __LINE__);
                     old_fra_id = -1;
                  }
                  else
                  {
                     old_fra_size = stat_buf.st_size;
                  }

                  /*
                   * We actually could remove the old file now. Better
                   * do it when we are done with it.
                   */
               }

               /*
                * Do NOT close the old file! Else some file system
                * optimisers (like fsr in Irix 5.x) move the contents
                * of the memory mapped file!
                */
            }
         }
         else
         {
            old_fra_id = -1;
         }
      }

      if (old_fra_id != -1)
      {
         old_no_of_dirs = *(int *)ptr;

         /* Mark it as stale */
         *(int *)ptr = STALE;

         /* Check if the version has changed. */
         if (*(ptr + sizeof(int) + 1 + 1 + 1) != CURRENT_FRA_VERSION)
         {
            unsigned char old_version = *(ptr + sizeof(int) + 1 + 1 + 1);

            /* Unmap old FRA file. */
#ifdef _NO_MMAP
            if (munmap_emu(ptr) == -1)
#else
            if (munmap(ptr, old_fra_size) == -1)
#endif
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Failed to munmap() %s : %s (%s %d)\n",
                         old_fra_stat, strerror(errno), __FILE__, __LINE__);
            }
            if ((ptr = convert_fra(old_fra_fd, old_fra_stat, &old_fra_size,
                                   old_no_of_dirs,
                                   old_version, CURRENT_FRA_VERSION)) == NULL)
            {
               (void)rec(sys_log_fd, ERROR_SIGN,
                         "Failed to convert_fra() %s (%s %d)\n",
                         old_fra_stat, __FILE__, __LINE__);
               old_fra_id = -1;
            }
         } /* FRA version has changed. */

         if (old_fra_id != -1)
         {
            /* Move pointer to correct position so */
            /* we can extract the relevant data.   */
            ptr += AFD_WORD_OFFSET;

            old_fra = (struct fileretrieve_status *)ptr;
         }
      }
   }

   /*
    * Create the new mmap region.
    */
   /* First calculate the new size */
   fra_size = AFD_WORD_OFFSET +
              (no_of_dirs * sizeof(struct fileretrieve_status));

   if ((old_fra_id + 1) > -1)
   {
      fra_id = old_fra_id + 1;
   }
   else
   {
      fra_id = 0;
   }
   (void)sprintf(new_fra_stat, "%s%s%s.%d",
                 p_work_dir, FIFO_DIR, FRA_STAT_FILE, fra_id);

   /* Now map the new FRA region to a file */
   if ((fra_fd = open(new_fra_stat, (O_RDWR | O_CREAT | O_TRUNC),
                      FILE_MODE)) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "Failed to open() %s : %s (%s %d)\n",
                new_fra_stat, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /*
    * Write the complete file before we mmap() to it. If we just lseek()
    * to the end, write one byte and then mmap to it can cause a SIGBUS
    * on some systems when the disk is full and data is written to the
    * mapped area. So to detect that the disk is full always write the
    * complete file we want to map.
    */
   loops = fra_size / 4096;
   rest = fra_size % 4096;
   for (i = 0; i < loops; i++)
   {
      if (write(fra_fd, buffer, 4096) != 4096)
      {
         (void)rec(sys_log_fd, FATAL_SIGN, "write() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }
   if (rest > 0)
   {
      if (write(fra_fd, buffer, rest) != rest)
      {
         (void)rec(sys_log_fd, FATAL_SIGN, "write() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
         exit(INCORRECT);
      }
   }

#ifdef _NO_MMAP
   if ((ptr = mmap_emu(0, fra_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
                       new_fra_stat, 0)) == (caddr_t) -1)
#else
   if ((ptr = mmap(0, fra_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
                   fra_fd, 0)) == (caddr_t) -1)
#endif
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "mmap() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   (void)memset(ptr, 0, fra_size);

   /* Write number of directories to new memory mapped region */
   *(int*)ptr = no_of_dirs;

   /* Reposition fra pointer after no_of_dirs */
   ptr += AFD_WORD_OFFSET;
   fra = (struct fileretrieve_status *)ptr;

   /*
    * Copy all the old and new data into the new mapped region.
    */
   if (old_fra_id < 0)
   {
      /*
       * There is NO old FRA.
       */
      for (i = 0; i < no_of_dirs; i++)
      {
         (void)strcpy(fra[i].dir_alias, dd[i].dir_alias);
         (void)strcpy(fra[i].host_alias, dd[i].host_alias);
         (void)strcpy(fra[i].url, dd[i].url);
         fra[i].fsa_pos                = dd[i].fsa_pos;
         fra[i].protocol               = dd[i].protocol;
         fra[i].priority               = dd[i].priority;
         fra[i].delete_files_flag      = dd[i].delete_files_flag;
         fra[i].unknown_file_time      = dd[i].unknown_file_time;
         fra[i].queued_file_time       = dd[i].queued_file_time;
         fra[i].report_unknown_files   = dd[i].report_unknown_files;
         fra[i].end_character          = dd[i].end_character;
         fra[i].important_dir          = dd[i].important_dir;
         fra[i].time_option            = dd[i].time_option;
         fra[i].remove                 = dd[i].remove;
         fra[i].stupid_mode            = dd[i].stupid_mode;
         fra[i].force_reread           = dd[i].force_reread;
         fra[i].max_process            = dd[i].max_process;
         fra[i].dir_pos                = dd[i].dir_pos;
         fra[i].last_retrieval         = 0L;
         fra[i].bytes_received         = 0L;
         fra[i].files_in_dir           = 0;
         fra[i].files_queued           = 0;
         fra[i].bytes_in_dir           = 0;
         fra[i].bytes_in_queue         = 0;
         fra[i].files_received         = 0;
         fra[i].no_of_process          = 0;
         fra[i].dir_status             = NORMAL_STATUS;
         fra[i].dir_flag               = 0;
         if (fra[i].time_option == YES)
         {
            (void)memcpy(&fra[i].te, &dd[i].te, sizeof(struct bd_time_entry));
            fra[i].next_check_time = calc_next_time(&fra[i].te);
         }
      } /* for (i = 0; i < no_of_dirs; i++) */
   }
   else /* There is an old database file. */
   {
      int gotcha;

      for (i = 0; i < no_of_dirs; i++)
      {
         (void)strcpy(fra[i].dir_alias, dd[i].dir_alias);
         (void)strcpy(fra[i].host_alias, dd[i].host_alias);
         (void)strcpy(fra[i].url, dd[i].url);
         fra[i].fsa_pos                = dd[i].fsa_pos;
         fra[i].protocol               = dd[i].protocol;
         fra[i].priority               = dd[i].priority;
         fra[i].delete_files_flag      = dd[i].delete_files_flag;
         fra[i].unknown_file_time      = dd[i].unknown_file_time;
         fra[i].queued_file_time       = dd[i].queued_file_time;
         fra[i].report_unknown_files   = dd[i].report_unknown_files;
         fra[i].end_character          = dd[i].end_character;
         fra[i].important_dir          = dd[i].important_dir;
         fra[i].time_option            = dd[i].time_option;
         fra[i].remove                 = dd[i].remove;
         fra[i].stupid_mode            = dd[i].stupid_mode;
         fra[i].force_reread           = dd[i].force_reread;
         fra[i].max_process            = dd[i].max_process;
         fra[i].dir_pos                = dd[i].dir_pos;
         fra[i].no_of_process          = 0;
         fra[i].dir_status             = NORMAL_STATUS;
         if (fra[i].time_option == YES)
         {
            (void)memcpy(&fra[i].te, &dd[i].te, sizeof(struct bd_time_entry));
            fra[i].next_check_time = calc_next_time(&fra[i].te);
         }

         /*
          * Search in the old FRA for this directory. If it is there use
          * the values from the old FRA or else initialise them with
          * defaults. When we find an old entry, remember this so we
          * can later check if there are entries in the old FRA but
          * there are no corresponding entries in the DIR_CONFIG.
          */
         gotcha = NO;
         for (k = 0; k < old_no_of_dirs; k++)
         {
            if (old_fra[k].dir_pos == fra[i].dir_pos)
            {
               gotcha = YES;
               break;
            }
         }

         if (gotcha == YES)
         {
            fra[i].last_retrieval         = old_fra[k].last_retrieval;
            fra[i].bytes_received         = old_fra[k].bytes_received;
            fra[i].files_received         = old_fra[k].files_received;
            fra[i].files_in_dir           = old_fra[k].files_in_dir;
            fra[i].files_queued           = old_fra[k].files_queued;
            fra[i].bytes_in_dir           = old_fra[k].bytes_in_dir;
            fra[i].bytes_in_queue         = old_fra[k].bytes_in_queue;
            fra[i].dir_status             = old_fra[k].dir_status;
            fra[i].dir_flag               = old_fra[k].dir_flag;
            fra[i].queued                 = old_fra[k].queued;
         }
         else /* This directory is not in the old FRA, therefor it is new. */
         {
            fra[i].last_retrieval         = 0L;
            fra[i].bytes_received         = 0L;
            fra[i].files_received         = 0;
            fra[i].files_in_dir           = 0;
            fra[i].files_queued           = 0;
            fra[i].bytes_in_dir           = 0;
            fra[i].bytes_in_queue         = 0;
            fra[i].dir_status             = NORMAL_STATUS;
            fra[i].dir_flag               = 0;
            fra[i].queued                 = NO;
         }
      } /* for (i = 0; i < no_of_dirs; i++) */
   }

   /* Release memory of the structure dir_data. */
   free((void *)dd);
   dd = NULL;

   /* Reposition fra pointer after no_of_dirs */
   ptr = (char *)fra;
   ptr -= AFD_WORD_OFFSET;
   *(ptr + sizeof(int) + 1 + 1 + 1) = CURRENT_FRA_VERSION; /* FRA version number */
   if (fra_size > 0)
   {
#ifdef _NO_MMAP
      if (msync_emu(ptr) == -1)
      {
         (void)rec(sys_log_fd, ERROR_SIGN, "msync_emu() error (%s %d)\n",
                   __FILE__, __LINE__);
      }
      if (munmap_emu(ptr) == -1)
#else
      if (munmap(ptr, fra_size) == -1)
#endif
      {
         (void)rec(sys_log_fd, ERROR_SIGN,
                   "Failed to munmap() %s : %s (%s %d)\n",
                   new_fra_stat, strerror(errno), __FILE__, __LINE__);
      }
   }

   /*
    * Unmap from old memory mapped region.
    */
   if (old_fra_size > -1)
   {
      ptr = (char *)old_fra;
      ptr -= AFD_WORD_OFFSET;

      /* Don't forget to unmap old FRA file. */
      if (old_fra_size > 0)
      {
#ifdef _NO_MMAP
         if (munmap_emu(ptr) == -1)
#else
         if (munmap(ptr, old_fra_size) == -1)
#endif
         {
            (void)rec(sys_log_fd, ERROR_SIGN,
                      "Failed to munmap() %s : %s (%s %d)\n",
                      old_fra_stat, strerror(errno), __FILE__, __LINE__);
         }
      }

      /* Remove the old FSA file if there was one. */
      if (unlink(old_fra_stat) < 0)
      {
         (void)rec(sys_log_fd, WARN_SIGN,
                   "Failed to unlink() %s : %s (%s %d)\n",
                   old_fra_stat, strerror(errno), __FILE__, __LINE__);
      }
   }

   /*
    * Copy the new fra_id into the locked FRA_ID_FILE file, unlock
    * and close the file.
    */
   /* Go to beginning in file */
   if (lseek(fd, 0, SEEK_SET) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not seek() to beginning of %s : %s (%s %d)\n",
                fra_id_file, strerror(errno), __FILE__, __LINE__);
   }

   /* Write new value into FRA_ID_FILE file */
   if (write(fd, &fra_id, sizeof(int)) != sizeof(int))
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not write value to FRA ID file : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Unlock file which holds the fsa_id */
   if (fcntl(fd, F_SETLKW, &ulock) < 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not unset write lock : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Close the FRA ID file */
   if (close(fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   /* Close file with new FRA */
   if (close(fra_fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }
   fra_fd = -1;

   /* Close old FRA file */
   if (old_fra_fd != -1)
   {
      if (close(old_fra_fd) == -1)
      {
         (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                   strerror(errno), __FILE__, __LINE__);
      }
   }

   return;
}
