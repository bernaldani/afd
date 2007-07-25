/*
 *  manage_trl_process.c - Part of AFD, an automatic file distribution program.
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
 **   manage_trl_process - set of functions that manage the transfer
 **                        rate limit
 **
 ** SYNOPSIS
 **   void init_trl_data(void)
 **   void calc_trl_per_process(int fsa_pos)
 **   void check_trl_file(void)
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
 **   28.02.2006 H.Kiehl Created
 **
 */
DESCR__E_M3

#include <stdio.h>       /* sprintf()                                    */
#include <string.h>      /* strlen(), strerror()                         */
#include <stdlib.h>      /* malloc(), realloc(), free(), strtoul()       */
#include <ctype.h>       /* isdigit()                                    */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>      /* close(), read()                              */
#include <errno.h>
#include "fddefs.h"

/* External global variables. */
extern int                        no_of_trl_groups,
                                  no_of_hosts;
extern char                       *p_work_dir;
extern struct filetransfer_status *fsa;

/* Local global variables. */
static int                        old_no_of_hosts;
static time_t                     trl_file_mtime = 0L;
static char                       *trl_filename = NULL;
static struct trl_group           *trlg = NULL;
static struct trl_cache           *trlc = NULL;


/*########################## init_trl_data() ############################*/
void
init_trl_data(void)
{
   int fd;

   if (trl_filename == NULL)
   {
      size_t length;

      length = strlen(p_work_dir) + sizeof(ETC_DIR) - 1 + sizeof(TRL_FILENAME) + 1;
      if ((trl_filename = malloc(length)) == NULL)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to malloc() %d bytes : %s",
                    length, strerror(length));
         return;
      }
      (void)sprintf(trl_filename, "%s%s/%s", p_work_dir, ETC_DIR, TRL_FILENAME);
   }
   if (no_of_trl_groups != 0)
   {
      int i;

      /* Free all memory of old file. */
      for (i = 0; i < no_of_trl_groups; i++)
      {
         free(trlg[i].group_name);
         free(trlg[i].fsa_pos);
      }
      free(trlg);
      trlg = NULL;
      no_of_trl_groups = 0;
   }

   if (trlc != NULL)
   {
      free(trlc);
      trlc = NULL;
   }

   if ((fd = coe_open(trl_filename, O_RDONLY)) != -1)
   {
      int         i,
                  length;
      off_t       file_size;
      char        *buffer,
                  *ptr,
                  *p_start;
      struct stat stat_buf;

      if (fstat(fd, &stat_buf) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to fstat() `%s' : %s",
                    trl_filename, strerror(errno));
         (void)close(fd);
         return;
      }
      if (stat_buf.st_size == 0)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Transfer rate limit file `%s' is empty.", trl_filename);
         (void)close(fd);
         return;
      }
      else if (stat_buf.st_size > 2097152)
           {
              system_log(WARN_SIGN, __FILE__, __LINE__,
                         "The function init_trl_process() was not made to handle large files. Ask author to change this.");
              (void)close(fd);
              return;
           }
      file_size = stat_buf.st_size;
      trl_file_mtime = stat_buf.st_mtime;
      if ((buffer = malloc(2 + file_size + 1)) == NULL)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
#if SIZEOF_OFF_T == 4
                    "Failed to malloc() %ld bytes : %s",
#else
                    "Failed to malloc() %lld bytes : %s",
#endif
                    (pri_off_t)(file_size + 1), strerror(errno));
         (void)close(fd);
         return;
      }
      if (read(fd, &buffer[2], file_size) == -1)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to read() `%s' : %s",
                    trl_filename, strerror(errno));
         (void)close(fd);
         free(buffer);
         return;
      }
      if (close(fd) == -1)
      {
         system_log(DEBUG_SIGN, __FILE__, __LINE__,
                    "Failed to close() %s : %s",
                    trl_filename, strerror(errno));
      }
      buffer[0] = '\n';
      buffer[1] = '\n';
      buffer[2 + file_size] = '\0';

      old_no_of_hosts = no_of_hosts;
      if ((trlc = malloc((old_no_of_hosts * sizeof(struct trl_cache)))) == NULL)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to malloc() %d bytes : %s",
                    (old_no_of_hosts * sizeof(struct trl_cache)),
                    strerror(errno));
         free(buffer);
         return;
      }
      for (i = 0; i < old_no_of_hosts; i++)
      {
         trlc[i].pos = -1;
      }

      /* Serch for headers. */
      ptr = buffer;
      while ((ptr = posi(ptr, "\n\n[")) != NULL)
      {
         p_start = ptr - 1;
         while ((*ptr != ']') && (*ptr != '\0'))
         {
            ptr++;
         }
         if (*ptr == ']')
         {
            if (no_of_trl_groups == 0)
            {
               if ((trlg = malloc(sizeof(struct trl_group))) == NULL)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to malloc() %d bytes : %s",
                             sizeof(struct trl_group), strerror(errno));
                  free(trlc);
                  trlc = NULL;
                  break;
               }
            }
            else
            {
               if ((trlg = realloc(trlg,
                                   ((no_of_trl_groups + 1) * sizeof(struct trl_group)))) == NULL)
               {
                  system_log(ERROR_SIGN, __FILE__, __LINE__,
                             "Failed to realloc() %d bytes : %s",
                             ((no_of_trl_groups + 1) * sizeof(struct trl_group)),
                             strerror(errno));
                  free(trlc);
                  trlc = NULL;
                  break;
               }
            }
            trlg[no_of_trl_groups].no_of_hosts = 0;
            trlg[no_of_trl_groups].fsa_pos = NULL;
            trlg[no_of_trl_groups].limit = 0;
            length = ptr - p_start;
            if ((trlg[no_of_trl_groups].group_name = malloc((length + 1))) == NULL)
            {
               system_log(ERROR_SIGN, __FILE__, __LINE__,
                          "Failed to malloc() %d bytes : %s",
                          (ptr + 1 - p_start), strerror(errno));
               free(trlc);
               trlc = NULL;
               break;
            }
            (void)memcpy(trlg[no_of_trl_groups].group_name, p_start, length);
            trlg[no_of_trl_groups].group_name[length] = '\0';
            ptr++;

            do
            {
               while (*ptr == '\n')
               {
                  ptr++;
               }
               p_start = ptr;
               while ((*ptr != '=') && (*ptr != ' ') &&
                      (*ptr != '\n') && (*ptr != '\0'))
               {
                  ptr++;
               }
               length = ptr - p_start;
               if (((*ptr == '=') || (*ptr == ' ')) &&
                   ((length == TRL_MEMBER_ID_LENGTH) ||
                    (length == TRL_LIMIT_ID_LENGTH)))
               {
                  /*
                   *                   MEMBERS
                   *                   =======
                   * Insert all members. Check that the given host is in
                   * the FSA, oterwise just ignore it. Format is as follows:
                   *        members=host1,host2,host3
                   */
                  if (memcmp(p_start, TRL_MEMBER_ID, TRL_MEMBER_ID_LENGTH) == 0)
                  {
                     int  with_wildcards = NO;
                     char tmp_char;

                     while ((*ptr == ' ') || (*ptr == '='))
                     {
                        ptr++;
                     }

                     do
                     {
                        while ((*ptr == ',') || (*ptr == ' '))
                        {
                           ptr++;
                        }
                        p_start = ptr;
                        while ((*ptr != ',') && (*ptr != '\n'))
                        {
                           if ((*ptr == '*') || (*ptr == '?'))
                           {
                              with_wildcards = YES;
                           }
                           ptr++;
                        }
                        if (p_start != ptr)
                        {
                           tmp_char = *ptr;
                           *ptr = '\0';
                           for (i = 0; i < no_of_hosts; i++)
                           {
                              if (pmatch(p_start, fsa[i].host_alias, NULL) == 0)
                              {
                                 if (trlg[no_of_trl_groups].no_of_hosts == 0)
                                 {
                                    if ((trlg[no_of_trl_groups].fsa_pos = malloc(sizeof(int))) == NULL)
                                    {
                                       system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                  "Failed to malloc() %d bytes : %s",
                                                  sizeof(int), strerror(errno));
                                       free(buffer);
                                       free(trlc);
                                       trlc = NULL;
                                       return;
                                    }
                                 }
                                 else
                                 {
                                    int gotcha = NO,
                                        j;

                                    /*
                                     * First ensure that this host is not
                                     * already in the list.
                                     */
                                    for (j = 0; j < trlg[no_of_trl_groups].no_of_hosts; j++)
                                    {
                                       if (trlg[no_of_trl_groups].fsa_pos[j] == i)
                                       {
                                          system_log(WARN_SIGN, __FILE__, __LINE__,
                                                     "Duplicate host alias entry in transfer rate list for host `%s', ignoring.",
                                                     fsa[i].host_alias);
                                          gotcha = YES;
                                          break;
                                       }
                                    }
                                    if (gotcha == NO)
                                    {
                                       if ((trlg[no_of_trl_groups].fsa_pos = realloc(trlg[no_of_trl_groups].fsa_pos, ((trlg[no_of_trl_groups].no_of_hosts + 1) * sizeof(int)))) == NULL)
                                       {
                                          system_log(ERROR_SIGN, __FILE__, __LINE__,
                                                     "Failed to realloc() %d bytes : %s",
                                                     sizeof(int), strerror(errno));
                                          free(buffer);
                                          free(trlc);
                                          trlc = NULL;
                                          return;
                                       }
                                    }
                                    else
                                    {
                                       continue;
                                    }
                                 }
                                 trlg[no_of_trl_groups].fsa_pos[trlg[no_of_trl_groups].no_of_hosts] = i;
                                 trlg[no_of_trl_groups].no_of_hosts++;
                                 if ((trlc[i].pos != -1) &&
                                     (trlc[i].pos < no_of_trl_groups))
                                 {
                                    system_log(WARN_SIGN, __FILE__, __LINE__,
                                               "Host `%s' is already in group `%s'. Having the same host in multiple group will produce incorrect transfer rate limits.",
                                               fsa[i].host_alias, trlg[trlc[i].pos].group_name);
                                 }
                                 trlc[i].pos = no_of_trl_groups;
                                 if (with_wildcards == NO)
                                 {
                                    i = no_of_hosts;
                                 }
                              }
                           }
                           *ptr = tmp_char;
                        }
                     } while (*ptr == ',');
                  }

                       /*
                        *                  LIMIT
                        *                  =====
                        * Insert the limit in bytes per second. Format is
                        * as follows:
                        *     limit=10240
                        */
                  else if (memcmp(p_start, TRL_LIMIT_ID, TRL_LIMIT_ID_LENGTH) == 0)
                       {
                          while ((*ptr == ' ') || (*ptr == '='))
                          {
                             ptr++;
                          }
                          p_start = ptr;
                          while (isdigit((int)(*ptr)))
                          {
                             ptr++;
                          }
                          if (p_start != ptr)
                          {
                             char tmp_char = *ptr;

                             *ptr = '\0';
#ifdef HAVE_STRTOULL
# if SIZEOF_OFF_T == 4
                             trlg[no_of_trl_groups].limit = (off_t)strtoul(p_start, NULL, 10) / 1024;
# else
                             trlg[no_of_trl_groups].limit = (off_t)strtoull(p_start, NULL, 10) / 1024;
# endif
#else
                             trlg[no_of_trl_groups].limit = (off_t)strtoul(p_start, NULL, 10) / 1024;
#endif
                             *ptr = tmp_char;
                          }
                          while ((*ptr != '\n') && (*ptr != '\0'))
                          {
                             ptr++;
                          }
                       }
                       else
                       {
                          /* Ignore this since we do not know what it is. */
                          while (*ptr != '\n')
                          {
                             ptr++;
                          }
                       }
               }
               else
               {
                  /* Ignore this since we do not know what it is. */
                  while (*ptr != '\n')
                  {
                     ptr++;
                  }
               }
            } while (((ptr + 1) < (buffer + file_size)) &&
                     (*(ptr + 1) != '\n') && (*ptr != '\0'));
            no_of_trl_groups++;
         }
         else
         {
            system_log(WARN_SIGN, __FILE__, __LINE__,
                       "Unable to find terminating ] in header. Ignoring rest of file.");
            break;
         }
      }
      free(buffer);

#ifdef TRL_DEBUG
      {
         int i, j, length;
         char buffer[1024];

         for (i = 0; i < no_of_trl_groups; i++)
         {
            system_log(DEBUG_SIGN, NULL, 0, "[%s]", trlg[i].group_name);
            length = sprintf(buffer, "%s=%s",
                             TRL_MEMBER_ID, fsa[trlg[i].fsa_pos[0]].host_alias);
            for (j = 1; j < trlg[i].no_of_hosts; j++)
            {
               length += sprintf(buffer + length, ",%s",
                                 fsa[trlg[i].fsa_pos[j]].host_alias);
            }
            system_log(DEBUG_SIGN, NULL, 0, "%s", buffer);
            system_log(DEBUG_SIGN, NULL, 0, "%s=%lld",
                       TRL_LIMIT_ID, trlg[i].limit);
         }
      }
#endif
   }
   return;
}


/*########################## check_trl_file() ###########################*/
void
check_trl_file(void)
{
   struct stat stat_buf;

   if (trl_filename == NULL)
   {
      size_t length;

      length = strlen(p_work_dir) + sizeof(ETC_DIR) - 1 + sizeof(TRL_FILENAME) + 1;
      if ((trl_filename = malloc(length)) == NULL)
      {
         system_log(WARN_SIGN, __FILE__, __LINE__,
                    "Failed to malloc() %d bytes : %s",
                    length, strerror(length));
         return;
      }
      (void)sprintf(trl_filename, "%s%s/%s", p_work_dir, ETC_DIR, TRL_FILENAME);
   }
   if (stat(trl_filename, &stat_buf) == -1)
   {
      if ((errno == ENOENT) && (no_of_trl_groups != 0))
      {
         int i;

         /* Free all memory of old file. */
         for (i = 0; i < no_of_trl_groups; i++)
         {
            free(trlg[i].group_name);
            free(trlg[i].fsa_pos);
         }
         free(trlg);
         trlg = NULL;
         no_of_trl_groups = 0;

         system_log(INFO_SIGN, NULL, 0,
                    "Group transfer rate limit file `%s' away, resetting limits.",
                    trl_filename);
         for (i = 0; i < no_of_hosts; i++)
         {
            calc_trl_per_process(i);
         }
      }
   }
   else
   {
      if ((stat_buf.st_mtime != trl_file_mtime) && (stat_buf.st_size > 0))
      {
         int i;

         system_log(INFO_SIGN, NULL, 0,
                    "Rereading group transfer rate limit file `%s'.",
                    trl_filename);
         init_trl_data();

         /* TRL file changed, so we must recalculate the limits. */
         for (i = 0; i < no_of_hosts; i++)
         {
            /*
             * NOTE: Lets recalculate everything, since we do not
             *       know what changed. Some hosts might have been
             *       in a group but with the new TRL file are no
             *       longer in a group and such a case must be set
             *       to zero.
             */
            calc_trl_per_process(i);
         }
      }
   }

   return;
}


/*####################### calc_trl_per_process() ########################*/
void
calc_trl_per_process(int fsa_pos)
{
   if ((no_of_trl_groups > 0) && (trlc[fsa_pos].pos != -1))
   {
      int   active_transfers = 0,
            i;
      off_t tmp_trl_per_process;

      for (i = 0; i < trlg[trlc[fsa_pos].pos].no_of_hosts; i++)
      {
         if ((fsa[trlg[trlc[fsa_pos].pos].fsa_pos[i]].active_transfers > 0) &&
             (fsa[trlg[trlc[fsa_pos].pos].fsa_pos[i]].transfer_rate_limit > 0))
         {
            trlc[trlg[trlc[fsa_pos].pos].fsa_pos[i]].trl_per_process = fsa[trlg[trlc[fsa_pos].pos].fsa_pos[i]].transfer_rate_limit/
                                                                       fsa[trlg[trlc[fsa_pos].pos].fsa_pos[i]].active_transfers;
            if (trlc[trlg[trlc[fsa_pos].pos].fsa_pos[i]].trl_per_process == 0)
            {
               trlc[trlg[trlc[fsa_pos].pos].fsa_pos[i]].trl_per_process = 1;
            }
         }
         else
         {
            trlc[trlg[trlc[fsa_pos].pos].fsa_pos[i]].trl_per_process = 0;
         }
         trlc[trlg[trlc[fsa_pos].pos].fsa_pos[i]].gotcha = NO;
         active_transfers += fsa[trlg[trlc[fsa_pos].pos].fsa_pos[i]].active_transfers;
      }
      if (active_transfers > 1)
      {
         off_t limit;

         limit = trlg[trlc[fsa_pos].pos].limit;
         do
         {
            tmp_trl_per_process = limit / active_transfers;
            if (tmp_trl_per_process == 0)
            {
               tmp_trl_per_process = 1;
            }
            for (i = 0; i < trlg[trlc[fsa_pos].pos].no_of_hosts; i++)
            {
               if ((trlc[trlg[trlc[fsa_pos].pos].fsa_pos[i]].trl_per_process > 0) &&
                   (trlc[trlg[trlc[fsa_pos].pos].fsa_pos[i]].gotcha != YES))
               {
                  if (trlc[trlg[trlc[fsa_pos].pos].fsa_pos[i]].trl_per_process < tmp_trl_per_process)
                  {
                     active_transfers -= fsa[trlg[trlc[fsa_pos].pos].fsa_pos[i]].active_transfers;
                     limit -= fsa[trlg[trlc[fsa_pos].pos].fsa_pos[i]].transfer_rate_limit;
                     trlc[trlg[trlc[fsa_pos].pos].fsa_pos[i]].gotcha = YES;
                     break;
                  }
                  else
                  {
                     trlc[trlg[trlc[fsa_pos].pos].fsa_pos[i]].trl_per_process = tmp_trl_per_process;
                  }
               }
            }
         } while ((i < trlg[trlc[fsa_pos].pos].no_of_hosts) &&
                  (active_transfers > 0));
      }
      else
      {
         if ((fsa[fsa_pos].transfer_rate_limit > 0) &&
             (fsa[fsa_pos].transfer_rate_limit < trlg[trlc[fsa_pos].pos].limit))
         {
            tmp_trl_per_process = fsa[fsa_pos].transfer_rate_limit;
         }
         else
         {
            tmp_trl_per_process = trlg[trlc[fsa_pos].pos].limit;
         }
      }
      for (i = 0; i < trlg[trlc[fsa_pos].pos].no_of_hosts; i++)
      {
         if (trlc[trlg[trlc[fsa_pos].pos].fsa_pos[i]].gotcha == YES)
         {
            fsa[trlg[trlc[fsa_pos].pos].fsa_pos[i]].trl_per_process = trlc[trlg[trlc[fsa_pos].pos].fsa_pos[i]].trl_per_process;
         }
         else
         {
            fsa[trlg[trlc[fsa_pos].pos].fsa_pos[i]].trl_per_process = tmp_trl_per_process;
         }
      }
   }
   else
   {
      if ((fsa[fsa_pos].active_transfers > 1) &&
          (fsa[fsa_pos].transfer_rate_limit > 0))
      {
         fsa[fsa_pos].trl_per_process = fsa[fsa_pos].transfer_rate_limit /
                                        fsa[fsa_pos].active_transfers;
         if (fsa[fsa_pos].trl_per_process == 0)
         {
            fsa[fsa_pos].trl_per_process = 1;
         }
         fsa[fsa_pos].mc_ctrl_per_process = fsa[fsa_pos].mc_ct_rate_limit /
                                            fsa[fsa_pos].active_transfers;
         if (fsa[fsa_pos].mc_ctrl_per_process == 0)
         {
            fsa[fsa_pos].mc_ctrl_per_process = 1;
         }
      }
      else
      {
         fsa[fsa_pos].trl_per_process = fsa[fsa_pos].transfer_rate_limit;
         fsa[fsa_pos].mc_ctrl_per_process = fsa[fsa_pos].mc_ct_rate_limit;
      }
   }
#ifdef TRL_DEBUG
   system_log(DEBUG_SIGN, NULL, 0, "fsa[%d].trl_per_process = %lld",
              fsa_pos, fsa[fsa_pos].trl_per_process);
#endif

   return;
}
