/*
 *  change_alias_order.c - Part of AFD, an automatic file distribution program.
 *  Copyright (c) 1997 - 1999 Deutscher Wetterdienst (DWD),
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
 **   change_alias_order - changes the order of hostnames in the FSA
 **
 ** SYNOPSIS
 **   void change_alias_order(char **p_host_names)
 **
 ** DESCRIPTION
 **   This function creates a new FSA (Filetransfer Status Area) with
 **   the hosnames ordered as they are found in p_host_names.
 **
 ** RETURN VALUES
 **   None.
 **
 ** AUTHOR
 **   H.Kiehl
 **
 ** HISTORY
 **   26.08.1997 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>              /* remove()                              */
#include <string.h>             /* strcpy(), strcat(), strerror()        */
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _NO_MMAP
#include <sys/mman.h>           /* mmap()                                */
#endif
#include <unistd.h>             /* read(), write(), close(), lseek()     */
#include <fcntl.h>
#include <errno.h>
#include "amgdefs.h"

/* External global variables */
extern int                        fsa_id,
                                  fsa_fd,
                                  no_of_hosts,
                                  sys_log_fd;
#ifndef _NO_MMAP
extern off_t                      fsa_size;
#endif
extern char                       *p_work_dir;
extern struct host_list           *hl;
extern struct filetransfer_status *fsa;


/*######################### change_alias_order() ########################*/
void
change_alias_order(char **p_host_names, int new_no_of_hosts)
{
   int                        i,
                              fd,
                              position,
                              old_no_of_hosts = no_of_hosts,
                              loop_no_of_hosts,
                              current_fsa_id,
                              new_fsa_fd;
#ifdef _NO_MMAP
   off_t                      fsa_size;
#endif
   char                       *ptr,
                              fsa_id_file[MAX_PATH_LENGTH],
                              new_fsa_stat[MAX_PATH_LENGTH];
   struct filetransfer_status *new_fsa;

   if (new_no_of_hosts != -1)
   {
      if (no_of_hosts > new_no_of_hosts)
      {
         loop_no_of_hosts = no_of_hosts;
      }
      else
      {
         loop_no_of_hosts = new_no_of_hosts;
      }
      no_of_hosts = new_no_of_hosts;
   }
   else
   {
      loop_no_of_hosts = no_of_hosts;
   }

   (void)strcpy(fsa_id_file, p_work_dir);
   (void)strcat(fsa_id_file, FIFO_DIR);
   (void)strcat(fsa_id_file, FSA_ID_FILE);

   for (i = 0; i < old_no_of_hosts; i++)
   {
      lock_region_w(fsa_fd, (char *)&fsa[i] - (char *)fsa);
   }

   /*
    * When changing the order of the hosts, lock the FSA_ID_FILE so no
    * one gets the idea to do the same thing or change the DIR_CONFIG
    * file.
    */
   if ((fd = lock_file(fsa_id_file, ON)) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN, "Failed to lock %s (%s %d)\n",
                fsa_id_file, __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Read the fsa_id */
   if (read(fd, &current_fsa_id, sizeof(int)) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not read the value of the fsa_id : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      (void)close(fd);
      exit(INCORRECT);
   }

   if (current_fsa_id != fsa_id)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN,
                "AAAaaaarrrrghhhh!!! DON'T CHANGE THE DIR_CONFIG FILE WHILE USING edit_hc!!!! (%s %d)\n",
                __FILE__, __LINE__);
      (void)close(fd);
      exit(INCORRECT);
   }
   current_fsa_id++;

   /* Mark old FSA as stale. */
   *(int *)((char *)fsa - AFD_WORD_OFFSET) = STALE;

   /*
    * Create a new FSA with the new ordering of the host aliases.
    */
   (void)sprintf(new_fsa_stat, "%s%s%s.%d",
                 p_work_dir, FIFO_DIR, FSA_STAT_FILE, current_fsa_id);

   /* Now map the new FSA region to a file */
   if ((new_fsa_fd = coe_open(new_fsa_stat,
                              (O_RDWR | O_CREAT | O_TRUNC), FILE_MODE)) < 0)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to open() %s : %s (%s %d)\n",
                new_fsa_stat, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   fsa_size = AFD_WORD_OFFSET +
              (no_of_hosts * sizeof(struct filetransfer_status));
   if (lseek(new_fsa_fd, fsa_size - 1, SEEK_SET) == -1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Failed to lseek() in %s : %s (%s %d)\n",
                new_fsa_stat, strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
   if (write(new_fsa_fd, "", 1) != 1)
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "write() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }
#ifdef _NO_MMAP
   if ((ptr = mmap_emu(0, fsa_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
                       new_fsa_stat, 0)) == (caddr_t) -1)
#else
   if ((ptr = mmap(0, fsa_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
                   new_fsa_fd, 0)) == (caddr_t) -1)
#endif
   {
      (void)rec(sys_log_fd, FATAL_SIGN, "mmap() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Write number of hosts to new shm region */
   *(int*)ptr = no_of_hosts;

   /* Reposition fsa pointer after no_of_host */
   ptr += AFD_WORD_OFFSET;
   new_fsa = (struct filetransfer_status *)ptr;

   /*
    * Now copy each entry from the old FSA to the new FSA in
    * the order they are found in the host_list_w.
    */
   for (i = 0; i < loop_no_of_hosts; i++)
   {
      if (p_host_names[i][0] != '\0')
      {
         if ((position = get_position(fsa, p_host_names[i], old_no_of_hosts)) < 0)
         {
            if (hl != NULL)
            {
               int k;

               /*
                * Hmmm. This host is not in the FSA. So lets assume this
                * is a new host.
                */
               (void)memset(&new_fsa[i], 0, sizeof(struct filetransfer_status));
               (void)strcpy(new_fsa[i].host_alias, hl[i].host_alias);
               (void)sprintf(new_fsa[i].host_dsp_name, "%-*s",
                             MAX_HOSTNAME_LENGTH, hl[i].host_alias);
               new_fsa[i].toggle_pos = strlen(new_fsa[i].host_alias);
               (void)strcpy(new_fsa[i].real_hostname[0], hl[i].real_hostname[0]);
               (void)strcpy(new_fsa[i].real_hostname[1], hl[i].real_hostname[1]);
               new_fsa[i].host_toggle = HOST_ONE;
               if (hl[i].host_toggle_str[0] != '\0')
               {
                  (void)memcpy(new_fsa[i].host_toggle_str, hl[i].host_toggle_str, 5);
                  if (hl[i].host_toggle_str[0] == AUTO_TOGGLE_OPEN)
                  {
                     new_fsa[i].auto_toggle = ON;
                  }
                  else
                  {
                     new_fsa[i].auto_toggle = OFF;
                  }
                  new_fsa[i].original_toggle_pos = DEFAULT_TOGGLE_HOST;
                  new_fsa[i].host_dsp_name[(int)new_fsa[i].toggle_pos] = hl[i].host_toggle_str[(int)new_fsa[i].original_toggle_pos];
               }
               else
               {
                  new_fsa[i].host_toggle_str[0] = '\0';
                  new_fsa[i].original_toggle_pos = NONE;
                  new_fsa[i].auto_toggle = OFF;
               }
               (void)strcpy(new_fsa[i].proxy_name, hl[i].proxy_name);
               new_fsa[i].allowed_transfers = hl[i].allowed_transfers;
               for (k = 0; k < new_fsa[i].allowed_transfers; k++)
               {
                  new_fsa[i].job_status[k].connect_status = DISCONNECT;
#if defined (_BURST_MODE) || defined (_OUTPUT_LOG)
                  new_fsa[i].job_status[k].job_id = NO_ID;
#endif
               }
               for (k = new_fsa[i].allowed_transfers; k < MAX_NO_PARALLEL_JOBS; k++)
               {
                  new_fsa[i].job_status[k].no_of_files = -1;
               }
               new_fsa[i].max_errors = hl[i].max_errors;
               new_fsa[i].retry_interval = hl[i].retry_interval;
               new_fsa[i].block_size = hl[i].transfer_blksize;
               new_fsa[i].max_successful_retries = hl[i].successful_retries;
               new_fsa[i].file_size_offset = hl[i].file_size_offset;
               new_fsa[i].transfer_timeout = hl[i].transfer_timeout;
               new_fsa[i].special_flag = (new_fsa[i].special_flag & (~NO_BURST_COUNT_MASK)) | hl[i].number_of_no_bursts;
            }
            else
            {
               (void)rec(sys_log_fd, DEBUG_SIGN,
                         "AAAaaaarrrrghhhh!!! Could not find hostname %s (%s %d)\n",
                         p_host_names[i], __FILE__, __LINE__);
               (void)close(fd);
               exit(INCORRECT);
            }
         }
         else
         {
            (void)memcpy(&new_fsa[i], &fsa[position],
                         sizeof(struct filetransfer_status));
         }
      }
   }
#ifndef _NO_MMAP
   if (msync(ptr - AFD_WORD_OFFSET, fsa_size, MS_SYNC) == -1)
   {
      (void)rec(sys_log_fd, WARN_SIGN, "msync() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }
#endif
   for (i = 0; i < old_no_of_hosts; i++)
   {
      unlock_region(fsa_fd, (char *)&fsa[i] - (char *)fsa);
   }

   if (fsa_detach() < 0)
   {
      (void)rec(sys_log_fd, WARN_SIGN, "Failed to detach from FSA (%s %d)\n",
                __FILE__, __LINE__);
   }
   fsa = new_fsa;
   fsa_fd = new_fsa_fd;
   fsa_id = current_fsa_id;

   /* Go to beginning in file */
   if (lseek(fd, 0, SEEK_SET) < 0)
   {
      (void)rec(sys_log_fd, ERROR_SIGN,
                "Could not seek() to beginning of %s : %s (%s %d)\n",
                fsa_id_file, strerror(errno), __FILE__, __LINE__);
   }

   /* Write new value into FSA_ID_FILE file */
   if (write(fd, &fsa_id, sizeof(int)) != sizeof(int))
   {
      (void)rec(sys_log_fd, FATAL_SIGN,
                "Could not write value to FSA ID file : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
      exit(INCORRECT);
   }

   /* Release the lock */
   if (close(fd) == -1)
   {
      (void)rec(sys_log_fd, DEBUG_SIGN, "close() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   /* Remove the old FSA file. */
   (void)sprintf(new_fsa_stat, "%s%s%s.%d",
                 p_work_dir, FIFO_DIR, FSA_STAT_FILE, current_fsa_id - 1);
   if (remove(new_fsa_stat) < 0)
   {
      (void)rec(sys_log_fd, WARN_SIGN, "remove() error : %s (%s %d)\n",
                strerror(errno), __FILE__, __LINE__);
   }

   return;
}